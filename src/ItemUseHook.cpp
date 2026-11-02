// ItemUseHook.cpp - 物品使用触发钩子实现
//
// 为什么还要裸钩子，而不是只用 LeviLamina 的事件：
//
//   LL 的 PlayerUseItemEvent 挂在 PlayerTransactionSubject::$baseUseItem 上，
//   PlayerInteractBlockEvent 挂在 GameMode::$useItemOn 上 —— 两者都是 LL 的 emitter，
//   本插件继续复用（见 EventRegistration.cpp）。但它们只覆盖"服务端已经决定要处理
//   这次物品使用"的路径：手持 minecraft:clock 这种没有使用行为的物品右键时，能够
//   到达 baseUseItem / useItemOn 的机会并不稳定，菜单因此时灵时不灵。
//
//   这里补上事务层（网络包 → 服务端事务）的入口 ItemUseInventoryTransaction::$handle：
//   ActionType::Use（右键空气）与 ActionType::Place（右键方块）在同一次调用里，
//   在任何物品行为判定之前就会到达，且 Player 是形参（不需要从 this 反推）。
//   —— 覆盖"useitem"和"useitemon"两个场景。
//
// 刻意**不挂** GameMode::$useItemOn：
//   该地址上 LL 已有 2 层 detour（PlayerPlacingBlockEventCallerHook /
//   PlayerInteractBlockEventHook），MeowLand 又叠了 1 层（MeowPlacingCallerHook）。
//   实测第 4 层会压垮最内层 trampoline（见 MeowLand/docs/领地保护实施清单.md
//   "05:13 崩溃" + MeowLand/src/EntityInteractHooks.cpp 的说明）。
//   "useitemon" 的拦截继续交给 LL 的 PlayerInteractBlockEvent 监听器。
#include "ItemUseHook.h"
#include "MeowMenu.h"
#include "TriggerRouter.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"

#include <mc/server/ServerInstance.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/inventory/transaction/InventoryTransactionError.h>
#include <mc/world/inventory/transaction/ItemUseInventoryTransaction.h>
#include <mc/world/item/ItemStackBase.h>

#include <thread>

namespace meowmenu {

namespace {

// 与 LSE / MeowLand 相同的判定：ServerInstance 的线程 id == 当前线程 id。
// 物品使用只应发生在服务器线程；非服务器线程直接放行，绝不在错误的线程上发表单。
bool isServerThread() {
    auto instance = ll::service::getServerInstance();
    return instance && std::this_thread::get_id() == instance->mServerInstanceThread->get_id();
}

// 触发判定 + 打开菜单；任何异常都不允许冒泡到被挂钩的游戏函数里
// 返回值：true = 手持物品是菜单触发物（调用方不应再执行物品使用）
bool routeTrigger(Player& player, ItemStackBase const& item) {
    try {
        return tryTriggerMenu(player, item);
    } catch (...) {
        return false;
    }
}

// 取手持物品再判定（取物品本身也可能抛异常）
void routeCarriedItem(Player& player) {
    try {
        tryTriggerMenu(player, player.getCarriedItem());
    } catch (...) {}
}

} // namespace

//=== 1. useitem / useitemon：物品使用事务（网络层 → 服务端事务的入口） ===
//
// ActionType::Use   = 右键空气使用物品（useitem）
// ActionType::Place = 右键/放置（useitemon）
// 其余（Destroy / UseAsAttack）与菜单无关，直接放行。
//
// 这里始终调用 origin：菜单触发物（钟）没有任何使用/放置行为，放行不会有副作用；
// "不要打开箱子/门"这类拦截仍由 PlayerInteractBlockEvent 监听器的 cancel 负责。
LL_TYPE_INSTANCE_HOOK(
    MeowItemUseTransactionHook,
    HookPriority::Normal,
    ItemUseInventoryTransaction,
    &ItemUseInventoryTransaction::$handle,
    InventoryTransactionError,
    Player& player,
    bool    isSenderAuthority
) {
    using ActionType = ItemUseInventoryTransaction::ActionType;

    // 事务处理恒在服务器线程；这里仍做一次守卫，避免在错误的线程上发表单
    if (isServerThread()) {
        // mActionType 是标量枚举，TypedStorage 对其不生成包装（直接就是 ActionType）
        switch (mActionType) {
        case ActionType::Use:   // 右键空气使用物品（useitem）
        case ActionType::Place: // 右键方块使用物品（useitemon）
            routeCarriedItem(player);
            break;
        default:
            break;
        }
    }

    return origin(player, isSenderAuthority);
}

//=== 2. useitem：玩家"使用物品"通知 ===
//
// 与 LL 的 PlayerUseItemEvent 同源但独立（LL 该事件挂在 PlayerTransactionSubject::
// $baseUseItem 上，本钩子挂在 Player::$useItem 上，是本插件自己的兜底路径，
// 即使 LL 的事件链被其它插件影响也仍能触发）。
// 触发物在这里不执行物品使用（对齐 JS 版 onUseItem 的 return false 语义）。
LL_TYPE_INSTANCE_HOOK(
    MeowPlayerUseItemHook,
    HookPriority::Normal,
    Player,
    &Player::$useItem,
    void,
    ItemStackBase& item,
    ItemUseMethod  itemUseMethod,
    bool           consumeItem
) {
    if (isServerThread() && routeTrigger(*this, item)) {
        return; // 触发物：吃掉这次物品使用
    }
    origin(item, itemUseMethod, consumeItem);
}

namespace {

bool gHooksInstalled = false;

} // namespace

bool installItemUseHooks() {
    if (gHooksInstalled) return true;

    auto& logger = lib().getSelf().getLogger();
    bool  ok     = true;

    if (MeowItemUseTransactionHook::hook() != 0) {
        logger.error("安装钩子失败: ItemUseInventoryTransaction::$handle (useitem/useitemon)");
        ok = false;
    }
    if (MeowPlayerUseItemHook::hook() != 0) {
        logger.error("安装钩子失败: Player::$useItem (useitem)");
        ok = false;
    }

    gHooksInstalled = ok;
    return ok;
}

void uninstallItemUseHooks() {
    if (!gHooksInstalled) return;

    MeowItemUseTransactionHook::unhook();
    MeowPlayerUseItemHook::unhook();
    gHooksInstalled = false;
}

} // namespace meowmenu
