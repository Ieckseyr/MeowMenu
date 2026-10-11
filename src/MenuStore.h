// MenuStore.h - 菜单存储（Meowdata/MeowMenu/data/*.json, 兼容 JS 版格式）
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

    // 创建菜单（已存在返回 false; level 默认一级）
    bool createMenu(
        std::string const& id,
        std::string const& title,
        std::string const& content,
        int                level = 1
    );

    // 按触发物品类型查一级菜单（二级不参与匹配; 找不到返回空串）
    std::string findMenuByTriggerItem(std::string const& itemType) const;

    // 保存菜单到内存和磁盘
    bool saveMenu(Menu const& menu);

    // 删除菜单
    bool deleteMenu(std::string const& id);

    // 获取所有菜单ID
    std::vector<std::string> getAllMenuIds() const;

    // 获取菜单文件路径
    static std::string getMenuPath(std::string const& id);

    // 默认主菜单模板（首次创建 + 迁移识别"初始状态"用）
    static Menu makeDefaultMainMenu(std::string const& id);

    // 是否仍为未修改的默认主菜单（只比 title + content, 兼容历史字段错位）
    static bool isDefaultMainMenu(Menu const& menu);

private:
    MenuStore() = default;

    // 从磁盘加载单个菜单
    bool loadMenu(std::string const& id);

    mutable std::shared_mutex                             mMutex;
    std::unordered_map<std::string, std::shared_ptr<Menu>> mMenus;
};

} // namespace meowmenu
