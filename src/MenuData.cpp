// MenuData.cpp - 菜单与按钮数据结构序列化实现
#include "MenuData.h"

namespace meowmenu {

//================================================================
// Button
//================================================================

nlohmann::json Button::toJson() const {
    nlohmann::json j;
    j["text"]  = text;
    if (!icon.empty()) j["icon"] = icon;
    j["type"]  = type;
    j["value"] = value;
    if (!item.empty()) j["item"] = item;
    return j;
}

Button Button::fromJson(nlohmann::json const& j) {
    Button b;
    b.text  = j.value("text", "");
    b.icon  = j.value("icon", "");
    b.type  = j.value("type", "command");
    b.value = j.value("value", "");
    b.item  = j.value("item", "");
    return b;
}

//================================================================
// Menu
//================================================================

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

    // 保留字段（向后兼容，解析但不使用）
    if (j.contains("chestUI"))  m.chestUI  = j["chestUI"];
    if (j.contains("infoBar"))  m.infoBar  = j["infoBar"];
    if (j.contains("carousel")) m.carousel = j["carousel"];
    if (j.contains("video"))    m.video    = j["video"];

    return m;
}

} // namespace meowmenu
