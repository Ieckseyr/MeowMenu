// MenuRenderer.h - 菜单渲染器
//
// 使用 LeviLamina SimpleForm API 渲染菜单表单，应用 DogeUI 标识符前缀。
//
// DogeUI 逻辑：
// - 每个菜单可独立配置 dogeui.enabled 和 dogeui.prefix
// - 启用时，将 prefix 前置到菜单标题
// - 菜单未配置 dogeui 字段时，回退到全局 config.useDogeUI（仅主菜单生效）
#pragma once

#include <string>

// 前向声明 BDS Player 类（位于全局命名空间）
class Player;

namespace meowmenu {

class MenuRenderer {
public:
    static MenuRenderer& getInstance();

    // 渲染菜单并发送给玩家
    // menuId: 菜单ID
    void renderMenu(Player& player, std::string const& menuId);

private:
    MenuRenderer() = default;

    // 判断是否应用 DogeUI 前缀
    // 优先级: 菜单 dogeui.enabled > 全局 config.useDogeUI（仅主菜单）
    bool shouldApplyDogeUI(std::string const& menuId) const;

    // 获取 DogeUI 前缀
    std::string getDogeUIPrefix(std::string const& menuId) const;
};

} // namespace meowmenu
