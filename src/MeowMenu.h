// MeowMenu.h - 菜单插件主模块
//
// 使用 LeviLamina Form API 发送 SimpleForm，
// 支持每菜单独立的 DogeUI 标识符（自定义前缀）。
// 暂时放弃动态菜单(DDUI)和箱子菜单模式，仅保留普通静态菜单。
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
