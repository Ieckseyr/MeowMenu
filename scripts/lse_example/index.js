// MeowMenu LSE API 调用示例
//
// 演示如何通过 RemoteCall 调用 MeowMenu 导出的全部 API：
//   1. 等待 MeowMenu 就绪
//   2. 创建菜单 + 添加按钮
//   3. 注册自定义物品触发（手持指南针打开导航菜单）
//   4. 主动渲染菜单
//   5. 查询/管理菜单
//
// 部署：将本文件放入 LSE 脚本目录（如 plugins/LegacyScriptEngine/scripts/）
// 启用后在服务器启动时自动执行。

// ===== 1. 等待 MeowMenu 就绪 =====
function waitForMeowMenu(timeoutMs = 10000) {
    return new Promise((resolve) => {
        const start = Date.now();
        const timer = setInterval(() => {
            try {
                const ready = RemoteCall.importAs("MeowMenu", "isPluginReady");
                if (ready && ready()) {
                    clearInterval(timer);
                    console.log("[MeowMenu示例] MeowMenu 已就绪");
                    resolve(true);
                    return;
                }
            } catch (e) {
                // importAs 在插件未加载时会抛异常，忽略继续等待
            }
            if (Date.now() - start > timeoutMs) {
                clearInterval(timer);
                console.warn("[MeowMenu示例] 等待 MeowMenu 超时，放弃初始化");
                resolve(false);
            }
        }, 200);
    });
}

// ===== 2. 导入常用 API =====
const Menu = {
    getMenuIds:        RemoteCall.importAs("MeowMenu", "getMenuIds"),
    getMenu:           RemoteCall.importAs("MeowMenu", "getMenu"),
    createMenu:        RemoteCall.importAs("MeowMenu", "createMenu"),
    saveMenu:          RemoteCall.importAs("MeowMenu", "saveMenu"),
    deleteMenu:        RemoteCall.importAs("MeowMenu", "deleteMenu"),
    reloadMenus:       RemoteCall.importAs("MeowMenu", "reloadMenus"),
    addButton:         RemoteCall.importAs("MeowMenu", "addButton"),
    updateButton:      RemoteCall.importAs("MeowMenu", "updateButton"),
    deleteButton:      RemoteCall.importAs("MeowMenu", "deleteButton"),
    getButtonCount:    RemoteCall.importAs("MeowMenu", "getButtonCount"),
    renderMenu:        RemoteCall.importAs("MeowMenu", "renderMenu"),
    renderMainMenu:    RemoteCall.importAs("MeowMenu", "renderMainMenu"),
    giveTriggerItem:   RemoteCall.importAs("MeowMenu", "giveTriggerItem"),
    registerItemTrigger:    RemoteCall.importAs("MeowMenu", "registerItemTrigger"),
    unregisterItemTrigger:  RemoteCall.importAs("MeowMenu", "unregisterItemTrigger"),
    getItemTriggers:        RemoteCall.importAs("MeowMenu", "getItemTriggers"),
    triggerButton:     RemoteCall.importAs("MeowMenu", "triggerButton"),
    getConfig:         RemoteCall.importAs("MeowMenu", "getConfig"),
    showEditor:        RemoteCall.importAs("MeowMenu", "showEditor"),
};

// ===== 3. 主流程 =====
(async function main() {
    const ok = await waitForMeowMenu();
    if (!ok) return;

    try {
        // --- 示例 A：创建一个导航菜单 ---
        const navId = "nav_example";
        const created = Menu.createMenu(navId, "§l§b导航菜单", "§7选择目的地");
        console.log(`[MeowMenu示例] 创建菜单 ${navId}: ${created}`);

        // 添加几个按钮
        const buttons = [
            { text: "§a主城",     type: "command", value: "tp 0 100 0",          icon: "textures/ui/icon_home",    iconItem: "", item: "" },
            { text: "§a商店",     type: "submenu", value: "shop_example",         icon: "textures/ui/icon_store",   iconItem: "", item: "" },
            { text: "§c返回出生点", type: "command", value: "spawn",              icon: "textures/ui/icon_recipe_nature", iconItem: "", item: "" },
        ];
        for (const btn of buttons) {
            Menu.addButton(navId, JSON.stringify(btn));
        }
        console.log(`[MeowMenu示例] 添加 ${buttons.length} 个按钮，当前按钮数: ${Menu.getButtonCount(navId)}`);

        // --- 示例 B：注册自定义物品触发 ---
        // 手持指南针（minecraft:compass）右键方块/空气时打开导航菜单
        const reg = Menu.registerItemTrigger("minecraft:compass", navId);
        console.log(`[MeowMenu示例] 注册指南针触发: ${reg}`);

        // 查看当前所有物品触发绑定
        const triggers = JSON.parse(Menu.getItemTriggers());
        console.log(`[MeowMenu示例] 当前物品触发绑定: ${JSON.stringify(triggers)}`);

        // --- 示例 C：查询所有菜单 ---
        const ids = JSON.parse(Menu.getMenuIds());
        console.log(`[MeowMenu示例] 当前所有菜单: ${JSON.stringify(ids)}`);

        // --- 示例 D：读取菜单数据 ---
        const navMenu = JSON.parse(Menu.getMenu(navId));
        console.log(`[MeowMenu示例] 菜单 ${navId} 标题: ${navMenu.title}, 按钮数: ${navMenu.buttons.length}`);

        // --- 示例 E：读取配置 ---
        const cfg = JSON.parse(Menu.getConfig());
        console.log(`[MeowMenu示例] 主菜单ID: ${cfg.mainMenuId}, DogeUI: ${cfg.useDogeUI}`);

        console.log("[MeowMenu示例] 初始化完成");
        console.log("[MeowMenu示例] 进服后手持指南针右键即可打开导航菜单");
    } catch (e) {
        console.warn(`[MeowMenu示例] 初始化失败: ${e}`);
    }
})();

// ===== 4. 可选：注册命令主动打开菜单 =====
// 取消注释后，玩家输入 /nav 可打开导航菜单
/*
mc.listen("onServerStarted", () => {
    // 注册命令需根据 LSE 版本适配，此处用指令回调示意
});

// 玩家进服时给予指南针（可选）
mc.listen("onJoin", (player) => {
    setTimeout(() => {
        try {
            // 进服 2 秒后为玩家打开主菜单（演示主动渲染）
            // Menu.renderMainMenu(player.realName);
        } catch (e) {}
    }, 2000);
});
*/

// ===== 5. 清理示例（如需卸载时移除测试数据，取消注释）=====
/*
function cleanup() {
    try {
        Menu.unregisterItemTrigger("minecraft:compass");
        Menu.deleteMenu("nav_example");
        Menu.deleteMenu("shop_example");
        console.log("[MeowMenu示例] 已清理测试数据");
    } catch (e) {}
}
*/
