// EventIdCompat.h - 26.40 事件 ID 对齐（MSVC 插件必需）:
// 官方 LeviLamina.dll 是 clang 编译, 事件 ID 只认 "ll::event::XxxEvent" —— 缺了这段,
// 监听器注册成功但永远不回调。**新增事件监听时必须在这里补一条。**
#pragma once

#include <ll/api/event/EventId.h>

#include <ll/api/event/player/PlayerInteractBlockEvent.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>

namespace ll::event {

// 玩家加入: 进服一次性发放触发物品（记账于 players.json）
template <>
constexpr EventIdView getEventId<::ll::event::player::PlayerJoinEvent> =
    EventIdView{"ll::event::PlayerJoinEvent"};

// 右键方块: 触发物品代替方块交互（打开菜单 + 取消原交互）
template <>
constexpr EventIdView getEventId<::ll::event::player::PlayerInteractBlockEvent> =
    EventIdView{"ll::event::PlayerInteractBlockEvent"};

// 右键空气: 触发物品打开菜单
template <>
constexpr EventIdView getEventId<::ll::event::player::PlayerUseItemEvent> =
    EventIdView{"ll::event::PlayerUseItemEvent"};

} // namespace ll::event
