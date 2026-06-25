// Config.cpp - 全局配置管理实现
#include "Config.h"
#include "MeowMenu.h"

#include <ll/api/service/Bedrock.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace meowmenu {

namespace fs = std::filesystem;

// 数据目录（与 JS 版相同：Meowdata/MeowMenu/）
static constexpr const char* DATA_DIR = "Meowdata/MeowMenu";

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

void ConfigManager::load() {
    mConfigPath = std::string(DATA_DIR) + "/config.json";

    // 确保目录存在
    fs::create_directories(DATA_DIR);
    fs::create_directories(std::string(DATA_DIR) + "/data");

    if (!fs::exists(mConfigPath)) {
        // 写入默认配置
        save();
        lib().getSelf().getLogger().info("已生成默认配置: {}", mConfigPath);
        return;
    }

    try {
        std::ifstream ifs(mConfigPath);
        std::stringstream ss;
        ss << ifs.rdbuf();
        auto j = nlohmann::json::parse(ss.str());

        mConfig.displayMode = j.value("displayMode", "static");
        mConfig.useDogeUI   = j.value("useDogeUI", false);
        mConfig.mainMenuId  = j.value("mainMenuId", "main");
        mConfig.allowPlayerModeChoice = j.value("allowPlayerModeChoice", false);

        // triggerItem
        if (j.contains("triggerItem")) {
            auto const& ti = j["triggerItem"];
            mConfig.triggerItem.type        = ti.value("type", "minecraft:clock");
            mConfig.triggerItem.displayName = ti.value("displayName", "菜单");
            mConfig.triggerItem.snbt        = ti.value("snbt", "");
            mConfig.triggerItem.count       = ti.value("count", 1);
        }

        // giveUs
        if (j.contains("giveUs")) {
            auto const& gu = j["giveUs"];
            mConfig.giveUs.enabled    = gu.value("enabled", true);
            mConfig.giveUs.giveOnJoin  = gu.value("giveOnJoin", true);

            if (gu.contains("command")) {
                auto const& cmd = gu["command"];
                mConfig.giveUs.commandName  = cmd.value("name", "mmenu");
                mConfig.giveUs.commandAlias = cmd.value("alias", "");
                mConfig.giveUs.commandDesc  = cmd.value("description", "§a打开§b服务器§e菜单");
            }

            if (gu.contains("messages")) {
                auto const& msg = gu["messages"];
                mConfig.giveUs.msgReceived      = msg.value("received", mConfig.giveUs.msgReceived);
                mConfig.giveUs.msgAlreadyHave   = msg.value("alreadyHave", mConfig.giveUs.msgAlreadyHave);
                mConfig.giveUs.msgInventoryFull = msg.value("inventoryFull", mConfig.giveUs.msgInventoryFull);
                mConfig.giveUs.msgCleared       = msg.value("cleared", mConfig.giveUs.msgCleared);
            }
        }

        lib().getSelf().getLogger().info("配置加载完成: displayMode={}, mainMenuId={}",
                                         mConfig.displayMode, mConfig.mainMenuId);
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error("加载配置失败: {}，使用默认配置", e.what());
    }
}

void ConfigManager::save() {
    if (mConfigPath.empty()) {
        mConfigPath = std::string(DATA_DIR) + "/config.json";
    }

    try {
        nlohmann::json j;
        j["displayMode"]          = mConfig.displayMode;
        j["useDogeUI"]            = mConfig.useDogeUI;
        j["mainMenuId"]           = mConfig.mainMenuId;
        j["allowPlayerModeChoice"] = mConfig.allowPlayerModeChoice;

        j["triggerItem"]["type"]        = mConfig.triggerItem.type;
        j["triggerItem"]["displayName"] = mConfig.triggerItem.displayName;
        j["triggerItem"]["snbt"]        = mConfig.triggerItem.snbt;
        j["triggerItem"]["count"]       = mConfig.triggerItem.count;

        j["giveUs"]["enabled"]    = mConfig.giveUs.enabled;
        j["giveUs"]["giveOnJoin"]  = mConfig.giveUs.giveOnJoin;
        j["giveUs"]["command"]["name"]        = mConfig.giveUs.commandName;
        j["giveUs"]["command"]["alias"]       = mConfig.giveUs.commandAlias;
        j["giveUs"]["command"]["description"] = mConfig.giveUs.commandDesc;
        j["giveUs"]["messages"]["received"]      = mConfig.giveUs.msgReceived;
        j["giveUs"]["messages"]["alreadyHave"]   = mConfig.giveUs.msgAlreadyHave;
        j["giveUs"]["messages"]["inventoryFull"] = mConfig.giveUs.msgInventoryFull;
        j["giveUs"]["messages"]["cleared"]       = mConfig.giveUs.msgCleared;

        std::ofstream ofs(mConfigPath);
        ofs << j.dump(2);
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error("保存配置失败: {}", e.what());
    }
}

std::string ConfigManager::formatMessage(std::string const& template_) const {
    std::string result = template_;
    // 替换 {item}
    {
        std::string search = "{item}";
        size_t      pos    = 0;
        while ((pos = result.find(search, pos)) != std::string::npos) {
            result.replace(pos, search.size(), mConfig.triggerItem.displayName);
            pos += mConfig.triggerItem.displayName.size();
        }
    }
    // 替换 {cmd}
    {
        std::string search = "{cmd}";
        size_t      pos    = 0;
        while ((pos = result.find(search, pos)) != std::string::npos) {
            result.replace(pos, search.size(), mConfig.giveUs.commandName);
            pos += mConfig.giveUs.commandName.size();
        }
    }
    return result;
}

} // namespace meowmenu
