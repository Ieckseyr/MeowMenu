# patch_editor_entry.py - 含菜单编辑器入口的菜单固定为表单样式（硬规则）
import io

# ── 1) MenuData.h: hasEditorEntry ──
p = 'src/MenuData.h'
s = io.open(p, encoding='utf-8').read()
OLD = '''    [[nodiscard]] bool allowsUiMode(std::string const& mode) const {'''
NEW = '''    // 该菜单是否含"打开菜单编辑器"的入口（按钮执行 /mmenu admin）
    //
    // 这类菜单被**固定为表单（JSON UI）样式**: 菜单编辑器本身只能由表单承载,
    // 入口菜单跟着变成箱子/NPC 会让管理入口难以辨认, 也容易在箱子里被误点。
    // 固定为硬规则, 不依赖作者手动配 uiAllow, 玩家在 /mmenu ui 里的选择对它不生效。
    [[nodiscard]] bool hasEditorEntry() const;

    [[nodiscard]] bool allowsUiMode(std::string const& mode) const {'''
assert OLD in s
s = s.replace(OLD, NEW, 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('MenuData.h ok')

# ── 2) MenuData.cpp: 实现（识别 /mmenu admin, 兼容配置的别名） ──
p = 'src/MenuData.cpp'
s = io.open(p, encoding='utf-8').read()
s = s.replace('''#include "MenuData.h"
#include "IconValidator.h"''', '''#include "MenuData.h"
#include "Config.h"
#include "IconValidator.h"''', 1)
OLD = '''int Menu::clickableButtonCount() const {'''
NEW = '''// 判断按钮值是否是"打开菜单编辑器"的命令: <命令名> admin [...]
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
    size_t      e    = rest.find_first_of(" \\t");
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

int Menu::clickableButtonCount() const {'''
assert OLD in s
s = s.replace(OLD, NEW, 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('MenuData.cpp ok')

# ── 3) MenuRenderer::resolveUiMode: 硬规则优先 ──
p = 'src/MenuRenderer.cpp'
s = io.open(p, encoding='utf-8').read()
OLD = '''std::string MenuRenderer::resolveUiMode(Player const& player, Menu const& menu) {
    // 玩家自选样式优先（auto = 跟随菜单自身配置）, 但不得越过菜单的固有约束（uiAllow）'''
NEW = '''std::string MenuRenderer::resolveUiMode(Player const& player, Menu const& menu) {
    // 硬规则: 含菜单编辑器入口的菜单固定为表单（JSON UI）—— 编辑器界面只能由表单承载,
    // 玩家样式与菜单自身配置对它都不生效。
    if (menu.hasEditorEntry()) {
        static std::atomic<bool> logged{false};
        if (!logged.exchange(true)) {
            lib().getSelf().getLogger().info(
                "菜单 {} 含菜单编辑器入口 → 固定为表单样式（不参与样式选择）",
                menu.id
            );
        }
        return ui_mode::kForm;
    }

    // 玩家自选样式优先（auto = 跟随菜单自身配置）, 但不得越过菜单的固有约束（uiAllow）'''
assert OLD in s
s = s.replace(OLD, NEW, 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('MenuRenderer ok')

# ── 4) 编辑器页面提示这条硬规则 ──
p = 'src/MenuEditor.cpp'
s = io.open(p, encoding='utf-8').read()
OLD = '''        "§7当前菜单配置: §f{}\\n"
        "§7说明: 这里设置的是**菜单自身**的样式; 玩家可以用 §a/mmenu ui §7选择自己喜欢的样式覆盖它。\\n"
        "§7可点击按钮: §f{}",
        menu->renderMode,
        menu->clickableButtonCount()'''
NEW = '''        "§7当前菜单配置: §f{}\\n"
        "§7说明: 这里设置的是**菜单自身**的样式; 玩家可以用 §a/mmenu ui §7选择自己喜欢的样式覆盖它。\\n"
        "§7可点击按钮: §f{}{}",
        menu->renderMode,
        menu->clickableButtonCount(),
        menu->hasEditorEntry()
            ? "\\n§e本菜单含菜单编辑器入口 → 已固定为表单样式（硬规则, 这里的设置对它不生效）"
            : ""'''
assert OLD in s
s = s.replace(OLD, NEW, 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('MenuEditor ok')
