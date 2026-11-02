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

// 添加菜单项（类型选择: 普通按钮 / NPC 菜单按钮 / header 大字体标题行 / label 文本行 / divider 分隔线）
void showAddButton(Player& player, std::string const& menuId);

// 添加普通按钮（命令/子菜单表单）
void showAddButtonForm(Player& player, std::string const& menuId);

// 添加 NPC 菜单按钮：先选类型（npc_menu = 以 NPC 对话呈现 / npc_spawn = 生成菜单 NPC）
void showAddNpcButtonType(Player& player, std::string const& menuId);

// 添加 NPC 菜单按钮（表单; npcType = "npc_menu" | "npc_spawn"）
void showAddNpcButtonForm(Player& player, std::string const& menuId, std::string const& npcType);

// 添加显示项（header / label / divider）
void showAddDisplayItemForm(Player& player, std::string const& menuId, std::string const& itemType);

// 编辑按钮
void showEditButton(Player& player, std::string const& menuId, int buttonIndex);

// 按钮操作页（编辑 / 上移 / 下移 / 手持设图标 / 删除）
void showButtonActions(Player& player, std::string const& menuId, int buttonIndex);

// 新建菜单（编辑器主界面入口；ID 校验 + 直接进入编辑页）
void showCreateMenuForm(Player& player);

// 图标选择器：分类浏览 → 分页列表（候选按真实纹理显示）/ 关键字搜索
void showIconPickerCategories(Player& player, std::string const& menuId, int buttonIndex);
void showIconPicker(
    Player&            player,
    std::string const& menuId,
    int                buttonIndex,
    int                page,
    std::string const& keyword
);

// 按钮移动到指定位置（滑块选目标序号）
void showMoveButtonTo(Player& player, std::string const& menuId, int buttonIndex);

// 显示模式设置（菜单自身样式: 表单 / NPC 对话 / 箱子）
void showUiModeSettings(Player& player, std::string const& menuId);

// 切换"允许的样式"（固有约束 uiAllow; 表单恒允许）
void toggleUiAllowed(Player& player, std::string const& menuId, std::string const& mode);

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

// NPC 对话模式设置（开关 / NPC 名 / 话语正文; 需要 HologramLib 可选前置）
void showNpcSettings(Player& player, std::string const& menuId);

// 物品设置
void showItemSettings(Player& player);

// 从手持物品设置
void setItemFromHand(Player& player);

} // namespace MenuEditor

} // namespace meowmenu
