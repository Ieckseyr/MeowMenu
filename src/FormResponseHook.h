// FormResponseHook.h - 钩取 ModalFormResponsePacket
//
// 钩取 ServerNetworkHandler::$handle(NetworkIdentifier, ModalFormResponsePacket)
// 拦截玩家对原始数据包发送的 SimpleForm 的响应，查找 FormManager 中的回调并分发。
//
// 仅处理由本插件 FormManager 分配的 formId（从 0x10000 开始）。
// 其他 formId 交给 BDS 原始处理（不影响 LL Form API 等其他插件）。
#pragma once

namespace meowmenu {

namespace FormResponseHook {

// 安装钩子
void hook();

// 卸载钩子
void unhook();

} // namespace FormResponseHook

} // namespace meowmenu
