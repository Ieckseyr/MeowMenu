// TriggerRouter.h - 触发物品 → 菜单路由
//
// 事件层（PlayerInteractBlockEvent / PlayerUseItemEvent）与 Hook 层
// （ItemUseInventoryTransaction::$handle / Player::$useItem）共用同一份判定：
//   1. 手持物品是否为菜单触发物（硬编码 clock → config.mainMenuId）
//   2. 是否为 LSE 注册的自定义触发物（ItemTrigger 注册表 → 对应菜单）
//   3. 100ms 冷却（防止连点重复打开）
//   4. 打开菜单
#pragma once

#include <string>

// 前向声明 BDS 类型（均位于全局命名空间）
class Player;
class ItemStackBase;

namespace meowmenu {

// 判定并处理"用触发物品打开菜单"。
//
// 返回值语义（调用方据此决定是否拦截原始物品使用行为）：
//   true  —— 手持物品是菜单触发物，已被消费（含"冷却中"的情况：触发物不应该被真的使用）
//   false —— 不是触发物，调用方应放行
bool tryTriggerMenu(Player& player, ItemStackBase const& item);

} // namespace meowmenu
