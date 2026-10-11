// ButtonHandler.cpp - 按钮点击处理实现
#include "ButtonHandler.h"
#include "Config.h"
#include "MenuStore.h"
#include "MenuRenderer.h"
#include "NpcMenuBridge.h"
#include "MeowMenu.h"

#include "meowpapi/PlaceholderApi.h"

#include <ll/api/service/Bedrock.h>

#include <mc/server/commands/CommandContext.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/MinecraftCommands.h>
#include <mc/server/commands/PlayerCommandOrigin.h>
#include <mc/world/Minecraft.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/level/Level.h>
#include <mc/deps/core/utility/MCRESULT.h>

#include <fmt/format.h>

#include <memory>

namespace meowmenu {

bool ButtonHandler::isOp(Player& player) {
    try {
        return (int)player.getCommandPermissionLevel() >= 1;
    } catch (...) {
        return false;
    }
}

std::string ButtonHandler::resolveCommand(Player& player, std::string const& command) {
    // 先通过 MeowPAPI 翻译变量（%name% 和 {name} 格式）
    std::string result = meowpapi::PlaceholderApi::getInstance().translateStringWithPlayer(command, &player);

    // 兼容旧的 {player} 变量（PAPI 未注册时回退）
    std::string playerName = player.getNameTag();
    std::string search = "{player}";
    size_t      pos    = 0;
    while ((pos = result.find(search, pos)) != std::string::npos) {
        result.replace(pos, search.size(), playerName);
        pos += playerName.size();
    }
    return result;
}

void ButtonHandler::executeCommand(Player& player, std::string const& command) {
    if (command.empty()) return;

    std::string resolved = resolveCommand(player, command);
    try {
        // 以玩家身份执行命令
        auto mc = ll::service::getMinecraft();
        if (!mc.has_value()) return;
        auto& minecraftCommands = mc->mCommands;
        if (!minecraftCommands) return;

        CommandContext ctx = CommandContext(
            resolved,
            std::make_unique<PlayerCommandOrigin>(player.getLevel(), player.getOrCreateUniqueID()),
            static_cast<int>(CurrentCmdVersion::Latest)
        );
        minecraftCommands->executeCommand(ctx, true);
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error("执行命令失败: {} - {}", resolved, e.what());
    }
}

void ButtonHandler::executeAdminCommand(Player& player, std::string const& command) {
    if (!isOp(player)) {
        player.sendMessage("§c[MeowMenu] 权限不足");
        return;
    }
    executeCommand(player, command);
}

void ButtonHandler::openSubMenu(Player& player, std::string const& subMenuId) {
    std::string id = subMenuId;
    if (id.empty()) {
        player.sendMessage("§c[MeowMenu] 子菜单ID为空");
        return;
    }

    // 检查菜单是否存在，不存在则自动创建
    auto menu = MenuStore::getInstance().getMenu(id);
    if (!menu) {
        MenuStore::getInstance().createMenu(id, id, "");
        menu = MenuStore::getInstance().getMenu(id);
    }

    if (!menu) {
        player.sendMessage("§c[MeowMenu] 无法打开子菜单: " + id);
        return;
    }

    MenuRenderer::getInstance().renderMenu(player, id);
}

void ButtonHandler::handleButton(Player& player, Button const& button) {
    if (button.type == "spacer") return; // 空格按钮跳过

    if (button.type == "command") {
        executeCommand(player, button.value);
    } else if (button.type == "admin_command") {
        executeAdminCommand(player, button.value);
    } else if (button.type == "submenu" || button.type == "folder") {
        openSubMenu(player, button.value);
    } else if (button.type == "admin_submenu") {
        if (!isOp(player)) {
            player.sendMessage("§c[MeowMenu] 权限不足，无法访问此菜单");
            return;
        }
        openSubMenu(player, button.value);
    } else if (button.type == "npc_menu") {
        openNpcMenu(player, button.value);
    } else if (button.type == "npc_spawn") {
        spawnNpcMenu(player, button.value, button.text);
    } else {
        // 未知类型: 明确提示而不是静默无反应（作者多半是类型名拼错了）
        lib().getSelf().getLogger().warn("未知按钮类型: {} (文本={})", button.type, button.text);
        player.sendMessage(
            "§c[MeowMenu] 未知按钮类型: §6" + button.type
            + "\n§7可用: command / admin_command / submenu / admin_submenu / folder / npc_menu / npc_spawn"
        );
    }
}

//==== NPC 菜单按钮（HologramLib 可选前置） ====
void ButtonHandler::openNpcMenu(Player& player, std::string const& menuId) {
    auto& bridge = NpcMenuBridge::getInstance();
    if (!bridge.isAvailable()) {
        player.sendMessage(
            "§c[MeowMenu] 此按钮需要 HologramLib（未安装或版本过低）\n§7可改用 §esubmenu §7类型以普通表单打开同一菜单"
        );
        return;
    }
    // 菜单不存在/玩家不在线时内部已提示
    // 用延迟版: 这个按钮也可能是从 NPC 对话里点的, 那时必须等库拆完旧对话再开（时序见 NpcMenuBridge.cpp）
    bridge.openDialogueDeferred(player, menuId);
}

void ButtonHandler::spawnNpcMenu(Player& player, std::string const& menuId, std::string const& npcName) {
    auto& bridge = NpcMenuBridge::getInstance();
    if (!bridge.canManageNpc()) {
        player.sendMessage("§c[MeowMenu] 此按钮需要 HologramLib 的 NPC 域（未安装或版本过低）");
        return;
    }

    // 未指定菜单时用主菜单; NPC 名默认取按钮文本（颜色码/换行由 NpcMenuBridge 统一清洗）
    std::string id   = menuId.empty() ? ConfigManager::getInstance().getConfig().mainMenuId : menuId;
    std::string name = npcName.empty() ? std::string("菜单NPC") : npcName;

    int64_t npcId = bridge.spawnNpcNear(player, name, id);
    if (npcId < 0) {
        player.sendMessage(
            "§c[MeowMenu] 菜单 NPC 生成失败: " + std::to_string(npcId)
            + "\n§7-1 = 前置/参数或菜单不存在, -2 = id 占用, -3 = 皮肤采集失败（需在线玩家）"
        );
        return;
    }

    auto pos = player.getPosition();
    player.sendMessage(fmt::format(
        "§a[MeowMenu] 已生成菜单 NPC §b{}§a (id={})\n§7位置: §f({:.1f}, {:.1f}, {:.1f})§7 维度 §f{}\n§7右键点击它即可打开菜单 §f{}",
        name,
        npcId,
        pos.x,
        pos.y,
        pos.z,
        (int)player.getDimensionId(),
        id
    ));
}

} // namespace meowmenu
