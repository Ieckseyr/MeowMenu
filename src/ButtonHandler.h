// ButtonHandler.h - 按钮点击处理（command / admin_command / submenu / admin_submenu /
// folder / npc_menu / npc_spawn / header / label / divider / spacer）
#pragma once

#include "MenuData.h"

#include <string>

// 前向声明 BDS Player 类（位于全局命名空间）
class Player;

namespace meowmenu {

class ButtonHandler {
public:
    // 处理按钮点击
    static void handleButton(Player& player, Button const& button);

    // 按钮动作执行原语（表单路径走 handleButton; NPC 对话路径需单独判断子菜单类型）
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
