// EventRegistration.cpp - 事件注册实现
//
// 监听事件：
// 1. PlayerJoinEvent - 玩家进服时自动给予触发物品
// 2. PlayerInteractBlockEvent - 右键方块时检测触发物品并打开菜单（useitemon）
// 3. PlayerUseItemEvent - 右键空气时检测触发物品并打开菜单（useitem）
//
// 注：物品使用还额外挂了事务层/物品层的裸钩子（见 ItemUseHook.cpp），
//     两者共用 TriggerRouter 的同一份判定与冷却 —— 哪条路径先到都只打开一次菜单。
#include "EventRegistration.h"
#include "Config.h"
#include "TriggerItem.h"
#include "TriggerRouter.h"
#include "MeowMenu.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "ll/api/event/player/PlayerUseItemEvent.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"

#include <mc/world/actor/player/Player.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/level/Level.h>

#include <chrono>

// ── LeviLamina 事件 ID 对齐补丁（与 MeowSidebar/Mtps 同款）──
// LL 26.x 把事件类挪进 inline namespace, EventId 基于 __FUNCSIG__ 反射类型名:
//   Clang (官方 LeviLamina.dll) → 省略 inline ns → "ll::event::PlayerJoinEvent"
//   MSVC (本插件)               → 保留 inline ns → "ll::event::player::PlayerJoinEvent"
// 两侧 hash 不一致 → MSVC 插件的监听器收不到事件（进服发物品/触发菜单全部失效）。
// 二进制验证: LeviLamina.dll 中字符串为无前缀变体。显式特化 getEventId 强制对齐。
namespace ll::event {
template <>
constexpr EventIdView getEventId<::ll::event::player::PlayerJoinEvent> =
    EventIdView{"ll::event::PlayerJoinEvent"};

template <>
constexpr EventIdView getEventId<::ll::event::player::PlayerInteractBlockEvent> =
    EventIdView{"ll::event::PlayerInteractBlockEvent"};

template <>
constexpr EventIdView getEventId<::ll::event::player::PlayerUseItemEvent> =
    EventIdView{"ll::event::PlayerUseItemEvent"};
} // namespace ll::event

namespace meowmenu {

void registerEvents(std::vector<ll::event::ListenerPtr>& mListenerPtrs) {
    auto& logger = lib().getSelf().getLogger();
    auto& bus    = ll::event::EventBus::getInstance();

    //=== 1. 玩家进服 - 自动给予触发物品 ===
    auto joinListener = bus.emplaceListener<ll::event::player::PlayerJoinEvent>(
        [](ll::event::player::PlayerJoinEvent& event) {
            auto& player = event.self();
            auto const& cfg = ConfigManager::getInstance().getConfig();

            if (!cfg.giveUs.enabled || !cfg.giveUs.giveOnJoin) return;

            // 延迟 500ms 给予物品（等待玩家完全加载）
            auto playerKey = player.getNetworkIdentifier().getIPAndPort();
            ll::thread::ServerThreadExecutor::getDefault().executeAfter(
                [playerKey]() {
                    auto level = ll::service::getLevel();
                    if (!level.has_value()) return;

                    Player* player = nullptr;
                    level->forEachPlayer([&](Player& p) -> bool {
                        if (p.getNetworkIdentifier().getIPAndPort() == playerKey) {
                            player = &p;
                            return true;
                        }
                        return false;
                    });

                    if (player) {
                        TriggerItem::giveItem(*player, true);
                    }
                },
                std::chrono::milliseconds(500)
            );
        }
    );
    mListenerPtrs.push_back(joinListener);

    //=== 2. 右键方块 - 检测触发物品 ===
    // LL 的 PlayerInteractBlockEvent 由 GameMode::$useItemOn 链发布；这里负责
    // "菜单代替方块交互"的拦截（cancel），触发判定与冷却复用 TriggerRouter。
    auto interactListener = bus.emplaceListener<ll::event::PlayerInteractBlockEvent>(
        [](ll::event::PlayerInteractBlockEvent& event) {
            auto& player = event.self();

            // 解析手持物品应打开的菜单（硬编码 clock 或 LSE 自定义触发）
            if (!tryTriggerMenu(player, event.item())) {
                return; // 无触发物品
            }

            // 取消事件（阻止方块交互，菜单代替箱子/门等交互）
            event.cancel();
        }
    );
    mListenerPtrs.push_back(interactListener);

    //=== 3. 右键空气（使用物品）- 检测触发物品 ===
    auto useItemListener = bus.emplaceListener<ll::event::PlayerUseItemEvent>(
        [](ll::event::PlayerUseItemEvent& event) {
            auto& player = event.self();

            // 解析手持物品应打开的菜单（硬编码 clock 或 LSE 自定义触发）
            if (!tryTriggerMenu(player, event.item())) {
                return; // 无触发物品
            }

            // 取消事件（触发物不被真的使用）
            event.cancel();
        }
    );
    mListenerPtrs.push_back(useItemListener);

    logger.info("事件监听已注册: PlayerJoin, PlayerInteractBlock, PlayerUseItem");
}

} // namespace meowmenu
