// MenuData.cpp - 菜单与按钮数据结构序列化实现
#include "MenuData.h"
#include "Config.h"
#include "IconValidator.h"

namespace meowmenu {

//==== Button ====

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

    // 加载时校验图标: 物品 ID → 纹理路径, 非法值清空
    if (!b.icon.empty()) {
        std::string normalized = IconValidator::getInstance().normalizeIcon(b.icon);
        if (normalized != b.icon) {
            // 图标被修复或清空
            b.icon = normalized;
        }
    }

    return b;
}

//==== Menu ====

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

// ASCII 小写（物品 ID 本来就是小写; 这里只是让配置里手打的大写也能命中）
static std::string toLowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

bool Menu::matchesTriggerItem(std::string const& itemType) const {
    // 二级菜单不能被触发物品打开: 配置了也不生效
    if (isSecondary()) return false;
    if (!triggerItem.configured() || itemType.empty()) return false;
    return toLowerAscii(triggerItem.type) == toLowerAscii(itemType);
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
    // 26.40.8: 聊天框头像/载体自定义
    j["npc"]["avatarType"]    = npc.avatarType;
    j["npc"]["avatarVariant"] = npc.avatarVariant;

    // 菜单分级（一级 = 可被触发物品打开; 二级 = 只能从别的菜单进入）
    j["level"] = level;

    // 触发物品：填了类型写全套（二级菜单填了也保存, 但不生效 → 编辑器会提示）;
    // 一级菜单即使没配也写出空串 "" —— 让"触发物品写在哪"一眼可见（见 docs/API.md 菜单分级规则）
    if (triggerItem.configured()) {
        j["triggerItem"]["type"]        = triggerItem.type;
        j["triggerItem"]["displayName"] = triggerItem.displayName;
        j["triggerItem"]["snbt"]        = triggerItem.snbt;
        j["triggerItem"]["count"]       = triggerItem.count;
    } else if (isPrimary()) {
        j["triggerItem"] = "";
    }

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
        m.npc.avatarType    = j["npc"].value("avatarType", "");
        m.npc.avatarVariant = j["npc"].value("avatarVariant", -1);
    }
    // 手写 JSON 的简写: renderMode = "npc" 也视为启用
    if (m.renderMode == "npc") m.npc.enabled = true;

    // 菜单分级：缺省 = 一级菜单（新建菜单默认一级; 老数据没有该字段时也按一级处理）
    // 取值只认 1 / 2, 其它值 / 写错类型一律按一级, 避免写错数字把菜单变成"打不开"
    m.level = 1;
    if (j.contains("level")) {
        auto const& lv = j["level"];
        if (lv.is_number()) {
            m.level = lv.get<int>();
        } else if (lv.is_string()) {
            std::string text = lv.get<std::string>();
            m.level = (text == "2" || text == "二级" || text == "secondary") ? 2 : 1;
        }
        if (m.level != 2) m.level = 1;
    }

    // 触发物品支持两种写法: 对象 或 字符串简写（"minecraft:compass"）
    if (j.contains("triggerItem")) {
        auto const& ti = j["triggerItem"];
        if (ti.is_string()) {
            m.triggerItem.type = ti.get<std::string>();
        } else if (ti.is_object()) {
            auto readString = [&ti](char const* key, std::string& out) {
                if (ti.contains(key) && ti[key].is_string()) out = ti[key].get<std::string>();
            };
            readString("type", m.triggerItem.type);
            readString("displayName", m.triggerItem.displayName);
            readString("snbt", m.triggerItem.snbt);
            if (ti.contains("count") && ti["count"].is_number()) {
                m.triggerItem.count = ti["count"].get<int>();
            }
        }
        if (m.triggerItem.count < 1) m.triggerItem.count = 1;
    }

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
