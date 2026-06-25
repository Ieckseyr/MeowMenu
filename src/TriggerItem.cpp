// TriggerItem.cpp - 触发物品系统实现
#include "TriggerItem.h"
#include "Config.h"
#include "MeowMenu.h"

#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/Inventory.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/ItemStackBase.h>
#include <mc/world/level/Level.h>

#include <mc/deps/nbt/CompoundTag.h>

#include <ll/api/Expected.h>

#include <fmt/format.h>

namespace meowmenu {

// 检查物品类型是否匹配触发物品配置
static bool isItemTypeMatch(ItemStack const& item, std::string const& triggerType) {
    try {
        return item.getTypeName() == triggerType;
    } catch (...) {
        return false;
    }
}

// 从 SNBT 创建物品
static ItemStack createItemFromSnbt(std::string const& snbt) {
    try {
        if (snbt.empty()) return ItemStack{};
        auto tagOpt = CompoundTag::fromSnbt(snbt);
        if (tagOpt.has_value()) {
            return ItemStack::fromTag(tagOpt.value());
        }
    } catch (...) {}
    return ItemStack{};
}

// 从类型创建物品
static ItemStack createItemFromType(std::string const& type, int count) {
    try {
        ItemStack item;
        item.reinit(std::string_view(type), count, 0);
        return item;
    } catch (...) {}
    return ItemStack{};
}

void TriggerItem::giveItem(Player& player, bool isJoin) {
    auto const& cfg = ConfigManager::getInstance().getConfig();
    if (!cfg.giveUs.enabled) return;

    auto const& triggerCfg = cfg.triggerItem;

    // 检查背包是否已有
    if (hasItemInInventory(player)) {
        if (!isJoin) {
            player.sendMessage(formatMessage(cfg.giveUs.msgAlreadyHave));
        }
        return;
    }

    // 创建物品
    ItemStack item;
    if (!triggerCfg.snbt.empty()) {
        item = createItemFromSnbt(triggerCfg.snbt);
    }
    if (item.isNull()) {
        item = createItemFromType(triggerCfg.type, triggerCfg.count);
    }

    if (item.isNull()) {
        player.sendMessage("§c[MeowMenu] 创建物品失败");
        return;
    }

    // 检查背包空间
    auto& inventory = player.getInventory();
    if (!inventory.hasRoomForItem(item)) {
        player.sendMessage(formatMessage(cfg.giveUs.msgInventoryFull));
        return;
    }

    // 给予物品
    player.add(item);
    player.sendMessage(formatMessage(cfg.giveUs.msgReceived));
}

bool TriggerItem::hasItemInInventory(Player& player) {
    auto const& cfg = ConfigManager::getInstance().getConfig();
    auto const& triggerCfg = cfg.triggerItem;

    try {
        auto& inventory = player.getInventory();
        int size = inventory.getContainerSize();
        for (int i = 0; i < size; ++i) {
            auto const& item = inventory.getItem(i);
            if (item.isNull()) continue;
            if (isItemTypeMatch(item, triggerCfg.type)) {
                return true;
            }
        }
    } catch (...) {}
    return false;
}

void TriggerItem::clearItem(Player& player) {
    auto const& cfg = ConfigManager::getInstance().getConfig();
    auto const& triggerCfg = cfg.triggerItem;

    try {
        auto& inventory = player.getInventory();
        int size = inventory.getContainerSize();
        for (int i = 0; i < size; ++i) {
            auto const& item = inventory.getItem(i);
            if (item.isNull()) continue;
            if (isItemTypeMatch(item, triggerCfg.type)) {
                inventory.removeItem(i, (int)item.mCount);
            }
        }
        player.sendMessage(formatMessage(cfg.giveUs.msgCleared));
    } catch (std::exception const& e) {
        player.sendMessage("§c[MeowMenu] 清除物品失败");
    }
}

bool TriggerItem::isTriggerItem(Player& player) {
    auto const& cfg = ConfigManager::getInstance().getConfig();
    auto const& triggerCfg = cfg.triggerItem;

    try {
        // 获取手持物品
        auto& item = player.getCarriedItem();
        if (item.isNull()) return false;
        return isItemTypeMatch(item, triggerCfg.type);
    } catch (...) {
        return false;
    }
}

std::string TriggerItem::formatMessage(std::string const& template_) {
    return ConfigManager::getInstance().formatMessage(template_);
}

} // namespace meowmenu
