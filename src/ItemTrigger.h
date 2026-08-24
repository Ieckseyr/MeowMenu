// ItemTrigger.h - 自定义物品→菜单触发注册表
//
// 允许 LSE 通过 RemoteCall API 动态注册「使用某物品打开某菜单」的绑定。
// 与硬编码的 TriggerItem（minecraft:clock）独立并存，互不影响。
//
// 匹配规则：按手持物品的 getTypeName()（如 "minecraft:compass"）精确匹配。
// 持久化：绑定写入 Meowdata/MeowMenu/item_triggers.json，重启后保留。
// 空注册表时仅一次哈希查找，无额外开销。
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

    // 注册物品类型→菜单绑定（持久化，幂等）
    // itemType: 物品类型 ID（如 "minecraft:compass"）
    // menuId:    要打开的菜单 ID
    // 返回是否写入成功（与原值不同时返回 true）
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
