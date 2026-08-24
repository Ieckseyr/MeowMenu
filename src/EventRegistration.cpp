// EventRegistration.cpp - 事件注册实现
//
// 监听事件：
// 1. PlayerJoinEvent - 玩家进服时自动给予触发物品
// 2. PlayerInteractBlockEvent - 右键方块时检测触发物品并打开菜单
// 3. PlayerUseItemEvent - 右键空气时检测触发物品并打开菜单
#include "EventRegistration.h"
#include "Config.h"
#include "TriggerItem.h"
#include "ItemTrigger.h"
#include "MenuRenderer.h"
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
#include <mutex>
#include <unordered_map>

namespace meowmenu {

// 使用冷却（防止快速重复触发）
static std::mutex                                       gCooldownMutex;
static std::unordered_map<std::string, int64_t>         gUseCooldown;
static constexpr int64_t COOLDOWN_MS = 100; // 0.1秒冷却

static bool checkCooldown(std::string const& playerKey) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    std::lock_guard lock(gCooldownMutex);
    auto last = gUseCooldown.find(playerKey);
    if (last != gUseCooldown.end() && (now - last->second) < COOLDOWN_MS) {
        return false; // 冷却中
    }
    gUseCooldown[playerKey] = now;
    return true;
}

// 解析玩家手持物品应打开的菜单 ID
// 优先级：
//   1. 硬编码 clock 触发物品 → config.mainMenuId
//   2. LSE 注册的自定义物品触发（ItemTrigger）→ 对应 menuId
//   3. 无匹配 → 返回空串
static std::string resolveTriggerMenu(Player& player) {
    // 1. 硬编码 clock 触发物品
    if (TriggerItem::isTriggerItem(player)) {
        auto const& cfg = ConfigManager::getInstance().getConfig();
        return cfg.mainMenuId;
    }

    // 2. 自定义物品触发（LSE 注册）
    try {
        auto& carried = player.getCarriedItem();
        if (!carried.isNull()) {
            std::string itemType = carried.getTypeName();
            if (!itemType.empty()) {
                std::string menuId = ItemTrigger::getInstance().findMenuId(itemType);
                if (!menuId.empty()) return menuId;
            }
        }
    } catch (...) {}

    return "";
}

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
    auto interactListener = bus.emplaceListener<ll::event::PlayerInteractBlockEvent>(
        [](ll::event::PlayerInteractBlockEvent& event) {
            auto& player = event.self();

            // 解析手持物品应打开的菜单（硬编码 clock 或 LSE 自定义触发）
            std::string menuId = resolveTriggerMenu(player);
            if (menuId.empty()) return; // 无触发物品

            // 冷却检查
            auto playerKey = player.getNetworkIdentifier().getIPAndPort();
            if (!checkCooldown(playerKey)) {
                event.cancel();
                return;
            }

            // 打开菜单
            MenuRenderer::getInstance().renderMenu(player, menuId);

            // 取消事件（阻止方块交互）
            event.cancel();
        }
    );
    mListenerPtrs.push_back(interactListener);

    //=== 3. 右键空气（使用物品）- 检测触发物品 ===
    auto useItemListener = bus.emplaceListener<ll::event::PlayerUseItemEvent>(
        [](ll::event::PlayerUseItemEvent& event) {
            auto& player = event.self();

            // 解析手持物品应打开的菜单（硬编码 clock 或 LSE 自定义触发）
            std::string menuId = resolveTriggerMenu(player);
            if (menuId.empty()) return; // 无触发物品

            // 冷却检查
            auto playerKey = player.getNetworkIdentifier().getIPAndPort();
            if (!checkCooldown(playerKey)) {
                event.cancel();
                return;
            }

            // 打开菜单
            MenuRenderer::getInstance().renderMenu(player, menuId);

            // 取消事件
            event.cancel();
        }
    );
    mListenerPtrs.push_back(useItemListener);

    logger.info("事件监听已注册: PlayerJoin, PlayerInteractBlock, PlayerUseItem");
}

} // namespace meowmenu
