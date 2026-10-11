// MenuEditor.h - 管理员工菜单编辑器（/mmenu admin; LeviLamina Form API）
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

// 菜单分级设置（一级菜单可被触发物品打开 / 二级菜单只能从其它菜单进入）
// 一级菜单（主菜单除外）在这里配置它自己的触发物品
void showMenuLevelSettings(Player& player, std::string const& menuId);

// 菜单分级: 触发物品设置表单（手输物品类型 ID，或从手持物品取）
void showTriggerItemForm(Player& player, std::string const& menuId);
void setTriggerItemFromHand(Player& player, std::string const& menuId);
void clearTriggerItem(Player& player, std::string const& menuId);

// 校验/补齐触发物品类型（空串 = 清除; 无命名空间按 minecraft: 补全; 钟留给主菜单）
bool validateTriggerItemType(
    Player&             player,
    std::string const&  menuId,
    std::string const&  input,
    std::string&        outType
);

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
