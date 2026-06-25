// CommandRegistration.cpp - /mmenu 命令注册
//
// 命令格式：
//   /mmenu            - 打开主菜单（所有玩家）
//   /mmenu admin      - 打开菜单编辑器（仅OP）
//   /mmenu mode       - 玩家模式选择（本版本仅 static，保留命令兼容）
//   /mmgv             - 快捷获取菜单物品
//   /clearmmenu       - 清除菜单物品
#include "MeowMenu.h"
#include "Config.h"
#include "MenuRenderer.h"
#include "MenuEditor.h"
#include "TriggerItem.h"

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"

#include <string>

namespace meowmenu {

// 命令参数结构体（必须是聚合体以支持 boost::pfr 反射）
struct ActionParam {
    std::string action;
};

void registerCommands() {
    auto& logger = lib().getSelf().getLogger();

    //=== /mmenu [admin|mode] - 打开菜单 ===
    auto& cmd = ll::command::CommandRegistrar::getInstance(false)
                    .getOrCreateCommand("mmenu", "§a打开§b服务器§e菜单", CommandPermissionLevel::Any);

    // /mmenu - 无参数，打开主菜单
    cmd.overload().execute(
        [](CommandOrigin const& origin, CommandOutput& output) {
            auto* entity = origin.getEntity();
            if (!entity || !entity->isPlayer()) {
                output.error("§c此指令仅允许游戏内玩家执行");
                return;
            }
            auto& player = static_cast<Player&>(*entity);
            auto const& cfg = ConfigManager::getInstance().getConfig();
            MenuRenderer::getInstance().renderMenu(player, cfg.mainMenuId);
        }
    );

    // /mmenu <action> - 带参数重载
    cmd.overload<ActionParam>().required("action").execute(
        [](CommandOrigin const& origin, CommandOutput& output, ActionParam const& param) {
            auto* entity = origin.getEntity();
            if (!entity || !entity->isPlayer()) {
                output.error("§c此指令仅允许游戏内玩家执行");
                return;
            }
            auto& player = static_cast<Player&>(*entity);

            if (param.action == "admin") {
                // 检查 OP 权限
                if ((int)player.getCommandPermissionLevel() < 1) {
                    player.sendMessage("§c[MeowMenu] 权限不足");
                    return;
                }
                MenuEditor::showEditorMain(player);
            } else if (param.action == "mode") {
                // 本版本仅 static 模式，保留命令兼容
                player.sendMessage("§a[MeowMenu] 当前版本仅支持普通菜单(static)模式");
            } else {
                auto const& cfg = ConfigManager::getInstance().getConfig();
                MenuRenderer::getInstance().renderMenu(player, cfg.mainMenuId);
            }
        }
    );

    //=== /mmgv - 快捷获取菜单物品 ===
    auto& mmgvCmd = ll::command::CommandRegistrar::getInstance(false)
                        .getOrCreateCommand("mmgv", "§a快捷获取菜单物品", CommandPermissionLevel::Any);
    mmgvCmd.overload().execute(
        [](CommandOrigin const& origin, CommandOutput& output) {
            auto* entity = origin.getEntity();
            if (!entity || !entity->isPlayer()) {
                output.error("§c此指令仅允许游戏内玩家执行");
                return;
            }
            auto& player = static_cast<Player&>(*entity);
            TriggerItem::giveItem(player, false);
        }
    );

    //=== /clearmmenu - 清除菜单物品 ===
    auto& clearCmd = ll::command::CommandRegistrar::getInstance(false)
                         .getOrCreateCommand("clearmmenu", "§c清除菜单物品", CommandPermissionLevel::Any);
    clearCmd.overload().execute(
        [](CommandOrigin const& origin, CommandOutput& output) {
            auto* entity = origin.getEntity();
            if (!entity || !entity->isPlayer()) {
                output.error("§c此指令仅允许游戏内玩家执行");
                return;
            }
            auto& player = static_cast<Player&>(*entity);
            TriggerItem::clearItem(player);
        }
    );

    logger.info("命令已注册: /mmenu [admin|mode] | /mmgv | /clearmmenu");
}

} // namespace meowmenu
