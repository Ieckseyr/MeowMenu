// MenuStore.cpp - 菜单存储管理实现
#include "MenuStore.h"
#include "Config.h"
#include "MeowMenu.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

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

Menu MenuStore::makeDefaultMainMenu(std::string const& id) {
    Menu main;
    main.id      = id;
    main.title   = "§l§b服务器菜单";
    main.content = "欢迎来到服务器！";

    // 使用指定初始化避免聚合初始化字段错位
    // （历史 bug：曾把 admin_command 赋给 iconItem、mmenu admin 赋给 type）
    Button btn{
        .text     = "§l§6菜单编辑器\n§7仅管理员可见",
        .icon     = "textures/ui/icon_setting",
        .iconItem = "",
        .type     = "admin_command",
        .value    = "mmenu admin",
        .item     = ""
    };
    main.buttons.push_back(btn);
    return main;
}

bool MenuStore::isDefaultMainMenu(Menu const& menu) {
    auto def = makeDefaultMainMenu(menu.id);
    // 仅比较 title + content: 兼容历史字段错位, 管理员改过标题/内容则视为已自定义
    return menu.title == def.title && menu.content == def.content;
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

    // 热重载时清空内存中已有的菜单，确保磁盘上的删除/改动能被正确反映
    {
        std::unique_lock lock(mMutex);
        mMenus.clear();
    }

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
            // 创建默认主菜单（字段顺序由 makeDefaultMainMenu 保证正确）
            Menu main = makeDefaultMainMenu(cfg.mainMenuId);
            saveMenu(main);
            count++;
        }
    }

    lib().getSelf().getLogger().info("已加载 {} 个菜单", count);

    // 分级体检: 主菜单恒一级（否则玩家手里的钟失效）; 二级配了触发物品 = 白写, 提示一下
    bool                                           mainWasSecondary = false;
    std::vector<std::pair<std::string, std::string>> secondaryWithTrigger; // id, 触发物品类型
    {
        std::shared_lock lock(mMutex);
        for (auto const& [id, menu] : mMenus) {
            if (!menu) continue;
            if (id == cfg.mainMenuId) {
                mainWasSecondary = menu->isSecondary();
                continue;
            }
            if (menu->isSecondary() && menu->triggerItem.configured()) {
                secondaryWithTrigger.emplace_back(id, menu->triggerItem.type);
            }
        }
    }

    if (mainWasSecondary) {
        if (auto main = getMenu(cfg.mainMenuId)) {
            Menu local  = *main;
            local.level = 1;
            saveMenu(local);
            lib().getSelf().getLogger().warn(
                "主菜单 {} 被配置成二级菜单 → 已强制改回一级菜单（主菜单的触发物品是钟）",
                cfg.mainMenuId
            );
        }
    }
    for (auto const& [id, type] : secondaryWithTrigger) {
        lib().getSelf().getLogger().warn(
            "菜单 {} 是二级菜单, 但它配置了触发物品 {} → 不生效（二级菜单只能从其它菜单进入）",
            id,
            type
        );
    }
}

std::string MenuStore::findMenuByTriggerItem(std::string const& itemType) const {
    if (itemType.empty()) return "";

    std::shared_lock lock(mMutex);
    for (auto const& [id, menu] : mMenus) {
        // Menu::matchesTriggerItem 内部已处理"二级菜单不生效"与大小写
        if (menu && menu->matchesTriggerItem(itemType)) return id;
    }
    return "";
}

std::shared_ptr<Menu> MenuStore::getMenu(std::string const& id) const {
    std::shared_lock lock(mMutex);
    auto it = mMenus.find(id);
    if (it != mMenus.end()) return it->second;
    return nullptr;
}

bool MenuStore::createMenu(
    std::string const& id,
    std::string const& title,
    std::string const& content,
    int                level
) {
    {
        std::shared_lock lock(mMutex);
        if (mMenus.find(id) != mMenus.end()) return false;
    }

    Menu m;
    m.id      = id;
    m.title   = title.empty() ? id : title;
    m.content = content;
    m.level   = (level == 2) ? 2 : 1; // 只认 1 / 2, 其余按一级
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
