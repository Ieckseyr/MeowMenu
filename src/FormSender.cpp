// FormSender.cpp - 协议层表单发送实现
//
// 使用 Protocol-v944 (sculk::protocol::abi_v944) 的 ModalFormRequestPacket
// 构造数据包，通过 NetworkPeer::sendPacket 发送。
#include "FormSender.h"
#include "MeowMenu.h"

#include <ll/api/service/Bedrock.h>

#include <mc/network/NetworkSystem.h>
#include <mc/network/NetworkPeer.h>
#include <mc/network/Compressibility.h>
#include <mc/world/actor/player/Player.h>

#include <sculk/protocol/codec/packet/ModalFormRequestPacket.hpp>
#include <sculk/protocol/codec/utility/deps/BinaryStream.hpp>

#include <nlohmann/json.hpp>
#include <vector>

namespace meowmenu {

namespace sp = sculk::protocol::abi_v944;

std::string FormSender::buildFormRequestPacket(uint32_t formId, std::string const& formJson) {
    sp::ModalFormRequestPacket packet;
    packet.mFormId   = formId;
    packet.mFormJson = formJson;

    std::vector<std::byte> buf;
    sp::BinaryStream       stream(buf);
    packet.writeWithHeader(stream); // VarUInt(100) + VarUInt(formId) + string(formJson)
    return std::string(stream.asStringView());
}

bool FormSender::sendToPlayer(Player& player, uint32_t formId, std::string const& formJson) {
    auto packetData = buildFormRequestPacket(formId, formJson);
    if (packetData.empty()) return false;

    auto networkSystem = ll::service::getNetworkSystem();
    if (!networkSystem.has_value()) {
        lib().getSelf().getLogger().error("发送表单失败: NetworkSystem 不可用");
        return false;
    }

    auto* peer = networkSystem->getPeerForUser(player.getNetworkIdentifier());
    if (!peer) {
        lib().getSelf().getLogger().error("发送表单失败: 找不到玩家的 NetworkPeer");
        return false;
    }

    peer->sendPacket(
        packetData,
        NetworkPeer::Reliability::ReliableOrdered,
        Compressibility::Compressible
    );
    return true;
}

std::string FormSender::buildSimpleFormJson(
    std::string const& title,
    std::string const& content,
    std::vector<std::pair<std::string, std::string>> const& buttons
) {
    nlohmann::json j;
    j["type"]    = "form";
    j["title"]   = title;
    j["content"] = content;

    nlohmann::json btns = nlohmann::json::array();
    for (auto const& [text, icon] : buttons) {
        nlohmann::json btn;
        btn["text"] = text;
        if (!icon.empty()) {
            btn["image"]["type"] = "path";
            btn["image"]["data"] = icon;
        }
        btns.push_back(btn);
    }
    j["buttons"] = btns;

    return j.dump();
}

} // namespace meowmenu
