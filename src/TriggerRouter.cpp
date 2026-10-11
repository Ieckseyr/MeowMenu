// TriggerRouter.cpp - 触发物品 → 菜单路由实现
#include "TriggerRouter.h"
#include "Config.h"
#include "ItemTrigger.h"
#include "MenuRenderer.h"
#include "MenuStore.h"
#include "MeowMenu.h"
#include "TriggerItem.h"

#include <mc/world/actor/player/Player.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/ItemStackBase.h>

#include <chrono>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace meowmenu {

namespace {

std::mutex                                     gCooldownMutex;
std::unordered_map<std::string, int64_t>       gUseCooldown;

constexpr int64_t COOLDOWN_MS = 100; // 触发冷却：防止右键连点刷屏

// 冷却检查：true = 允许本次触发
bool checkCooldown(std::string const& playerKey) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch()
    ).count();

    std::lock_guard lock(gCooldownMutex);
    auto            last = gUseCooldown.find(playerKey);
    if (last != gUseCooldown.end() && (now - last->second) < COOLDOWN_MS) {
        return false; // 冷却中
    }
    gUseCooldown[playerKey] = now;
    return true;
}

// 解析手持物品应打开的菜单 ID（优先级: 主菜单触发物 → 一级菜单触发物 → LSE 绑定）
std::string resolveTriggerMenu(ItemStackBase const& item) {
    if (item.isNull()) return "";

    // 1. 主菜单触发物品（config.triggerItem, 默认钟）
    if (TriggerItem::isTriggerItem(item)) {
        return ConfigManager::getInstance().getConfig().mainMenuId;
    }

    std::string itemType;
    try {
        itemType = item.getTypeName();
    } catch (...) {}
    if (itemType.empty()) return "";

    // 2. 一级菜单自己配置的触发物品
    std::string byMenu = MenuStore::getInstance().findMenuByTriggerItem(itemType);
    if (!byMenu.empty()) return byMenu;

    // 3. 自定义物品触发（LSE 通过 registerItemTrigger 注册）
    std::string byApi = ItemTrigger::getInstance().findMenuId(itemType);
    if (byApi.empty()) return "";

    // 二级菜单不能由物品触发打开 → 忽略该绑定（只提示一次, 避免刷日志）
    if (auto menu = MenuStore::getInstance().getMenu(byApi); menu && menu->isSecondary()) {
        static std::mutex              warnedMutex;
        static std::unordered_set<std::string> warned;
        std::lock_guard                lock(warnedMutex);
        if (warned.insert(byApi).second) {
            lib().getSelf().getLogger().warn(
                "物品触发 {} 指向的菜单 {} 是二级菜单 → 已忽略（二级菜单只能从其它菜单进入）",
                itemType,
                byApi
            );
        }
        return "";
    }

    return byApi;
}

} // namespace

bool tryTriggerMenu(Player& player, ItemStackBase const& item) {
    std::string menuId = resolveTriggerMenu(item);
    if (menuId.empty()) return false; // 不是触发物 → 放行

    std::string playerKey;
    try {
        playerKey = player.getNetworkIdentifier().getIPAndPort();
    } catch (...) {}

    // 冷却中：仍然返回 true（触发物不该被真的使用），但不再打开菜单
    if (!checkCooldown(playerKey)) return true;

    MenuRenderer::getInstance().renderMenu(player, menuId);
    return true;
}

} // namespace meowmenu
