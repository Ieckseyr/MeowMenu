// ItemUseHook.h - 物品使用触发钩子（useitem / useitemon）
#pragma once

namespace meowmenu {

// 安装物品使用触发钩子；返回是否全部安装成功
bool installItemUseHooks();

// 卸载物品使用触发钩子（幂等）
void uninstallItemUseHooks();

} // namespace meowmenu
