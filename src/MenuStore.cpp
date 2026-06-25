// MenuStore.cpp - 菜单存储管理实现
#include "MenuStore.h"
#include "Config.h"
#include "MeowMenu.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace meowmenu {

namespace fs = std::filesystem;

static constexpr const char* DATA_DIR     = "Meowdata/MeowMenu";
static constexpr const char* DATA_DATA_DIR = "Meowdata/MeowMenu/data";

MenuStore& MenuStore::getInstance() {
    static MenuStore instance;
    return instance;
}

std::string MenuStore::getMenuPath(std::string const& id) {
    return std::string(DATA_DATA_DIR) + "/" + id + ".json";
}

bool MenuStore::loadMenu(std::string const& id) {
    std::string path = getMenuPath(id);
    if (!fs::exists(path)) return false;

    try {
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        auto j = nlohmann::json::parse(ss.str());

        auto menu = std::make_shared<Menu>(Menu::fromJson(j, id));

        std::unique_lock lock(mMutex);
        mMenus[id] = menu;
        return true;
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error("加载菜单 {} 失败: {}", id, e.what());
        return false;
    }
}

void MenuStore::loadAll() {
    fs::create_directories(DATA_DATA_DIR);

    if (!fs::exists(DATA_DATA_DIR)) return;

    int count = 0;
    for (auto const& entry : fs::directory_iterator(DATA_DATA_DIR)) {
        if (!entry.is_regular_file()) continue;
        auto path = entry.path();
        if (path.extension() != ".json") continue;

        std::string id = path.stem().string();
        if (loadMenu(id)) {
            count++;
        }
    }

    // 确保主菜单存在
    auto const& cfg = ConfigManager::getInstance().getConfig();
    {
        std::shared_lock lock(mMutex);
        if (mMenus.find(cfg.mainMenuId) == mMenus.end()) {
            lock.unlock();
            // 创建默认主菜单
            Menu main;
            main.id      = cfg.mainMenuId;
            main.title   = "§l§b服务器菜单";
            main.content = "欢迎来到服务器！";
            main.buttons.push_back({
                "§l§6菜单编辑器\n§7仅管理员可见",
                "textures/ui/icon_setting",
                "admin_command",
                "mmenu admin",
                "minecraft:command_block"
            });
            saveMenu(main);
            count++;
        }
    }

    lib().getSelf().getLogger().info("已加载 {} 个菜单", count);
}

std::shared_ptr<Menu> MenuStore::getMenu(std::string const& id) const {
    std::shared_lock lock(mMutex);
    auto it = mMenus.find(id);
    if (it != mMenus.end()) return it->second;
    return nullptr;
}

bool MenuStore::createMenu(std::string const& id, std::string const& title, std::string const& content) {
    {
        std::shared_lock lock(mMutex);
        if (mMenus.find(id) != mMenus.end()) return false;
    }

    Menu m;
    m.id      = id;
    m.title   = title.empty() ? id : title;
    m.content = content;
    return saveMenu(m);
}

bool MenuStore::saveMenu(Menu const& menu) {
    {
        std::unique_lock lock(mMutex);
        mMenus[menu.id] = std::make_shared<Menu>(menu);
    }

    // 写入磁盘
    try {
        fs::create_directories(DATA_DATA_DIR);
        std::string path = getMenuPath(menu.id);
        std::ofstream ofs(path);
        ofs << menu.toJson().dump(2);
        return true;
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error("保存菜单 {} 失败: {}", menu.id, e.what());
        return false;
    }
}

bool MenuStore::deleteMenu(std::string const& id) {
    {
        std::unique_lock lock(mMutex);
        mMenus.erase(id);
    }

    try {
        fs::remove(getMenuPath(id));
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> MenuStore::getAllMenuIds() const {
    std::shared_lock lock(mMutex);
    std::vector<std::string> ids;
    ids.reserve(mMenus.size());
    for (auto const& [id, _] : mMenus) {
        ids.push_back(id);
    }
    return ids;
}

} // namespace meowmenu
