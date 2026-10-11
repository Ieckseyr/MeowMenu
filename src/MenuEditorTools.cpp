// MenuEditorTools.cpp - 编辑器增强页: 新建菜单 / 按钮移动位置 / 图标选择器（图标来自嵌入库, 必然合法）
#include "MenuEditor.h"

#include "Config.h"
#include "IconValidator.h"
#include "MenuData.h"
#include "MenuStore.h"
#include "MeowMenu.h"

#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>

#include <mc/world/actor/player/Player.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace meowmenu {
namespace MenuEditor {

using ll::form::CustomForm;
using ll::form::CustomFormElementResult;
using ll::form::CustomFormResult;
using ll::form::SimpleForm;

namespace {

constexpr size_t kIconsPerPage = 24;

// 下拉结果取下标（LL 里可能是 uint64 / double）
uint64_t formResultToIndex(CustomFormElementResult const& v) {
    if (auto p = std::get_if<unsigned long long>(&v)) return *p;
    if (auto p = std::get_if<double>(&v)) return static_cast<uint64_t>(*p);
    return 0;
}

// 菜单 ID 合法性（会作为文件名）：非空、无路径分隔与非法字符、不与现有菜单重名
bool isValidMenuId(std::string const& id, std::string& reason) {
    if (id.empty()) {
        reason = "菜单ID不能为空";
        return false;
    }
    if (id.size() > 48) {
        reason = "菜单ID过长（≤48）";
        return false;
    }
    char const* bad = "\\/:*?\"<>| ";
    for (char const c : id) {
        if (std::strchr(bad, c) != nullptr) {
            reason = std::string("菜单ID不能包含 ") + c;
            return false;
        }
    }
    if (MenuStore::getInstance().getMenu(id)) {
        reason = "同名菜单已存在";
        return false;
    }
    return true;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// 全部合法纹理路径（首次调用时构建一次；仅含 textures/ 开头的规范形式）
std::vector<std::string> const& allTexturePaths() {
    static std::vector<std::string> cache = IconValidator::getInstance().getAllTexturePaths();
    return cache;
}

std::vector<std::string> filterPaths(std::string const& keyword) {
    auto const& all = allTexturePaths();
    if (keyword.empty()) return all;
    std::string key = lower(keyword);
    std::vector<std::string> out;
    for (auto const& path : all) {
        if (lower(path).find(key) != std::string::npos) out.push_back(path);
    }
    return out;
}

// 显示名：去掉 textures/ 前缀，便于阅读
std::string shortName(std::string const& path) {
    if (path.rfind("textures/", 0) == 0) return path.substr(9);
    return path;
}

// 选中：写入按钮 icon（库内路径 → normalizeIcon 原样通过），并清掉 iconItem 以免歧义
void applyIcon(Player& player, std::string const& menuId, int buttonIndex, std::string const& path) {
    auto menu = MenuStore::getInstance().getMenu(menuId);
    if (!menu || buttonIndex < 0 || buttonIndex >= static_cast<int>(menu->buttons.size())) return;
    menu->buttons[buttonIndex].icon = IconValidator::getInstance().normalizeIcon(path);
    menu->buttons[buttonIndex].iconItem.clear();
    MenuStore::getInstance().saveMenu(*menu);
    player.sendMessage("§a[MeowMenu] 按钮 " + std::to_string(buttonIndex) + " 的图标已设为 §f" + path);
    showButtonActions(player, menuId, buttonIndex);
}

} // namespace

//==== 图标选择器 ====
void showIconPickerCategories(Player& player, std::string const& menuId, int buttonIndex) {
    auto fm = SimpleForm{};
    fm.setTitle("§l§b选择图标");
    fm.setContent("§7按分类浏览，或搜索关键字。候选按真实纹理显示，点哪个就是哪个。");

    auto cat = [&](std::string const& label, std::string const& desc, std::string const& keyword) {
        fm.appendButton(label + "\n§7" + desc, [menuId, buttonIndex, keyword](Player& pl) {
            showIconPicker(pl, menuId, buttonIndex, 0, keyword);
        });
    };
    cat("§f全部图标", fmt::format("共 {} 条", allTexturePaths().size()), "");
    cat("§b界面 ui/*", "按钮、图标、界面素材", "textures/ui/");
    cat("§a物品 items/*", "物品图标（对应 minecraft:xxx）", "textures/items/");
    cat("§6方块 blocks/*", "方块材质", "textures/blocks/");
    cat("§d实体 entity/*", "生物、模型贴图", "textures/entity/");
    cat("§e界面框架 gui/*", "原版 GUI 框架", "textures/gui/");
    cat("§7编辑器 editor/*", "编辑器类素材", "textures/editor/");

    fm.appendButton("§l§e搜索关键字", [menuId, buttonIndex](Player& pl) {
        auto search = CustomForm{"§l§e搜索图标"};
        search.appendInput("kw", "§e关键字§7(路径的一部分，如 gear / diamond / ui/)", "gear", "");
        search.sendTo(pl, [menuId, buttonIndex](Player& p2, CustomFormResult res, ll::form::FormCancelReason) {
            std::string kw;
            try {
                if (res.has_value()) kw = std::get<std::string>(res->at("kw"));
            } catch (...) {}
            showIconPicker(p2, menuId, buttonIndex, 0, kw);
        });
    });

    fm.appendButton("§l§c返回按钮操作页", [menuId, buttonIndex](Player& pl) {
        showButtonActions(pl, menuId, buttonIndex);
    });
    fm.sendTo(player);
}

void showIconPicker(
    Player&             player,
    std::string const&  menuId,
    int                 buttonIndex,
    int                 page,
    std::string const&  keyword
) {
    std::vector<std::string> list = filterPaths(keyword);
    size_t const             total = list.size();
    size_t const             pages = total == 0 ? 1 : (total + kIconsPerPage - 1) / kIconsPerPage;

    if (page < 0) page = 0;
    if (static_cast<size_t>(page) >= pages) page = static_cast<int>(pages) - 1;

    auto fm = SimpleForm{};
    fm.setTitle(fmt::format("§l§b选择图标 §7{}/{}", page + 1, pages));
    fm.setContent(fmt::format(
        "§7共 §f{} §7个候选{}\n§7点一个即为按钮 §f#{} §7设置图标（写入 icon，库内路径必然合法）",
        total,
        keyword.empty() ? "" : fmt::format("（关键字: §f{}§7）", keyword),
        buttonIndex
    ));

    if (total == 0) {
        fm.setContent(fmt::format("§c没有匹配 §f{} §c的图标", keyword));
    }

    size_t const begin = static_cast<size_t>(page) * kIconsPerPage;
    size_t const end   = std::min(begin + kIconsPerPage, total);
    for (size_t i = begin; i < end; ++i) {
        std::string const path = list[i];
        fm.appendButton("§f" + shortName(path), path, "path", [menuId, buttonIndex, path](Player& pl) {
            applyIcon(pl, menuId, buttonIndex, path);
        });
    }

    if (page > 0) {
        fm.appendButton("§l§7← 上一页", [menuId, buttonIndex, page, keyword](Player& pl) {
            showIconPicker(pl, menuId, buttonIndex, page - 1, keyword);
        });
    }
    if (static_cast<size_t>(page) + 1 < pages) {
        fm.appendButton("§l§7下一页 →", [menuId, buttonIndex, page, keyword](Player& pl) {
            showIconPicker(pl, menuId, buttonIndex, page + 1, keyword);
        });
    }
    fm.appendButton("§l§b重新分类 / 搜索", [menuId, buttonIndex](Player& pl) {
        showIconPickerCategories(pl, menuId, buttonIndex);
    });
    fm.appendButton("§l§c返回按钮操作页", [menuId, buttonIndex](Player& pl) {
        showButtonActions(pl, menuId, buttonIndex);
    });
    fm.sendTo(player);
}

//==== 新建菜单 ====
void showCreateMenuForm(Player& player) {
    // 默认分级来自 config 的 menuLevel.defaultLevel（默认一级菜单）
    size_t const defaultLevelIndex =
        (ConfigManager::getInstance().getConfig().menuLevel.defaultLevel == 2) ? 1 : 0;

    auto fm = CustomForm{"§l§a新建菜单"};
    fm.appendInput("id", "§e菜单ID§7(英文/数字/下划线, 会作为文件名)", "myshop", "");
    fm.appendInput("title", "§b标题§7(支持 § 颜色码)", "§b我的商店", "§b新菜单");
    fm.appendInput("content", "§7内容", "欢迎光临", "");
    fm.appendDivider();
    fm.appendDropdown(
        "level",
        "§d菜单分级",
        {"一级菜单§7(可被触发物品打开; 也能被其它菜单打开)",
         "二级菜单§7(只能从其它菜单进入, 触发物品不生效)"},
        defaultLevelIndex
    );
    fm.appendInput(
        "triggerItem",
        "§e触发物品ID§7(一级菜单用, 可留空稍后设置)",
        "minecraft:compass",
        ""
    );
    fm.appendDivider();
    fm.appendLabel("§7创建后直接进入该菜单的编辑页；分级与触发物品在编辑页里也能改。");

    fm.sendTo(player, [](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        if (!res.has_value()) {
            showEditorMain(pl);
            return;
        }
        std::string id, title, content, triggerInput;
        size_t      levelIndex = 0;
        try {
            id           = std::get<std::string>(res->at("id"));
            title        = std::get<std::string>(res->at("title"));
            content      = std::get<std::string>(res->at("content"));
            triggerInput = std::get<std::string>(res->at("triggerItem"));
            levelIndex   = static_cast<size_t>(formResultToIndex(res->at("level")));
        } catch (...) {}
        // 去首尾空白
        auto trim = [](std::string s) {
            size_t b = s.find_first_not_of(" \t\r\n");
            if (b == std::string::npos) return std::string{};
            size_t e = s.find_last_not_of(" \t\r\n");
            return s.substr(b, e - b + 1);
        };
        id = trim(id);
        if (title.empty()) title = "§b" + id;

        std::string reason;
        if (!isValidMenuId(id, reason)) {
            pl.sendMessage("§c[MeowMenu] " + reason);
            showCreateMenuForm(pl);
            return;
        }

        int const level = (levelIndex == 1) ? 2 : 1; // 默认一级菜单
        if (!MenuStore::getInstance().createMenu(id, title, content, level)) {
            pl.sendMessage("§c[MeowMenu] 创建失败（可能已存在同名文件）");
            showCreateMenuForm(pl);
            return;
        }

        // 一级菜单顺手写上触发物品（校验用编辑器同一套规则: 补全命名空间 / 查重 / 钟留给主菜单）
        std::string triggerType;
        bool        triggerOk = true;
        if (level == 1 && !trim(triggerInput).empty()) {
            triggerOk = validateTriggerItemType(pl, id, triggerInput, triggerType);
            if (triggerOk) {
                if (auto menu = MenuStore::getInstance().getMenu(id)) {
                    Menu local = *menu;
                    local.triggerItem.type        = triggerType;
                    local.triggerItem.displayName = triggerType;
                    MenuStore::getInstance().saveMenu(local);
                }
            }
        }

        pl.sendMessage(fmt::format(
            "§a[MeowMenu] 菜单 §f{} §a已创建（{}），直接进入编辑",
            id,
            level == 1 ? "§a一级菜单§a" : "§e二级菜单§a"
        ));
        if (level == 1) {
            if (!triggerType.empty()) {
                pl.sendMessage("§7触发物品: §f" + triggerType);
            } else if (!triggerOk) {
                pl.sendMessage("§7触发物品没设置成功, 可在编辑页 → 「菜单分级 / 触发物品」里补上");
            } else {
                pl.sendMessage("§c注意: 一级菜单还没配触发物品, 现在只能用菜单按钮打开它");
            }
        }
        showEditMenu(pl, id);
    });
}

//==== 移动到指定位置 ====
void showMoveButtonTo(Player& player, std::string const& menuId, int buttonIndex) {
    auto menu = MenuStore::getInstance().getMenu(menuId);
    if (!menu || buttonIndex < 0 || buttonIndex >= static_cast<int>(menu->buttons.size())) {
        player.sendMessage("§c[MeowMenu] 按钮不存在");
        return;
    }
    int const size = static_cast<int>(menu->buttons.size());
    if (size < 2) {
        player.sendMessage("§7[MeowMenu] 只有一个按钮，无需移动");
        showButtonActions(player, menuId, buttonIndex);
        return;
    }

    auto fm = CustomForm{fmt::format("§l§e移动到指定位置 §7#{}", buttonIndex)};
    fm.appendLabel(fmt::format(
        "§7当前: §f#{} §7{} / 共 §f{} §7个按钮\n§7目标位置从 1 开始（1 = 最前，{} = 最后）",
        buttonIndex,
        menu->buttons[static_cast<size_t>(buttonIndex)].text,
        size,
        size
    ));
    fm.appendSlider("pos", "§e目标位置", 1, size, 1, buttonIndex + 1);

    fm.sendTo(player, [menuId, buttonIndex](Player& pl, CustomFormResult res, ll::form::FormCancelReason) {
        auto m = MenuStore::getInstance().getMenu(menuId);
        if (!m || !res.has_value()) {
            showButtonActions(pl, menuId, buttonIndex);
            return;
        }
        int pos = buttonIndex + 1;
        try {
            if (auto const* v = std::get_if<double>(&res->at("pos"))) pos = static_cast<int>(*v);
        } catch (...) {}

        int const count = static_cast<int>(m->buttons.size());
        if (pos < 1) pos = 1;
        if (pos > count) pos = count;
        int const target = pos - 1;

        if (target != buttonIndex && buttonIndex >= 0 && buttonIndex < count) {
            Button moved = m->buttons[static_cast<size_t>(buttonIndex)];
            m->buttons.erase(m->buttons.begin() + buttonIndex);
            m->buttons.insert(m->buttons.begin() + target, moved);
            MenuStore::getInstance().saveMenu(*m);
            pl.sendMessage(fmt::format("§a[MeowMenu] 按钮已移动到 §f#{}", target));
        }
        showButtonActions(pl, menuId, target);
    });
}

} // namespace MenuEditor
} // namespace meowmenu
