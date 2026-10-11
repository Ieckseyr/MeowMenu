// MenuRenderer.cpp - 菜单渲染器（SimpleForm; DogeUI 前缀 + MeowPAPI 翻译）
#include "MenuRenderer.h"
#include "Config.h"
#include "MenuStore.h"
#include "ButtonHandler.h"
#include "NpcMenuBridge.h"
#include "PlayerUiPref.h"
#include "MeowMenu.h"
#include "IconValidator.h"

#include "meowpapi/PlaceholderApi.h"

#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/thread/ServerThreadExecutor.h>

#include <mc/world/actor/player/Player.h>
#include <mc/world/level/Level.h>

#include <fmt/format.h>

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace meowmenu {

MenuRenderer& MenuRenderer::getInstance() {
    static MenuRenderer instance;
    return instance;
}

bool MenuRenderer::shouldApplyDogeUI(std::string const& menuId) const {
    auto menu = MenuStore::getInstance().getMenu(menuId);
    if (menu) {
        // 菜单级别配置优先
        return menu->dogeui.enabled;
    }

    // 菜单不存在时，回退到全局配置（仅主菜单）
    auto const& cfg = ConfigManager::getInstance().getConfig();
    return cfg.useDogeUI && (menuId == cfg.mainMenuId);
}

std::string MenuRenderer::getDogeUIPrefix(std::string const& menuId) const {
    auto menu = MenuStore::getInstance().getMenu(menuId);
    if (menu && menu->dogeui.enabled) {
        return menu->dogeui.prefix;
    }

    // 全局默认前缀
    return "/D ";
}

std::string MenuRenderer::resolveUiMode(Player const& player, Menu const& menu) {
    // 硬规则 1: 启用了 DogeUI 的菜单固定为表单 —— DogeUI 是"给表单标题加标识符前缀"的功能,
    // 箱子/NPC 对话框不显示该前缀, 跟着切换会让标识符失效。
    if (menu.dogeui.enabled) {
        return ui_mode::kForm;
    }

    // 硬规则 2: 含菜单编辑器入口的菜单固定为表单（JSON UI）—— 编辑器界面只能由表单承载,
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

    // 玩家自选样式优先（auto = 跟随菜单自身配置）, 但不得越过菜单的固有约束（uiAllow）
    std::string pref = PlayerUiPref::getInstance().get(player);
    if ((pref == ui_mode::kNpc || pref == ui_mode::kChest || pref == ui_mode::kForm)
        && menu.allowsUiMode(pref)) {
        return pref;
    }
    std::string own = menu.uiMode();
    if (menu.allowsUiMode(own)) return own;
    return ui_mode::kForm; // 兜底: 表单永远可用
}

void MenuRenderer::renderMenuDeferred(Player& player, std::string const& menuId) {
    std::string playerName = player.getRealName();
    try {
        ll::thread::ServerThreadExecutor::getDefault().executeAfter(
            [playerName, menuId]() {
                auto level = ll::service::getLevel();
                if (!level.has_value()) return;
                Player* target = level->getPlayer(playerName);
                if (!target) return;
                MenuRenderer::getInstance().renderMenu(*target, menuId);
            },
            std::chrono::milliseconds(50)
        );
    } catch (...) {
        renderMenu(player, menuId); // 排不进队列就直接渲染, 不静默失败
    }
}

void MenuRenderer::renderMenu(Player& player, std::string const& menuId) {
    auto& logger = lib().getSelf().getLogger();

    // 获取菜单
    auto menu = MenuStore::getInstance().getMenu(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在: " + menuId);
        return;
    }

    // 界面样式分派: 玩家自选样式 > 菜单自身模式（都受 uiAllow 约束）,
    // 需要 HologramLib 的样式不可用时回退表单
    std::string mode   = resolveUiMode(player, *menu);
    auto&       bridge = NpcMenuBridge::getInstance();
    if (mode == ui_mode::kNpc) {
        if (bridge.openDialogue(player, menuId)) return;
        static std::atomic<bool> warnedNpc{false};
        if (!bridge.isAvailable() && !warnedNpc.exchange(true)) {
            logger.warn("菜单 {} 需要 NPC 对话样式, 但 HologramLib 不可用 → 已回退普通表单", menuId);
        }
    } else if (mode == ui_mode::kChest) {
        if (bridge.openChestMenu(player, menuId)) return;
        static std::atomic<bool> warnedChest{false};
        if (!bridge.isChestAvailable() && !warnedChest.exchange(true)) {
            logger.warn("菜单 {} 需要箱子样式, 但 HologramLib 不可用 → 已回退普通表单", menuId);
        }
    }

    // 应用 DogeUI 标识符前缀
    std::string title = menu->title;
    if (shouldApplyDogeUI(menuId)) {
        std::string prefix = getDogeUIPrefix(menuId);
        title = prefix + title;
        logger.info("菜单 {} 应用 DogeUI 标识符: '{}'", menuId, prefix);
    }

    // 通过 MeowPAPI 翻译变量（%name% 和 {name} 格式）
    auto& papi = meowpapi::PlaceholderApi::getInstance();
    title         = papi.translateStringWithPlayer(title, &player);
    std::string content = papi.translateStringWithPlayer(menu->content, &player);

    // 构建 SimpleForm（跳过 spacer 类型按钮）
    auto form = ll::form::SimpleForm{};
    form.setTitle(title);
    form.setContent(content);

    for (auto const& btn : menu->buttons) {
        if (btn.type == "spacer") continue; // 空格按钮在表单模式下跳过

        std::string translatedText = papi.translateStringWithPlayer(btn.text, &player);

        // 显示项（不可点击）: header 大字标题 / label 文本行 / divider 分隔线
        if (btn.type == "header") {
            form.appendHeader(translatedText);
            continue;
        }
        if (btn.type == "label") {
            form.appendLabel(translatedText);
            continue;
        }
        if (btn.type == "divider") {
            form.appendDivider();
            continue;
        }

        // 解析最终使用的图标路径
        // 优先级：icon（文本路径）> iconItem（物品 ID）> 无图标
        std::string resolvedIcon = IconValidator::getInstance().resolveIcon(btn.icon, btn.iconItem);

        // 捕获按钮数据副本，回调中处理点击
        Button buttonCopy = btn;
        if (resolvedIcon.empty()) {
            form.appendButton(translatedText, [buttonCopy](Player& pl) {
                ButtonHandler::handleButton(pl, buttonCopy);
            });
        } else {
            form.appendButton(translatedText, resolvedIcon, "path", [buttonCopy](Player& pl) {
                ButtonHandler::handleButton(pl, buttonCopy);
            });
        }
    }

    // 发送表单给玩家（LL Form API 内部处理 formId 分配和响应回调）
    form.sendTo(player);
}

} // namespace meowmenu
