// TriggerItem.h - 触发物品系统
//
// 管理菜单触发物品（minecraft:clock）的给予、检查、清除。
// 物品 NBT 硬编码于 TriggerItem.cpp（严格固定，不读 config.snbt）。
// 给予采用 Player::add（事务系统）+ refreshInventory（全包同步），确保可见且可移动。
// 玩家进服自动给予（每次进服检查背包，无 clock 才给予）。
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
