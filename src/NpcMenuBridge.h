// NpcMenuBridge.h - HologramLib 界面桥接（可选前置）
//
// 把菜单渲染成 HologramLib 的两种界面（NPC 对话 / 虚拟容器）并路由点击; NPC 生成/绑定/持久化也在这里。
// 库经 ABI 运行时连接（GetProcAddress, 不参与链接）: 没装库 → 本模块整体禁用, 其余功能不受影响。
#pragma once

#include "HologramLibAbi.h"

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

// 前向声明 BDS Player 类（位于全局命名空间）
class Player;

namespace meowmenu {

struct Menu;

class NpcMenuBridge {
public:
    static NpcMenuBridge& getInstance();

    // 运行时连接 HologramLib、注册监听、重建持久化 NPC（缺库/版本不足就安全退出）
    void initialize();

    // 移除监听并销毁本插件创建的 NPC（绑定数据保留, 下次 enable 重建）
    void shutdown();

    // HologramLib 是否已连接且版本达标（NPC 对话可用）
    [[nodiscard]] bool isAvailable() const;
    // NPC 生成能力是否可用（playerNpcs 域）
    [[nodiscard]] bool canManageNpc() const;

    // 人类可读状态（命令与日志用）
    [[nodiscard]] std::string statusText() const;
    // 已绑定 NPC 的 JSON 列表
    [[nodiscard]] std::string listNpcsJson() const;

    //===== NPC 对话菜单（按钮回调里必须用 openDialogueDeferred, 原因见 .cpp）=====
    bool openDialogue(Player& player, std::string const& menuId, std::string const& npcName = "");
    // 延迟到下一 tick 再打开（点按钮回调专用; 库里在回调返回后才会拆掉旧对话与载体）
    bool openDialogueDeferred(Player& player, std::string const& menuId, std::string const& npcName = "");
    // 关闭该玩家当前对话
    bool closeDialogue(Player& player);
    // 返回上一级（无上级时忽略）
    bool backDialogue(Player& player);

    //===== 箱子界面（虚拟容器）=====
    [[nodiscard]] bool isChestAvailable() const;
    // 以箱子界面打开菜单（menuId 空 = 主菜单）; 点按钮回调内请用 openChestMenuDeferred
    bool openChestMenu(Player& player, std::string const& menuId);
    // 延迟一 tick 打开（点按钮回调专用, 时序见 .cpp 说明）
    bool openChestMenuDeferred(Player& player, std::string const& menuId);
    // 关闭该玩家的箱子界面
    bool closeChestMenu(Player& player);

    // 按槽刷新当前箱子界面的某一格（无延迟无闪烁; itemType 空 = 清空该格）
    bool refreshChestSlot(Player& player, int slot, std::string const& itemType, std::string const& name);

    //===== NPC 生成与绑定（身前 2 格, 采集操作者皮肤; 返回库内 NPC id, <0 失败）=====
    int64_t spawnNpcNear(Player& opPlayer, std::string const& npcName, std::string const& menuId);
    // 在指定坐标生成（脚本/恢复用）。skinSourcePlayer 非空且皮肤未注册时尝试采集该玩家皮肤
    int64_t spawnNpcAt(
        std::string const& npcName,
        std::string const& menuId,
        float              x,
        float              y,
        float              z,
        int                dim,
        std::string const& skinSourcePlayer = ""
    );
    // 删除 NPC 及其绑定
    bool removeNpc(int64_t npcId);
    // 把（任意来源的）HologramLib NPC 绑定到菜单并持久化
    bool bindNpc(int64_t npcId, std::string const& menuId);
    // 重新读取 npc_menus.json: 销毁自建 NPC 后按文件重建
    bool reloadBindings();

private:
    NpcMenuBridge() = default;

    // ── 持久化的 NPC 记录 ──
    struct NpcBinding {
        int64_t     id{-1};        // HologramLib 库内 id
        std::string name;          // NPC 显示名
        std::string menuId;        // 点击后打开的菜单
        float       x{0}, y{64}, z{0};
        int         dim{0};
        std::string skinId;        // 皮肤注册名
        std::string skinBlobB64;   // 皮肤全字段快照（base64）, 重启后 registerSkinFromBlob 恢复
    };

    // ── 前置连接 ──
    void connectHologramLib();

    // ── 监听回调（HologramLib → 本插件）──
    void onDialogClick(hologramlib::NpcDialogClickEvent const& event);
    void onGhostInteract(hologramlib::GhostInteractEvent const& event);

    // 构建对话框规格; outIndexMap: 下发的按钮下标 → 菜单按钮下标（-1 = 附加按钮）
    hologramlib::NpcDialogSpec buildSpec(
        Menu const&        menu,
        bool               withBack,
        std::string const& npcName,
        std::vector<int>&  outIndexMap,
        Player*            player,
        int&               page,
        int&               outPageCount
    ) const;
    // 打开一层对话（不含历史入栈/出栈处理）; 有活着的对话时优先 INpcDialogue::update 就地换内容
    bool renderScene(Player& player, std::string const& menuId, std::string const& npcName = "");
    // 对话分页跳转（delta = +1 / -1）; 就地刷新不出界面
    void jumpDialoguePage(Player& player, int delta);
    // 把 renderScene 推迟到下一 tick（点按钮回调内必须走这里, 见 .cpp 中的时序说明）
    void scheduleRenderScene(std::string const& playerName, std::string const& menuId, std::string const& npcName = "");
    // 按钮触发来源（决定重推界面用哪种样式、要不要延迟）
    enum class ClickSource { Direct, NpcDialog, Container };

    // 执行某个按钮：子菜单类 → 下一层界面；其余 → ButtonHandler
    bool executeMenuButton(Player& player, std::string const& menuId, int buttonIndex, ClickSource source);

    // ── 箱子界面: 槽位 → 菜单按钮下标（-1 返回/-2 上页/-3 下页/-4 空槽） ──
    struct ChestScene {
        std::vector<std::string> ancestors; // 上级菜单栈
        std::vector<int>         slotMap;   // 最近一次下发的槽位映射
        std::string              menuId;    // 当前菜单
        int                      page{0};
        int                      rows{0};          // 当前箱子已按几行打开（就地刷新要求行数不变）
        int64_t                  containerId{-1}; // 库内容器菜单 id
    };

    [[nodiscard]] bool isChestMode(Menu const& menu) const;
    // 构建箱子规格（同时写回 slotMap / page）
    hologramlib::ContainerMenuSpec buildChestSpec(Menu const& menu, bool withBack, Player* player, ChestScene& out) const;
    // inPlace = true: 有活箱子就 update() 就地刷新, 否则 open()
    bool                           renderChestScene(Player& player, std::string const& menuId, int page, bool inPlace = true);

    void scheduleChestScene(std::string const& playerName, std::string const& menuId, int page);
    void onContainerClick(hologramlib::ContainerClickEvent const& event);
    void pushChestScene(std::string const& playerName, std::string const& menuId);
    bool popChestScene(std::string const& playerName, std::string& outMenuId);

    // ── 每玩家对话状态（上级栈 / 下标映射 / NPC 名 / 分页 / 当前 dialogId）──
    struct SceneState {
        std::vector<std::string> ancestors;      // 上级菜单栈（栈顶 = 返回目标）
        std::vector<int>         indexMap;       // 最近一次下发按钮 → 菜单按钮下标
        std::string              npcName;        // 当前对话的 NPC 显示名
        std::string              menuId;         // 当前层菜单
        int                      page{0};        // 当前页（分页时）
        int64_t                  dialogId{-1};   // 当前占用的 library dialogId（close/update 用）
    };

    void       pushScene(std::string const& playerName, std::string const& menuId);
    bool       popScene(std::string const& playerName, std::string& outMenuId);

    // ── 持久化 ──
    void loadBindings();
    void saveBindings();
    void restoreNpcs();      // 依据 mBindings 重建 NPC（启动时）
    void destroyOwnedNpcs(); // 销毁本插件创建的全部 NPC
    // 查询绑定（拷贝返回, 避免持锁外使用内部指针）
    bool lookupBinding(int64_t npcId, NpcBinding& out) const;
    // 仅供持锁场景内部使用
    NpcBinding*       findBindingLocked(int64_t npcId);
    NpcBinding const* findBindingLocked(int64_t npcId) const;

    // ── 前置句柄（仅当连接成功时非空）──
    hologramlib::IHologramLib*   mApi{nullptr};
    hologramlib::INpcDialogue*   mDialogs{nullptr};
    hologramlib::IPlayerNpc*     mNpcs{nullptr};
    hologramlib::IContainerMenu* mContainers{nullptr};
    uint32_t                     mApiVersion{0};
    uint64_t                     mClickToken{0};
    uint64_t                     mGhostToken{0};
    uint64_t                     mContainerClickToken{0};

    mutable std::shared_mutex                    mMutex;
    std::unordered_map<std::string, SceneState>  mScenes;      // realName -> 对话状态
    std::unordered_map<std::string, ChestScene>  mChestScenes; // realName -> 箱子状态
    std::vector<NpcBinding>                      mBindings;    // 持久化的 NPC 绑定
};

} // namespace meowmenu
