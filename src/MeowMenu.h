// MeowMenu.h - 插件主模块（表单菜单; 每菜单可配 DogeUI 前缀）
#pragma once

#include "ll/api/event/ListenerBase.h"
#include "ll/api/mod/NativeMod.h"

#include <vector>

namespace meowmenu {

class MeowMenu {
public:
    static MeowMenu& getInstance();

    MeowMenu() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();

private:
    ll::mod::NativeMod&                  mSelf;
    std::vector<ll::event::ListenerPtr>  mListenerPtrs;
};

MeowMenu& lib();

} // namespace meowmenu
