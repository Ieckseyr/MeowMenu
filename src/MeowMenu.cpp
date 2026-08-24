// MeowMenu.cpp - 菜单插件主模块实现
//
// 使用 LeviLamina Form API 实现菜单表单的发送与回调处理。
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
#include "MigrationManager.h"
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

        // 2. 初始化图标校验器（解析嵌入的 texture_path.json）
        //    必须在加载菜单数据之前完成，以便加载菜单时修复非法图标
        IconValidator::getInstance().initialize();

        // 3. 加载菜单数据
        MenuStore::getInstance().loadAll();

        // 3.1 自动迁移其他菜单插件格式（CD/PMenu/CMENU）到 data/
        //     扫描 Meowdata/MeowMenu/migrations/ 下的 JSON，识别后转换导入
        MigrationManager::getInstance().autoMigrate();

        // 3. 初始化 MeowPAPI 客户端模式（通过 RemoteCall 调用 MeowSidebar 的 PAPI 中心）
        meowpapi::PlaceholderApi::getInstance().initAsClient();

        // 4. 注册命令 /mmenu
        registerCommands();

        // 5. 注册事件监听（玩家加入、物品使用）
        registerEvents(mListenerPtrs);

        // 6. 初始化自定义物品触发注册表（LSE 通过 API 注册的物品→菜单绑定）
        ItemTrigger::getInstance().initialize();

        // 7. 注册 RemoteCall API 导出（命名空间: MeowMenu）
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
