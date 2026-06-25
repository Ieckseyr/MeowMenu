// TriggerItem.h - 触发物品系统
//
// 管理菜单触发物品（默认时钟）的给予、检查、清除。
// 支持从 SNBT 创建物品，玩家进服自动给予。
#pragma once

#include <string>

// 前向声明 BDS Player 类（位于全局命名空间）
class Player;

namespace meowmenu {

class TriggerItem {
public:
    // 给予玩家触发物品
    // isJoin: 是否为进服自动给予（true 时不显示"已拥有"消息）
    static void giveItem(Player& player, bool isJoin);

    // 检查玩家背包是否有触发物品
    static bool hasItemInInventory(Player& player);

    // 清除玩家背包中的触发物品
    static void clearItem(Player& player);

    // 检查玩家手持物品是否为触发物品
    static bool isTriggerItem(Player& player);

    // 格式化消息
    static std::string formatMessage(std::string const& template_);
};

} // namespace meowmenu
