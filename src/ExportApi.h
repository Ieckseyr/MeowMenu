// ExportApi.h - RemoteCall API 导出注册
//
// 将 MeowMenu 的全部功能通过 RemoteCall 导出，供 LSE（Node.js）或其他插件调用。
// 命名空间: "MeowMenu"
// 复杂数据（菜单/按钮/配置）一律用 JSON 字符串传递。
//
// 用法（LSE 端）：
//   const getMenuIds = RemoteCall.importAs("MeowMenu", "getMenuIds");
//   const ids = JSON.parse(getMenuIds());
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
    void registerTriggerItemApi();  // 硬编码 clock 触发物品
    void registerItemTriggerApi();  // 自定义物品→菜单绑定
    void registerButtonHandlerApi();// 按钮点击模拟
    void registerConfigApi();       // 配置
    void registerIconApi();         // 图标校验
    void registerEditorApi();       // 管理员编辑器表单
    void registerUtilApi();         // 工具函数
    void registerMigrationApi();    // 外部菜单格式迁移/导入
};

} // namespace meowmenu
