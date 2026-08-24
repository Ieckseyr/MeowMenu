// TriggerItem.cpp - 触发物品系统实现
//
// 给予方式：Player::add（走完整 InventoryTransactionManager 事务系统，保证物品可移动）
//           + Player::refreshInventory（强制全包同步，确保客户端立即可见）
// 单格 InventorySlotPacket 仅用于 clearItem 清空槽位场景。
// 物品 NBT 硬编码，严格固定，不允许通过配置改动。
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

// 菜单触发物品类型（与 NBT 中的 Name 一致）
static constexpr const char* MENU_ITEM_TYPE = "minecraft:clock";

// 菜单触发物品的唯一 NBT（严格固定，不允许使用其他 NBT）
// minecraft:clock，自定义名"§r§4服§6务§g器§a菜§b单"，含电脑/手机使用说明 Lore，
// minecraft:keep_on_death=1b（死亡不掉落）
static constexpr const char* MENU_ITEM_SNBT =
    "{\"Count\":1b,\"Damage\":0s,\"Name\":\"minecraft:clock\",\"WasPickedUp\":0b,"
    "\"tag\":{\"RepairCost\":0,"
    "\"display\":{\"Lore\":["
    "\"§a§[电脑]▶§4- §r§l 手持右键即可打开菜单\","
    "\"§b§[手机]▶§4- §r§l 手持长按或点击地面即可打开菜单\""
    "],\"Name\":\"§r§4服§6务§g器§a菜§b单\"},"
    "\"minecraft:keep_on_death\":1b}}";

// 检查物品类型是否为菜单触发物品
static bool isMenuTypeMatch(ItemStack const& item) {
    try {
        return item.getTypeName() == MENU_ITEM_TYPE;
    } catch (...) {
        return false;
    }
}

// 从硬编码 SNBT 创建菜单物品
static ItemStack createMenuItem() {
    try {
        auto tagOpt = CompoundTag::fromSnbt(MENU_ITEM_SNBT);
        if (tagOpt.has_value()) {
            return ItemStack::fromTag(tagOpt.value());
        }
    } catch (...) {}
    return ItemStack{};
}

// 向指定槽位发送单格同步包（客户端立即看到该格变化，无需全包 refreshInventory）
static void sendSlotPacket(Player& player, int slot, ItemStack const& item) {
    try {
        InventorySlotPacket pkt(ContainerID::Inventory, static_cast<uint>(slot), item);
        player.sendNetworkPacket(pkt);
    } catch (...) {}
}

void TriggerItem::giveItem(Player& player, bool isJoin) {
    auto const& cfg = ConfigManager::getInstance().getConfig();
    if (!cfg.giveUs.enabled) return;

    // 检查背包是否已有菜单物品
    if (hasItemInInventory(player)) {
        if (!isJoin) {
            player.sendMessage(formatMessage(cfg.giveUs.msgAlreadyHave));
        }
        return;
    }

    // 从硬编码 NBT 创建物品（严格使用唯一 NBT）
    ItemStack item = createMenuItem();
    if (item.isNull()) {
        player.sendMessage("§c[MeowMenu] 创建菜单物品失败");
        return;
    }

    // 通过 Player::add 走完整背包事务系统（InventoryTransactionManager），
    // 确保物品在客户端可正常移动。
    // 旧实现用 inventory.setItem + 手动 InventorySlotPacket 绕过事务管理器，
    // 导致客户端能显示但无法移动（事务管理器视图过期，移动请求被服务端拒绝）。
    ItemStack toAdd = item;
    bool added = false;
    try {
        added = player.add(toAdd);
    } catch (...) {
        player.sendMessage("§c[MeowMenu] 写入背包失败");
        return;
    }

    if (!added) {
        // 背包已满（add 返回 false 表示未能完整放入）
        player.sendMessage(formatMessage(cfg.giveUs.msgInventoryFull));
        return;
    }

    // 强制刷新客户端背包（全包同步）
    // 进服给予时（PlayerJoinEvent 后 500ms），player.add 触发的单格同步包可能
    // 因客户端初始背包加载流程未完成而被覆盖/丢失，导致物品写入服务端但客户端不可见。
    // 此处主动 refreshInventory 重新发送完整背包，确保玩家立即可见。
    try {
        player.refreshInventory();
    } catch (...) {}

    player.sendMessage(formatMessage(cfg.giveUs.msgReceived));
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
        if (item.isNull()) return false;
        return isMenuTypeMatch(item);
    } catch (...) {
        return false;
    }
}

std::string TriggerItem::formatMessage(std::string const& template_) {
    return ConfigManager::getInstance().formatMessage(template_);
}

} // namespace meowmenu
