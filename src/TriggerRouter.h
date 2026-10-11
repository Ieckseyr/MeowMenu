// TriggerRouter.h - 触发物品 → 菜单路由（事件层与 Hook 层共用同一份判定:
// 主菜单触发物 → 一级菜单触发物 → LSE 绑定, 带 100ms 冷却）
#pragma once

#include <string>

// 前向声明 BDS 类型（均位于全局命名空间）
class Player;
class ItemStackBase;

namespace meowmenu {

// 判定并处理「用触发物品打开菜单」; true = 是触发物（含冷却中, 调用方不应再执行物品使用）
bool tryTriggerMenu(Player& player, ItemStackBase const& item);

} // namespace meowmenu
