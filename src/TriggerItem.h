// TriggerItem.h - 触发物品的发放/检查/清除
#pragma once

#include <string>

// 前向声明 BDS 类（位于全局命名空间）
class Player;
class ItemStackBase;

namespace meowmenu {

class TriggerItem {
public:
    // 给予触发物品; isJoin = 进服自动发放（不弹"已拥有"）; 返回玩家现在是否持有
    static bool giveItem(Player& player, bool isJoin);

    // 检查玩家背包是否有触发物品
    static bool hasItemInInventory(Player& player);

    // 清除玩家背包中的触发物品
    static void clearItem(Player& player);

    // 检查玩家手持物品是否为触发物品
    static bool isTriggerItem(Player& player);

    // 检查给定物品是否为触发物品（不含玩家上下文，供 Hook / 路由层复用）
    static bool isTriggerItem(ItemStackBase const& item);

    // 格式化消息
    static std::string formatMessage(std::string const& template_);
};

} // namespace meowmenu
