// Config.h - 全局配置管理
//
// 读取/保存 Meowdata/MeowMenu/config.json
// 向后兼容现有 JS 版配置格式。displayMode 字段保留但本版本仅使用 static 模式。
#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace meowmenu {

struct TriggerItemConfig {
    std::string type        = "minecraft:clock";
    std::string displayName = "菜单";
    std::string snbt;                              // 可选 SNBT
    int         count       = 1;
};

struct GiveUsConfig {
    bool        enabled    = true;
    bool        giveOnJoin  = true;
    std::string commandName = "mmenu";
    std::string commandAlias;
    std::string commandDesc = "§a打开§b服务器§e菜单";
    std::string msgReceived    = "§6你§a获得了§c一个§e{item}§6。";
    std::string msgAlreadyHave = "§6你§e背包里§c已经§a有{item}了";
    std::string msgInventoryFull =
        "§6你的§b背包§d满了，§a自动给{item}§c失败，§6请清理§b背包§6后,输入§e/{cmd}§6获取§a{item}。";
    std::string msgCleared = "§a已清除你的{item}";
};

struct Config {
    std::string         displayMode = "static";   // 保留字段，本版本仅 static
    bool                useDogeUI   = false;      // 全局默认 DogeUI（菜单无 dogeui 字段时使用）
    std::string         mainMenuId  = "main";
    bool                allowPlayerModeChoice = false; // 保留字段，本版本忽略
    // 箱子界面翻页/导航用"就地刷新"(HologramLib 的 update + 逐格 setItem) 还是"整体重开"(open):
    //   就地刷新 = 无延迟无闪烁; 整体重开 = 每次付库的打开节奏(默认 ~350ms)但兼容性最稳。
    // 默认就地刷新; 若某些客户端上就地刷新后内容不更新, 把这里设为 false 即回退。
    bool                chestInPlaceRefresh = true;
    TriggerItemConfig   triggerItem;
    GiveUsConfig        giveUs;
};

class ConfigManager {
public:
    static ConfigManager& getInstance();

    void load();
    void save();

    Config const& getConfig() const { return mConfig; }
    Config&       getConfig()       { return mConfig; }

    // 格式化消息模板（替换 {item} 和 {cmd}）
    std::string formatMessage(std::string const& template_) const;

private:
    ConfigManager() = default;

    Config mConfig;
    std::string mConfigPath;
};

} // namespace meowmenu
