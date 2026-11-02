// MenuData.cpp - 菜单与按钮数据结构序列化实现
#include "MenuData.h"
#include "Config.h"
#include "IconValidator.h"

namespace meowmenu {

//================================================================
// Button
//================================================================

nlohmann::json Button::toJson() const {
    nlohmann::json j;
    j["text"]  = text;
    if (!icon.empty()) j["icon"] = icon;
    if (!iconItem.empty()) j["iconItem"] = iconItem;
    j["type"]  = type;
    j["value"] = value;
    if (!item.empty()) j["item"] = item;
    return j;
}

Button Button::fromJson(nlohmann::json const& j) {
    Button b;
    b.text     = j.value("text", "");
    b.icon     = j.value("icon", "");
    b.iconItem = j.value("iconItem", "");
    b.type     = j.value("type", "command");
    b.value    = j.value("value", "");
    b.item     = j.value("item", "");

    // 加载时校验并修复非法图标路径
    // - 若 icon 是物品 ID（如 minecraft:dirt），转换为对应的纹理路径
    // - 若 icon 是非法值（既非纹理路径也非物品 ID），清空
    // - iconItem 保持原样，渲染时再解析
    if (!b.icon.empty()) {
        std::string normalized = IconValidator::getInstance().normalizeIcon(b.icon);
        if (normalized != b.icon) {
            // 图标被修复或清空
            b.icon = normalized;
        }
    }

    return b;
}

//================================================================
// Menu
//================================================================

// 判断按钮值是否是"打开菜单编辑器"的命令: <命令名> admin [...]
// 命令名取 /mmenu 及其配置别名, 避免别名用户漏判
static bool isEditorCommand(std::string const& value) {
    std::string text = value;
    size_t      b    = text.find_first_not_of(" /");
    if (b == std::string::npos) return false;
    text = text.substr(b);

    std::string cmdName = "mmenu";
    try {
        auto const& alias = ConfigManager::getInstance().getConfig().giveUs.commandAlias;
        if (!alias.empty() && text.rfind(alias + " ", 0) == 0) cmdName = alias;
    } catch (...) {}

    if (text.rfind(cmdName + " ", 0) != 0) return false;
    std::string rest = text.substr(cmdName.size() + 1);
    size_t      e    = rest.find_first_of(" \t");
    if (e != std::string::npos) rest = rest.substr(0, e);
    return rest == "admin";
}

bool Menu::hasEditorEntry() const {
    for (auto const& button : buttons) {
        if (button.type != "command" && button.type != "admin_command") continue;
        if (isEditorCommand(button.value)) return true;
    }
    return false;
}

int Menu::clickableButtonCount() const {
    int n = 0;
    for (auto const& b : buttons) {
        if (b.type == "header" || b.type == "label" || b.type == "divider" || b.type == "spacer") continue;
        ++n;
    }
    return n;
}

nlohmann::json Menu::toJson() const {
    nlohmann::json j;
    j["id"]      = id;
    j["title"]   = title;
    j["content"] = content;
    j["renderMode"] = renderMode;

    // 按钮
    nlohmann::json btns = nlohmann::json::array();
    for (auto const& b : buttons) {
        btns.push_back(b.toJson());
    }
    j["buttons"] = btns;

    // DogeUI 配置（每菜单独立）
    j["dogeui"]["enabled"] = dogeui.enabled;
    j["dogeui"]["prefix"]  = dogeui.prefix;

    // NPC 对话模式配置
    j["npc"]["enabled"]  = npc.enabled;
    j["npc"]["name"]     = npc.name;
    j["npc"]["dialogue"] = npc.dialogue;

    // 允许的界面样式（空数组 = 不写该字段 = 三种都允许）
    if (!uiAllowed.empty()) {
        j["uiAllow"] = nlohmann::json::array();
        for (auto const& mode : uiAllowed) {
            j["uiAllow"].push_back(mode);
        }
    }

    // 保留字段（向后兼容）
    if (!chestUI.is_null())  j["chestUI"]  = chestUI;
    if (!infoBar.is_null())  j["infoBar"]  = infoBar;
    if (!carousel.is_null()) j["carousel"] = carousel;
    if (!video.is_null())    j["video"]    = video;

    return j;
}

Menu Menu::fromJson(nlohmann::json const& j, std::string const& fallbackId) {
    Menu m;
    m.id      = j.value("id", fallbackId);
    m.title   = j.value("title", "");
    m.content = j.value("content", "");
    m.renderMode = j.value("renderMode", "default");

    // 按钮
    if (j.contains("buttons") && j["buttons"].is_array()) {
        for (auto const& bj : j["buttons"]) {
            m.buttons.push_back(Button::fromJson(bj));
        }
    }

    // DogeUI 配置（新增字段，向后兼容：不存在时使用默认值）
    if (j.contains("dogeui") && j["dogeui"].is_object()) {
        m.dogeui.enabled = j["dogeui"].value("enabled", false);
        m.dogeui.prefix  = j["dogeui"].value("prefix", "/D ");
    }

    // NPC 对话模式配置（新增字段，向后兼容）
    if (j.contains("npc") && j["npc"].is_object()) {
        m.npc.enabled  = j["npc"].value("enabled", false);
        m.npc.name     = j["npc"].value("name", "");
        m.npc.dialogue = j["npc"].value("dialogue", "");
    }
    // 手写 JSON 的简写: renderMode = "npc" 也视为启用
    if (m.renderMode == "npc") m.npc.enabled = true;

    // 允许的界面样式（可选; 不写 = 三种都允许）
    if (j.contains("uiAllow") && j["uiAllow"].is_array()) {
        for (auto const& item : j["uiAllow"]) {
            if (item.is_string()) m.uiAllowed.push_back(item.get<std::string>());
        }
    }

    // 保留字段（向后兼容，解析但不使用）
    if (j.contains("chestUI"))  m.chestUI  = j["chestUI"];
    if (j.contains("infoBar"))  m.infoBar  = j["infoBar"];
    if (j.contains("carousel")) m.carousel = j["carousel"];
    if (j.contains("video"))    m.video    = j["video"];

    return m;
}

} // namespace meowmenu
