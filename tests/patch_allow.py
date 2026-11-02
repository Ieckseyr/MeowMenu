# patch_editor_allow.py - 编辑器: 显示模式页支持"固有约束"(uiAllow)
import io

p = 'src/MenuEditor.cpp'
s = io.open(p, encoding='utf-8').read()

old = '''    for (auto const& option : options) {
        bool isCurrent = menu->renderMode == option.mode
                         || (option.mode[0] == 'd' && menu->renderMode != "npc" && menu->renderMode != "chest");
        std::string text = fmt::format(
            "{}{}\\n§7{}",
            isCurrent ? "§a✔ " : "§l",
            option.label,
            option.available ? option.desc : "§c需要服务器安装 HologramLib"
        );
        std::string mode = option.mode;
        fm.appendButton(text, [menuId, mode](Player& pl) {
            auto m = getMenuForEdit(menuId);
            if (!m) return;
            m->renderMode = mode;
            saveMenuAndNotify(pl, *m);
            showUiModeSettings(pl, menuId);
        });
    }

    fm.appendButton("§l§c返回菜单编辑", [menuId](Player& pl) { showEditMenu(pl, menuId); });
    fm.sendTo(player);
}'''

new = '''    for (auto const& option : options) {
        bool isCurrent = menu->renderMode == option.mode
                         || (option.mode[0] == 'd' && menu->renderMode != "npc" && menu->renderMode != "chest");
        std::string text = fmt::format(
            "{}{}\\n§7{}",
            isCurrent ? "§a✔ " : "§l",
            option.label,
            option.available ? option.desc : "§c需要服务器安装 HologramLib"
        );
        std::string mode = option.mode;
        fm.appendButton(text, [menuId, mode](Player& pl) {
            auto m = getMenuForEdit(menuId);
            if (!m) return;
            if (!m->allowsUiMode(mode)) {
                pl.sendMessage("§c[MeowMenu] 该菜单被锁定为只能使用: §f" + joinModes(m->uiAllowed));
                return;
            }
            m->renderMode = mode;
            saveMenuAndNotify(pl, *m);
            showUiModeSettings(pl, menuId);
        });
    }

    // 固有约束: 允许哪些样式。表单永远允许（兜底渲染）, 所以只能开/关 npc 与 chest。
    // 典型用途: 编辑器这类界面只能走 JSON UI（表单）, 或者依赖 § 颜色码/图标/超 6 按钮的排版菜单。
    fm.appendButton(
        fmt::format("§l§e固有约束\\n§7允许的样式: §f{}", joinModes(menu->uiAllowed)),
        [menuId](Player& pl) {
            auto m = getMenuForEdit(menuId);
            if (!m) return;
            auto allows = [&](char const* mode) { return m->allowsUiMode(mode); };
            auto fm2    = SimpleForm{};
            fm2.setTitle("§l§e固有约束: " + menuId);
            fm2.setContent(
                "§7这里决定该菜单**允许**被哪些样式呈现。\\n"
                "§7玩家在 §a/mmenu ui §7里选的样式若不在允许范围内, 对这个菜单不生效。\\n"
                "§7表单是任何情况下的兜底, 不能被关闭。"
            );
            fm2.appendButton(
                fmt::format("{}NPC 对话§7(最多 6 按钮 / 无 § 颜色码)", allows("npc") ? "§a✔ " : "§c✘ "),
                [menuId](Player& p2) { toggleUiAllowed(p2, menuId, "npc"); }
            );
            fm2.appendButton(
                fmt::format("{}箱子界面§7(格子即按钮 / 支持染色)", allows("chest") ? "§a✔ " : "§c✘ "),
                [menuId](Player& p2) { toggleUiAllowed(p2, menuId, "chest"); }
            );
            fm2.appendButton("§l§c返回显示模式", [menuId](Player& p2) { showUiModeSettings(p2, menuId); });
            fm2.sendTo(pl);
        }
    );

    fm.appendButton("§l§c返回菜单编辑", [menuId](Player& pl) { showEditMenu(pl, menuId); });
    fm.sendTo(player);
}'''

assert old in s
s = s.replace(old, new, 1)

# 工具函数 + 白名单切换（插在 showUiModeSettings 之前）
ANCHOR = '''//================================================================
// 显示模式（菜单自身配置的样式; 玩家可用 /mmenu ui 覆盖）
//================================================================'''
TOOLS = r'''// 允许样式集合的可读文本（空 = 三种都允许）
static std::string joinModes(std::vector<std::string> const& allowed) {
    if (allowed.empty()) return "全部（表单 / NPC 对话 / 箱子）";
    std::string out = "表单";
    for (auto const& mode : allowed) {
        if (mode == "npc") out += " / NPC 对话";
        else if (mode == "chest") out += " / 箱子";
    }
    return out;
}

// 切换某个样式是否允许（表单恒允许; 三种都允许时清空数组 = 不写该字段）
void toggleUiAllowed(Player& player, std::string const& menuId, std::string const& mode) {
    auto menu = getMenuForEdit(menuId);
    if (!menu) return;

    bool npcAllowed   = menu->allowsUiMode("npc");
    bool chestAllowed = menu->allowsUiMode("chest");
    if (mode == "npc") npcAllowed = !npcAllowed;
    if (mode == "chest") chestAllowed = !chestAllowed;

    std::vector<std::string> allowed{"form"};
    if (npcAllowed) allowed.push_back("npc");
    if (chestAllowed) allowed.push_back("chest");

    menu->uiAllowed = (npcAllowed && chestAllowed) ? std::vector<std::string>{} : allowed;

    // 当前模式若已不被允许 → 退回表单
    if (!menu->allowsUiMode(menu->renderMode)) menu->renderMode = "default";

    saveMenuAndNotify(player, *menu);
    player.sendMessage("§a[MeowMenu] 允许的样式已更新: §f" + joinModes(menu->uiAllowed));
}

''' + ANCHOR
assert ANCHOR in s
s = s.replace(ANCHOR, TOOLS, 1)

# 声明
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('MenuEditor.cpp ok')

p = 'src/MenuEditor.h'
s = io.open(p, encoding='utf-8').read()
s = s.replace('''// 显示模式设置（菜单自身样式: 表单 / NPC 对话 / 箱子）
void showUiModeSettings(Player& player, std::string const& menuId);''',
'''// 显示模式设置（菜单自身样式: 表单 / NPC 对话 / 箱子）
void showUiModeSettings(Player& player, std::string const& menuId);

// 切换"允许的样式"（固有约束 uiAllow; 表单恒允许）
void toggleUiAllowed(Player& player, std::string const& menuId, std::string const& mode);''', 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('MenuEditor.h ok')
