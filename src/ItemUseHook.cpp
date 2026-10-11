// ItemUseHook.cpp - 物品使用裸钩子（事务层兜底, 覆盖 LL 事件够不到的右键）
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

// 非服务器线程直接放行（物品使用只应发生在服务器线程）
bool isServerThread() {
    auto instance = ll::service::getServerInstance();
    return instance && std::this_thread::get_id() == instance->mServerInstanceThread->get_id();
}

// 触发判定 + 打开菜单; 返回 true = 手持物是触发物
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

//=== useitem / useitemon: 物品使用事务入口（ActionType::Use / Place） ===
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

//=== useitem: Player::$useItem 兜底钩子（与 LL 的 PlayerUseItemEvent 独立） ===
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
