// MenuRenderer.h - 菜单渲染器（SimpleForm; DogeUI 前缀, 无 dogeui 字段时回退 config.useDogeUI）
#pragma once

#include <string>

// 前向声明 BDS Player 类（位于全局命名空间）
class Player;

namespace meowmenu {
struct Menu;
}

namespace meowmenu {

class MenuRenderer {
public:
    static MenuRenderer& getInstance();

    // 渲染菜单并发送给玩家（按玩家样式 / 菜单模式自动分派: 表单 / NPC 对话 / 箱子）
    // menuId: 菜单ID
    void renderMenu(Player& player, std::string const& menuId);

    // 延迟一个 tick 再渲染。点按钮 / 点击回包的回调内必须用这个:
    // HologramLib 在回调返回后才会拆掉旧界面与载体, 同步重开会把刚开的界面一起删掉。
    void renderMenuDeferred(Player& player, std::string const& menuId);

    // 解析玩家当前应使用的界面样式: "form" / "npc" / "chest"
    // 玩家自选样式（/mmenu ui）优先; 未设置时用菜单自身配置的模式
    static std::string resolveUiMode(Player const& player, Menu const& menu);

private:
    MenuRenderer() = default;

    // 判断是否应用 DogeUI 前缀
    // 优先级: 菜单 dogeui.enabled > 全局 config.useDogeUI（仅主菜单）
    bool shouldApplyDogeUI(std::string const& menuId) const;

    // 获取 DogeUI 前缀
    std::string getDogeUIPrefix(std::string const& menuId) const;
};

} // namespace meowmenu
