// PlayerRecord.cpp - 玩家进服发放记录实现
#include "PlayerRecord.h"
#include "MeowMenu.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace meowmenu {

namespace fs = std::filesystem;

// 与 Config.cpp 的 DATA_DIR 一致（Meowdata/MeowMenu/）
static constexpr char const* kDataDir      = "Meowdata/MeowMenu";
static constexpr char const* kFileName     = "players.json";
static constexpr int         kFileVersion  = 1;

PlayerRecordStore& PlayerRecordStore::getInstance() {
    static PlayerRecordStore instance;
    return instance;
}

void PlayerRecordStore::load() {
    mPath = std::string(kDataDir) + "/" + kFileName;
    mPlayers.clear();

    try {
        if (!fs::exists(mPath)) {
            // 首次运行：不立刻建文件，等第一名玩家进服记账时再写
            return;
        }

        std::ifstream ifs(mPath);
        std::stringstream ss;
        ss << ifs.rdbuf();
        auto j = nlohmann::json::parse(ss.str());

        if (j.contains("players") && j["players"].is_object()) {
            for (auto it = j["players"].begin(); it != j["players"].end(); ++it) {
                PlayerRecordEntry e;
                auto const& v = it.value();
                if (v.is_object()) {
                    e.firstJoin   = v.value("firstJoin", 0LL);
                    e.menuGiven   = v.value("menuGiven", false);
                    e.menuGivenAt = v.value("menuGivenAt", 0LL);
                }
                mPlayers[it.key()] = e;
            }
        }

        lib().getSelf().getLogger().info(
            "进服发放记录已加载: {} 名玩家 ({} 名已发放菜单物品) ← {}",
            size(),
            givenCount(),
            mPath
        );
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().warn("进服发放记录读取失败 ({}): {}", mPath, e.what());
    }
}

void PlayerRecordStore::save() {
    try {
        fs::create_directories(kDataDir);

        nlohmann::json j;
        j["version"] = kFileVersion;
        auto players = nlohmann::json::object();
        for (auto const& [name, e] : mPlayers) {
            players[name] = {
                {"firstJoin", e.firstJoin},
                {"menuGiven", e.menuGiven},
                {"menuGivenAt", e.menuGivenAt},
            };
        }
        j["players"] = std::move(players);

        std::ofstream ofs(mPath, std::ios::trunc);
        ofs << j.dump(2) << std::endl;
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().warn("进服发放记录写入失败 ({}): {}", mPath, e.what());
    }
}

bool PlayerRecordStore::hasRecord(std::string const& name) const {
    return mPlayers.find(name) != mPlayers.end();
}

bool PlayerRecordStore::menuGiven(std::string const& name) const {
    auto it = mPlayers.find(name);
    return it != mPlayers.end() && it->second.menuGiven;
}

PlayerRecordEntry const* PlayerRecordStore::find(std::string const& name) const {
    auto it = mPlayers.find(name);
    return it == mPlayers.end() ? nullptr : &it->second;
}

std::size_t PlayerRecordStore::givenCount() const {
    std::size_t n = 0;
    for (auto const& [name, e] : mPlayers) {
        if (e.menuGiven) ++n;
    }
    return n;
}

bool PlayerRecordStore::noteFirstJoin(std::string const& name, long long now) {
    auto it = mPlayers.find(name);
    if (it != mPlayers.end()) {
        // 老记录：只补缺失字段（例如早期只有 menuGiven 没有 firstJoin）
        if (it->second.firstJoin <= 0) {
            it->second.firstJoin = now;
            save();
        }
        return false;
    }

    PlayerRecordEntry e;
    e.firstJoin = now;
    mPlayers[name] = e;
    save(); // 首次登记立刻落盘：即使之后发放失败，也不会重复记"首次进服"
    return true;
}

void PlayerRecordStore::markMenuGiven(std::string const& name, long long now) {
    auto& e      = mPlayers[name];
    e.menuGiven  = true;
    e.menuGivenAt = now;
    if (e.firstJoin <= 0) e.firstJoin = now;
    save();
}

} // namespace meowmenu
