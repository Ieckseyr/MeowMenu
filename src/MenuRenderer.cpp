// MenuRenderer.cpp - 菜单渲染器实现
//
// 使用 LeviLamina SimpleForm API 发送表单给玩家。
// 应用 DogeUI 标识符前缀到标题，支持 MeowPAPI 变量翻译（标题、内容、按钮文本）。
#include "MenuRenderer.h"
#include "Config.h"
#include "MenuStore.h"
#include "ButtonHandler.h"
#include "MeowMenu.h"
#include "IconValidator.h"

#include "meowpapi/PlaceholderApi.h"

#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>

#include <mc/world/actor/player/Player.h>

#include <fmt/format.h>

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

void MenuRenderer::renderMenu(Player& player, std::string const& menuId) {
    auto& logger = lib().getSelf().getLogger();

    // 获取菜单
    auto menu = MenuStore::getInstance().getMenu(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在: " + menuId);
        return;
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
