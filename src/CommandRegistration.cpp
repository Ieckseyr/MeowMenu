// CommandRegistration.cpp - 命令注册（/mmenu · /mmgv · /clearmmenu · /mnpc · /mmenu ui）
#include "MeowMenu.h"
#include "Config.h"
#include "MenuStore.h"
#include "MenuRenderer.h"
#include "MenuEditor.h"
#include "TriggerItem.h"
#include "NpcMenuBridge.h"
#include "PlayerUiPref.h"

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"

#include <ll/api/form/SimpleForm.h>

#include <fmt/format.h>

#include <string>

namespace meowmenu {

// 样式名 → 显示名
static std::string uiStyleName(std::string const& mode) {
    if (mode == ui_mode::kNpc) return "§bNPC 对话";
    if (mode == ui_mode::kChest) return "§6箱子界面";
    if (mode == ui_mode::kForm) return "§f普通表单";
    return "§7跟随菜单";
}

// 命令参数结构体（必须是聚合体以支持 boost::pfr 反射）
struct ActionParam {
    std::string action;
};

// 前向声明（定义在本文件下方）
void showUiStyleForm(Player& player);

// /mmenu 与别名命令的共享执行逻辑（action 空 = 无参重载, 打开主菜单）
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

    if (action == "ui") {
        // 玩家自选界面样式: /mmenu ui 打开选择界面
        showUiStyleForm(player);
        return;
    }

    if (action == "admin") {
        if ((int)player.getCommandPermissionLevel() < 1) {
            player.sendMessage("§c[MeowMenu] 权限不足");
            return;
        }
        MenuEditor::showEditorMain(player);
    } else if (action == "mode") {
        // 兼容旧指令: 指向新的界面样式选择
        showUiStyleForm(player);
    } else {
        // 未知子指令给出帮助提示
        player.sendMessage("§e[MeowMenu] 未知子指令: §6" + action);
        player.sendMessage("§7可用子指令: §areload§7(热重载) | §aadmin§7(编辑器) | §aui§7(界面样式)");
        player.sendMessage("§7NPC 对话菜单: §a/mnpc status§7(状态) | §a/mnpc test§7(一键测试) | §a/mnpc open");
        player.sendMessage("§7直接输入 §a/mmenu §7可打开主菜单");
    }
}

//==== /mnpc - NPC 对话菜单（status / list / open / close / test / spawn / bind / remove / reload） ====

// 配套测试菜单: Meowdata/MeowMenu/data/npc_test_*.json
static constexpr char const* kNpcTestMenuId = "npc_test_main";
static constexpr char const* kNpcTestName   = "菜单测试";

struct NpcSubParam {
    std::string sub;
};
struct NpcSub1Param {
    std::string sub;
    std::string arg1;
};
struct NpcSub2Param {
    std::string sub;
    std::string arg1;
    std::string arg2;
};

static bool parseNpcId(std::string const& text, int64_t& out) {
    if (text.empty()) return false;
    try {
        size_t pos = 0;
        out        = std::stoll(text, &pos);
        return pos == text.size();
    } catch (...) {
        return false;
    }
}

static void npcMenuCommand(
    CommandOrigin const& origin,
    CommandOutput&       output,
    std::string const&   sub,
    std::string const&   arg1,
    std::string const&   arg2
) {
    auto&   bridge = NpcMenuBridge::getInstance();
    auto*   entity = origin.getEntity();
    Player* player = (entity && entity->isPlayer()) ? static_cast<Player*>(entity) : nullptr;

    auto requirePlayer = [&]() -> bool {
        if (!player) {
            output.error("§c此指令仅允许游戏内玩家执行");
            return false;
        }
        return true;
    };

    //===== 所有玩家可用 =====
    if (sub == "status") {
        output.success("§a[MeowMenu] NPC 菜单状态:\n§f" + bridge.statusText());
        return;
    }
    if (sub == "list") {
        output.success("§a[MeowMenu] 菜单 NPC 绑定列表:\n§f" + bridge.listNpcsJson());
        return;
    }
    if (sub == "open") {
        if (!requirePlayer()) return;
        if (!bridge.isAvailable()) {
            output.error("§cHologramLib 不可用（未安装或版本过低）→ NPC 对话菜单暂不可用");
            return;
        }
        if (!bridge.openDialogue(*player, arg1)) return;
        output.success("§a已以 NPC 对话形式打开: " + (arg1.empty() ? std::string("主菜单") : arg1));
        return;
    }
    if (sub == "close") {
        if (!requirePlayer()) return;
        bridge.closeDialogue(*player);
        return;
    }

    //===== 以下需要 OP =====
    if (player && static_cast<int>(player->getCommandPermissionLevel()) < 1) {
        output.error("§c[MeowMenu] 权限不足");
        return;
    }

    if (sub == "test") {
        if (!requirePlayer()) return;
        if (!bridge.isAvailable()) {
            output.error("§cHologramLib 不可用（未安装或版本过低）→ 请先装好 HologramLib 再执行本测试");
            return;
        }
        if (!bridge.canManageNpc()) {
            output.error("§cHologramLib 缺少 NPC 生成域, 无法生成测试 NPC；可直接用 §a/mnpc open " + std::string(kNpcTestMenuId) + " §c查看对话菜单");
            return;
        }
        int64_t npcId = bridge.spawnNpcNear(*player, kNpcTestName, kNpcTestMenuId);
        if (npcId < 0) {
            output.error(
                "§c测试 NPC 生成失败: " + std::to_string(npcId)
                + " §7(-1 前置/菜单缺失, -2 id 占用, -3 皮肤未注册)"
            );
            output.error("§7请确认菜单数据存在: Meowdata/MeowMenu/data/" + std::string(kNpcTestMenuId) + ".json");
            return;
        }
        output.success("§a[MeowMenu] 测试 NPC 已生成 (id=" + std::to_string(npcId) + "), 位置在你身前 2 格");
        output.success("§7右键点击它即可打开 NPC 对话菜单（三层嵌套 + 真实命令执行）");
        output.success("§7也可随时用 §a/mnpc open " + std::string(kNpcTestMenuId) + " §7直接打开");
        // 立刻打开一次, 省去跑过去右键
        bridge.openDialogue(*player, kNpcTestMenuId, kNpcTestName);
        return;
    }

    if (sub == "spawn") {
        if (!requirePlayer()) return;
        if (arg1.empty() || arg2.empty()) {
            output.error("§c用法: /mnpc spawn <NPC名> <菜单ID>");
            return;
        }
        if (!bridge.canManageNpc()) {
            output.error("§cHologramLib 不可用或缺少 NPC 生成域");
            return;
        }
        int64_t npcId = bridge.spawnNpcNear(*player, arg1, arg2);
        if (npcId < 0) {
            output.error(
                "§c生成失败: " + std::to_string(npcId) + " §7(-1 前置/菜单不存在, -2 id 占用, -3 皮肤未注册)"
            );
            return;
        }
        output.success(
            "§a菜单 NPC 已生成: id=" + std::to_string(npcId) + ", 绑定菜单 " + arg2 + "（右键点击即打开）"
        );
        return;
    }

    if (sub == "bind") {
        int64_t npcId = 0;
        if (!parseNpcId(arg1, npcId) || arg2.empty()) {
            output.error("§c用法: /mnpc bind <npcId> <菜单ID>");
            return;
        }
        if (!bridge.bindNpc(npcId, arg2)) {
            output.error("§c绑定失败: NPC 不存在或菜单 ID 无效（先用 /mnpc list 或 HologramLib 侧确认 id）");
            return;
        }
        output.success("§a已把 NPC " + std::to_string(npcId) + " 绑定到菜单 " + arg2);
        return;
    }

    if (sub == "remove") {
        int64_t npcId = 0;
        if (!parseNpcId(arg1, npcId)) {
            output.error("§c用法: /mnpc remove <npcId>（id 见 /mnpc list）");
            return;
        }
        if (!bridge.removeNpc(npcId)) {
            output.error("§c删除失败: 该 id 不在本插件的绑定表中且库内不存在");
            return;
        }
        output.success("§a已删除菜单 NPC " + std::to_string(npcId));
        return;
    }

    if (sub == "reload") {
        if (!bridge.canManageNpc()) {
            output.error("§cHologramLib 不可用或缺少 NPC 生成域");
            return;
        }
        bridge.reloadBindings();
        output.success("§a已重新加载 npc_menus.json 并重建菜单 NPC");
        return;
    }

    output.error("§e[MeowMenu] 未知子指令: §6" + sub);
    output.success(
        "§7可用: §astatus §7| §alist §7| §aopen [菜单ID] §7| §aclose §7| §atest §7| "
        "§aspawn <名> <菜单ID> §7| §abind <npcId> <菜单ID> §7| §aremove <npcId> §7| §areload"
    );
}

static void registerNpcCommands() {
    auto& cmd = ll::command::CommandRegistrar::getInstance(false).getOrCreateCommand(
        "mnpc",
        "§bNPC 对话菜单§7(HologramLib) §a测试与管理",
        CommandPermissionLevel::Any
    );

    cmd.overload<NpcSubParam>().required("sub").execute(
        [](CommandOrigin const& origin, CommandOutput& output, NpcSubParam const& param) {
            npcMenuCommand(origin, output, param.sub, "", "");
        }
    );
    cmd.overload<NpcSub1Param>().required("sub").required("arg1").execute(
        [](CommandOrigin const& origin, CommandOutput& output, NpcSub1Param const& param) {
            npcMenuCommand(origin, output, param.sub, param.arg1, "");
        }
    );
    cmd.overload<NpcSub2Param>().required("sub").required("arg1").required("arg2").execute(
        [](CommandOrigin const& origin, CommandOutput& output, NpcSub2Param const& param) {
            npcMenuCommand(origin, output, param.sub, param.arg1, param.arg2);
        }
    );
}

//==== 界面样式选择（跟随菜单 / 表单 / NPC 对话 / 箱子） ====
void showUiStyleForm(Player& player) {
    auto& pref = PlayerUiPref::getInstance();
    auto& bridge = NpcMenuBridge::getInstance();
    std::string current = pref.get(player);

    auto fm = ll::form::SimpleForm{};
    fm.setTitle("§l§b界面样式");
    fm.setContent(fmt::format(
        "§7当前: {}\n"
        "§7可用: §f表单§7(始终可用){} {}\n"
        "§7选择后你自己的菜单都用这个样式显示; 选『跟随菜单』则由菜单自己的配置决定。",
        uiStyleName(current),
        bridge.isAvailable() ? " | §fNPC 对话§7(可用)" : " | §8NPC 对话(缺 HologramLib)",
        bridge.isChestAvailable() ? " | §f箱子§7(可用)" : " | §8箱子(缺 HologramLib)"
    ));

    auto addOption = [&](char const* mode, std::string const& label, std::string const& desc, bool enabled) {
        std::string text = fmt::format(
            "{}{}\n§7{}",
            current == mode ? "§a " : "§l",
            label,
            enabled ? desc : "§c需要服务器安装 HologramLib"
        );
        fm.appendButton(text, [mode](Player& pl) {
            if (PlayerUiPref::getInstance().set(pl, mode)) {
                pl.sendMessage("§a[MeowMenu] 界面样式已设为: " + uiStyleName(mode));
            } else {
                pl.sendMessage("§c[MeowMenu] 设置失败");
            }
        });
    };

    addOption(ui_mode::kAuto, "跟随菜单", "菜单自己配的模式优先", true);
    addOption(ui_mode::kForm, "普通表单", "LeviLamina 表单, 无前置要求", true);
    addOption(ui_mode::kNpc, "NPC 对话", "原版 NPC 对话框形态, 最多 6 个按钮", bridge.isAvailable());
    addOption(ui_mode::kChest, "箱子界面", "虚拟箱子, 格子即按钮, 最多 54 格", bridge.isChestAvailable());

    fm.appendButton("§l§c返回", [](Player& pl) {
        MenuRenderer::getInstance().renderMenu(pl, ConfigManager::getInstance().getConfig().mainMenuId);
    });
    fm.sendTo(player);
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

    //=== /mnpc - HologramLib NPC 对话菜单（可选前置）===
    registerNpcCommands();

    logger.info("命令已注册: /mmenu [admin|reload|mode] | /mmgv | /clearmmenu | /mnpc [status|open|test|...]");
}

} // namespace meowmenu
