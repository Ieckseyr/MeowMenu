// NpcMenuBridge.h - HologramLib 界面桥接（可选前置）
//
// 承载 HologramLib 提供的两种界面样式的渲染与点击路由:
//   · NPC 对话（INpcDialogue）    —— 菜单呈现成原版 NPC 对话框
//   · 箱子界面（IContainerMenu）  —— 菜单呈现成虚拟容器（物品格 = 按钮）
// （类名沿用了最初只做 NPC 时的名字; 职责已扩展为"所有 HologramLib 界面"）
//
// 作用: 把 MeowMenu 的菜单以 HologramLib 的 NPC 对话界面呈现, 并把 NPC 点击绑定到菜单。
//
// 前置关系:
//   HologramLib 是**可选前置** —— 通过 ABI 运行时连接（GetProcAddress 解析库导出的
//   getInstance 符号）, 不参与链接, 因此：
//     · 未安装 HologramLib → 本模块整体静默禁用, MeowMenu 其余功能（表单菜单/编辑器/API）完全不受影响
//     · 已安装且版本达标 → NPC 对话菜单、NPC 点击开菜单、NPC 生成/绑定/持久化全部可用
//
// 功能:
//   · openDialogue     把菜单渲染成 NPC 对话（多层级: 子菜单在同一界面里逐层推进, 可返回上一级）
//   · 按钮执行         复用 ButtonHandler（command/admin_command 真执行, 支持 PAPI/{player} 占位符）
//   · spawnNpcNear/At  生成一只"点击即打开指定菜单"的 NPC（纯协议假玩家, 不占实体系统）
//   · 持久化           Meowdata/MeowMenu/npc_menus.json 保存 NPC 位置/绑定/皮肤 blob,
//                      重启后自动重建（HologramLib 不落盘, 持久化是消费方职责）
//
// 线程: 监听回调由 HologramLib 在网络处理路径（主线程）回调, 与 BDS 命令执行同线程,
//       故这里直接同步处理（执行命令 → 需要主线程）。
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

    // 运行时连接 HologramLib、注册点击/交互监听、重建已持久化的 NPC
    // 前置缺失或版本不足时安全退出（只记一条日志, 不抛异常）
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

    //===== NPC 对话菜单 =====
    // 以 NPC 对话形式打开菜单（menuId 为空 = 配置的主菜单）
    // npcName 为空时用菜单配置的 npc.name（再空则用菜单标题）
    // 注意: 在"对话按钮点击回调"里必须用 openDialogueDeferred, 不能用这个（原因见 .cpp）
    bool openDialogue(Player& player, std::string const& menuId, std::string const& npcName = "");
    // 延迟到下一 tick 再打开（点按钮回调专用; 库里在回调返回后才会拆掉旧对话与载体）
    bool openDialogueDeferred(Player& player, std::string const& menuId, std::string const& npcName = "");
    // 关闭该玩家当前对话
    bool closeDialogue(Player& player);
    // 返回上一级（无上级时忽略）
    bool backDialogue(Player& player);

    //===== 箱子界面（虚拟容器; 需 HologramLib）=====
    // 箱子域是否可用
    [[nodiscard]] bool isChestAvailable() const;
    // 以箱子界面打开菜单（menuId 空 = 主菜单）; 点按钮回调内请用 openChestMenuDeferred
    bool openChestMenu(Player& player, std::string const& menuId);
    // 延迟一 tick 打开（点按钮回调专用, 时序见 .cpp 说明）
    bool openChestMenuDeferred(Player& player, std::string const& menuId);
    // 关闭该玩家的箱子界面
    bool closeChestMenu(Player& player);

    // 按槽刷新当前箱子界面的某一格（无延迟无闪烁; itemType 空 = 清空该格）
    bool refreshChestSlot(Player& player, int slot, std::string const& itemType, std::string const& name);

    //===== NPC 生成与绑定 =====
    // 在操作者身前 2 格生成可点击 NPC（皮肤自动采集操作者本人; 持久化）
    // 返回库内 NPC id, <0 失败（-1 前置/参数问题, 其余为 HologramLib 原生错误码: -2 id 占用, -3 皮肤未注册）
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

    // ── 对话渲染 ──
    // 用菜单数据构建对话框规格; 按钮 actionId 编码为 "menu:<菜单id>#<按钮下标>"
    // outIndexMap: 下发的对话框按钮下标 → 菜单按钮下标（-1 = 返回等附加按钮）,
    //              用于 actionId 未被回填时按 buttonIndex 兜底定位
    // player: 非空时对 NPC 名/话语/按钮文本做 MeowPAPI 占位符与 {player} 替换（与表单路径一致）
    // page: 入参为请求页, 返回被夹紧后的实际页; outPageCount: 分页时的总页数
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
    // 按钮触发来源（决定"重推当前层"用哪种界面, 以及是否需要延迟）
    // Direct = 非点击回调（脚本/API 触发）; 另两种处于点击回调内, 重推界面需延迟
    enum class ClickSource { Direct, NpcDialog, Container };

    // 执行某个按钮：子菜单类 → 下一层界面；其余 → ButtonHandler
    bool executeMenuButton(Player& player, std::string const& menuId, int buttonIndex, ClickSource source);

    // ── 箱子界面 ──
    // 槽位 → 菜单按钮下标（-1 = 返回上一级, -2 = 上一页, -3 = 下一页, -4 = 空槽）
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
    // inPlace = true 时: 若该玩家已有活着的箱子, 走 update() 就地刷新内容（无延迟无闪烁）,
    //            失败或没有活箱子才走 open()（首次打开会付库的打开节奏）
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
