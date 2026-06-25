// EventRegistration.h - 事件注册声明
#pragma once

#include "ll/api/event/ListenerBase.h"

#include <vector>

namespace meowmenu {

// 注册事件监听（玩家加入、物品使用）
// mListenerPtrs: 用于在 disable 时移除监听器
void registerEvents(std::vector<ll::event::ListenerPtr>& mListenerPtrs);

} // namespace meowmenu
