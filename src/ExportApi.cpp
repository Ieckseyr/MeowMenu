// ExportApi.cpp - RemoteCall 导出实现（命名空间 "MeowMenu"; 一律 try/catch 兜底）
#include "ExportApi.h"
#include "Config.h"
#include "MenuStore.h"
#include "MenuData.h"
#include "MenuRenderer.h"
#include "MenuEditor.h"
#include "TriggerItem.h"
#include "ButtonHandler.h"
#include "IconValidator.h"
#include "ItemTrigger.h"
#include "MigrationManager.h"
#include "NpcMenuBridge.h"
#include "PlayerUiPref.h"
#include "MeowMenu.h"

#include "meowpapi/RemoteCallAPI.h"

#include <mc/world/actor/player/Player.h>
#include <mc/world/level/Level.h>
#include <ll/api/service/Bedrock.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <string>

namespace meowmenu {

// 插件就绪标志（registerAll 末尾置 true）
static std::atomic<bool> gReady{false};

// 统一玩家查找：按名称获取在线玩家
static Player* findPlayer(std::string const& name) {
    auto level = ll::service::getLevel();
    if (!level.has_value()) return nullptr;
    return level->getPlayer(name);
}

ExportApi& ExportApi::getInstance() {
    static ExportApi instance;
    return instance;
}

void ExportApi::registerAll() {
    registerMenuApi();
    registerButtonApi();
    registerRenderApi();
    registerTriggerItemApi();
    registerItemTriggerApi();
    registerButtonHandlerApi();
    registerConfigApi();
    registerIconApi();
    registerEditorApi();
    registerNpcMenuApi();
    registerUtilApi();
    registerMigrationApi();
    gReady.store(true);
}

//==== 菜单管理 API ====
void ExportApi::registerMenuApi() {
    // getMenuIds() -> string  (JSON 数组)
    RemoteCall::exportAs("MeowMenu", "getMenuIds", []() -> std::string {
        try {
            auto ids = MenuStore::getInstance().getAllMenuIds();
            nlohmann::json j = nlohmann::json::array();
            for (auto const& id : ids) j.push_back(id);
            return j.dump();
        } catch (...) {}
        return "[]";
    });

    // getMenu(menuId) -> string  (菜单 JSON，不存在返回 "{}")
    RemoteCall::exportAs("MeowMenu", "getMenu", [](std::string const& menuId) -> std::string {
        try {
            auto menu = MenuStore::getInstance().getMenu(menuId);
            if (!menu) return "{}";
            return menu->toJson().dump();
        } catch (...) {}
        return "{}";
    });

    // createMenu(id, title, content) -> bool
    RemoteCall::exportAs("MeowMenu", "createMenu",
        [](std::string const& id, std::string const& title, std::string const& content) -> bool {
            try {
                return MenuStore::getInstance().createMenu(id, title, content);
            } catch (...) {}
            return false;
        });

    // saveMenu(menuJson) -> bool  (整体覆盖保存，含按钮)
    RemoteCall::exportAs("MeowMenu", "saveMenu", [](std::string const& menuJson) -> bool {
        try {
            auto j = nlohmann::json::parse(menuJson);
            Menu menu = Menu::fromJson(j);
            // 校验图标
            for (auto& btn : menu.buttons) {
                if (!btn.icon.empty()) {
                    btn.icon = IconValidator::getInstance().normalizeIcon(btn.icon);
                }
            }
            return MenuStore::getInstance().saveMenu(menu);
        } catch (...) {}
        return false;
    });

    // deleteMenu(menuId) -> bool
    RemoteCall::exportAs("MeowMenu", "deleteMenu", [](std::string const& menuId) -> bool {
        try {
            return MenuStore::getInstance().deleteMenu(menuId);
        } catch (...) {}
        return false;
    });

    // reloadMenus() -> bool
    RemoteCall::exportAs("MeowMenu", "reloadMenus", []() -> bool {
        try {
            MenuStore::getInstance().loadAll();
            return true;
        } catch (...) {}
        return false;
    });

    // setMenuTitle(menuId, title) -> bool  (仅修改标题，保留其余字段)
    RemoteCall::exportAs("MeowMenu", "setMenuTitle",
        [](std::string const& menuId, std::string const& title) -> bool {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu) return false;
                Menu local = *menu;
                local.title = title;
                return MenuStore::getInstance().saveMenu(local);
            } catch (...) {}
            return false;
        });

    // setMenuContent(menuId, content) -> bool  (仅修改内容文本)
    RemoteCall::exportAs("MeowMenu", "setMenuContent",
        [](std::string const& menuId, std::string const& content) -> bool {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu) return false;
                Menu local = *menu;
                local.content = content;
                return MenuStore::getInstance().saveMenu(local);
            } catch (...) {}
            return false;
        });

    // setDogeUI(menuId, enabled, prefix) -> bool  (修改 DogeUI 配置)
    RemoteCall::exportAs("MeowMenu", "setDogeUI",
        [](std::string const& menuId, bool enabled, std::string const& prefix) -> bool {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu) return false;
                Menu local = *menu;
                local.dogeui.enabled = enabled;
                local.dogeui.prefix  = prefix.empty() ? std::string("/D ") : prefix;
                return MenuStore::getInstance().saveMenu(local);
            } catch (...) {}
            return false;
        });

    // createMenuWithLevel(id, title, content, level) -> bool  (新建菜单并指定分级: 1 = 一级, 2 = 二级)
    RemoteCall::exportAs("MeowMenu", "createMenuWithLevel",
        [](std::string const& id, std::string const& title, std::string const& content, int level) -> bool {
            try {
                return MenuStore::getInstance().createMenu(id, title, content, level);
            } catch (...) {}
            return false;
        });

    // setMenuLevel(menuId, level) — 修改菜单分级（二级不被触发物品打开）
    RemoteCall::exportAs("MeowMenu", "setMenuLevel",
        [](std::string const& menuId, int level) -> bool {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu) return false;
                Menu local = *menu;
                local.level = (level == 2) ? 2 : 1;
                return MenuStore::getInstance().saveMenu(local);
            } catch (...) {}
            return false;
        });

    // setMenuTriggerItem(menuId, itemType, displayName) — 设置一级菜单的触发物品（空串 = 清除）
    RemoteCall::exportAs("MeowMenu", "setMenuTriggerItem",
        [](std::string const& menuId, std::string const& itemType, std::string const& displayName) -> bool {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu) return false;
                Menu local = *menu;
                local.triggerItem.type        = itemType;
                local.triggerItem.displayName = displayName;
                return MenuStore::getInstance().saveMenu(local);
            } catch (...) {}
            return false;
        });

    // getMenuLevel(menuId) -> int  (1 = 一级菜单, 2 = 二级菜单; 菜单不存在返回 -1)
    RemoteCall::exportAs("MeowMenu", "getMenuLevel", [](std::string const& menuId) -> int {
        try {
            auto menu = MenuStore::getInstance().getMenu(menuId);
            if (!menu) return -1;
            return menu->level;
        } catch (...) {}
        return -1;
    });

    // getMenuByTriggerItem(itemType) — 按触发物品类型查一级菜单 ID（无匹配返回 ""）
    RemoteCall::exportAs("MeowMenu", "getMenuByTriggerItem", [](std::string const& itemType) -> std::string {
        try {
            return MenuStore::getInstance().findMenuByTriggerItem(itemType);
        } catch (...) {}
        return "";
    });
}

//==== 按钮管理 API ====
void ExportApi::registerButtonApi() {
    // getButton(menuId, index) -> string  (按钮 JSON，越界返回 "{}")
    RemoteCall::exportAs("MeowMenu", "getButton",
        [](std::string const& menuId, int index) -> std::string {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu || index < 0 || index >= (int)menu->buttons.size()) return "{}";
                return menu->buttons[index].toJson().dump();
            } catch (...) {}
            return "{}";
        });

    // addButton(menuId, buttonJson) -> bool
    RemoteCall::exportAs("MeowMenu", "addButton",
        [](std::string const& menuId, std::string const& buttonJson) -> bool {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu) return false;
                auto j = nlohmann::json::parse(buttonJson);
                Button btn = Button::fromJson(j);
                if (!btn.icon.empty()) {
                    btn.icon = IconValidator::getInstance().normalizeIcon(btn.icon);
                }
                Menu local = *menu;
                local.buttons.push_back(btn);
                return MenuStore::getInstance().saveMenu(local);
            } catch (...) {}
            return false;
        });

    // updateButton(menuId, index, buttonJson) -> bool
    RemoteCall::exportAs("MeowMenu", "updateButton",
        [](std::string const& menuId, int index, std::string const& buttonJson) -> bool {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu || index < 0 || index >= (int)menu->buttons.size()) return false;
                auto j = nlohmann::json::parse(buttonJson);
                Button btn = Button::fromJson(j);
                if (!btn.icon.empty()) {
                    btn.icon = IconValidator::getInstance().normalizeIcon(btn.icon);
                }
                Menu local = *menu;
                local.buttons[index] = btn;
                return MenuStore::getInstance().saveMenu(local);
            } catch (...) {}
            return false;
        });

    // deleteButton(menuId, index) -> bool
    RemoteCall::exportAs("MeowMenu", "deleteButton",
        [](std::string const& menuId, int index) -> bool {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu || index < 0 || index >= (int)menu->buttons.size()) return false;
                Menu local = *menu;
                local.buttons.erase(local.buttons.begin() + index);
                return MenuStore::getInstance().saveMenu(local);
            } catch (...) {}
            return false;
        });

    // getButtonCount(menuId) -> int
    RemoteCall::exportAs("MeowMenu", "getButtonCount", [](std::string const& menuId) -> int {
        try {
            auto menu = MenuStore::getInstance().getMenu(menuId);
            if (!menu) return -1;
            return (int)menu->buttons.size();
        } catch (...) {}
        return -1;
    });

    // insertButton(menuId, index, buttonJson) — 在指定位置插入按钮（index 越界自动钳制）
    RemoteCall::exportAs("MeowMenu", "insertButton",
        [](std::string const& menuId, int index, std::string const& buttonJson) -> bool {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu) return false;
                auto j = nlohmann::json::parse(buttonJson);
                Button btn = Button::fromJson(j);
                if (!btn.icon.empty()) {
                    btn.icon = IconValidator::getInstance().normalizeIcon(btn.icon);
                }
                Menu local  = *menu;
                int  sz     = (int)local.buttons.size();
                int  pos    = (index < 0) ? 0 : (index > sz ? sz : index);
                local.buttons.insert(local.buttons.begin() + pos, btn);
                return MenuStore::getInstance().saveMenu(local);
            } catch (...) {}
            return false;
        });

    // moveButton(menuId, fromIndex, toIndex) -> bool  (调整按钮顺序)
    RemoteCall::exportAs("MeowMenu", "moveButton",
        [](std::string const& menuId, int fromIndex, int toIndex) -> bool {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu) return false;
                int sz = (int)menu->buttons.size();
                if (fromIndex < 0 || fromIndex >= sz) return false;
                if (toIndex < 0 || toIndex >= sz) return false;
                if (fromIndex == toIndex) return true;
                Menu local = *menu;
                Button btn = std::move(local.buttons[fromIndex]);
                local.buttons.erase(local.buttons.begin() + fromIndex);
                local.buttons.insert(local.buttons.begin() + toIndex, std::move(btn));
                return MenuStore::getInstance().saveMenu(local);
            } catch (...) {}
            return false;
        });

    // clearButtons(menuId) -> bool  (清空菜单所有按钮，保留菜单本身)
    RemoteCall::exportAs("MeowMenu", "clearButtons", [](std::string const& menuId) -> bool {
        try {
            auto menu = MenuStore::getInstance().getMenu(menuId);
            if (!menu) return false;
            Menu local = *menu;
            local.buttons.clear();
            return MenuStore::getInstance().saveMenu(local);
        } catch (...) {}
        return false;
    });
}

//==== 渲染 API ====
void ExportApi::registerRenderApi() {
    // renderMenu(playerName, menuId) -> bool
    RemoteCall::exportAs("MeowMenu", "renderMenu",
        [](std::string const& playerName, std::string const& menuId) -> bool {
            try {
                auto* player = findPlayer(playerName);
                if (!player) return false;
                MenuRenderer::getInstance().renderMenu(*player, menuId);
                return true;
            } catch (...) {}
            return false;
        });

    // renderMainMenu(playerName) -> bool
    RemoteCall::exportAs("MeowMenu", "renderMainMenu", [](std::string const& playerName) -> bool {
        try {
            auto* player = findPlayer(playerName);
            if (!player) return false;
            auto const& cfg = ConfigManager::getInstance().getConfig();
            MenuRenderer::getInstance().renderMenu(*player, cfg.mainMenuId);
            return true;
        } catch (...) {}
        return false;
    });
}

//==== 触发物品 API（config.triggerItem, 默认 clock） ====
void ExportApi::registerTriggerItemApi() {
    // giveTriggerItem(playerName) -> bool
    RemoteCall::exportAs("MeowMenu", "giveTriggerItem", [](std::string const& playerName) -> bool {
        try {
            auto* player = findPlayer(playerName);
            if (!player) return false;
            TriggerItem::giveItem(*player, false);
            return true;
        } catch (...) {}
        return false;
    });

    // clearTriggerItem(playerName) -> bool
    RemoteCall::exportAs("MeowMenu", "clearTriggerItem", [](std::string const& playerName) -> bool {
        try {
            auto* player = findPlayer(playerName);
            if (!player) return false;
            TriggerItem::clearItem(*player);
            return true;
        } catch (...) {}
        return false;
    });

    // hasTriggerItem(playerName) -> bool
    RemoteCall::exportAs("MeowMenu", "hasTriggerItem", [](std::string const& playerName) -> bool {
        try {
            auto* player = findPlayer(playerName);
            if (!player) return false;
            return TriggerItem::hasItemInInventory(*player);
        } catch (...) {}
        return false;
    });
}

//==== 自定义物品触发 API ====
void ExportApi::registerItemTriggerApi() {
    // registerItemTrigger(itemType, menuId) -> bool
    RemoteCall::exportAs("MeowMenu", "registerItemTrigger",
        [](std::string const& itemType, std::string const& menuId) -> bool {
            try {
                return ItemTrigger::getInstance().registerTrigger(itemType, menuId);
            } catch (...) {}
            return false;
        });

    // unregisterItemTrigger(itemType) -> bool
    RemoteCall::exportAs("MeowMenu", "unregisterItemTrigger", [](std::string const& itemType) -> bool {
        try {
            return ItemTrigger::getInstance().unregisterTrigger(itemType);
        } catch (...) {}
        return false;
    });

    // getItemTriggers() -> string  (JSON: {"itemType":"menuId"})
    RemoteCall::exportAs("MeowMenu", "getItemTriggers", []() -> std::string {
        try {
            return ItemTrigger::getInstance().getAllTriggersJson();
        } catch (...) {}
        return "{}";
    });

    // clearItemTriggers() -> bool
    RemoteCall::exportAs("MeowMenu", "clearItemTriggers", []() -> bool {
        try {
            return ItemTrigger::getInstance().clearAll();
        } catch (...) {}
        return false;
    });
}

//==== 按钮处理 API ====
void ExportApi::registerButtonHandlerApi() {
    // triggerButton(playerName, menuId, buttonIndex) -> bool
    RemoteCall::exportAs("MeowMenu", "triggerButton",
        [](std::string const& playerName, std::string const& menuId, int buttonIndex) -> bool {
            try {
                auto* player = findPlayer(playerName);
                if (!player) return false;
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu || buttonIndex < 0 || buttonIndex >= (int)menu->buttons.size()) return false;
                ButtonHandler::handleButton(*player, menu->buttons[buttonIndex]);
                return true;
            } catch (...) {}
            return false;
        });
}

//==== 配置 API ====
void ExportApi::registerConfigApi() {
    // getConfig() -> string
    RemoteCall::exportAs("MeowMenu", "getConfig", []() -> std::string {
        try {
            auto const& cfg = ConfigManager::getInstance().getConfig();
            nlohmann::json j;
            j["displayMode"]          = cfg.displayMode;
            j["useDogeUI"]            = cfg.useDogeUI;
            j["mainMenuId"]           = cfg.mainMenuId;
            j["allowPlayerModeChoice"] = cfg.allowPlayerModeChoice;
            j["triggerItem"]["type"]        = cfg.triggerItem.type;
            j["triggerItem"]["displayName"] = cfg.triggerItem.displayName;
            j["triggerItem"]["snbt"]        = cfg.triggerItem.snbt;
            j["triggerItem"]["count"]       = cfg.triggerItem.count;
            j["giveUs"]["enabled"]          = cfg.giveUs.enabled;
            j["giveUs"]["giveOnJoin"]       = cfg.giveUs.giveOnJoin;
            j["giveUs"]["commandName"]      = cfg.giveUs.commandName;
            j["giveUs"]["commandAlias"]     = cfg.giveUs.commandAlias;
            j["giveUs"]["commandDesc"]      = cfg.giveUs.commandDesc;
            j["giveUs"]["msgReceived"]      = cfg.giveUs.msgReceived;
            j["giveUs"]["msgAlreadyHave"]   = cfg.giveUs.msgAlreadyHave;
            j["giveUs"]["msgInventoryFull"] = cfg.giveUs.msgInventoryFull;
            j["giveUs"]["msgCleared"]       = cfg.giveUs.msgCleared;
            j["menuLevel"]["defaultLevel"]  = cfg.menuLevel.defaultLevel;
            return j.dump();
        } catch (...) {}
        return "{}";
    });

    // setConfig(key, valueJson) — valueJson 是 JSON 编码的值（如 true / "main" / 123）
    RemoteCall::exportAs("MeowMenu", "setConfig",
        [](std::string const& key, std::string const& valueJson) -> bool {
            try {
                auto& cfg = ConfigManager::getInstance().getConfig();
                auto value = nlohmann::json::parse(valueJson);

                if (key == "displayMode")            cfg.displayMode = value.get<std::string>();
                else if (key == "useDogeUI")          cfg.useDogeUI = value.get<bool>();
                else if (key == "mainMenuId")         cfg.mainMenuId = value.get<std::string>();
                else if (key == "allowPlayerModeChoice") cfg.allowPlayerModeChoice = value.get<bool>();
                else if (key == "chestInPlaceRefresh") cfg.chestInPlaceRefresh = value.get<bool>();
                else if (key == "triggerItem.type")        cfg.triggerItem.type = value.get<std::string>();
                else if (key == "triggerItem.displayName") cfg.triggerItem.displayName = value.get<std::string>();
                else if (key == "triggerItem.snbt")        cfg.triggerItem.snbt = value.get<std::string>();
                else if (key == "triggerItem.count")       cfg.triggerItem.count = value.get<int>();
                else if (key == "menuLevel.defaultLevel")  cfg.menuLevel.defaultLevel = (value.get<int>() == 2) ? 2 : 1;
                else if (key == "giveUs.enabled")          cfg.giveUs.enabled = value.get<bool>();
                else if (key == "giveUs.giveOnJoin")       cfg.giveUs.giveOnJoin = value.get<bool>();
                else if (key == "giveUs.commandName")      cfg.giveUs.commandName = value.get<std::string>();
                else if (key == "giveUs.commandAlias")     cfg.giveUs.commandAlias = value.get<std::string>();
                else if (key == "giveUs.commandDesc")      cfg.giveUs.commandDesc = value.get<std::string>();
                else if (key == "giveUs.msgReceived")      cfg.giveUs.msgReceived = value.get<std::string>();
                else if (key == "giveUs.msgAlreadyHave")   cfg.giveUs.msgAlreadyHave = value.get<std::string>();
                else if (key == "giveUs.msgInventoryFull") cfg.giveUs.msgInventoryFull = value.get<std::string>();
                else if (key == "giveUs.msgCleared")       cfg.giveUs.msgCleared = value.get<std::string>();
                else return false;

                ConfigManager::getInstance().save();
                return true;
            } catch (...) {}
            return false;
        });

    // reloadConfig() -> bool
    RemoteCall::exportAs("MeowMenu", "reloadConfig", []() -> bool {
        try {
            ConfigManager::getInstance().load();
            return true;
        } catch (...) {}
        return false;
    });
}

//==== 图标校验 API ====
void ExportApi::registerIconApi() {
    // isValidItemId(itemId) -> bool
    RemoteCall::exportAs("MeowMenu", "isValidItemId", [](std::string const& itemId) -> bool {
        try {
            return IconValidator::getInstance().isValidItemId(itemId);
        } catch (...) {}
        return false;
    });

    // isValidTexturePath(path) -> bool
    RemoteCall::exportAs("MeowMenu", "isValidTexturePath", [](std::string const& path) -> bool {
        try {
            return IconValidator::getInstance().isValidTexturePath(path);
        } catch (...) {}
        return false;
    });

    // getTexturePath(itemId) -> string
    RemoteCall::exportAs("MeowMenu", "getTexturePath", [](std::string const& itemId) -> std::string {
        try {
            return IconValidator::getInstance().getTexturePathFor(itemId);
        } catch (...) {}
        return "";
    });

    // normalizeIcon(icon) -> string
    RemoteCall::exportAs("MeowMenu", "normalizeIcon", [](std::string const& icon) -> std::string {
        try {
            return IconValidator::getInstance().normalizeIcon(icon);
        } catch (...) {}
        return "";
    });
}

//==== 管理员编辑器 API（表单 UI） ====
void ExportApi::registerEditorApi() {
    // showEditor(playerName) -> bool
    RemoteCall::exportAs("MeowMenu", "showEditor", [](std::string const& playerName) -> bool {
        try {
            auto* player = findPlayer(playerName);
            if (!player) return false;
            MenuEditor::showEditorMain(*player);
            return true;
        } catch (...) {}
        return false;
    });

    // showMenuList(playerName) -> bool
    RemoteCall::exportAs("MeowMenu", "showMenuList", [](std::string const& playerName) -> bool {
        try {
            auto* player = findPlayer(playerName);
            if (!player) return false;
            MenuEditor::showMenuList(*player);
            return true;
        } catch (...) {}
        return false;
    });

    // showEditMenu(playerName, menuId) -> bool
    RemoteCall::exportAs("MeowMenu", "showEditMenu",
        [](std::string const& playerName, std::string const& menuId) -> bool {
            try {
                auto* player = findPlayer(playerName);
                if (!player) return false;
                MenuEditor::showEditMenu(*player, menuId);
                return true;
            } catch (...) {}
            return false;
        });
}

//==== NPC 对话菜单 API（HologramLib 可选前置; 缺库时一律返回 false/0/空串） ====
void ExportApi::registerNpcMenuApi() {
    // isNpcMenuAvailable() -> bool  前置是否已连接且 API 版本达标
    RemoteCall::exportAs("MeowMenu", "isNpcMenuAvailable", []() -> bool {
        try {
            return NpcMenuBridge::getInstance().isAvailable();
        } catch (...) {}
        return false;
    });

    // getNpcMenuStatus() -> string  连接状态 / ABI 版本 / 监听 token / 绑定数量
    RemoteCall::exportAs("MeowMenu", "getNpcMenuStatus", []() -> std::string {
        try {
            return NpcMenuBridge::getInstance().statusText();
        } catch (...) {}
        return "";
    });

    // openNpcMenuDialog(playerName, menuId) — 以 NPC 对话界面打开菜单（menuId 空 = 主菜单）
    RemoteCall::exportAs("MeowMenu", "openNpcMenuDialog",
        [](std::string const& playerName, std::string const& menuId) -> bool {
            try {
                auto* player = findPlayer(playerName);
                if (!player) return false;
                return NpcMenuBridge::getInstance().openDialogue(*player, menuId);
            } catch (...) {}
            return false;
        });

    // closeNpcMenuDialog(playerName) -> bool
    RemoteCall::exportAs("MeowMenu", "closeNpcMenuDialog", [](std::string const& playerName) -> bool {
        try {
            auto* player = findPlayer(playerName);
            if (!player) return false;
            return NpcMenuBridge::getInstance().closeDialogue(*player);
        } catch (...) {}
        return false;
    });

    // spawnNpcMenuAt(npcName, menuId, x, y, z, dim) — 生成"点击即打开菜单"的 NPC（持久化）
    RemoteCall::exportAs("MeowMenu", "spawnNpcMenuAt",
        [](std::string const& npcName,
           std::string const& menuId,
           float              x,
           float              y,
           float              z,
           int                dim) -> int64_t {
            try {
                return NpcMenuBridge::getInstance().spawnNpcAt(npcName, menuId, x, y, z, dim);
            } catch (...) {}
            return -1;
        });

    // bindNpcMenu(npcId, menuId) -> bool  把已有 HologramLib NPC 绑定到菜单
    RemoteCall::exportAs("MeowMenu", "bindNpcMenu",
        [](int64_t npcId, std::string const& menuId) -> bool {
            try {
                return NpcMenuBridge::getInstance().bindNpc(npcId, menuId);
            } catch (...) {}
            return false;
        });

    // removeNpcMenu(npcId) -> bool  删除 NPC 及其绑定
    RemoteCall::exportAs("MeowMenu", "removeNpcMenu", [](int64_t npcId) -> bool {
        try {
            return NpcMenuBridge::getInstance().removeNpc(npcId);
        } catch (...) {}
        return false;
    });

    // getNpcMenus() -> string  JSON 数组: [{id,name,menu,x,y,z,dim,skin}, ...]
    RemoteCall::exportAs("MeowMenu", "getNpcMenus", []() -> std::string {
        try {
            return NpcMenuBridge::getInstance().listNpcsJson();
        } catch (...) {}
        return "[]";
    });

    // reloadNpcMenus() -> bool  重读 npc_menus.json 并重建 NPC
    RemoteCall::exportAs("MeowMenu", "reloadNpcMenus", []() -> bool {
        try {
            return NpcMenuBridge::getInstance().reloadBindings();
        } catch (...) {}
        return false;
    });

    // setMenuNpcMode(menuId, enabled, npcName, dialogue) — 把菜单设为/取消 NPC 对话模式
    RemoteCall::exportAs("MeowMenu", "setMenuNpcMode",
        [](std::string const& menuId,
           bool               enabled,
           std::string const& npcName,
           std::string const& dialogue) -> bool {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu) return false;
                menu->npc.enabled  = enabled;
                menu->npc.name     = npcName;
                menu->npc.dialogue = dialogue;
                return MenuStore::getInstance().saveMenu(*menu);
            } catch (...) {}
            return false;
        });

    // setMenuNpcAvatar(menuId, avatarType, avatarVariant) — 聊天框头像/载体自定义（avatarType 空 = 默认）
    RemoteCall::exportAs("MeowMenu", "setMenuNpcAvatar",
        [](std::string const& menuId, std::string const& avatarType, int avatarVariant) -> bool {
            try {
                auto menu = MenuStore::getInstance().getMenu(menuId);
                if (!menu) return false;
                menu->npc.avatarType    = avatarType;
                menu->npc.avatarVariant = avatarVariant;
                return MenuStore::getInstance().saveMenu(*menu);
            } catch (...) {}
            return false;
        });

    // openChestMenu(playerName, menuId) -> bool  以箱子界面打开菜单（需 HologramLib）
    RemoteCall::exportAs("MeowMenu", "openChestMenu",
        [](std::string const& playerName, std::string const& menuId) -> bool {
            try {
                auto* player = findPlayer(playerName);
                if (!player) return false;
                return NpcMenuBridge::getInstance().openChestMenu(*player, menuId);
            } catch (...) {}
            return false;
        });

    // setChestSlot(playerName, slot, itemType, name) — 按槽刷新箱子界面一格（itemType 空 = 清空）
    RemoteCall::exportAs("MeowMenu", "setChestSlot",
        [](std::string const& playerName, int slot, std::string const& itemType, std::string const& name) -> bool {
            try {
                auto* player = findPlayer(playerName);
                if (!player) return false;
                return NpcMenuBridge::getInstance().refreshChestSlot(*player, slot, itemType, name);
            } catch (...) {}
            return false;
        });

    // closeChestMenu(playerName) -> bool
    RemoteCall::exportAs("MeowMenu", "closeChestMenu", [](std::string const& playerName) -> bool {
        try {
            auto* player = findPlayer(playerName);
            if (!player) return false;
            return NpcMenuBridge::getInstance().closeChestMenu(*player);
        } catch (...) {}
        return false;
    });

    // getPlayerUiMode(playerName) -> string  auto | form | npc | chest
    RemoteCall::exportAs("MeowMenu", "getPlayerUiMode", [](std::string const& playerName) -> std::string {
        try {
            auto* player = findPlayer(playerName);
            if (!player) return "";
            return PlayerUiPref::getInstance().get(*player);
        } catch (...) {}
        return "";
    });

    // setPlayerUiMode(playerName, mode) -> bool  设置玩家自选界面样式（mode: auto|form|npc|chest）
    RemoteCall::exportAs("MeowMenu", "setPlayerUiMode",
        [](std::string const& playerName, std::string const& mode) -> bool {
            try {
                auto* player = findPlayer(playerName);
                if (!player) return false;
                return PlayerUiPref::getInstance().set(*player, mode);
            } catch (...) {}
            return false;
        });

    // getAllPlayerUiModes() -> string  全部玩家样式偏好 JSON
    RemoteCall::exportAs("MeowMenu", "getAllPlayerUiModes", []() -> std::string {
        try {
            return PlayerUiPref::getInstance().getAllJson();
        } catch (...) {}
        return "{}";
    });

    // getMenuNpcMode(menuId) -> string  JSON: {"enabled":bool,"name":string,"dialogue":string,"clickableButtons":int,"maxButtons":6}
    RemoteCall::exportAs("MeowMenu", "getMenuNpcMode", [](std::string const& menuId) -> std::string {
        try {
            auto menu = MenuStore::getInstance().getMenu(menuId);
            if (!menu) return "";
            return nlohmann::json{
                {"enabled", menu->npc.enabled},
                {"name", menu->npc.name},
                {"dialogue", menu->npc.dialogue},
                {"avatarType", menu->npc.avatarType},
                {"avatarVariant", menu->npc.avatarVariant},
                {"maxButtons", 6},
                {"clickableButtons", menu->clickableButtonCount()},
            }.dump();
        } catch (...) {}
        return "";
    });
}

//==== 工具 API ====
void ExportApi::registerUtilApi() {
    // formatMessage(template) -> string  (替换 {item} / {cmd})
    RemoteCall::exportAs("MeowMenu", "formatMessage", [](std::string const& template_) -> std::string {
        try {
            return ConfigManager::getInstance().formatMessage(template_);
        } catch (...) {}
        return template_;
    });

    // isPluginReady() -> bool
    RemoteCall::exportAs("MeowMenu", "isPluginReady", []() -> bool {
        return gReady.load();
    });
}

//==== 菜单迁移 API（外部格式导入） ====
void ExportApi::registerMigrationApi() {
    // autoMigrateMenus() — 扫描 migrations/ 导入 CD/PMenu/CMENU, 返回导入数量
    RemoteCall::exportAs("MeowMenu", "autoMigrateMenus", []() -> int {
        try {
            return MigrationManager::getInstance().autoMigrate();
        } catch (...) {}
        return 0;
    });

    // importMenu(menuId, formatJson) — 转换外部格式 JSON 并保存（已存在则覆盖）
    RemoteCall::exportAs("MeowMenu", "importMenu",
        [](std::string const& menuId, std::string const& formatJson) -> bool {
            try {
                return MigrationManager::getInstance().importMenu(menuId, formatJson);
            } catch (...) {}
            return false;
        });
}

} // namespace meowmenu
