// PlayerUiPref.cpp - 玩家自选的界面样式偏好实现
#include "PlayerUiPref.h"

#include "MeowMenu.h"

#include <mc/world/actor/player/Player.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace meowmenu {

namespace fs = std::filesystem;

namespace {
constexpr char const* kDataDir  = "Meowdata/MeowMenu";
constexpr char const* kPrefFile = "Meowdata/MeowMenu/player_ui.json";
} // namespace

PlayerUiPref& PlayerUiPref::getInstance() {
    static PlayerUiPref instance;
    return instance;
}

bool PlayerUiPref::isValidMode(std::string const& mode) {
    return mode == ui_mode::kAuto || mode == ui_mode::kForm || mode == ui_mode::kNpc || mode == ui_mode::kChest;
}

std::string PlayerUiPref::keyOf(Player const& player) {
    std::string xuid = player.getXuid();
    if (!xuid.empty()) return xuid;
    return player.getRealName(); // 兜底（理论上 xuid 总是可用）
}

void PlayerUiPref::initialize() {
    try {
        fs::create_directories(kDataDir);
        if (!fs::exists(kPrefFile)) {
            save();
            return;
        }
        std::ifstream     ifs(kPrefFile);
        std::stringstream ss;
        ss << ifs.rdbuf();
        auto j = nlohmann::json::parse(ss.str());

        std::unique_lock lock(mMutex);
        mPrefs.clear();
        if (j.is_object()) {
            for (auto const& [key, value] : j.items()) {
                if (value.is_string() && isValidMode(value.get<std::string>())) {
                    mPrefs[key] = value.get<std::string>();
                }
            }
        }
        lib().getSelf().getLogger().info("已加载 {} 条玩家界面样式偏好", mPrefs.size());
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error("读取 player_ui.json 失败: {}", e.what());
    }
}

std::string PlayerUiPref::get(Player const& player) const { return getByKey(keyOf(player)); }

std::string PlayerUiPref::getByKey(std::string const& key) const {
    std::shared_lock lock(mMutex);
    auto             it = mPrefs.find(key);
    if (it == mPrefs.end()) return ui_mode::kAuto;
    return it->second;
}

bool PlayerUiPref::set(Player const& player, std::string const& mode) { return setByKey(keyOf(player), mode); }

bool PlayerUiPref::setByKey(std::string const& key, std::string const& mode) {
    if (key.empty() || !isValidMode(mode)) return false;
    {
        std::unique_lock lock(mMutex);
        if (mode == ui_mode::kAuto) {
            mPrefs.erase(key); // auto 是默认值, 不占存储
        } else {
            mPrefs[key] = mode;
        }
    }
    save();
    return true;
}

std::string PlayerUiPref::getAllJson() const {
    nlohmann::json j = nlohmann::json::object();
    {
        std::shared_lock lock(mMutex);
        for (auto const& [key, mode] : mPrefs) {
            j[key] = mode;
        }
    }
    return j.dump(2);
}

void PlayerUiPref::save() {
    try {
        fs::create_directories(kDataDir);
        nlohmann::json j = nlohmann::json::object();
        {
            std::shared_lock lock(mMutex);
            for (auto const& [key, mode] : mPrefs) {
                j[key] = mode;
            }
        }
        std::ofstream ofs(kPrefFile);
        ofs << j.dump(2);
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error("写入 player_ui.json 失败: {}", e.what());
    }
}

} // namespace meowmenu
