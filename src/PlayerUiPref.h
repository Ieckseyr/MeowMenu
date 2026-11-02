// PlayerUiPref.h - 玩家自选的界面样式偏好
//
// 玩家可以用 /mmenu ui 选择自己喜欢的菜单呈现方式（表单 / NPC 对话 / 箱子），
// 与「菜单自身配置的模式」的关系:
//   玩家选了具体样式 → 优先按玩家样式渲染（前提是该样式当前可用）
//   玩家选「跟随菜单」(auto, 默认) → 按菜单自己配置的模式渲染
//
// 持久化: Meowdata/MeowMenu/player_ui.json = {"<xuid>": "auto|form|npc|chest"}
// 与 item_triggers.json 一样用平铺对象, 便于手改与外部脚本维护。
#pragma once

#include <shared_mutex>
#include <string>
#include <unordered_map>

// 前向声明 BDS Player 类（位于全局命名空间）
class Player;

namespace meowmenu {

// 样式取值（与 JSON 里的字符串一致）
namespace ui_mode {
constexpr char const* kAuto  = "auto";   // 跟随菜单配置（默认）
constexpr char const* kForm  = "form";   // 普通表单
constexpr char const* kNpc   = "npc";    // NPC 对话（需 HologramLib）
constexpr char const* kChest = "chest";  // 箱子界面（需 HologramLib）
} // namespace ui_mode

class PlayerUiPref {
public:
    static PlayerUiPref& getInstance();

    // 从磁盘加载（文件不存在则创建空表）
    void initialize();

    // 查询玩家偏好（未设置返回 "auto"）
    std::string get(Player const& player) const;
    std::string getByKey(std::string const& key) const;

    // 设置并持久化（mode 非法返回 false）
    bool set(Player const& player, std::string const& mode);
    bool setByKey(std::string const& key, std::string const& mode);

    // 全部偏好（JSON 字符串）
    std::string getAllJson() const;

    // 是否合法样式名
    static bool isValidMode(std::string const& mode);

private:
    PlayerUiPref() = default;

    // 玩家标识: 优先 xuid（跨改名稳定）, 取不到时退回 realName
    static std::string keyOf(Player const& player);

    void save();

    mutable std::shared_mutex                    mMutex;
    std::unordered_map<std::string, std::string> mPrefs;
};

} // namespace meowmenu
