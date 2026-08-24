// MenuEditor.h - 管理员工菜单编辑器
//
// 使用 LeviLamina Form API（自动处理 formId 和回调）。
// 仅管理员可访问，通过 /mmenu admin 打开。
//
// 功能：
// - 菜单列表（查看/编辑/删除）
// - 编辑菜单标题、内容
// - 添加/编辑/删除按钮
// - DogeUI 设置（每菜单独立的启用开关和自定义前缀）
// - 触发物品设置
#pragma once

#include <mc/world/actor/player/Player.h>

#include <string>

namespace meowmenu {

namespace MenuEditor {

// 编辑器主界面
void showEditorMain(Player& player);

// 菜单列表
void showMenuList(Player& player);

// 编辑指定菜单
void showEditMenu(Player& player, std::string const& menuId);

// 编辑标题和内容
void showEditTitleContent(Player& player, std::string const& menuId);

// 添加按钮
void showAddButton(Player& player, std::string const& menuId);

// 编辑按钮
void showEditButton(Player& player, std::string const& menuId, int buttonIndex);

// 删除按钮选择器（列出按钮供选择删除）
void showDeleteButtonSelector(Player& player, std::string const& menuId);

// 删除菜单二次确认
void showDeleteMenuConfirm(Player& player, std::string const& menuId);

// 从手持物品设置按钮图标（选择按钮列表）
void showIconFromHandSelector(Player& player, std::string const& menuId);

// 从手持物品设置指定按钮的图标
void setButtonIconFromHand(Player& player, std::string const& menuId, int buttonIndex);

// DogeUI 设置
void showDogeUISettings(Player& player, std::string const& menuId);

// 物品设置
void showItemSettings(Player& player);

// 从手持物品设置
void setItemFromHand(Player& player);

} // namespace MenuEditor

} // namespace meowmenu
