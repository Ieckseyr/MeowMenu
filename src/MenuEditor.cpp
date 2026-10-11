// MenuEditor.cpp - 管理员工菜单编辑器实现
#include "MenuEditor.h"
#include "Config.h"
#include "MenuStore.h"
#include "MenuData.h"
#include "TriggerItem.h"
#include "NpcMenuBridge.h"
#include "MeowMenu.h"
#include "IconValidator.h"

#include <ll/api/form/SimpleForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/ModalForm.h>

#include <mc/world/actor/player/Player.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/SaveContextFactory.h>
#include <mc/deps/nbt/CompoundTag.h>

#include <fmt/format.h>

#include <memory>
#include <string>
#include <vector>

namespace meowmenu {

using ll::form::SimpleForm;
using ll::form::CustomForm;
using ll::form::CustomFormResult;
using ll::form::CustomFormElementResult;
using ll::form::ModalForm;
using ll::form::ModalFormResult;
using ll::form::ModalFormSelectedButton;

namespace MenuEditor {

// 可选按钮类型（下拉, 避免手打类型名打错导致点击无反应）
static std::vector<std::string> const kButtonTypeValues = {
    "command", "admin_command", "submenu", "admin_submenu", "folder", "npc_menu", "npc_spawn"
};
static std::vector<std::string> const kButtonTypeLabels = {
    "普通命令 (command)",
    "管理命令 (admin_command)",
    "子菜单 (submenu)",
    "管理子菜单 (admin_submenu)",
    "自动建子菜单 (folder)",
    "NPC 对话打开 (npc_menu)",
    "生成菜单NPC (npc_spawn)",
};

// 当前类型 → 下拉下标（不在表内则追加一项, 保证编辑时不丢类型）
static std::vector<std::string> typeOptionsFor(std::string const& current, size_t& outIndex) {
    std::vector<std::string> options = kButtonTypeLabels;
    for (size_t i = 0; i < kButtonTypeValues.size(); ++i) {
        if (kButtonTypeValues[i] == current) {
            outIndex = i;
            return options;
        }
    }
    options.push_back("§7(显示项/其他) " + current);
    outIndex = kButtonTypeValues.size();
    return options;
}

// 下拉下标 → 类型名
static std::string typeFromIndex(uint64_t index) {
    if (index < kButtonTypeValues.size()) return kButtonTypeValues[static_cast<size_t>(index)];
    return "command";
}

static uint64_t formResultToIndex(CustomFormElementResult const& v) {
    if (auto p = std::get_if<unsigned long long>(&v)) return *p;
    if (auto p = std::get_if<double>(&v)) return static_cast<uint64_t>(*p);
    return 0;
}

// 获取菜单副本（用于编辑）
static std::shared_ptr<Menu> getMenuForEdit(std::string const& menuId) {
    return MenuStore::getInstance().getMenu(menuId);
}

// 去掉首尾空白
static std::string trim(std::string s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string{};
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// 菜单分级的可读文本（编辑页 / 分级页共用）
static std::string describeLevel(Menu const& menu, std::string const& menuId, std::string const& mainMenuId) {
    if (menu.isSecondary()) {
        return "§e二级菜单§7(只能从其它菜单进入)";
    }
    if (menuId == mainMenuId) {
        return "§a一级菜单§7(主菜单 · 触发物品固定为钟)";
    }
    if (!menu.triggerItem.configured()) {
        return "§a一级菜单§7(§c未配置触发物品§7 → 只能用菜单按钮打开)";
    }
    return "§a一级菜单§7(触发物品: §f" + menu.triggerItem.label() + "§7)";
}

// 保存菜单并通知玩家
static void saveMenuAndNotify(Player& player, Menu const& menu) {
    if (MenuStore::getInstance().saveMenu(menu)) {
        player.sendMessage("§a[MeowMenu] 菜单 " + menu.id + " 已保存");
    } else {
        player.sendMessage("§c[MeowMenu] 菜单 " + menu.id + " 保存失败");
    }
}

//==== 菜单列表 ====
void showMenuList(Player& player) {
    auto ids = MenuStore::getInstance().getAllMenuIds();
    if (ids.empty()) {
        player.sendMessage("§c[MeowMenu] 没有菜单");
        showEditorMain(player);
        return;
    }

    auto fm = SimpleForm{};
    fm.setTitle("§l§b菜单列表");
    fm.setContent(fmt::format("§7共 {} 个菜单", ids.size()));

    for (auto const& id : ids) {
        auto menu = MenuStore::getInstance().getMenu(id);
        std::string title = menu ? menu->title : id;
        std::string btnText = fmt::format("§l§e{}\n§7ID: {}", title, id);
        fm.appendButton(btnText, "textures/ui/icon_book_writable", "path",
            [id](Player& pl) { showEditMenu(pl, id); });
    }

    fm.appendButton("§l§c返回\n§7返回主菜单", "textures/ui/cancel", "path",
        [](Player& pl) { showEditorMain(pl); });

    fm.sendTo(player);
}

//==== 编辑指定菜单 ====
void showEditMenu(Player& player, std::string const& menuId) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在: " + menuId);
        showMenuList(player);
        return;
    }

    auto fm = SimpleForm{};
    fm.setTitle("§l§b编辑菜单: " + menuId);
    fm.setContent(fmt::format(
        "§7标题: {}\n§7内容: {}\n§7按钮数: {}\n§7分级: {}\n§7DogeUI: {}\n§7NPC 对话模式: {}",
        menu->title, menu->content, menu->buttons.size(),
        describeLevel(*menu, menuId, ConfigManager::getInstance().getConfig().mainMenuId),
        menu->dogeui.enabled ? "§a启用(前缀:" + menu->dogeui.prefix + ")" : "§c禁用",
        menu->isNpcMode() ? fmt::format("§a启用(NPC名:{} 按钮:{}/6)",
                                        menu->npc.name.empty() ? "菜单标题" : menu->npc.name,
                                        menu->clickableButtonCount())
                          : "§c禁用"
    ));

    fm.appendButton("§l§e编辑标题和内容", "textures/ui/book_addtextpage_default", "path",
        [menuId](Player& pl) { showEditTitleContent(pl, menuId); });

    fm.appendButton("§l§a添加按钮", "textures/ui/plus", "path",
        [menuId](Player& pl) { showAddButton(pl, menuId); });

    fm.appendButton("§l§b从手持物品设置图标\n§7选择按钮后用手持物品作为图标", "textures/ui/subcategory_icons/hands", "path",
        [menuId](Player& pl) { showIconFromHandSelector(pl, menuId); });

    fm.appendButton("§l§b显示模式\n§7跟随玩家 / 表单 / NPC 对话 / 箱子", "textures/ui/icon_book_writable", "path",
        [menuId](Player& pl) { showUiModeSettings(pl, menuId); });

    fm.appendButton("§l§b菜单分级 / 触发物品\n§7一级: 可用触发物品打开 / 二级: 只能从菜单进入", "textures/ui/icon_map", "path",
        [menuId](Player& pl) { showMenuLevelSettings(pl, menuId); });

    fm.appendButton("§l§bNPC 对话设置\n§7对话框 NPC 名 / 话语正文（NPC 样式用）", "textures/ui/icon_steve", "path",
        [menuId](Player& pl) { showNpcSettings(pl, menuId); });

    // 列出现有按钮
    for (int i = 0; i < (int)menu->buttons.size(); ++i) {
        auto const& btn = menu->buttons[i];
        std::string btnText = fmt::format("§l§6[{}] §r{}\n§7{}", i, btn.text, btn.type);
        fm.appendButton(btnText, [menuId, i](Player& pl) { showButtonActions(pl, menuId, i); });
    }

    // 删除按钮（仅当有按钮时显示）
    if (!menu->buttons.empty()) {
        fm.appendButton("§l§c删除按钮\n§7选择要删除的按钮",
            [menuId](Player& pl) { showDeleteButtonSelector(pl, menuId); });
    }

    fm.appendButton("§l§dDogeUI 设置\n§7配置 DogeUI 标识符", "textures/ui/icons/icon_spring", "path",
        [menuId](Player& pl) { showDogeUISettings(pl, menuId); });

    fm.appendButton("§l§c删除菜单\n§7永久删除此菜单（不可恢复）",
        [menuId](Player& pl) { showDeleteMenuConfirm(pl, menuId); });

    fm.appendButton("§l§c返回\n§7返回菜单列表", "textures/ui/cancel", "path",
        [](Player& pl) { showMenuList(pl); });

    fm.sendTo(player);
}

//==== 编辑标题和内容 ====
void showEditTitleContent(Player& player, std::string const& menuId) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在");
        return;
    }

    auto fm = CustomForm{"§l§b编辑标题和内容"};
    fm.appendInput("title", "§e菜单标题", "string", menu->title);
    fm.appendInput("content", "§e菜单内容", "string", menu->content);

    fm.sendTo(player, [menuId](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        if (!res.has_value()) { showEditMenu(pl, menuId); return; }
        auto menu = getMenuForEdit(menuId);
        if (!menu) { pl.sendMessage("§c菜单不存在"); return; }

        try {
            menu->title = std::get<std::string>(res->at("title"));
            menu->content = std::get<std::string>(res->at("content"));
        } catch (...) {}
        saveMenuAndNotify(pl, *menu);
        showEditMenu(pl, menuId);
    });
}

//==== 添加菜单项（类型选择） ====
void showAddButton(Player& player, std::string const& menuId) {
    auto fm = SimpleForm{};
    fm.setTitle("§l§b添加菜单项");
    fm.setContent("§7选择要添加的项类型: §f普通按钮走命令/子菜单, §bNPC 按钮§f把菜单呈现成 NPC 界面, §e显示项§f用于排版分组");

    fm.appendButton("§l§a普通按钮\n§7命令 / 管理命令 / 子菜单等", "textures/ui/icon_setting", "path",
        [menuId](Player& pl) { showAddButtonForm(pl, menuId); });
    fm.appendButton("§l§bNPC 菜单按钮§7（需 HologramLib）\n§b以 NPC 对话呈现§7 / §a生成菜单 NPC", "textures/ui/icon_steve", "path",
        [menuId](Player& pl) { showAddNpcButtonType(pl, menuId); });
    fm.appendButton("§l§6大字体标题行§7（header）\n§e分组大标题, 不可点击", "textures/ui/icon_book_writable", "path",
        [menuId](Player& pl) { showAddDisplayItemForm(pl, menuId, "header"); });
    fm.appendButton("§l§b文本行§7（label）\n§7普通说明文字, 不可点击", "textures/ui/icon_sign", "path",
        [menuId](Player& pl) { showAddDisplayItemForm(pl, menuId, "label"); });
    fm.appendButton("§l§d分隔线§7（divider）\n§7水平分隔线", "textures/ui/divider", "path",
        [menuId](Player& pl) { showAddDisplayItemForm(pl, menuId, "divider"); });

    fm.appendButton("§l§c返回\n§7返回菜单编辑", "textures/ui/cancel", "path",
        [menuId](Player& pl) { showEditMenu(pl, menuId); });

    fm.sendTo(player);
}

//==== 添加 NPC 菜单按钮（npc_menu = NPC 对话呈现; npc_spawn = 生成可右键的 NPC） ====
void showAddNpcButtonType(Player& player, std::string const& menuId) {
    auto fm = SimpleForm{};
    fm.setTitle("§l§bNPC 菜单按钮");
    fm.setContent(
        "§7需要服务器安装 §fHologramLib§7（可选前置; 未安装时按钮会提示, 不会无反应）\n"
        "§7两者都用 §6value§7 指定要呈现的菜单（留空 = 主菜单）"
    );

    fm.appendButton("§l§b以 NPC 对话打开§7（npc_menu）\n§7原版 NPC 对话框形态, 子菜单逐层进入并可返回", "textures/ui/icon_book_writable", "path",
        [menuId](Player& pl) { showAddNpcButtonForm(pl, menuId, "npc_menu"); });
    fm.appendButton("§l§a生成菜单 NPC§7（npc_spawn）\n§7在身前生成 NPC, 右键它即打开菜单（重启后自动重建）", "textures/ui/icon_steve", "path",
        [menuId](Player& pl) { showAddNpcButtonForm(pl, menuId, "npc_spawn"); });
    fm.appendButton("§l§c返回\n§7返回添加菜单项", "textures/ui/cancel", "path",
        [menuId](Player& pl) { showAddButton(pl, menuId); });

    fm.sendTo(player);
}

//==== 添加 NPC 菜单按钮（表单） ====
void showAddNpcButtonForm(Player& player, std::string const& menuId, std::string const& npcType) {
    bool isSpawn = npcType == "npc_spawn";

    auto fm = CustomForm{isSpawn ? "§l§a添加菜单 NPC 按钮" : "§l§b添加 NPC 对话按钮"};
    fm.appendInput("text", "§e按钮文本", "string", isSpawn ? "§a生成菜单 NPC" : "§a以 NPC 对话打开");
    fm.appendInput(
        "icon",
        "§b图标路径(可选)",
        "string",
        isSpawn ? "textures/ui/icon_steve" : "textures/ui/icon_book_writable"
    );
    fm.appendInput("value", "§6目标菜单ID§7(留空 = 主菜单)", "string", menuId);
    fm.appendInput("type", "§d按钮类型§7(勿改)", "string", npcType);

    fm.sendTo(player, [menuId](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        if (!res.has_value()) { showAddButton(pl, menuId); return; }
        auto menu = getMenuForEdit(menuId);
        if (!menu) { pl.sendMessage("§c菜单不存在"); return; }

        Button btn;
        try {
            btn.text  = std::get<std::string>(res->at("text"));
            btn.icon  = std::get<std::string>(res->at("icon"));
            btn.type  = std::get<std::string>(res->at("type"));
            btn.value = std::get<std::string>(res->at("value"));
        } catch (...) {}

        if (btn.type != "npc_menu" && btn.type != "npc_spawn") {
            pl.sendMessage("§c按钮类型被改成 " + btn.type + " 了, 请从「添加菜单项 → NPC 菜单按钮」重来");
            showAddButton(pl, menuId);
            return;
        }
        if (!btn.icon.empty()) {
            btn.icon = IconValidator::getInstance().normalizeIcon(btn.icon);
        }
        // 目标菜单不存在则自动建一个空的（与 submenu 的处理一致, 便于随后补内容）
        if (!btn.value.empty() && !MenuStore::getInstance().getMenu(btn.value)) {
            MenuStore::getInstance().createMenu(btn.value, btn.text, "");
        }

        menu->buttons.push_back(btn);
        saveMenuAndNotify(pl, *menu);
        showEditMenu(pl, menuId);
    });
}

//==== 添加普通按钮（表单） ====
void showAddButtonForm(Player& player, std::string const& menuId) {
    auto fm = CustomForm{"§l§b添加按钮"};
    fm.appendInput("text", "§e按钮文本", "string", "§a新按钮");
    fm.appendInput("icon", "§b图标路径(可选,留空则使用物品图标)", "string", "textures/ui/icon_setting");
    fm.appendInput("iconItem", "§b图标物品ID(可选,如 minecraft:dirt)", "string", "");
    size_t typeIndex = 0;
    fm.appendDropdown("type", "§d按钮类型", typeOptionsFor("command", typeIndex), typeIndex);
    fm.appendInput("value", "§6按钮值(命令/子菜单ID)", "string", "");
    fm.appendInput("item", "§7箱子UI物品(可选)", "string", "");

    fm.sendTo(player, [menuId](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        if (!res.has_value()) { showAddButton(pl, menuId); return; }
        auto menu = getMenuForEdit(menuId);
        if (!menu) { pl.sendMessage("§c菜单不存在"); return; }

        Button btn;
        try {
            btn.text     = std::get<std::string>(res->at("text"));
            btn.icon     = std::get<std::string>(res->at("icon"));
            btn.iconItem = std::get<std::string>(res->at("iconItem"));
            btn.type     = typeFromIndex(formResultToIndex(res->at("type")));
            btn.value    = std::get<std::string>(res->at("value"));
            btn.item     = std::get<std::string>(res->at("item"));
        } catch (...) {}

        // 保存前校验并修复图标路径
        if (!btn.icon.empty()) {
            btn.icon = IconValidator::getInstance().normalizeIcon(btn.icon);
        }

        // 如果是 folder 类型，自动创建子菜单
        if (btn.type == "folder" && btn.value.empty()) {
            btn.value = menuId + "_folder_" + std::to_string(std::time(nullptr));
            MenuStore::getInstance().createMenu(btn.value, btn.text, "");
        }
        // 如果是 submenu/admin_submenu，自动创建目标菜单
        if ((btn.type == "submenu" || btn.type == "admin_submenu") && !btn.value.empty()) {
            if (!MenuStore::getInstance().getMenu(btn.value)) {
                MenuStore::getInstance().createMenu(btn.value, btn.text, "");
            }
        }

        menu->buttons.push_back(btn);
        saveMenuAndNotify(pl, *menu);

        // 没填任何图标 → 直接进图标选择器挑一个（库内路径必然合法, 避免手打导致的破图）
        if (btn.icon.empty() && btn.iconItem.empty()) {
            showIconPickerCategories(pl, menuId, static_cast<int>(menu->buttons.size()) - 1);
            return;
        }
        showEditMenu(pl, menuId);
    });
}

//==== 添加显示项（header / label / divider） ====
void showAddDisplayItemForm(Player& player, std::string const& menuId, std::string const& itemType) {
    std::string label;
    std::string defText;
    if (itemType == "header") {
        label   = "§e大字体标题行文本";
        defText = "§l§e分组标题";
    } else if (itemType == "label") {
        label   = "§b文本行内容";
        defText = "§7说明文字";
    } else {
        label   = "§d分隔线无需文本§7（此输入将被忽略）";
        defText = "";
    }

    auto fm = CustomForm{"§l§b添加显示项"};
    fm.appendInput("text", label, "string", defText);

    fm.sendTo(player, [menuId, itemType](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        if (!res.has_value()) { showAddButton(pl, menuId); return; }
        auto menu = getMenuForEdit(menuId);
        if (!menu) { pl.sendMessage("§c菜单不存在"); return; }

        Button btn;
        btn.type = itemType;
        try {
            btn.text = std::get<std::string>(res->at("text"));
        } catch (...) {}

        menu->buttons.push_back(btn);
        saveMenuAndNotify(pl, *menu);
        showEditMenu(pl, menuId);
    });
}

//==== 编辑按钮 ====
void showEditButton(Player& player, std::string const& menuId, int buttonIndex) {
    auto menu = getMenuForEdit(menuId);
    if (!menu || buttonIndex < 0 || buttonIndex >= (int)menu->buttons.size()) {
        player.sendMessage("§c[MeowMenu] 按钮不存在");
        showEditMenu(player, menuId);
        return;
    }

    auto const& btn = menu->buttons[buttonIndex];

    auto fm = CustomForm{"§l§b编辑按钮 " + std::to_string(buttonIndex)};
    fm.appendInput("text", "§e按钮文本", "string", btn.text);
    fm.appendInput("icon", "§b图标路径(留空则使用物品图标)", "string", btn.icon);
    fm.appendInput("iconItem", "§b图标物品ID(如 minecraft:dirt)", "string", btn.iconItem);
    size_t typeIndex = 0;
    fm.appendDropdown("type", "§d按钮类型", typeOptionsFor(btn.type, typeIndex), typeIndex);
    fm.appendInput("value", "§6按钮值", "string", btn.value);
    fm.appendInput("item", "§7箱子UI物品", "string", btn.item);

    fm.sendTo(player, [menuId, buttonIndex](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        if (!res.has_value()) { showEditMenu(pl, menuId); return; }
        auto menu = getMenuForEdit(menuId);
        if (!menu || buttonIndex < 0 || buttonIndex >= (int)menu->buttons.size()) {
            pl.sendMessage("§c按钮不存在");
            return;
        }

        try {
            menu->buttons[buttonIndex].text     = std::get<std::string>(res->at("text"));
            menu->buttons[buttonIndex].icon     = std::get<std::string>(res->at("icon"));
            menu->buttons[buttonIndex].iconItem = std::get<std::string>(res->at("iconItem"));
            menu->buttons[buttonIndex].type     = typeFromIndex(formResultToIndex(res->at("type")));
            menu->buttons[buttonIndex].value    = std::get<std::string>(res->at("value"));
            menu->buttons[buttonIndex].item     = std::get<std::string>(res->at("item"));
        } catch (...) {}

        // 保存前校验并修复图标路径
        if (!menu->buttons[buttonIndex].icon.empty()) {
            menu->buttons[buttonIndex].icon = IconValidator::getInstance().normalizeIcon(menu->buttons[buttonIndex].icon);
        }

        // 改成 NPC 按钮时, 目标菜单不存在则自动建一个空的（与新增路径一致）
        {
            auto const& edited = menu->buttons[buttonIndex];
            if ((edited.type == "npc_menu" || edited.type == "npc_spawn") && !edited.value.empty()
                && !MenuStore::getInstance().getMenu(edited.value)) {
                MenuStore::getInstance().createMenu(edited.value, edited.text, "");
            }
        }

        saveMenuAndNotify(pl, *menu);
        showEditMenu(pl, menuId);
    });
}

//==== 删除按钮选择器 ====
void showDeleteButtonSelector(Player& player, std::string const& menuId) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在");
        showMenuList(player);
        return;
    }

    if (menu->buttons.empty()) {
        player.sendMessage("§c[MeowMenu] 此菜单没有按钮");
        showEditMenu(player, menuId);
        return;
    }

    auto fm = SimpleForm{};
    fm.setTitle("§l§c删除按钮");
    fm.setContent("§7请选择要删除的按钮\n§c提示: 删除后不可恢复");

    for (int i = 0; i < (int)menu->buttons.size(); ++i) {
        auto const& btn = menu->buttons[i];
        std::string btnText = fmt::format("§l§c[{}]\n§7{} | {}", i, btn.text, btn.type);
        fm.appendButton(btnText, [menuId, i](Player& pl) {
            // 二次确认
            auto m = getMenuForEdit(menuId);
            if (!m || i < 0 || i >= (int)m->buttons.size()) {
                pl.sendMessage("§c[MeowMenu] 按钮不存在");
                showDeleteButtonSelector(pl, menuId);
                return;
            }
            std::string content = fmt::format(
                "§7确定要删除按钮 [§e{}§7] 吗？\n§7文本: §f{}\n§7类型: §f{}\n§c此操作不可恢复！",
                i, m->buttons[i].text, m->buttons[i].type);
            ModalForm confirm("§l§c删除按钮", content, "§c确认删除", "取消");
            confirm.sendTo(pl, [menuId, i](Player& p, ModalFormResult res, ll::form::FormCancelReason) {
                if (res.has_value() && *res == ModalFormSelectedButton::Upper) {
                    auto mm = getMenuForEdit(menuId);
                    if (mm && i >= 0 && i < (int)mm->buttons.size()) {
                        mm->buttons.erase(mm->buttons.begin() + i);
                        saveMenuAndNotify(p, *mm);
                    } else {
                        p.sendMessage("§c[MeowMenu] 按钮不存在，删除失败");
                    }
                    showEditMenu(p, menuId);
                } else {
                    // 取消，回到删除选择器
                    showDeleteButtonSelector(p, menuId);
                }
            });
        });
    }

    fm.appendButton("§l§a返回\n§7返回菜单编辑", "textures/ui/cancel", "path",
        [menuId](Player& pl) { showEditMenu(pl, menuId); });

    fm.sendTo(player);
}

//==== 删除菜单二次确认 ====
void showDeleteMenuConfirm(Player& player, std::string const& menuId) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在");
        showMenuList(player);
        return;
    }

    std::string content = fmt::format(
        "§7确定要删除菜单 §e{} §7吗？\n§7标题: §f{}\n§7按钮数: §f{}\n§c此操作不可恢复，菜单文件将被删除！",
        menuId, menu->title, menu->buttons.size());

    ModalForm confirm("§l§c删除菜单", content, "§c确认删除", "取消");
    confirm.sendTo(player, [menuId](Player& pl, ModalFormResult res, ll::form::FormCancelReason) {
        if (res.has_value() && *res == ModalFormSelectedButton::Upper) {
            if (MenuStore::getInstance().deleteMenu(menuId)) {
                pl.sendMessage("§a[MeowMenu] 菜单 " + menuId + " 已删除");
            } else {
                pl.sendMessage("§c[MeowMenu] 删除菜单 " + menuId + " 失败");
            }
            showMenuList(pl);
        } else {
            // 取消，回到菜单编辑
            showEditMenu(pl, menuId);
        }
    });
}

//==== 从手持物品设置按钮图标（选择按钮列表） ====
void showIconFromHandSelector(Player& player, std::string const& menuId) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在");
        showMenuList(player);
        return;
    }

    if (menu->buttons.empty()) {
        player.sendMessage("§c[MeowMenu] 此菜单没有按钮，请先添加按钮");
        showEditMenu(player, menuId);
        return;
    }

    auto fm = SimpleForm{};
    fm.setTitle("§l§b从手持物品设置图标");
    fm.setContent("§7请选择要设置图标的按钮\n§e提示: 请先将要作为图标的物品拿在手上");

    for (int i = 0; i < (int)menu->buttons.size(); ++i) {
        auto const& btn = menu->buttons[i];
        std::string currentIcon;
        if (!btn.icon.empty()) {
            currentIcon = "§a路径: " + btn.icon;
        } else if (!btn.iconItem.empty()) {
            currentIcon = "§d物品: " + btn.iconItem;
        } else {
            currentIcon = "§c无图标";
        }
        std::string btnText = fmt::format("§l§6[{}]\n§7{}\n§7{}", i, btn.text, currentIcon);
        fm.appendButton(btnText, [menuId, i](Player& pl) { setButtonIconFromHand(pl, menuId, i); });
    }

    fm.appendButton("§l§c返回\n§7返回菜单编辑", "textures/ui/cancel", "path",
        [menuId](Player& pl) { showEditMenu(pl, menuId); });

    fm.sendTo(player);
}

//==== 从手持物品设置指定按钮的图标 ====
void setButtonIconFromHand(Player& player, std::string const& menuId, int buttonIndex) {
    auto menu = getMenuForEdit(menuId);
    if (!menu || buttonIndex < 0 || buttonIndex >= (int)menu->buttons.size()) {
        player.sendMessage("§c[MeowMenu] 按钮不存在");
        showIconFromHandSelector(player, menuId);
        return;
    }

    try {
        auto& handItem = player.getCarriedItem();
        if (handItem.isNull()) {
            player.sendMessage("§c[MeowMenu] 请先手持要作为图标的物品");
            showIconFromHandSelector(player, menuId);
            return;
        }

        std::string itemType = handItem.getTypeName();
        if (itemType.empty()) {
            player.sendMessage("§c[MeowMenu] 无法获取手持物品类型");
            showIconFromHandSelector(player, menuId);
            return;
        }

        // 校验物品 ID 在内置图标库中是否有对应纹理
        auto& validator = IconValidator::getInstance();
        std::string texPath = validator.getTexturePathFor(itemType);

        // 设置按钮的 iconItem
        menu->buttons[buttonIndex].iconItem = itemType;

        // 提示用户当前状态
        if (texPath.empty()) {
            player.sendMessage(fmt::format(
                "§e[MeowMenu] 已设置按钮 [{}] 的图标物品为 §b{}\n§7注意: 该物品在图标库中未找到对应纹理，图标可能无法显示",
                buttonIndex, itemType));
        } else {
            player.sendMessage(fmt::format(
                "§a[MeowMenu] 已设置按钮 [{}] 的图标物品为 §b{} §a(纹理: {})",
                buttonIndex, itemType, texPath));
        }

        // 如果按钮已有 icon 文本路径，提示优先级
        if (!menu->buttons[buttonIndex].icon.empty()) {
            player.sendMessage(fmt::format(
                "§e注意: 此按钮已设置图标路径 §b{}§e, 渲染时将优先使用路径。\n§7如需使用物品图标, 请清空图标路径。",
                menu->buttons[buttonIndex].icon));
        }

        saveMenuAndNotify(player, *menu);
        showIconFromHandSelector(player, menuId);
    } catch (std::exception const& e) {
        player.sendMessage(fmt::format("§c[MeowMenu] 获取手持物品失败: {}", e.what()));
        showIconFromHandSelector(player, menuId);
    }
}

// 表单控件结果取 bool（toggle 在 LL 里是 uint64 0/1; 这里容忍几种可能类型）
static bool formResultToBool(CustomFormElementResult const& v) {
    if (auto p = std::get_if<unsigned long long>(&v)) return *p != 0;
    if (auto p = std::get_if<double>(&v)) return *p != 0;
    if (auto p = std::get_if<std::string>(&v)) return *p == "true" || *p == "1";
    return false;
}

//==== 按钮操作页（编辑 / 排序 / 图标 / 删除） ====
void showButtonActions(Player& player, std::string const& menuId, int buttonIndex) {
    auto menu = getMenuForEdit(menuId);
    if (!menu || buttonIndex < 0 || buttonIndex >= static_cast<int>(menu->buttons.size())) {
        player.sendMessage("§c[MeowMenu] 按钮不存在");
        showEditMenu(player, menuId);
        return;
    }
    auto const& btn = menu->buttons[buttonIndex];

    auto fm = SimpleForm{};
    fm.setTitle(fmt::format("§l§b按钮 [{}]", buttonIndex));
    fm.setContent(fmt::format(
        "§7文本: §f{}\n§7类型: §f{}\n§7值: §f{}\n§7图标: §f{}\n§7位置: §f{} / {}",
        btn.text,
        btn.type,
        btn.value,
        btn.icon.empty() ? (btn.iconItem.empty() ? "§8(无)" : btn.iconItem) : btn.icon,
        buttonIndex + 1,
        menu->buttons.size()
    ));

    fm.appendButton("§l§e编辑内容", "textures/ui/book_addtextpage_default", "path",
        [menuId, buttonIndex](Player& pl) { showEditButton(pl, menuId, buttonIndex); });

    fm.appendButton("§l§a↑ 上移一位", [menuId, buttonIndex](Player& pl) {
        auto m = getMenuForEdit(menuId);
        if (!m || buttonIndex <= 0 || buttonIndex >= static_cast<int>(m->buttons.size())) return;
        std::swap(m->buttons[buttonIndex - 1], m->buttons[buttonIndex]);
        saveMenuAndNotify(pl, *m);
        showButtonActions(pl, menuId, buttonIndex - 1);
    });

    fm.appendButton("§l§a↓ 下移一位", [menuId, buttonIndex](Player& pl) {
        auto m = getMenuForEdit(menuId);
        if (!m || buttonIndex < 0 || buttonIndex + 1 >= static_cast<int>(m->buttons.size())) return;
        std::swap(m->buttons[buttonIndex], m->buttons[buttonIndex + 1]);
        saveMenuAndNotify(pl, *m);
        showButtonActions(pl, menuId, buttonIndex + 1);
    });

    fm.appendButton("§l§b从图标库挑图标\n§7分类浏览 / 搜索，候选按真实纹理显示", "textures/ui/magnifying_glass", "path",
        [menuId, buttonIndex](Player& pl) { showIconPickerCategories(pl, menuId, buttonIndex); });

    fm.appendButton("§l§b用手中物品作图标", "textures/ui/subcategory_icons/hands", "path",
        [menuId, buttonIndex](Player& pl) { setButtonIconFromHand(pl, menuId, buttonIndex); });

    if (menu->buttons.size() > 1) {
        fm.appendButton("§l§e移动到指定位置\n§7按钮多时比逐格上移/下移快", "textures/ui/icon_map", "path",
            [menuId, buttonIndex](Player& pl) { showMoveButtonTo(pl, menuId, buttonIndex); });
    }

    fm.appendButton("§l§c删除此按钮", [menuId, buttonIndex](Player& pl) {
        auto m = getMenuForEdit(menuId);
        if (!m || buttonIndex < 0 || buttonIndex >= static_cast<int>(m->buttons.size())) return;
        std::string text = m->buttons[buttonIndex].text;
        auto       confirm = ModalForm{};
        confirm.setTitle("§l§c删除确认");
        confirm.setContent(fmt::format("§7确定删除按钮 [§e{}§7] 吗？\n§7文本: §f{}\n§c此操作不可恢复", buttonIndex, text));
        confirm.setUpperButton("§c删除");
        confirm.setLowerButton("§7取消");
        confirm.sendTo(pl, [menuId, buttonIndex](Player& p2, ModalFormResult res, ll::form::FormCancelReason) {
            if (!res.has_value() || res.value() != ModalFormSelectedButton::Upper) {
                showButtonActions(p2, menuId, buttonIndex);
                return;
            }
            auto mm = getMenuForEdit(menuId);
            if (!mm || buttonIndex < 0 || buttonIndex >= static_cast<int>(mm->buttons.size())) return;
            mm->buttons.erase(mm->buttons.begin() + buttonIndex);
            saveMenuAndNotify(p2, *mm);
            showEditMenu(p2, menuId);
        });
    });

    fm.appendButton("§l§7返回菜单编辑", [menuId](Player& pl) { showEditMenu(pl, menuId); });
    fm.sendTo(player);
}

// 允许样式集合的可读文本（空 = 三种都允许）
static std::string joinModes(std::vector<std::string> const& allowed) {
    if (allowed.empty()) return "全部（表单 / NPC 对话 / 箱子）";
    std::string out = "表单";
    for (auto const& mode : allowed) {
        if (mode == "npc") out += " / NPC 对话";
        else if (mode == "chest") out += " / 箱子";
    }
    return out;
}

// 切换"某个样式是否允许"（表单恒允许; 三种都允许时清空数组 = 不写该字段）
void toggleUiAllowed(Player& player, std::string const& menuId, std::string const& mode) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) return;

    bool npcAllowed   = menu->allowsUiMode("npc");
    bool chestAllowed = menu->allowsUiMode("chest");
    if (mode == "npc") npcAllowed = !npcAllowed;
    if (mode == "chest") chestAllowed = !chestAllowed;

    std::vector<std::string> allowed{"form"};
    if (npcAllowed) allowed.push_back("npc");
    if (chestAllowed) allowed.push_back("chest");

    menu->uiAllowed = (npcAllowed && chestAllowed) ? std::vector<std::string>{} : allowed;

    // 当前选定的样式若已不被允许 → 退回表单
    if (!menu->allowsUiMode(menu->renderMode)) menu->renderMode = "default";

    saveMenuAndNotify(player, *menu);
    player.sendMessage("§a[MeowMenu] 允许的样式已更新: §f" + joinModes(menu->uiAllowed));
    showUiModeSettings(player, menuId);
}

//==== 显示模式（菜单自身样式; 玩家可用 /mmenu ui 覆盖） ====
void showUiModeSettings(Player& player, std::string const& menuId) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在: " + menuId);
        showMenuList(player);
        return;
    }
    auto& bridge = NpcMenuBridge::getInstance();

    struct ModeOption {
        char const* mode;   // 写入 renderMode 的值
        char const* label;
        char const* desc;
        bool        available;
    };
    std::vector<ModeOption> const options = {
        {"default", "§f普通表单", "SimpleForm 表单（始终可用）", true},
        {"npc", "§bNPC 对话", "原版 NPC 对话框; 最多 6 个按钮（需 HologramLib）", bridge.isAvailable()},
        {"chest", "§6箱子界面", "虚拟箱子; 格子即按钮, 最多 54 格（需 HologramLib）", bridge.isChestAvailable()},
    };

    auto fm = SimpleForm{};
    fm.setTitle("§l§b显示模式: " + menuId);
    fm.setContent(fmt::format(
        "§7当前菜单配置: §f{}\n"
        "§7说明: 这里设置的是**菜单自身**的样式; 玩家可以用 §a/mmenu ui §7选择自己喜欢的样式覆盖它。\n"
        "§7可点击按钮: §f{}{}",
        menu->renderMode,
        menu->clickableButtonCount(),
        menu->hasEditorEntry()
            ? "\n§e本菜单含菜单编辑器入口 → 已固定为表单样式（硬规则, 这里的设置对它不生效）"
            : ""
    ));

    for (auto const& option : options) {
        bool isCurrent = menu->renderMode == option.mode
                         || (option.mode[0] == 'd' && menu->renderMode != "npc" && menu->renderMode != "chest");
        std::string text = fmt::format(
            "{}{}\n§7{}",
            isCurrent ? "§a " : "§l",
            option.label,
            option.available ? option.desc : "§c需要服务器安装 HologramLib"
        );
        std::string mode = option.mode;
        fm.appendButton(text, [menuId, mode](Player& pl) {
            auto m = getMenuForEdit(menuId);
            if (!m) return;
            if (!m->allowsUiMode(mode)) {
                pl.sendMessage("§c[MeowMenu] 该菜单被锁定为只能使用: §f" + joinModes(m->uiAllowed));
                return;
            }
            m->renderMode = mode;
            saveMenuAndNotify(pl, *m);
            showUiModeSettings(pl, menuId);
        });
    }

    // 固有约束: 该菜单允许被哪些样式呈现（表单永远允许; 典型用途: 编辑器这类界面只能走 JSON UI）
    fm.appendButton(
        fmt::format("§l§e固有约束\n§7允许的样式: §f{}", joinModes(menu->uiAllowed)),
        [menuId](Player& pl) {
            auto m = getMenuForEdit(menuId);
            if (!m) return;
            bool npcAllowed   = m->allowsUiMode("npc");
            bool chestAllowed = m->allowsUiMode("chest");

            auto fm2 = SimpleForm{};
            fm2.setTitle("§l§e固有约束: " + menuId);
            fm2.setContent(
                "§7这里决定该菜单**允许**被哪些样式呈现。\n"
                "§7玩家在 §a/mmenu ui §7里选的样式若不在允许范围内, 对这个菜单不生效。\n"
                "§7表单是任何情况下的兜底渲染, 不能被关闭。"
            );
            fm2.appendButton(
                fmt::format("{}NPC 对话§7(最多 6 按钮 / 不解析 § 颜色码)", npcAllowed ? "§a " : "§c✘ "),
                [menuId](Player& p2) { toggleUiAllowed(p2, menuId, "npc"); }
            );
            fm2.appendButton(
                fmt::format("{}箱子界面§7(格子即按钮 / 支持染色)", chestAllowed ? "§a " : "§c✘ "),
                [menuId](Player& p2) { toggleUiAllowed(p2, menuId, "chest"); }
            );
            fm2.appendButton("§l§c返回显示模式", [menuId](Player& p2) { showUiModeSettings(p2, menuId); });
            fm2.sendTo(pl);
        }
    );

    fm.appendButton("§l§c返回菜单编辑", [menuId](Player& pl) { showEditMenu(pl, menuId); });
    fm.sendTo(player);
}

//==== 菜单分级 / 触发物品: 一级可被触发物品打开; 二级只能从别处进入 ====

// 触发物品类型规范化: 空串 = 清除, 无命名空间按 minecraft: 补全, 钟留给主菜单
static bool validateTriggerType(
    Player&             player,
    std::string const&  menuId,
    std::string         input,
    std::string&        outType
) {
    std::string type = trim(input);
    if (type.empty()) {
        outType.clear();
        return true;
    }
    if (type.find(':') == std::string::npos) {
        type = "minecraft:" + type;
    }

    if (type == "minecraft:clock") {
        player.sendMessage("§c[MeowMenu] 钟固定用于打开主菜单, 换一个物品吧（例如 minecraft:compass）");
        return false;
    }

    std::string owner = MenuStore::getInstance().findMenuByTriggerItem(type);
    if (!owner.empty() && owner != menuId) {
        player.sendMessage(fmt::format(
            "§c[MeowMenu] 物品 §f{} §c已经被菜单 §f{} §c用作触发物品了（一个物品只能绑一个一级菜单）",
            type,
            owner
        ));
        return false;
    }

    outType = type;
    return true;
}

void showMenuLevelSettings(Player& player, std::string const& menuId) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在: " + menuId);
        showMenuList(player);
        return;
    }

    bool isMain = (menuId == ConfigManager::getInstance().getConfig().mainMenuId);

    std::string triggerLine;
    if (isMain) {
        triggerLine = "§7触发物品: §f钟 minecraft:clock §7（主菜单固定, 进服自动发放）";
    } else if (menu->isSecondary()) {
        triggerLine = menu->triggerItem.configured()
                          ? fmt::format(
                                "§7触发物品: §8{}§7（已填写, §c但二级菜单不生效§7）",
                                menu->triggerItem.label()
                            )
                          : "§7触发物品: §8未配置§7（二级菜单用不到）";
    } else {
        triggerLine = menu->triggerItem.configured()
                          ? fmt::format(
                                "§7触发物品: §f{} §7（类型 §8{}§7）",
                                menu->triggerItem.label(),
                                menu->triggerItem.type
                            )
                          : "§7触发物品: §c未配置§7 → 现在只能用菜单按钮打开它";
    }

    auto fm = SimpleForm{};
    fm.setTitle("§l§b菜单分级: " + menuId);
    fm.setContent(fmt::format(
        "§7当前: {}\n{}\n\n"
        "§7一级菜单§7: 既能被其它菜单打开, 也能用触发物品直接打开\n"
        "§7二级菜单§7: 只能从其它菜单进入, 触发物品填了也不生效\n"
        "§7主菜单§7: 默认的一级菜单, 触发物品固定是钟",
        menu->isPrimary() ? "§a一级菜单" : "§e二级菜单",
        triggerLine
    ));

    // 分级切换（主菜单固定一级: 它的触发物品就是钟, 标成二级会让玩家手里的钟失效）
    if (isMain) {
        fm.appendLabel("§8主菜单固定为一级菜单（触发物品: 钟, 进服自动发放）");
    } else if (menu->isPrimary()) {
        fm.appendButton("§l§e改为二级菜单\n§7只能从其它菜单进入（触发物品不再生效）", [menuId](Player& pl) {
            auto m = getMenuForEdit(menuId);
            if (!m) return;
            m->level = 2;
            saveMenuAndNotify(pl, *m);
            pl.sendMessage("§7已改为二级菜单: 触发物品即使填了也不会生效");
            showMenuLevelSettings(pl, menuId);
        });
    } else {
        fm.appendButton("§l§a改为一级菜单\n§7可以用触发物品直接打开（需要填触发物品）", [menuId](Player& pl) {
            auto m = getMenuForEdit(menuId);
            if (!m) return;
            m->level = 1;
            saveMenuAndNotify(pl, *m);
            pl.sendMessage("§7已改为一级菜单: 记得设置触发物品, 否则只能用菜单按钮打开");
            showMenuLevelSettings(pl, menuId);
        });
    }

    // 触发物品（只有一级菜单用得着; 主菜单固定用钟）
    if (menu->isPrimary() && !isMain) {
        fm.appendButton("§l§b设置触发物品\n§7手输物品ID, 如 minecraft:compass", "textures/ui/icon_recipe_item", "path",
            [menuId](Player& pl) { showTriggerItemForm(pl, menuId); });
        fm.appendButton("§l§a从手持物品设置触发物品\n§7拿在手上, 点这里即可", "textures/ui/subcategory_icons/hands", "path",
            [menuId](Player& pl) { setTriggerItemFromHand(pl, menuId); });
        if (menu->triggerItem.configured()) {
            fm.appendButton("§l§c清除触发物品\n§7清除后只能用菜单按钮打开", [menuId](Player& pl) {
                clearTriggerItem(pl, menuId);
            });
        }
    }
    if (menu->isSecondary() && menu->triggerItem.configured()) {
        fm.appendButton("§l§c清除已填的触发物品\n§7二级菜单用不到, 可清掉避免误解", [menuId](Player& pl) {
            clearTriggerItem(pl, menuId);
        });
    }

    fm.appendButton("§l§c返回菜单编辑", [menuId](Player& pl) { showEditMenu(pl, menuId); });
    fm.sendTo(player);
}

// 公开版本: 新建菜单表单（MenuEditorTools）也走同一套校验
bool validateTriggerItemType(
    Player&             player,
    std::string const&  menuId,
    std::string const&  input,
    std::string&        outType
) {
    return validateTriggerType(player, menuId, input, outType);
}

void showTriggerItemForm(Player& player, std::string const& menuId) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在: " + menuId);
        showMenuList(player);
        return;
    }

    auto fm = CustomForm{"§l§b触发物品: " + menuId};
    fm.appendLabel("§7一级菜单的触发物品: 玩家手持它右键即可打开这个菜单");
    fm.appendInput("type", "§e物品类型ID§7(留空 = 清除)", "minecraft:compass", menu->triggerItem.type);
    fm.appendInput("displayName", "§b显示名§7(可选, 消息里用它)", "商店菜单", menu->triggerItem.displayName);

    fm.sendTo(player, [menuId](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        if (!res.has_value()) { showMenuLevelSettings(pl, menuId); return; }
        auto menu = getMenuForEdit(menuId);
        if (!menu) { pl.sendMessage("§c菜单不存在"); return; }

        std::string type, name;
        try {
            type = std::get<std::string>(res->at("type"));
            name = std::get<std::string>(res->at("displayName"));
        } catch (...) {}

        std::string checked;
        if (!validateTriggerType(pl, menuId, type, checked)) {
            showTriggerItemForm(pl, menuId);
            return;
        }

        menu->triggerItem.type        = checked;
        menu->triggerItem.displayName = checked.empty() ? std::string{} : trim(name);
        saveMenuAndNotify(pl, *menu);
        if (checked.empty()) {
            pl.sendMessage("§7已清除触发物品");
        } else {
            pl.sendMessage(fmt::format("§a[MeowMenu] 触发物品已设为 §f{}", menu->triggerItem.label()));
        }
        showMenuLevelSettings(pl, menuId);
    });
}

void setTriggerItemFromHand(Player& player, std::string const& menuId) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在: " + menuId);
        showMenuList(player);
        return;
    }

    std::string type;
    std::string customName;
    try {
        auto& handItem = player.getCarriedItem();
        if (handItem.isNull()) {
            player.sendMessage("§c[MeowMenu] 请先手持要作为触发物品的物品");
            showMenuLevelSettings(player, menuId);
            return;
        }
        type       = handItem.getTypeName();
        customName = handItem.getCustomName();
    } catch (std::exception const& e) {
        player.sendMessage(fmt::format("§c[MeowMenu] 获取手持物品失败: {}", e.what()));
        showMenuLevelSettings(player, menuId);
        return;
    }

    std::string checked;
    if (!validateTriggerType(player, menuId, type, checked) || checked.empty()) {
        showMenuLevelSettings(player, menuId);
        return;
    }

    menu->triggerItem.type        = checked;
    menu->triggerItem.displayName = trim(customName);
    saveMenuAndNotify(player, *menu);
    player.sendMessage(fmt::format("§a[MeowMenu] 触发物品已设为 §f{} §a(手持它右键即可打开本菜单)",
                                   menu->triggerItem.label()));
    showMenuLevelSettings(player, menuId);
}

void clearTriggerItem(Player& player, std::string const& menuId) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) return;
    menu->triggerItem = MenuTriggerItem{};
    saveMenuAndNotify(player, *menu);
    player.sendMessage("§7已清除触发物品: 本菜单只能用菜单按钮打开");
    showMenuLevelSettings(player, menuId);
}
//==== NPC 对话模式（可选前置 HologramLib）: 名/话语取自 npc.name / npc.dialogue ====
void showNpcSettings(Player& player, std::string const& menuId) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在: " + menuId);
        showMenuList(player);
        return;
    }

    auto& bridge    = NpcMenuBridge::getInstance();
    int   clickable = menu->clickableButtonCount();

    auto fm = CustomForm{"§l§bNPC 对话模式: " + menuId};
    fm.appendLabel(fmt::format(
        "§7前置 HologramLib: {}",
        bridge.isAvailable() ? "§a已连接" : "§c不可用(本模式会自动回退成普通表单)"
    ));
    fm.appendLabel(fmt::format(
        "§7本菜单可点击按钮: §f{} §7/ 对话框上限 §f6{}",
        clickable,
        clickable > 6 ? " §c(超出部分不显示, 建议移入子菜单)" : ""
    ));
    fm.appendLabel("§7关闭对话: 游戏内按 §fESC");
    fm.appendDivider();
    fm.appendToggle("enabled", "§e启用 NPC 对话模式", menu->npc.enabled, "开启后该菜单直接以 NPC 对话界面打开");
    fm.appendInput("name", "§b对话框里的 NPC 名", "村长", menu->npc.name);
    fm.appendInput("dialogue", "§a话语/正文 (用 \n 换行)", "欢迎来到服务器", menu->npc.dialogue);

    fm.sendTo(player, [menuId](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        if (!res.has_value()) { showEditMenu(pl, menuId); return; }
        auto edited = getMenuForEdit(menuId);
        if (!edited) { pl.sendMessage("§c菜单不存在"); return; }

        try {
            edited->npc.enabled  = formResultToBool(res->at("enabled"));
            edited->npc.name     = std::get<std::string>(res->at("name"));
            edited->npc.dialogue = std::get<std::string>(res->at("dialogue"));
        } catch (...) {}

        saveMenuAndNotify(pl, *edited);
        if (edited->npc.enabled) {
            pl.sendMessage(fmt::format(
                "§7已启用 NPC 对话模式 (按钮 §f{}/6§7)\n§7可用 §a/mnpc open {} §7预览, 或从菜单入口直接打开",
                edited->clickableButtonCount(),
                menuId
            ));
        }
        showEditMenu(pl, menuId);
    });
}

//==== DogeUI 设置（每菜单独立） ====
void showDogeUISettings(Player& player, std::string const& menuId) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在");
        return;
    }

    auto fm = CustomForm{"§l§dDogeUI 设置"};
    fm.appendLabel("§7为菜单 §e" + menuId + " §7配置 DogeUI 标识符");
    fm.appendLabel("§7启用后，前缀将添加到菜单标题前，由 DogeUI 资源包识别。");
    fm.appendToggle("enabled", "§a启用 DogeUI", menu->dogeui.enabled);
    fm.appendInput("prefix", "§bDogeUI 标识符前缀", "string", menu->dogeui.prefix);

    fm.sendTo(player, [menuId](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        if (!res.has_value()) { showEditMenu(pl, menuId); return; }
        auto menu = getMenuForEdit(menuId);
        if (!menu) { pl.sendMessage("§c菜单不存在"); return; }

        try {
            menu->dogeui.enabled = std::get<uint64_t>(res->at("enabled")) != 0;
            menu->dogeui.prefix = std::get<std::string>(res->at("prefix"));
            if (menu->dogeui.prefix.empty()) {
                menu->dogeui.prefix = "/D ";
            }
        } catch (...) {}
        saveMenuAndNotify(pl, *menu);
        showEditMenu(pl, menuId);
    });
}

//==== 物品设置 ====
void showItemSettings(Player& player) {
    auto const& cfg = ConfigManager::getInstance().getConfig();
    auto const& triggerCfg = cfg.triggerItem;

    auto fm = CustomForm{"§l§b物品设置"};
    fm.appendLabel("§7这里是内置钟物品（§f主菜单§7的触发物品, 进服自动发放）的显示名/消息占位;");
    fm.appendLabel("§7物品本体 NBT 固定, 不受这里影响。");
    fm.appendLabel("§7其它菜单要配触发物品: 菜单编辑页 → 「菜单分级 / 触发物品」");
    fm.appendInput("type", "§e物品类型ID", "string", triggerCfg.type);
    fm.appendInput("displayName", "§b显示名称", "string", triggerCfg.displayName);
    fm.appendInput("snbt", "§d物品SNBT(留空使用默认)", "string", triggerCfg.snbt);

    fm.sendTo(player, [](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        if (!res.has_value()) { showEditorMain(pl); return; }
        auto& cfg = ConfigManager::getInstance().getConfig();
        try {
            cfg.triggerItem.type = std::get<std::string>(res->at("type"));
            cfg.triggerItem.displayName = std::get<std::string>(res->at("displayName"));
            cfg.triggerItem.snbt = std::get<std::string>(res->at("snbt"));
        } catch (...) {}
        ConfigManager::getInstance().save();
        pl.sendMessage("§a[MeowMenu] 物品设置已保存");
        showEditorMain(pl);
    });
}

//==== 从手持物品设置 ====
void setItemFromHand(Player& player) {
    try {
        auto& handItem = player.getCarriedItem();
        if (handItem.isNull()) {
            player.sendMessage("§c[MeowMenu] 请先手持要设置的物品");
            showEditorMain(player);
            return;
        }

        auto& cfg = ConfigManager::getInstance().getConfig();
        cfg.triggerItem.type = handItem.getTypeName();
        // 26.32: hasCustomHoverName 已移除, 以 getCustomName() 非空判定自定义名
        std::string const customName = handItem.getCustomName();
        cfg.triggerItem.displayName  = !customName.empty() ? customName : cfg.triggerItem.type;
        cfg.triggerItem.count = handItem.mCount;

        // 获取 SNBT
        // 26.32: SaveContextFactory::createDiskSaveContext 已移除, 直接构造 SaveContext 并指定 SaveToDisk
        try {
            ::SaveContext saveCtx{};
            saveCtx.mSaveUseCase = ::SaveContext::SaveUseCase::SaveToDisk;
            auto nbt = handItem.save(saveCtx);
            if (nbt) {
                cfg.triggerItem.snbt = nbt->toSnbt();
            }
        } catch (...) {}

        ConfigManager::getInstance().save();
        player.sendMessage(fmt::format("§a[MeowMenu] 已设置物品为: §b{}", cfg.triggerItem.displayName));
        showEditorMain(player);
    } catch (std::exception const& e) {
        player.sendMessage(fmt::format("§c[MeowMenu] 获取手持物品失败: {}", e.what()));
        showEditorMain(player);
    }
}

//==== 编辑器主界面 ====
void showEditorMain(Player& player) {
    auto fm = SimpleForm{};
    fm.setTitle("§l§bMeowMenu 编辑器");
    fm.setContent("§7请选择一个功能");

    fm.appendButton("§l§a新建菜单\n§7填 ID / 标题 / 内容, 创建后直接编辑", "textures/ui/plus", "path",
        [](Player& pl) { showCreateMenuForm(pl); });

    fm.appendButton("§l§e菜单列表\n§7查看/编辑/删除菜单", "textures/ui/icon_book_writable", "path",
        [](Player& pl) { showMenuList(pl); });

    fm.appendButton("§l§d物品设置\n§7内置钟（主菜单触发物品）的显示名/消息占位", "textures/ui/icon_recipe_nature", "path",
        [](Player& pl) { showItemSettings(pl); });

    fm.appendButton("§l§a从手持物品设置\n§7把手中物品设为钟的显示名（不改变物品本体）", "textures/ui/subcategory_icons/hands", "path",
        [](Player& pl) { setItemFromHand(pl); });

    fm.sendTo(player);
}

} // namespace MenuEditor

} // namespace meowmenu
