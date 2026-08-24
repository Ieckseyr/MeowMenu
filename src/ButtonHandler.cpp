// ButtonHandler.cpp - 按钮点击处理实现
#include "ButtonHandler.h"
#include "MenuStore.h"
#include "MenuRenderer.h"
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
    }
}

} // namespace meowmenu
