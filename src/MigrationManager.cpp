// MigrationManager.cpp - 外部菜单格式导入实现
#include "MigrationManager.h"
#include "MenuStore.h"
#include "IconValidator.h"
#include "Config.h"
#include "MeowMenu.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace meowmenu {

namespace fs = std::filesystem;

static const std::string MIGRATION_DIR = "Meowdata/MeowMenu/migrations";
static const std::string DONE_DIR     = "Meowdata/MeowMenu/migrations/done";

MigrationManager& MigrationManager::getInstance() {
    static MigrationManager instance;
    return instance;
}

// 去除字符串两端空白
static std::string trim(std::string s) {
    auto notSpace = [](int ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::shared_ptr<Menu> MigrationManager::convertToMeowMenu(std::string const& menuId, nlohmann::json const& data) {
    // PMenu: 对象且含 buttons 数组
    if (data.is_object() && data.contains("buttons") && data["buttons"].is_array()) {
        return convertPMenu(menuId, data);
    }
    // CMENU: 数组且符合 CMENU type 约束
    if (data.is_array() && isCMenuFormat(data)) {
        return convertCMenu(menuId, data);
    }
    // CD(旧): 任意数组
    if (data.is_array()) {
        return convertCD(menuId, data);
    }
    // CD(新): 对象含 items 数组
    if (data.is_object() && data.contains("items") && data["items"].is_array()) {
        return convertCD(menuId, data["items"]);
    }
    return nullptr;
}

std::shared_ptr<Menu> MigrationManager::convertCD(std::string const& menuId, nlohmann::json const& items) {
    if (!items.is_array()) return nullptr;
    auto menu = std::make_shared<Menu>();
    menu->id      = menuId;
    menu->title   = "§l§b" + menuId;
    menu->content = "";

    for (auto const& item : items) {
        if (!item.is_object()) continue;
        std::string type = item.value("type", "");
        if (type == "divider" || type == "label") continue;

        Button btn;
        btn.text     = item.value("name", "");
        std::string subtitle = item.value("subtitle", "");
        if (!subtitle.empty()) btn.text += "\n§7" + subtitle;
        std::string image = item.value("image", "");
        btn.icon     = trim(image); // 空或纯空白 → 空
        btn.iconItem = "";
        btn.type     = "command";
        btn.value    = "";
        btn.item     = "";

        std::string open = item.value("open", "");
        bool adminOnly = item.value("adminOnly", false);
        if (type == "form") {
            btn.type = "submenu"; btn.value = open;
        } else if (type == "opform") {
            btn.type = "admin_submenu"; btn.value = open;
        } else if (type == "comm") {
            btn.type = adminOnly ? "admin_command" : "command"; btn.value = open;
        } else if (type == "opcomm" || type == "console") {
            btn.type = "admin_command"; btn.value = open;
        } else {
            btn.value = open;
        }
        menu->buttons.push_back(btn);
    }
    return menu;
}

bool MigrationManager::isCMenuFormat(nlohmann::json const& items) const {
    if (!items.is_array() || items.empty()) return false;
    static const std::unordered_set<std::string> cmenuTypes = {"comm", "opcomm", "form", "opform"};
    for (auto const& item : items) {
        if (!item.is_object()) return false;
        if (!item.contains("name") || !item.contains("open") || !item.contains("type")) return false;
        std::string type = item.value("type", "");
        if (cmenuTypes.find(type) == cmenuTypes.end()) return false;
    }
    return true;
}

std::shared_ptr<Menu> MigrationManager::convertCMenu(std::string const& menuId, nlohmann::json const& items) {
    auto menu = std::make_shared<Menu>();
    menu->id      = menuId;
    menu->title   = "§l§b" + menuId;
    menu->content = "";

    for (auto const& item : items) {
        Button btn;
        btn.text     = item.value("name", "");
        std::string image = item.value("image", "");
        btn.icon     = trim(image);
        btn.iconItem = "";
        btn.value    = item.value("open", "");
        btn.item     = "";
        btn.type     = "command";

        std::string type = item.value("type", "");
        if (type == "form") btn.type = "submenu";
        else if (type == "opform") btn.type = "admin_submenu";
        else if (type == "comm") btn.type = "command";
        else if (type == "opcomm") btn.type = "admin_command";
        menu->buttons.push_back(btn);
    }
    return menu;
}

std::shared_ptr<Menu> MigrationManager::convertPMenu(std::string const& menuId, nlohmann::json const& data) {
    if (!data.is_object()) return nullptr;
    if (!data.contains("buttons") || !data["buttons"].is_array()) return nullptr;

    auto menu = std::make_shared<Menu>();
    menu->id      = menuId;
    menu->title   = data.value("title", menuId);
    menu->content = data.value("content", "");

    for (auto const& btn : data["buttons"]) {
        if (!btn.is_object()) continue;
        Button b;
        b.text     = btn.value("text", "");
        // JS: (btn.images && btn.image) ? btn.image : ''
        if (btn.contains("images") && btn.contains("image")) {
            b.icon = btn.value("image", "");
        } else {
            b.icon = "";
        }
        b.iconItem = "";
        b.value    = btn.value("command", "");
        b.item     = "";
        b.type     = "command";

        std::string type = btn.value("type", "");
        if (type == "form") b.type = "submenu";
        else if (type == "opfm" || type == "opform") b.type = "admin_submenu";
        else if (type == "comm") b.type = "command";
        else if (type == "opcm" || type == "vipcm") b.type = "admin_command";
        else if (type == "vipfm" || type == "vipform") b.type = "submenu";
        else b.type = "command";
        menu->buttons.push_back(b);
    }
    return menu;
}

void MigrationManager::fixMenuIcons(Menu& menu) const {
    auto& validator = IconValidator::getInstance();
    for (auto& btn : menu.buttons) {
        if (!btn.icon.empty()) {
            btn.icon = validator.normalizeIcon(btn.icon);
        }
    }
}

int MigrationManager::autoMigrate() {
    auto& logger = lib().getSelf().getLogger();

    try {
        fs::create_directories(MIGRATION_DIR);
    } catch (std::exception const& e) {
        logger.error("迁移目录创建失败: {}", e.what());
        return 0;
    }

    std::vector<std::string> files;
    try {
        for (auto const& entry : fs::directory_iterator(MIGRATION_DIR)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ".json") {
                files.push_back(entry.path().filename().string());
            }
        }
    } catch (...) {}
    if (files.empty()) return 0;

    try {
        fs::create_directories(DONE_DIR);
    } catch (...) {}

    int count = 0;
    auto const& cfg = ConfigManager::getInstance().getConfig();

    for (auto const& file : files) {
        std::string menuId = file.substr(0, file.size() - 5); // strip .json

        // 目标菜单已存在时的处理
        auto existing = MenuStore::getInstance().getMenu(menuId);
        if (existing) {
            // 特例：主菜单若仍为初始状态（未自定义），允许迁移版本直接替换
            if (menuId == cfg.mainMenuId && MenuStore::isDefaultMainMenu(*existing)) {
                logger.info("主菜单 {} 为初始状态，使用迁移版本替换", menuId);
                // 落入下方转换 + 保存 + 移动到 done/ 的流程
            } else {
                logger.warn("迁移跳过: {}，菜单 {} 已存在", file, menuId);
                continue;
            }
        }

        try {
            std::ifstream ifs(MIGRATION_DIR + "/" + file);
            std::stringstream ss;
            ss << ifs.rdbuf();
            auto data = nlohmann::json::parse(ss.str());

            auto menu = convertToMeowMenu(menuId, data);
            if (menu) {
                try { fixMenuIcons(*menu); } catch (...) {}
                if (MenuStore::getInstance().saveMenu(*menu)) {
                    std::error_code ec;
                    fs::rename(MIGRATION_DIR + "/" + file, DONE_DIR + "/" + file, ec);
                    count++;
                }
            } else {
                logger.warn("迁移跳过: {}，无法识别格式", file);
            }
        } catch (std::exception const& e) {
            logger.error("迁移失败({}): {}", file, e.what());
        }
    }

    if (count > 0) {
        logger.info("菜单迁移完成: 已导入 {} 个菜单", count);
    }
    return count;
}

bool MigrationManager::importMenu(std::string const& menuId, std::string const& formatJson) {
    try {
        auto data = nlohmann::json::parse(formatJson);
        auto menu = convertToMeowMenu(menuId, data);
        if (!menu) return false;
        fixMenuIcons(*menu);
        return MenuStore::getInstance().saveMenu(*menu);
    } catch (...) {}
    return false;
}

} // namespace meowmenu
