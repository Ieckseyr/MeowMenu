// MigrationManager.h - 菜单配置迁移/导入
//
// 移植自 JS 版 MeowMenu 的 MigrationManager。
// 支持 自动识别 并 转换 以下外部菜单插件格式到 MeowMenu 标准格式：
//   1. PMenu  : {title, content, buttons:[{text, command, type, image, images, money}]}
//   2. CMENU  : [{name, image?, open, type}]，type 仅 comm/opcomm/form/opform
//   3. CD(旧) : [{name, open, type, adminOnly, image, subtitle}]
//   4. CD(新) : {ddui, items:[...]}（items 同 CD 旧格式）
//
// 工作流程：
//   - 腐竹把外部菜单 JSON 文件放入 Meowdata/MeowMenu/migrations/ 目录
//   - 启动时 autoMigrate() 自动扫描、识别、转换、保存到 data/
//   - 导入成功的文件移动到 migrations/done/ 避免重复导入
//   - 目标菜单已存在则跳过（不覆盖）
//
// 同时导出为 RemoteCall API，供 LSE 编程式导入。
#pragma once

#include "MenuData.h"

#include <memory>
#include <string>

namespace meowmenu {

class MigrationManager {
public:
    static MigrationManager& getInstance();

    // 启动时自动扫描 migrations/ 目录导入（返回成功导入的数量）
    int autoMigrate();

    // 传入外部格式 JSON 字符串，自动识别格式并转换为 MeowMenu 菜单后保存
    // menuId     : 目标菜单 ID（已存在则覆盖）
    // formatJson : 外部格式 JSON 字符串
    // 返回是否转换并保存成功
    bool importMenu(std::string const& menuId, std::string const& formatJson);

private:
    MigrationManager() = default;

    // 自动识别格式并转换，无法识别返回 nullptr
    std::shared_ptr<Menu> convertToMeowMenu(std::string const& menuId, nlohmann::json const& data);

    // CD 格式（旧/新共用）：items 为按钮数组
    std::shared_ptr<Menu> convertCD(std::string const& menuId, nlohmann::json const& items);

    // CMENU 格式：items 为按钮数组
    std::shared_ptr<Menu> convertCMenu(std::string const& menuId, nlohmann::json const& items);

    // PMenu 格式：data 为对象 {title, content, buttons:[...]}
    std::shared_ptr<Menu> convertPMenu(std::string const& menuId, nlohmann::json const& data);

    // 检测数组是否为 CMENU 格式
    bool isCMenuFormat(nlohmann::json const& items) const;

    // 转换后修复非法图标（normalizeIcon）
    void fixMenuIcons(Menu& menu) const;
};

} // namespace meowmenu
