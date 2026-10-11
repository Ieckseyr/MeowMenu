// MigrationManager.h - 外部菜单格式导入（PMenu / CMENU / CD 旧新）
// 扫描 Meowdata/MeowMenu/migrations/, 识别转换到 data/, 成功的文件移入 migrations/done/。
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

    // 转换外部格式 JSON 并保存（menuId 已存在则覆盖）; 返回是否成功
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
