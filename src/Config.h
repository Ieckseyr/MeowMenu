// Config.h - 全局配置（Meowdata/MeowMenu/config.json; 兼容 JS 版格式）
#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace meowmenu {

// 默认触发物品 SNBT（主菜单的钟）; config 里 triggerItem.snbt 留空 = 用这一份
inline constexpr char const* kDefaultTriggerSnbt =
    "{\"Count\":1b,\"Damage\":0s,\"Name\":\"minecraft:clock\",\"WasPickedUp\":0b,"
    "\"tag\":{\"RepairCost\":0,"
    "\"display\":{\"Lore\":["
    "\"§a§[电脑]▶§4- §r§l 手持右键即可打开菜单\","
    "\"§b§[手机]▶§4- §r§l 手持长按或点击地面即可打开菜单\""
    "],\"Name\":\"§r§4服§6务§g器§a菜§b单\"},"
    "\"minecraft:keep_on_death\":1b}}";

// 主菜单触发物品: type 决定什么能触发, snbt/count 决定发什么（改 type 记得同步 snbt 的 Name）
struct TriggerItemConfig {
    std::string type        = "minecraft:clock";
    std::string displayName = "菜单";
    std::string snbt        = kDefaultTriggerSnbt; // 空 = 用默认（kDefaultTriggerSnbt）
    int         count       = 1;                   // 发放数量
};

struct GiveUsConfig {
    bool        enabled    = true;
    bool        giveOnJoin  = true;
    std::string commandName = "mmenu";
    std::string commandAlias;
    std::string commandDesc = "§a打开§b服务器§e菜单";
    // 物品只是"打开菜单用的钥匙", 不是新手礼包 —— 措辞统一说明它怎么用
    std::string msgReceived    = "§6已发放§e{item}§6, 手持§a右键§6即可打开菜单。";
    std::string msgAlreadyHave = "§6你的背包里已经有§e{item}§6了, 手持§a右键§6即可打开菜单。";
    std::string msgInventoryFull =
        "§6你的§b背包§d满了§6, 无法发放§e{item}§6。§7请清理背包后输入 §a/{cmd} §7获取。";
    std::string msgCleared = "§a已清除你的§e{item}§6。";
};

// 新建菜单的默认分级（每个菜单自己的 level 写在菜单数据里）
struct MenuLevelConfig {
    int defaultLevel = 1; // 新建菜单的分级: 1 = 一级菜单, 2 = 二级菜单
};

struct Config {
    std::string         displayMode = "static";   // 保留字段，本版本仅 static
    bool                useDogeUI   = false;      // 全局默认 DogeUI（菜单无 dogeui 字段时使用）
    std::string         mainMenuId  = "main";
    bool                allowPlayerModeChoice = false; // 保留字段，本版本忽略
    // 箱子翻页用就地刷新（无延迟无闪烁）; 某些客户端不更新时设 false 回退整体重开
    bool                chestInPlaceRefresh = true;
    TriggerItemConfig   triggerItem;
    GiveUsConfig        giveUs;
    MenuLevelConfig     menuLevel;
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
