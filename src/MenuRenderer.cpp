// MenuRenderer.cpp - 菜单渲染器实现
//
// 构建 SimpleForm JSON，应用 DogeUI 标识符前缀，通过协议层发送。
#include "MenuRenderer.h"
#include "Config.h"
#include "MenuStore.h"
#include "FormSender.h"
#include "FormManager.h"
#include "ButtonHandler.h"
#include "MeowMenu.h"

#include <ll/api/service/Bedrock.h>

#include <mc/world/actor/player/Player.h>
#include <mc/world/level/Level.h>

#include <fmt/format.h>

#include <memory>
#include <optional>
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

    // 构建按钮列表（跳过 spacer 类型）
    std::vector<std::pair<std::string, std::string>> buttons;
    buttons.reserve(menu->buttons.size());
    for (auto const& btn : menu->buttons) {
        if (btn.type == "spacer") continue; // 空格按钮在表单模式下跳过
        buttons.emplace_back(btn.text, btn.icon);
    }

    // 构建 SimpleForm JSON
    std::string formJson = FormSender::buildSimpleFormJson(title, menu->content, buttons);

    // 存储玩家网络标识（用于回调时重新获取玩家）
    std::string networkKey = player.getNetworkIdentifier().getIPAndPort();

    // 分配 formId 并注册回调
    uint32_t formId = FormManager::getInstance().allocate(
        [menuId, menuCopy = *menu, networkKey](uint32_t, std::optional<int> buttonIndex) {
            if (!buttonIndex.has_value()) {
                return; // 玩家关闭表单
            }

            // 通过 networkKey 获取玩家
            auto level = ll::service::getLevel();
            if (!level.has_value()) return;

            Player* player = nullptr;
            level->forEachPlayer([&](Player& p) -> bool {
                if (p.getNetworkIdentifier().getIPAndPort() == networkKey) {
                    player = &p;
                    return true;
                }
                return false;
            });

            if (!player) return;

            // 映射可见按钮索引到实际按钮
            int visibleIndex = 0;
            for (auto const& btn : menuCopy.buttons) {
                if (btn.type == "spacer") continue;

                if (visibleIndex == buttonIndex.value()) {
                    ButtonHandler::handleButton(*player, btn);
                    break;
                }
                visibleIndex++;
            }
        }
    );

    // 发送表单
    if (!FormSender::sendToPlayer(player, formId, formJson)) {
        logger.error("发送菜单表单失败: menuId={}, player={}", menuId, player.getNameTag());
        player.sendMessage("§c[MeowMenu] 菜单打开失败，请稍后重试");
    }
}

} // namespace meowmenu
