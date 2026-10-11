// MenuData.h - 菜单与按钮数据结构（向后兼容 JS 版 data/*.json）
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

// NPC 对话模式（开启后该菜单被任何入口打开都以原版 NPC 对话呈现; 最多 6 个按钮槽位）
struct NpcSettings {
    bool        enabled = false;  // 该菜单以 NPC 对话模式打开
    std::string name;             // 对话框里显示的 NPC 名（空 = 菜单标题）
    std::string dialogue;         // 话语/正文（空 = 菜单 content）
    // 聊天框头像/载体自定义: avatarType 换载体实体类型, avatarVariant 换 NPC 内置皮肤变体（avatarSkin 已移除）
    std::string avatarType;        // 空 = 默认 minecraft:npc; 否则载体实体类型（minecraft:zombie / minecraft:chicken…）
    int         avatarVariant{-1}; // NPC 内置皮肤变体 0..59（-1 = 默认, 不加 SkinId 项）
};

// 按钮数据
struct Button {
    std::string text;      // 显示文本（支持 § 颜色码）
    std::string icon;      // 图标纹理路径（如 textures/ui/icon_setting）— 文本填入
    std::string iconItem;  // 图标物品 ID（如 minecraft:dirt）— 选择背包物品作为图标
                           // 渲染优先级：icon 非空时使用 icon，否则使用 iconItem，两者都空则无图标
    std::string type;      // command | admin_command | submenu | admin_submenu | folder
    std::string value;     // 命令字符串 / 子菜单ID（npc_menu/npc_spawn 为目标菜单ID, 空 = 主菜单）
    std::string item;      // 箱子UI物品（仅箱子模式使用，本版本忽略）

    // 序列化
    nlohmann::json toJson() const;
    static Button  fromJson(nlohmann::json const& j);
};

// 菜单的触发物品（一级菜单专用, 按物品类型精确匹配, 不看 NBT; snbt/count 仅存档保留）
struct MenuTriggerItem {
    std::string type;         // 物品类型 ID（如 minecraft:compass）；空 = 未配置
    std::string displayName;  // 显示名（消息与编辑器展示用；空则显示 type）
    std::string snbt;         // 存档保留字段（不参与匹配）
    int         count = 1;    // 存档保留字段（不参与匹配）

    [[nodiscard]] bool configured() const { return !type.empty(); }

    // 展示用名字：displayName 优先，否则退回类型 ID
    [[nodiscard]] std::string label() const { return displayName.empty() ? type : displayName; }
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

    // 菜单分级: 1 = 一级（可被触发物品打开, 主菜单的触发物品固定是钟）/ 2 = 二级（只能从别的菜单进入）
    int             level = 1;
    MenuTriggerItem triggerItem;       // 触发物品（仅一级菜单生效）

    [[nodiscard]] bool isPrimary() const { return level <= 1; }
    [[nodiscard]] bool isSecondary() const { return !isPrimary(); }

    // 该菜单的触发物品当前是否真的生效：二级菜单恒不生效
    [[nodiscard]] bool triggerItemActive() const { return isPrimary() && triggerItem.configured(); }

    // 手持 itemType 能否触发打开本菜单（二级恒不触发; 一级需已配置且类型一致）
    [[nodiscard]] bool matchesTriggerItem(std::string const& itemType) const;

    // 该菜单自身的界面样式（玩家用 /mmenu ui 可覆盖; 见 PlayerUiPref）
    [[nodiscard]] std::string uiMode() const {
        if (npc.enabled || renderMode == "npc") return "npc";
        if (renderMode == "chest") return "chest";
        return "form";
    }

    // 是否以 NPC 对话模式打开（等价于 uiMode() == "npc"）
    [[nodiscard]] bool isNpcMode() const { return uiMode() == "npc"; }

    // 允许被哪些样式呈现（空 = 三种都允许; 钉成 {"form"} 的菜单不受玩家自选样式影响）
    std::vector<std::string> uiAllowed;

    // 含菜单编辑器入口的菜单固定为表单样式（硬规则）
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
