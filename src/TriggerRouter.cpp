// TriggerRouter.cpp - 触发物品 → 菜单路由实现
#include "TriggerRouter.h"
#include "Config.h"
#include "ItemTrigger.h"
#include "MenuRenderer.h"
#include "TriggerItem.h"

#include <mc/world/actor/player/Player.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/ItemStackBase.h>

#include <chrono>
#include <mutex>
#include <unordered_map>

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

// 解析手持物品应打开的菜单 ID
// 优先级：
//   1. 硬编码 clock 触发物品 → config.mainMenuId
//   2. LSE 注册的自定义物品触发（ItemTrigger）→ 对应 menuId
//   3. 无匹配 → 返回空串
std::string resolveTriggerMenu(ItemStackBase const& item) {
    if (item.isNull()) return "";

    // 1. 硬编码 clock 触发物品
    if (TriggerItem::isTriggerItem(item)) {
        return ConfigManager::getInstance().getConfig().mainMenuId;
    }

    // 2. 自定义物品触发（LSE 通过 registerItemTrigger 注册）
    try {
        std::string itemType = item.getTypeName();
        if (!itemType.empty()) {
            return ItemTrigger::getInstance().findMenuId(itemType);
        }
    } catch (...) {}

    return "";
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
