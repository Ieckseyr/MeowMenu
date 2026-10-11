// ItemTrigger.h - 自定义物品→菜单绑定（LSE 注册; 与主菜单触发物品独立）
// 按 getTypeName() 精确匹配; 持久化到 Meowdata/MeowMenu/item_triggers.json。
#pragma once

#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace meowmenu {

class ItemTrigger {
public:
    static ItemTrigger& getInstance();

    // 从磁盘加载 item_triggers.json（文件不存在则创建空注册表）
    void initialize();

    // 注册物品类型→菜单绑定（持久化, 幂等）; 返回是否写入成功
    bool registerTrigger(std::string const& itemType, std::string const& menuId);

    // 解绑指定物品类型
    // 返回是否原本存在并已移除
    bool unregisterTrigger(std::string const& itemType);

    // 查询物品类型对应的菜单 ID（不存在返回空串）
    std::string findMenuId(std::string const& itemType) const;

    // 返回所有绑定的 JSON 字符串：{"itemType":"menuId",...}
    std::string getAllTriggersJson() const;

    // 清空所有绑定（并持久化）
    bool clearAll();

private:
    ItemTrigger() = default;

    // 写入磁盘
    void save();

    mutable std::shared_mutex                                  mMutex;
    std::unordered_map<std::string, std::string>               mTriggers;
};

} // namespace meowmenu
