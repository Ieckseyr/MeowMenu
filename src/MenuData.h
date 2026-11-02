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

// NPC 对话模式配置（可选前置 HologramLib; 见 docs/NPC_MENU.md）
//
// 开启后, 这个菜单**被任何入口打开时都直接以原版 NPC 对话界面呈现**
// （/mmenu、子菜单按钮、触发物品、RemoteCall renderMenu 全部一致）,
// 不需要额外的按钮类型, 也不会生成任何 NPC 实体。
//
// 原版 NPC 对话框最多 6 个按钮槽位: 本层自己的可点击按钮最多 6 个;
// 从上一层进入时末尾会追加「返回上一级」, 此时本层最多 5 个。
struct NpcSettings {
    bool        enabled = false;  // 该菜单以 NPC 对话模式打开
    std::string name;             // 对话框里显示的 NPC 名（空 = 菜单标题）
    std::string dialogue;         // 话语/正文（空 = 菜单 content）
};

// 按钮数据
struct Button {
    std::string text;      // 显示文本（支持 § 颜色码）
    std::string icon;      // 图标纹理路径（如 textures/ui/icon_setting）— 文本填入
    std::string iconItem;  // 图标物品 ID（如 minecraft:dirt）— 选择背包物品作为图标
                           // 渲染优先级：icon 非空时使用 icon，否则使用 iconItem，两者都空则无图标
    std::string type;      // command | admin_command | submenu | admin_submenu | folder
                           // | npc_menu | npc_spawn（NPC 菜单按钮, 需 HologramLib 可选前置）
                           // | header | label | divider | spacer（显示项, 不可点击）
    std::string value;     // 命令字符串 / 子菜单ID（npc_menu/npc_spawn 为目标菜单ID, 空 = 主菜单）
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
    std::string         renderMode = "default"; // 保留字段（"npc" 亦可作为 NPC 模式的简写）
    std::vector<Button> buttons;       // 按钮列表
    DogeUISettings      dogeui;        // DogeUI 配置（新增）
    NpcSettings         npc;           // NPC 对话模式配置（新增）

    // 该菜单自身配置的界面样式: "npc"（NPC 对话）/ "chest"（箱子）/ "form"（普通表单）
    // 玩家自选样式可以覆盖它（见 PlayerUiPref / MenuRenderer::resolveUiMode）
    [[nodiscard]] std::string uiMode() const {
        if (npc.enabled || renderMode == "npc") return "npc";
        if (renderMode == "chest") return "chest";
        return "form";
    }

    // 是否以 NPC 对话模式打开（等价于 uiMode() == "npc"）
    [[nodiscard]] bool isNpcMode() const { return uiMode() == "npc"; }

    // 该菜单**允许**被哪些样式呈现（固有约束）。空 = 三种都允许。
    // 用途: 有些菜单离不开表单能力（编辑器、依赖 § 颜色码/图标/超 6 按钮的排版菜单），
    // 这类菜单把自己钉成 {"form"}, 玩家在 /mmenu ui 里选的其它样式对它们不生效。
    // 表单永远允许（它是任何情况下的兜底渲染）。
    std::vector<std::string> uiAllowed;

    // 该菜单是否含"打开菜单编辑器"的入口（按钮执行 /mmenu admin）
    //
    // 这类菜单被**固定为表单（JSON UI）样式**: 菜单编辑器本身只能由表单承载,
    // 入口菜单跟着变成箱子/NPC 会让管理入口难以辨认, 也容易在箱子里被误点。
    // 固定为硬规则, 不依赖作者手动配 uiAllow, 玩家在 /mmenu ui 里的选择对它不生效。
    [[nodiscard]] bool hasEditorEntry() const;

    [[nodiscard]] bool allowsUiMode(std::string const& mode) const {
        if (mode == "form") return true;
        if (uiAllowed.empty()) return true;
        for (auto const& allowed : uiAllowed) {
            if (allowed == mode) return true;
        }
        return false;
    }

    // 该菜单可点击按钮数量（header/label/divider/spacer 是显示项, 不计入）
    [[nodiscard]] int clickableButtonCount() const;

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
