// TriggerItem.cpp - 触发物品（主菜单钥匙）的发放/检查/清除; 属性来自 config.triggerItem
#include "TriggerItem.h"
#include "Config.h"
#include "MeowMenu.h"

#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/Inventory.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/ItemStackBase.h>
#include <mc/world/level/Level.h>
#include <mc/world/ContainerID.h>
#include <mc/network/packet/InventorySlotPacket.h>

#include <mc/deps/nbt/CompoundTag.h>

#include <ll/api/Expected.h>

#include <fmt/format.h>

namespace meowmenu {

// 主菜单触发物品类型（config.triggerItem.type；空则回落 minecraft:clock）
static std::string menuItemType() {
    auto const& cfg = ConfigManager::getInstance().getConfig();
    return cfg.triggerItem.type.empty() ? std::string("minecraft:clock") : cfg.triggerItem.type;
}

// 主菜单触发物品 SNBT（config.triggerItem.snbt；空则用内置默认 kDefaultTriggerSnbt）
static std::string menuItemSnbt() {
    auto const& cfg = ConfigManager::getInstance().getConfig();
    if (!cfg.triggerItem.snbt.empty()) return cfg.triggerItem.snbt;

    // 留空 = 内置默认; 若 type 改成了别的物品, 把默认模板里的 Name 跟着换成 type
    // （否则发出去的还是钟, 与按 type 的触发判定对不上）
    std::string snbt = kDefaultTriggerSnbt;
    std::string type = menuItemType();
    if (type != "minecraft:clock") {
        std::string const from = "\"Name\":\"minecraft:clock\"";
        auto              pos  = snbt.find(from);
        if (pos != std::string::npos) {
            snbt.replace(pos, from.size(), "\"Name\":\"" + type + "\"");
        }
    }
    return snbt;
}

// 检查物品类型是否为菜单触发物品
static bool isMenuTypeMatch(ItemStackBase const& item) {
    try {
        return item.getTypeName() == menuItemType();
    } catch (...) {
        return false;
    }
}

// 从 config 指定的 SNBT 创建菜单物品
static ItemStack createMenuItem() {
    auto& logger = lib().getSelf().getLogger();

    auto build = [](std::string const& snbt) -> ItemStack {
        try {
            auto tagOpt = CompoundTag::fromSnbt(snbt);
            if (tagOpt.has_value()) {
                return ItemStack::fromTag(tagOpt.value());
            }
        } catch (...) {}
        return ItemStack{};
    };

    ItemStack item = build(menuItemSnbt());
    if (item.isNull()) {
        logger.warn("触发物品 SNBT 无效（config.triggerItem.snbt）→ 本次改用内置默认 clock");
        item = build(kDefaultTriggerSnbt);
        if (item.isNull()) return ItemStack{}; // 内置 SNBT 也失败（理论上不会发生）
    }

    // SNBT 内 Name 与 triggerItem.type 不一致时，发出的物品按类型匹配识别不出来 —— 明确告警
    std::string typeName;
    try {
        typeName = item.getTypeName();
    } catch (...) {}
    if (!typeName.empty() && typeName != menuItemType()) {
        logger.warn(
            "config.triggerItem.type={} 与 SNBT 内 Name={} 不一致：触发判定按 type，发出的物品将无法打开菜单",
            menuItemType(),
            typeName
        );
    }

    int count = ConfigManager::getInstance().getConfig().triggerItem.count;
    if (count >= 1 && static_cast<uchar>(count) != item.mCount) {
        try {
            item.set(count);
        } catch (...) {}
    }
    return item;
}

// 向指定槽位发送单格同步包（客户端立即看到该格变化，无需全包 refreshInventory）
static void sendSlotPacket(Player& player, int slot, ItemStack const& item) {
    try {
        // 26.32: InventorySlotPacket 3 参构造移至 Payload
        InventorySlotPacket pkt{::InventorySlotPacketPayload(ContainerID::Inventory, slot, item)};
        player.sendNetworkPacket(pkt);
    } catch (...) {}
}

bool TriggerItem::giveItem(Player& player, bool isJoin) {
    auto const& cfg = ConfigManager::getInstance().getConfig();
    if (!cfg.giveUs.enabled) return false;

    // 背包里已经有菜单物品：目标已达成（进服路径不提示；手动路径提示一句）
    if (hasItemInInventory(player)) {
        if (!isJoin) {
            player.sendMessage(formatMessage(cfg.giveUs.msgAlreadyHave));
        }
        return true;
    }

    // 从 config 指定的 SNBT 创建物品
    ItemStack item = createMenuItem();
    if (item.isNull()) {
        player.sendMessage("§c[MeowMenu] 创建菜单物品失败（检查 config.triggerItem.snbt）");
        return false;
    }

    // 走 Player::add 事务系统, 客户端才能正常移动这件物品
    ItemStack toAdd = item;
    bool added = false;
    try {
        added = player.add(toAdd);
    } catch (...) {
        player.sendMessage("§c[MeowMenu] 写入背包失败");
        return false;
    }

    if (!added) {
        // 背包已满（add 返回 false 表示未能完整放入）
        player.sendMessage(formatMessage(cfg.giveUs.msgInventoryFull));
        return false;
    }

    // 进服时单格同步可能被客户端初始加载覆盖, 这里全量刷新一次背包
    try {
        player.refreshInventory();
    } catch (...) {}

    player.sendMessage(formatMessage(cfg.giveUs.msgReceived));
    return true;
}

bool TriggerItem::hasItemInInventory(Player& player) {
    try {
        auto& inventory = player.getInventory();
        int size = inventory.getContainerSize();
        for (int i = 0; i < size; ++i) {
            auto const& item = inventory.getItem(i);
            if (item.isNull()) continue;
            if (isMenuTypeMatch(item)) {
                return true;
            }
        }
    } catch (...) {}
    return false;
}

void TriggerItem::clearItem(Player& player) {
    auto const& cfg = ConfigManager::getInstance().getConfig();

    try {
        auto& inventory = player.getInventory();
        int size = inventory.getContainerSize();

        ItemStack emptyItem; // 空物品，用于客户端清空槽位
        for (int i = 0; i < size; ++i) {
            auto const& item = inventory.getItem(i);
            if (item.isNull()) continue;
            if (isMenuTypeMatch(item)) {
                inventory.removeItem(i, (int)item.mCount);
                // 单格同步：客户端立即看到该格清空
                sendSlotPacket(player, i, emptyItem);
            }
        }

        player.sendMessage(formatMessage(cfg.giveUs.msgCleared));
    } catch (std::exception const&) {
        player.sendMessage("§c[MeowMenu] 清除物品失败");
    }
}

bool TriggerItem::isTriggerItem(Player& player) {
    try {
        // 获取手持物品
        auto& item = player.getCarriedItem();
        return isTriggerItem(item);
    } catch (...) {
        return false;
    }
}

bool TriggerItem::isTriggerItem(ItemStackBase const& item) {
    if (item.isNull()) return false;
    return isMenuTypeMatch(item);
}

std::string TriggerItem::formatMessage(std::string const& template_) {
    return ConfigManager::getInstance().formatMessage(template_);
}

} // namespace meowmenu
