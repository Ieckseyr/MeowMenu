// MeowMenu.cpp - 插件主模块（加载/启用流程）
#include "MeowMenu.h"
#include "Config.h"
#include "MenuStore.h"
#include "ButtonHandler.h"
#include "MenuRenderer.h"
#include "TriggerItem.h"
#include "MenuEditor.h"
#include "EventRegistration.h"
#include "IconValidator.h"
#include "ItemTrigger.h"
#include "ItemUseHook.h"
#include "MigrationManager.h"
#include "NpcMenuBridge.h"
#include "PlayerUiPref.h"
#include "PlayerRecord.h"
#include "ExportApi.h"

#include "meowpapi/PlaceholderApi.h"

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

        // 1.1 玩家自选界面样式偏好（/mmenu ui）
        PlayerUiPref::getInstance().initialize();

        // 2. 初始化图标校验器（嵌入图标库; 必须在加载菜单数据之前）
        IconValidator::getInstance().initialize();

        // 3. 加载菜单数据
        MenuStore::getInstance().loadAll();

        // 3.1 自动迁移其它菜单插件格式（CD/PMenu/CMENU → data/）
        MigrationManager::getInstance().autoMigrate();

        // 3.2 进服发放记录（players.json）: 发过的进服直接跳过, 失败不记账、下次重试
        PlayerRecordStore::getInstance().load();

        // 3. 初始化 MeowPAPI 客户端模式（通过 RemoteCall 调用 MeowSidebar 的 PAPI 中心）
        meowpapi::PlaceholderApi::getInstance().initAsClient();

        // 4. 注册命令 /mmenu
        registerCommands();

        // 5. 注册事件监听（玩家加入、物品使用）
        registerEvents(mListenerPtrs);

        // 5.1 物品使用裸钩子（兜住 LL 事件覆盖不到的右键）
        if (installItemUseHooks()) {
            logger.info("物品触发钩子已安装: ItemUseInventoryTransaction::$handle, Player::$useItem");
        } else {
            logger.warn("部分物品触发钩子安装失败，物品触发将仅依赖 LL 事件");
        }

        // 6. 初始化自定义物品触发注册表（LSE 通过 API 注册的物品→菜单绑定）
        ItemTrigger::getInstance().initialize();

        // 7. 连接 HologramLib 的 NPC 菜单（可选前置; 必须在菜单数据加载之后）
        NpcMenuBridge::getInstance().initialize();

        // 8. 注册 RemoteCall API 导出（命名空间: MeowMenu）
        ExportApi::getInstance().registerAll();

        logger.info("MeowMenu (C++) enabled. 基于 LeviLamina Form API，DogeUI 每菜单可自定义前缀。");
        logger.info("Usage: /mmenu [admin|reload|mode] | /mmgv");
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

    // 卸载物品使用触发钩子（避免钩子悬挂到已卸载的代码上）
    uninstallItemUseHooks();

    // 断开 HologramLib NPC 菜单桥接（移除监听并销毁本插件创建的 NPC）
    NpcMenuBridge::getInstance().shutdown();

    // 移除事件监听
    auto& eventBus = ll::event::EventBus::getInstance();
    for (auto& ptr : mListenerPtrs) {
        eventBus.removeListener(ptr);
    }
    mListenerPtrs.clear();

    logger.info("MeowMenu (C++) disabled.");
    return true;
}

} // namespace meowmenu

LL_REGISTER_MOD(meowmenu::MeowMenu, meowmenu::MeowMenu::getInstance());
