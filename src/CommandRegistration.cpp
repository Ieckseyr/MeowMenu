// CommandRegistration.cpp - /mmenu 命令注册
//
// 命令格式：
//   /mmenu            - 打开主菜单（所有玩家）
//   /mmenu admin      - 打开菜单编辑器（仅OP）
//   /mmenu reload     - 热重载配置与菜单数据（仅OP）
//   /mmenu mode       - 玩家模式选择（本版本仅 static，保留命令兼容）
//   /mmgv             - 快捷获取菜单物品
//   /clearmmenu       - 清除菜单物品
#include "MeowMenu.h"
#include "Config.h"
#include "MenuStore.h"
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

// /mmenu 与别名命令的共享执行逻辑
// action 为空串表示无参重载（打开主菜单），非空串表示子指令
static void executeMenuAction(CommandOrigin const& origin, CommandOutput& output, std::string const& action) {
    auto* entity = origin.getEntity();

    // 无参：打开主菜单
    if (action.empty()) {
        if (!entity || !entity->isPlayer()) {
            output.error("§c此指令仅允许游戏内玩家执行");
            return;
        }
        auto& player = static_cast<Player&>(*entity);
        auto const& cfg = ConfigManager::getInstance().getConfig();
        MenuRenderer::getInstance().renderMenu(player, cfg.mainMenuId);
        return;
    }

    // reload 子指令：配置 + 菜单数据热重载（仅OP/控制台）
    if (action == "reload") {
        auto* player = (entity && entity->isPlayer()) ? static_cast<Player*>(entity) : nullptr;
        if (player && (int)player->getCommandPermissionLevel() < 1) {
            player->sendMessage("§c[MeowMenu] 权限不足");
            return;
        }
        try {
            ConfigManager::getInstance().load();
            MenuStore::getInstance().loadAll();
            output.success("§a[MeowMenu] 配置与菜单数据已热重载");
        } catch (std::exception const& e) {
            output.error("§c[MeowMenu] 热重载失败: " + std::string(e.what()));
        }
        return;
    }

    // 以下子指令需要玩家上下文
    if (!entity || !entity->isPlayer()) {
        output.error("§c此指令仅允许游戏内玩家执行");
        return;
    }
    auto& player = static_cast<Player&>(*entity);

    if (action == "admin") {
        if ((int)player.getCommandPermissionLevel() < 1) {
            player.sendMessage("§c[MeowMenu] 权限不足");
            return;
        }
        MenuEditor::showEditorMain(player);
    } else if (action == "mode") {
        // 本版本仅 static 模式，保留命令兼容
        player.sendMessage("§a[MeowMenu] 当前版本仅支持普通菜单(static)模式");
    } else {
        // 未知子指令给出帮助提示
        player.sendMessage("§e[MeowMenu] 未知子指令: §6" + action);
        player.sendMessage("§7可用子指令: §areload§7(热重载) | §aadmin§7(编辑器) | §amode§7(模式)");
        player.sendMessage("§7直接输入 §a/mmenu §7可打开主菜单");
    }
}

void registerCommands() {
    auto& logger = lib().getSelf().getLogger();

    //=== /mmenu [admin|mode] - 打开菜单 ===
    auto& cmd = ll::command::CommandRegistrar::getInstance(false)
                    .getOrCreateCommand("mmenu", "§a打开§b服务器§e菜单", CommandPermissionLevel::Any);

    // /mmenu - 无参数，打开主菜单
    cmd.overload().execute(
        [](CommandOrigin const& origin, CommandOutput& output) {
            executeMenuAction(origin, output, "");
        }
    );

    // /mmenu <action> - 带参数重载（子指令均为具体功能，不再打开菜单）
    cmd.overload<ActionParam>().required("action").execute(
        [](CommandOrigin const& origin, CommandOutput& output, ActionParam const& param) {
            executeMenuAction(origin, output, param.action);
        }
    );

    //=== 别名命令（如果配置了 giveUs.command.alias）===
    auto const& cfg = ConfigManager::getInstance().getConfig();
    if (!cfg.giveUs.commandAlias.empty()) {
        try {
            auto& aliasCmd = ll::command::CommandRegistrar::getInstance(false)
                                .getOrCreateCommand(cfg.giveUs.commandAlias, cfg.giveUs.commandDesc,
                                                    CommandPermissionLevel::Any);
            aliasCmd.overload().execute(
                [](CommandOrigin const& origin, CommandOutput& output) {
                    executeMenuAction(origin, output, "");
                }
            );
            aliasCmd.overload<ActionParam>().required("action").execute(
                [](CommandOrigin const& origin, CommandOutput& output, ActionParam const& param) {
                    executeMenuAction(origin, output, param.action);
                }
            );
            logger.info("命令别名已注册: /{} (同 /mmenu)", cfg.giveUs.commandAlias);
        } catch (std::exception const& e) {
            logger.warn("注册命令别名 {} 失败（可能已被占用）: {}", cfg.giveUs.commandAlias, e.what());
        } catch (...) {
            logger.warn("注册命令别名 {} 失败：未知异常", cfg.giveUs.commandAlias);
        }
    }

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

    logger.info("命令已注册: /mmenu [admin|reload|mode] | /mmgv | /clearmmenu");
}

} // namespace meowmenu
