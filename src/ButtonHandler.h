// ButtonHandler.h - 按钮点击处理
//
// 处理菜单按钮点击事件，支持以下按钮类型：
// - command: 以玩家身份执行命令（支持 {player} 变量替换）
// - admin_command: OP 权限检查后执行命令
// - submenu: 打开子菜单（表单）
// - admin_submenu: OP 权限检查后打开子菜单
// - folder: 自动创建子菜单并打开
// - npc_menu: 以 HologramLib 的 NPC 对话形式打开菜单（可选前置, 见 NPC_MENU.md）
// - npc_spawn: 在身前生成一只"点击即打开菜单"的 NPC（可选前置）
// - spacer: 空格（仅箱子UI，本版本跳过）
#pragma once

#include "MenuData.h"

#include <string>

// 前向声明 BDS Player 类（位于全局命名空间）
class Player;

namespace meowmenu {

class ButtonHandler {
public:
    // 处理按钮点击
    // player: 点击按钮的玩家
    // button: 按钮数据
    static void handleButton(Player& player, Button const& button);

    // 以下为按钮动作的执行原语。表单路径走 handleButton；
    // NPC 对话菜单（NpcMenuBridge）需要在执行前判断子菜单类型以推进对话层级，故单独暴露。
    // 执行命令（以玩家身份，支持 {player} 与 PAPI 变量替换）
    static void executeCommand(Player& player, std::string const& command);

    // 执行管理员命令（OP 权限检查）
    static void executeAdminCommand(Player& player, std::string const& command);

    // 检查玩家是否为 OP
    static bool isOp(Player& player);

    // 替换命令中的 {player} 变量
    static std::string resolveCommand(Player& player, std::string const& command);

    // NPC 菜单按钮（需要 HologramLib 可选前置; 未安装时给出明确提示而不是静默无反应）
    // 以 NPC 对话形式打开菜单（menuId 空 = 主菜单）
    static void openNpcMenu(Player& player, std::string const& menuId);
    // 在玩家身前生成一只绑定菜单的 NPC（menuId 空 = 主菜单; npcName 空 = 默认名）
    static void spawnNpcMenu(Player& player, std::string const& menuId, std::string const& npcName);

private:
    // 打开子菜单表单（仅表单路径使用; NPC 对话路径在桥接内自行推进层级）
    static void openSubMenu(Player& player, std::string const& subMenuId);
};

} // namespace meowmenu
