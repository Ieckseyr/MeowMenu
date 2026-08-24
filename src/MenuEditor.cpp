// MenuEditor.cpp - 管理员工菜单编辑器实现
#include "MenuEditor.h"
#include "Config.h"
#include "MenuStore.h"
#include "MenuData.h"
#include "TriggerItem.h"
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
using ll::form::ModalForm;
using ll::form::ModalFormResult;
using ll::form::ModalFormSelectedButton;

namespace MenuEditor {

// 获取菜单副本（用于编辑）
static std::shared_ptr<Menu> getMenuForEdit(std::string const& menuId) {
    return MenuStore::getInstance().getMenu(menuId);
}

// 保存菜单并通知玩家
static void saveMenuAndNotify(Player& player, Menu const& menu) {
    if (MenuStore::getInstance().saveMenu(menu)) {
        player.sendMessage("§a[MeowMenu] 菜单 " + menu.id + " 已保存");
    } else {
        player.sendMessage("§c[MeowMenu] 菜单 " + menu.id + " 保存失败");
    }
}

//================================================================
// 菜单列表
//================================================================
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

//================================================================
// 编辑指定菜单
//================================================================
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
        "§7标题: {}\n§7内容: {}\n§7按钮数: {}\n§7DogeUI: {}",
        menu->title, menu->content, menu->buttons.size(),
        menu->dogeui.enabled ? "§a启用(前缀:" + menu->dogeui.prefix + ")" : "§c禁用"
    ));

    fm.appendButton("§l§e编辑标题和内容", "textures/ui/icon_text_edit", "path",
        [menuId](Player& pl) { showEditTitleContent(pl, menuId); });

    fm.appendButton("§l§a添加按钮", "textures/ui/icon_plus", "path",
        [menuId](Player& pl) { showAddButton(pl, menuId); });

    fm.appendButton("§l§b从手持物品设置图标\n§7选择按钮后用手持物品作为图标", "textures/ui/icon_hand", "path",
        [menuId](Player& pl) { showIconFromHandSelector(pl, menuId); });

    // 列出现有按钮
    for (int i = 0; i < (int)menu->buttons.size(); ++i) {
        auto const& btn = menu->buttons[i];
        std::string btnText = fmt::format("§l§6[{}]\n§7{} | {}", i, btn.text, btn.type);
        fm.appendButton(btnText, [menuId, i](Player& pl) { showEditButton(pl, menuId, i); });
    }

    // 删除按钮（仅当有按钮时显示）
    if (!menu->buttons.empty()) {
        fm.appendButton("§l§c删除按钮\n§7选择要删除的按钮",
            [menuId](Player& pl) { showDeleteButtonSelector(pl, menuId); });
    }

    fm.appendButton("§l§dDogeUI 设置\n§7配置 DogeUI 标识符", "textures/ui/icon_spring", "path",
        [menuId](Player& pl) { showDogeUISettings(pl, menuId); });

    fm.appendButton("§l§c删除菜单\n§7永久删除此菜单（不可恢复）",
        [menuId](Player& pl) { showDeleteMenuConfirm(pl, menuId); });

    fm.appendButton("§l§c返回\n§7返回菜单列表", "textures/ui/cancel", "path",
        [](Player& pl) { showMenuList(pl); });

    fm.sendTo(player);
}

//================================================================
// 编辑标题和内容
//================================================================
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

//================================================================
// 添加按钮
//================================================================
void showAddButton(Player& player, std::string const& menuId) {
    auto fm = CustomForm{"§l§b添加按钮"};
    fm.appendInput("text", "§e按钮文本", "string", "§a新按钮");
    fm.appendInput("icon", "§b图标路径(可选,留空则使用物品图标)", "string", "textures/ui/icon_setting");
    fm.appendInput("iconItem", "§b图标物品ID(可选,如 minecraft:dirt)", "string", "");
    fm.appendInput("type", "§d按钮类型", "string", "command");
    fm.appendInput("value", "§6按钮值(命令/子菜单ID)", "string", "");
    fm.appendInput("item", "§7箱子UI物品(可选)", "string", "");

    fm.sendTo(player, [menuId](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        if (!res.has_value()) { showEditMenu(pl, menuId); return; }
        auto menu = getMenuForEdit(menuId);
        if (!menu) { pl.sendMessage("§c菜单不存在"); return; }

        Button btn;
        try {
            btn.text     = std::get<std::string>(res->at("text"));
            btn.icon     = std::get<std::string>(res->at("icon"));
            btn.iconItem = std::get<std::string>(res->at("iconItem"));
            btn.type     = std::get<std::string>(res->at("type"));
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
        showEditMenu(pl, menuId);
    });
}

//================================================================
// 编辑按钮
//================================================================
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
    fm.appendInput("type", "§d按钮类型", "string", btn.type);
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
            menu->buttons[buttonIndex].type     = std::get<std::string>(res->at("type"));
            menu->buttons[buttonIndex].value    = std::get<std::string>(res->at("value"));
            menu->buttons[buttonIndex].item     = std::get<std::string>(res->at("item"));
        } catch (...) {}

        // 保存前校验并修复图标路径
        if (!menu->buttons[buttonIndex].icon.empty()) {
            menu->buttons[buttonIndex].icon = IconValidator::getInstance().normalizeIcon(menu->buttons[buttonIndex].icon);
        }

        saveMenuAndNotify(pl, *menu);
        showEditMenu(pl, menuId);
    });
}

//================================================================
// 删除按钮选择器
//================================================================
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

//================================================================
// 删除菜单二次确认
//================================================================
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

//================================================================
// 从手持物品设置按钮图标（选择按钮列表）
//================================================================
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

//================================================================
// 从手持物品设置指定按钮的图标
//================================================================
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

        // 校验物品 ID 是否在 texture_path.json 中有对应纹理
        auto& validator = IconValidator::getInstance();
        std::string texPath = validator.getTexturePathFor(itemType);

        // 设置按钮的 iconItem
        menu->buttons[buttonIndex].iconItem = itemType;

        // 提示用户当前状态
        if (texPath.empty()) {
            player.sendMessage(fmt::format(
                "§e[MeowMenu] 已设置按钮 [{}] 的图标物品为 §b{}\n§7注意: 该物品在 texture_path.json 中未找到对应纹理，图标可能无法显示",
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

//================================================================
// DogeUI 设置（每菜单独立）
//================================================================
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

//================================================================
// 物品设置
//================================================================
void showItemSettings(Player& player) {
    auto const& cfg = ConfigManager::getInstance().getConfig();
    auto const& triggerCfg = cfg.triggerItem;

    auto fm = CustomForm{"§l§b物品设置"};
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

//================================================================
// 从手持物品设置
//================================================================
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
        cfg.triggerItem.displayName = handItem.hasCustomHoverName() ? handItem.getCustomName() : cfg.triggerItem.type;
        cfg.triggerItem.count = handItem.mCount;

        // 获取 SNBT
        try {
            auto saveCtx = SaveContextFactory::createDiskSaveContext();
            if (saveCtx) {
                auto nbt = handItem.save(*saveCtx);
                if (nbt) {
                    cfg.triggerItem.snbt = nbt->toSnbt();
                }
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

//================================================================
// 编辑器主界面
//================================================================
void showEditorMain(Player& player) {
    auto fm = SimpleForm{};
    fm.setTitle("§l§bMeowMenu 编辑器");
    fm.setContent("§7请选择一个功能");

    fm.appendButton("§l§e菜单列表\n§7查看/编辑/删除菜单", "textures/ui/icon_book_writable", "path",
        [](Player& pl) { showMenuList(pl); });

    fm.appendButton("§l§d物品设置\n§7配置触发物品", "textures/ui/icon_recipe_nature", "path",
        [](Player& pl) { showItemSettings(pl); });

    fm.appendButton("§l§a从手持物品设置\n§7将手持物品设为触发物品", "textures/ui/icon_hand", "path",
        [](Player& pl) { setItemFromHand(pl); });

    fm.sendTo(player);
}

} // namespace MenuEditor

} // namespace meowmenu
