// MeowMenu.cpp - 协议层菜单插件主模块实现
#include "MeowMenu.h"
#include "Config.h"
#include "MenuStore.h"
#include "FormManager.h"
#include "FormSender.h"
#include "FormResponseHook.h"
#include "ButtonHandler.h"
#include "MenuRenderer.h"
#include "TriggerItem.h"
#include "MenuEditor.h"
#include "EventRegistration.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/mod/RegisterHelper.h"

namespace meowmenu {

// 前向声明（定义在 CommandRegistration.cpp 中）
void registerCommands();

MeowMenu& MeowMenu::getInstance() {
    static MeowMenu instance;
    return instance;
}

MeowMenu& lib() {
    return MeowMenu::getInstance();
}

bool MeowMenu::load() {
    auto& logger = getSelf().getLogger();
    logger.info("MeowMenu (C++) loading...");
    return true;
}

bool MeowMenu::enable() {
    auto& logger = getSelf().getLogger();
    logger.info("MeowMenu (C++) enabling...");

    try {
        // 1. 加载配置
        ConfigManager::getInstance().load();

        // 2. 加载菜单数据
        MenuStore::getInstance().loadAll();

        // 3. 初始化表单管理器（formId 分配 + 回调注册表）
        FormManager::getInstance().init();

        // 4. 安装 ModalFormResponsePacket 钩子（接收玩家表单响应）
        FormResponseHook::hook();

        // 5. 注册命令 /mmenu
        registerCommands();

        // 6. 注册事件监听（玩家加入、物品使用）
        registerEvents(mListenerPtrs);

        logger.info("MeowMenu (C++) enabled. 仅支持普通菜单(static)模式，DogeUI 每菜单可自定义前缀。");
        logger.info("Usage: /mmenu [admin|mode] | /mmgv");
    } catch (std::exception const& e) {
        logger.error("MeowMenu enable failed: {}", e.what());
    } catch (...) {
        logger.error("MeowMenu enable failed: unknown exception");
    }
    return true;
}

bool MeowMenu::disable() {
    auto& logger = getSelf().getLogger();
    logger.info("MeowMenu (C++) disabling...");

    // 移除事件监听
    auto& eventBus = ll::event::EventBus::getInstance();
    for (auto& ptr : mListenerPtrs) {
        eventBus.removeListener(ptr);
    }
    mListenerPtrs.clear();

    // 卸载表单响应钩子
    FormResponseHook::unhook();

    // 清理表单管理器
    FormManager::getInstance().shutdown();

    logger.info("MeowMenu (C++) disabled.");
    return true;
}

} // namespace meowmenu

LL_REGISTER_MOD(meowmenu::MeowMenu, meowmenu::MeowMenu::getInstance());
