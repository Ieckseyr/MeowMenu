// PlayerRecord.h - 玩家进服发放记录（Meowdata/MeowMenu/players.json）
// 发放成功才记账, 之后进服完全跳过; 失败不记账、下次重试。
// 结构: {"version":1, "players":{名:{firstJoin, menuGiven, menuGivenAt}}}
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace meowmenu {

struct PlayerRecordEntry {
    long long firstJoin   = 0;     // 首次进服时间（unix 秒）
    bool      menuGiven   = false; // 菜单物品是否已发放成功（发放成功那一刻记账）
    long long menuGivenAt = 0;     // 发放时间（unix 秒）
};

class PlayerRecordStore {
public:
    static PlayerRecordStore& getInstance();

    // 读取 players.json（不存在则保持空；首次 save 时创建）
    void load();
    // 落盘（每次变更后立刻写，不依赖关服）
    void save();

    [[nodiscard]] bool                     hasRecord(std::string const& name) const;
    [[nodiscard]] bool                     menuGiven(std::string const& name) const;
    [[nodiscard]] PlayerRecordEntry const* find(std::string const& name) const;
    [[nodiscard]] std::size_t              size() const { return mPlayers.size(); }
    // 已发放人数（启动日志用）
    [[nodiscard]] std::size_t              givenCount() const;

    // 首次进服登记（已有记录则只补缺失字段）；返回是否是第一次见到这名玩家
    bool noteFirstJoin(std::string const& name, long long now);
    // 发放成功记账并立刻落盘
    void markMenuGiven(std::string const& name, long long now);

private:
    PlayerRecordStore() = default;

    std::string                                                 mPath;
    std::unordered_map<std::string, PlayerRecordEntry>          mPlayers;
};

} // namespace meowmenu
