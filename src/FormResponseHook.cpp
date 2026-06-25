// FormResponseHook.cpp - 钩取 ModalFormResponsePacket 实现
//
// 拦截玩家对协议层发送的 SimpleForm 的响应，解析按钮索引并分发回调。
#include "FormResponseHook.h"
#include "FormManager.h"
#include "MeowMenu.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"

#include "mc/network/ServerNetworkHandler.h"
#include "mc/network/packet/ModalFormResponsePacket.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include <mc/deps/json/Value.h>

#include <chrono>
#include <optional>
#include <string>

using namespace ll::memory;

namespace meowmenu {

namespace FormResponseHook {

// 获取日志器
static auto& getLogger() {
    return lib().getSelf().getLogger();
}

// 安全获取玩家（通过 NetworkIdentifier）
static Player* getPlayerByNetworkId(NetworkIdentifier const& source) {
    auto level = ll::service::getLevel();
    if (!level.has_value()) return nullptr;

    Player* result = nullptr;
    level->forEachPlayer([&](Player& p) -> bool {
        if (p.getNetworkIdentifier() == source) {
            result = &p;
            return true; // found
        }
        return false; // continue
    });
    return result;
}

// 解析 SimpleForm 响应
// 响应格式: Json::Value 为数字（按钮索引）或 null（关闭）
static std::optional<int> parseSimpleFormResponse(std::optional<Json::Value> const& response) {
    if (!response.has_value()) return std::nullopt; // 玩家关闭表单

    auto const& val = response.value();
    if (val.isNull()) return std::nullopt;

    if (val.isInt()) {
        return val.asInt(-1);
    }
    if (val.isUInt()) {
        return (int)val.asUInt((uint)-1);
    }
    if (val.isInt64()) {
        return (int)val.asInt64(-1);
    }
    return std::nullopt;
}

// 钩子: 拦截 ModalFormResponsePacket
LL_TYPE_INSTANCE_HOOK(
    ModalFormResponsePacketHook,
    HookPriority::Normal,
    ServerNetworkHandler,
    &ServerNetworkHandler::$handle,
    void,
    NetworkIdentifier const& source,
    ModalFormResponsePacket const& packet
) {
    // 检查是否为本插件管理的 formId
    uint32_t formId = packet.mFormId;

    if (!FormManager::getInstance().hasForm(formId)) {
        // 不是本插件的表单，交给 BDS 原始处理
        return origin(source, packet);
    }

    // 消费回调
    auto callback = FormManager::getInstance().consume(formId);
    if (!callback) {
        return; // 回调已过期
    }

    // 解析响应
    std::optional<int> buttonIndex = std::nullopt;
    try {
        buttonIndex = parseSimpleFormResponse(packet.mJSONResponse);
    } catch (...) {
        getLogger().error("解析表单响应失败: formId={}", formId);
    }

    // 获取玩家（回调可能需要）
    Player* player = getPlayerByNetworkId(source);
    std::string playerName = player ? player->getNameTag() : "unknown";

    getLogger().info("表单响应: formId={}, player={}, buttonIndex={}",
                     formId, playerName,
                     buttonIndex.has_value() ? std::to_string(buttonIndex.value()) : "null");

    // 在服务器线程上执行回调（确保线程安全）
    ll::thread::ServerThreadExecutor::getDefault().executeAfter(
        [callback, buttonIndex, formId]() {
            try {
                if (callback) {
                    callback(formId, buttonIndex);
                }
            } catch (...) {
                getLogger().error("执行表单回调异常: formId={}", formId);
            }
        },
        std::chrono::milliseconds(0)
    );

    // 不调用 origin() - 本插件的表单由我们自行处理
    // 注意：不调用 origin 不会影响其他插件，因为其他插件的 formId 会在上面的 hasForm 检查中放行
}

void hook() {
    ModalFormResponsePacketHook::hook();
    getLogger().info("ModalFormResponsePacket 钩子已安装");
}

void unhook() {
    ModalFormResponsePacketHook::unhook();
    getLogger().info("ModalFormResponsePacket 钩子已卸载");
}

} // namespace FormResponseHook

} // namespace meowmenu
