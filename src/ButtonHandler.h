// ButtonHandler.h - 按钮点击处理
//
// 处理菜单按钮点击事件，支持以下按钮类型：
// - command: 以玩家身份执行命令（支持 {player} 变量替换）
// - admin_command: OP 权限检查后执行命令
// - submenu: 打开子菜单
// - admin_submenu: OP 权限检查后打开子菜单
// - folder: 自动创建子菜单并打开
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

private:
    // 执行命令（以玩家身份，支持 {player} 变量替换）
    static void executeCommand(Player& player, std::string const& command);

    // 执行管理员命令（OP 权限检查）
    static void executeAdminCommand(Player& player, std::string const& command);

    // 打开子菜单
    static void openSubMenu(Player& player, std::string const& subMenuId);

    // 检查玩家是否为 OP
    static bool isOp(Player& player);

    // 替换命令中的 {player} 变量
    static std::string resolveCommand(Player& player, std::string const& command);
};

} // namespace meowmenu
