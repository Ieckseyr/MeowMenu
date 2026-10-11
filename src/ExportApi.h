// ExportApi.h - RemoteCall 导出（命名空间 "MeowMenu", 复杂数据用 JSON 字符串）
#pragma once

namespace meowmenu {

class ExportApi {
public:
    static ExportApi& getInstance();

    // 注册全部 API（在 MeowMenu::enable() 中调用一次）
    void registerAll();

private:
    ExportApi() = default;

    void registerMenuApi();         // 菜单增删改查
    void registerButtonApi();       // 按钮增删改
    void registerRenderApi();       // 渲染
    void registerTriggerItemApi();  // 主菜单触发物品（config.triggerItem）
    void registerItemTriggerApi();  // 自定义物品→菜单绑定
    void registerButtonHandlerApi();// 按钮点击模拟
    void registerConfigApi();       // 配置
    void registerIconApi();         // 图标校验
    void registerEditorApi();       // 管理员编辑器表单
    void registerNpcMenuApi();      // HologramLib NPC 对话菜单（可选前置）
    void registerUtilApi();         // 工具函数
    void registerMigrationApi();    // 外部菜单格式迁移/导入
};

} // namespace meowmenu
