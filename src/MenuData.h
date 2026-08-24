// MenuData.h - 菜单与按钮数据结构
//
// 向后兼容现有 JS 版 MeowMenu 的数据格式（Meowdata/MeowMenu/data/*.json）。
// 新增 dogeui 字段：每个菜单可独立配置 DogeUI 标识符（自定义前缀），可独立启用/禁用。
// renderMode / chestUI / infoBar / carousel / video 字段保留解析但不再使用
// （已暂时放弃动态菜单和箱子菜单模式）。
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace meowmenu {

// DogeUI 每菜单配置
struct DogeUISettings {
    bool        enabled = false;       // 是否为此菜单启用 DogeUI
    std::string prefix  = "/D ";      // DogeUI 标识符前缀（自定义）
};

// 按钮数据
struct Button {
    std::string text;      // 显示文本（支持 § 颜色码）
    std::string icon;      // 图标纹理路径（如 textures/ui/icon_setting）— 文本填入
    std::string iconItem;  // 图标物品 ID（如 minecraft:dirt）— 选择背包物品作为图标
                           // 渲染优先级：icon 非空时使用 icon，否则使用 iconItem，两者都空则无图标
    std::string type;      // command | admin_command | submenu | admin_submenu | folder | spacer
    std::string value;     // 命令字符串 或 子菜单ID
    std::string item;      // 箱子UI物品（仅箱子模式使用，本版本忽略）

    // 序列化
    nlohmann::json toJson() const;
    static Button  fromJson(nlohmann::json const& j);
};

// 菜单数据
struct Menu {
    std::string         id;            // 菜单唯一标识
    std::string         title;         // 标题（支持 § 颜色码）
    std::string         content;       // 内容文本
    std::string         renderMode = "default"; // 保留字段，本版本忽略
    std::vector<Button> buttons;       // 按钮列表
    DogeUISettings      dogeui;        // DogeUI 配置（新增）

    // 以下字段保留解析以实现向后兼容，但本版本不使用
    nlohmann::json chestUI;
    nlohmann::json infoBar;
    nlohmann::json carousel;
    nlohmann::json video;

    // 序列化
    nlohmann::json toJson() const;
    static Menu    fromJson(nlohmann::json const& j, std::string const& fallbackId = "");
};

} // namespace meowmenu
