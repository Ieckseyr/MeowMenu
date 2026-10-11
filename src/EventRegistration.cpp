// EventRegistration.cpp - 事件注册（进服发放 / 右键触发）
#include "EventRegistration.h"
#include "Config.h"
#include "TriggerItem.h"
#include "TriggerRouter.h"
#include "PlayerRecord.h"
#include "MeowMenu.h"

// 26.40 事件 ID 对齐: 必须在注册监听器之前 include（缺了监听器会全部静默失效）
#include "EventIdCompat.h"

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

namespace meowmenu {

namespace {

long long nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// 按名字找在线玩家（进服 500ms 延迟后重新取；名字比 IP:port 可靠 —— 同 IP 多号/重连都不会找错人）
Player* findPlayerByName(std::string const& name) {
    auto level = ll::service::getLevel();
    if (!level.has_value()) return nullptr;

    Player* found = nullptr;
    level->forEachPlayer([&](Player& p) -> bool {
        try {
            if (p.getRealName() == name) {
                found = &p;
                return true;
            }
        } catch (...) {}
        return false;
    });
    return found;
}

} // namespace

void registerEvents(std::vector<ll::event::ListenerPtr>& mListenerPtrs) {
    auto& logger = lib().getSelf().getLogger();
    auto& bus    = ll::event::EventBus::getInstance();

    // 玩家进服: 一次性发放触发物品（players.json 里记过账的直接跳过）
    auto joinListener = bus.emplaceListener<ll::event::player::PlayerJoinEvent>(
        [](ll::event::player::PlayerJoinEvent& event) {
            auto& player = event.self();
            auto const& cfg = ConfigManager::getInstance().getConfig();

            if (!cfg.giveUs.enabled || !cfg.giveUs.giveOnJoin) return;

            std::string name;
            try {
                name = player.getRealName();
            } catch (...) {}
            if (name.empty()) return;

            auto& store = PlayerRecordStore::getInstance();

            // 发过 = 不用管（这就是"不需要每次进服重复检查"的开关）
            if (store.menuGiven(name)) return;

            // 首次进服登记（顺便补全 missing 字段），便于管理员查看新玩家
            if (store.noteFirstJoin(name, nowSeconds())) {
                lib().getSelf().getLogger().info("首次进服玩家: {}（已登记到 players.json）", name);
            }

            // 延迟 500ms 给予物品（等待玩家完全加载）
            ll::thread::ServerThreadExecutor::getDefault().executeAfter(
                [name]() {
                    Player* player = findPlayerByName(name);
                    if (!player) return; // 已离线

                    if (TriggerItem::giveItem(*player, true)) {
                        PlayerRecordStore::getInstance().markMenuGiven(name, nowSeconds());
                        lib().getSelf().getLogger().info("进服发放: {} 已取得菜单物品（记账于 players.json）", name);
                    } else {
                        // 不记账 → 下次进服自动重试；也给一条手动路径
                        lib().getSelf().getLogger().warn(
                            "进服发放失败: {}（背包满或物品创建失败 → 不记账, 下次进服重试; 可手动输入 /mmenu 获取）",
                            name
                        );
                    }
                },
                std::chrono::milliseconds(500)
            );
        }
    );
    mListenerPtrs.push_back(joinListener);

    // 右键方块: 触发物品打开菜单, 并取消方块交互
    auto interactListener = bus.emplaceListener<ll::event::PlayerInteractBlockEvent>(
        [](ll::event::PlayerInteractBlockEvent& event) {
            auto& player = event.self();

            // 解析手持物品应打开的菜单（主菜单触发物品 或 LSE 自定义触发）
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

            // 解析手持物品应打开的菜单（主菜单触发物品 或 LSE 自定义触发）
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
