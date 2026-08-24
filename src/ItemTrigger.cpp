// ItemTrigger.cpp - 自定义物品→菜单触发注册表实现
#include "ItemTrigger.h"
#include "MeowMenu.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace meowmenu {

namespace fs = std::filesystem;

static constexpr const char* DATA_DIR        = "Meowdata/MeowMenu";
static constexpr const char* TRIGGER_FILE    = "Meowdata/MeowMenu/item_triggers.json";

ItemTrigger& ItemTrigger::getInstance() {
    static ItemTrigger instance;
    return instance;
}

void ItemTrigger::initialize() {
    fs::create_directories(DATA_DIR);

    if (!fs::exists(TRIGGER_FILE)) {
        // 空注册表，写入空 JSON 对象
        save();
        return;
    }

    try {
        std::ifstream ifs(TRIGGER_FILE);
        std::stringstream ss;
        ss << ifs.rdbuf();
        auto j = nlohmann::json::parse(ss.str());

        std::unique_lock lock(mMutex);
        mTriggers.clear();
        if (j.is_object()) {
            for (auto const& [k, v] : j.items()) {
                if (v.is_string()) {
                    mTriggers[k] = v.get<std::string>();
                }
            }
        }
        lib().getSelf().getLogger().info("已加载 {} 个自定义物品触发绑定", mTriggers.size());
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error("加载 item_triggers.json 失败: {}", e.what());
    }
}

void ItemTrigger::save() {
    try {
        fs::create_directories(DATA_DIR);
        nlohmann::json j = nlohmann::json::object();

        {
            std::shared_lock lock(mMutex);
            for (auto const& [k, v] : mTriggers) {
                j[k] = v;
            }
        }

        std::ofstream ofs(TRIGGER_FILE);
        ofs << j.dump(2);
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error("保存 item_triggers.json 失败: {}", e.what());
    }
}

bool ItemTrigger::registerTrigger(std::string const& itemType, std::string const& menuId) {
    if (itemType.empty() || menuId.empty()) return false;

    {
        std::unique_lock lock(mMutex);
        auto it = mTriggers.find(itemType);
        if (it != mTriggers.end() && it->second == menuId) {
            // 幂等：绑定未变化
            return true;
        }
        mTriggers[itemType] = menuId;
    }
    save();
    return true;
}

bool ItemTrigger::unregisterTrigger(std::string const& itemType) {
    bool removed = false;
    {
        std::unique_lock lock(mMutex);
        removed = mTriggers.erase(itemType) > 0;
    }
    if (removed) save();
    return removed;
}

std::string ItemTrigger::findMenuId(std::string const& itemType) const {
    std::shared_lock lock(mMutex);
    auto it = mTriggers.find(itemType);
    if (it != mTriggers.end()) return it->second;
    return "";
}

std::string ItemTrigger::getAllTriggersJson() const {
    nlohmann::json j = nlohmann::json::object();
    {
        std::shared_lock lock(mMutex);
        for (auto const& [k, v] : mTriggers) {
            j[k] = v;
        }
    }
    return j.dump();
}

bool ItemTrigger::clearAll() {
    {
        std::unique_lock lock(mMutex);
        mTriggers.clear();
    }
    save();
    return true;
}

} // namespace meowmenu
