// MenuStore.h - 菜单存储管理
//
// 从 Meowdata/MeowMenu/data/ 加载菜单 JSON 文件，保存时写回。
// 向后兼容现有 JS 版数据格式。
#pragma once

#include "MenuData.h"

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace meowmenu {

class MenuStore {
public:
    static MenuStore& getInstance();

    // 加载所有菜单文件
    void loadAll();

    // 获取菜单（不存在返回 nullptr）
    std::shared_ptr<Menu> getMenu(std::string const& id) const;

    // 创建菜单（已存在则返回 false）
    bool createMenu(std::string const& id, std::string const& title, std::string const& content);

    // 保存菜单到内存和磁盘
    bool saveMenu(Menu const& menu);

    // 删除菜单
    bool deleteMenu(std::string const& id);

    // 获取所有菜单ID
    std::vector<std::string> getAllMenuIds() const;

    // 获取菜单文件路径
    static std::string getMenuPath(std::string const& id);

    // 构造默认主菜单模板（用于首次创建 + 迁移时识别"初始状态"）
    // 字段顺序严格匹配 Button 结构体（text, icon, iconItem, type, value, item）
    static Menu makeDefaultMainMenu(std::string const& id);

    // 判断菜单是否仍为未修改的默认主菜单（仅比较 title + content，
    // 兼容历史字段错位版本：只要标题与内容仍是默认值即视为初始状态）
    static bool isDefaultMainMenu(Menu const& menu);

private:
    MenuStore() = default;

    // 从磁盘加载单个菜单
    bool loadMenu(std::string const& id);

    mutable std::shared_mutex                             mMutex;
    std::unordered_map<std::string, std::shared_ptr<Menu>> mMenus;
};

} // namespace meowmenu
