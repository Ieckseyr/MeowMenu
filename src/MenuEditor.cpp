// MenuEditor.cpp - 管理员工菜单编辑器实现
#include "MenuEditor.h"
#include "Config.h"
#include "MenuStore.h"
#include "MenuData.h"
#include "TriggerItem.h"
#include "MeowMenu.h"

#include <ll/api/form/SimpleForm.h>
#include <ll/api/form/CustomForm.h>

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

    // 列出现有按钮
    for (int i = 0; i < (int)menu->buttons.size(); ++i) {
        auto const& btn = menu->buttons[i];
        std::string btnText = fmt::format("§l§6[{}]\n§7{} | {}", i, btn.text, btn.type);
        fm.appendButton(btnText, [menuId, i](Player& pl) { showEditButton(pl, menuId, i); });
    }

    fm.appendButton("§l§dDogeUI 设置\n§7配置 DogeUI 标识符", "textures/ui/icon_spring", "path",
        [menuId](Player& pl) { showDogeUISettings(pl, menuId); });

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
    fm.appendInput("icon", "§b图标路径(可选)", "string", "textures/ui/icon_setting");
    fm.appendInput("type", "§d按钮类型", "string", "command");
    fm.appendInput("value", "§6按钮值(命令/子菜单ID)", "string", "");
    fm.appendInput("item", "§7箱子UI物品(可选)", "string", "");

    fm.sendTo(player, [menuId](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        if (!res.has_value()) { showEditMenu(pl, menuId); return; }
        auto menu = getMenuForEdit(menuId);
        if (!menu) { pl.sendMessage("§c菜单不存在"); return; }

        Button btn;
        try {
            btn.text = std::get<std::string>(res->at("text"));
            btn.icon = std::get<std::string>(res->at("icon"));
            btn.type = std::get<std::string>(res->at("type"));
            btn.value = std::get<std::string>(res->at("value"));
            btn.item = std::get<std::string>(res->at("item"));
        } catch (...) {}

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
    fm.appendInput("icon", "§b图标路径", "string", btn.icon);
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
            menu->buttons[buttonIndex].text = std::get<std::string>(res->at("text"));
            menu->buttons[buttonIndex].icon = std::get<std::string>(res->at("icon"));
            menu->buttons[buttonIndex].type = std::get<std::string>(res->at("type"));
            menu->buttons[buttonIndex].value = std::get<std::string>(res->at("value"));
            menu->buttons[buttonIndex].item = std::get<std::string>(res->at("item"));
        } catch (...) {}
        saveMenuAndNotify(pl, *menu);
        showEditMenu(pl, menuId);
    });
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
