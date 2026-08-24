// ExportApi.cpp - RemoteCall API 导出注册实现
//
// 命名空间: "MeowMenu"
// 所有导出函数均 try/catch(...) 兜底，失败返回 false/空串，避免 RemoteCall _onCallError
// 抛出非 std 异常（参考 MeowSync 教训）。
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
    registerUtilApi();
    registerMigrationApi();
    gReady.store(true);
}

//================================================================
// 菜单管理 API
//================================================================
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
}

//================================================================
// 按钮管理 API
//================================================================
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

    // insertButton(menuId, index, buttonJson) -> bool  (在指定位置插入按钮)
    // index 越界时自动钳制：<0 视为 0，>size 视为末尾追加
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

//================================================================
// 渲染 API
//================================================================
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

//================================================================
// 触发物品 API（硬编码 clock）
//================================================================
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

//================================================================
// 自定义物品触发 API（核心新功能）
//================================================================
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

//================================================================
// 按钮处理 API
//================================================================
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

//================================================================
// 配置 API
//================================================================
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
            return j.dump();
        } catch (...) {}
        return "{}";
    });

    // setConfig(key, valueJson) -> bool
    // valueJson 是 JSON 编码的值，如 true / "main" / 123
    RemoteCall::exportAs("MeowMenu", "setConfig",
        [](std::string const& key, std::string const& valueJson) -> bool {
            try {
                auto& cfg = ConfigManager::getInstance().getConfig();
                auto value = nlohmann::json::parse(valueJson);

                if (key == "displayMode")            cfg.displayMode = value.get<std::string>();
                else if (key == "useDogeUI")          cfg.useDogeUI = value.get<bool>();
                else if (key == "mainMenuId")         cfg.mainMenuId = value.get<std::string>();
                else if (key == "allowPlayerModeChoice") cfg.allowPlayerModeChoice = value.get<bool>();
                else if (key == "triggerItem.type")        cfg.triggerItem.type = value.get<std::string>();
                else if (key == "triggerItem.displayName") cfg.triggerItem.displayName = value.get<std::string>();
                else if (key == "triggerItem.snbt")        cfg.triggerItem.snbt = value.get<std::string>();
                else if (key == "triggerItem.count")       cfg.triggerItem.count = value.get<int>();
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

//================================================================
// 图标校验 API
//================================================================
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

//================================================================
// 管理员编辑器 API（表单 UI）
//================================================================
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

//================================================================
// 工具 API
//================================================================
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

//================================================================
// 菜单迁移 API（外部菜单插件格式导入）
//================================================================
void ExportApi::registerMigrationApi() {
    // autoMigrateMenus() -> int
    // 手动触发扫描 Meowdata/MeowMenu/migrations/ 下的 JSON 文件，
    // 自动识别 CD/PMenu/CMENU 格式并转换导入到 data/，导入后移至 migrations/done/
    // 返回成功导入的菜单数量
    RemoteCall::exportAs("MeowMenu", "autoMigrateMenus", []() -> int {
        try {
            return MigrationManager::getInstance().autoMigrate();
        } catch (...) {}
        return 0;
    });

    // importMenu(menuId, formatJson) -> bool
    // 直接传入外部格式 JSON 字符串，自动识别格式并转换为 MeowMenu 菜单后保存
    // menuId     : 目标菜单 ID（已存在则覆盖）
    // formatJson : 外部格式 JSON（PMenu/CMENU/CD旧/CD新）
    RemoteCall::exportAs("MeowMenu", "importMenu",
        [](std::string const& menuId, std::string const& formatJson) -> bool {
            try {
                return MigrationManager::getInstance().importMenu(menuId, formatJson);
            } catch (...) {}
            return false;
        });
}

} // namespace meowmenu
