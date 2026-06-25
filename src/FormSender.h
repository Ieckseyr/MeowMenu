// FormSender.h - 协议层表单发送
//
// 使用 Protocol-v944 的 ModalFormRequestPacket 构造原始数据包，
// 通过 BDS 底层 NetworkPeer::sendPacket 发送字节流。
//
// 设计原则（与 MeowLib 一致）：
// 1. 纯协议层实现 - 数据包构造、序列化均由 v944 协议库完成
// 2. 不依赖 BDS Packet 基类 - 不使用 MinecraftPacket / Packet 基类 / cereal
// 3. 仅借用底层通道 - 只使用 NetworkPeer::sendPacket 发送原始字节流
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// 前向声明 BDS Player 类（位于全局命名空间）
class Player;

namespace meowmenu {

class FormSender {
public:
    // 构建 ModalFormRequestPacket 原始字节流（含包头）
    // formId: 表单ID, formJson: SimpleForm JSON 字符串
    static std::string buildFormRequestPacket(uint32_t formId, std::string const& formJson);

    // 发送 SimpleForm 给指定玩家
    // formId: 由 FormManager 分配的 formId
    // formJson: SimpleForm JSON 字符串
    // Returns: 是否发送成功
    static bool sendToPlayer(Player& player, uint32_t formId, std::string const& formJson);

    // 构建 SimpleForm JSON
    // buttons: 按钮列表，每个元素 {text, icon} - icon 为空表示无图标
    static std::string buildSimpleFormJson(
        std::string const& title,
        std::string const& content,
        std::vector<std::pair<std::string, std::string>> const& buttons
    );
};

} // namespace meowmenu
