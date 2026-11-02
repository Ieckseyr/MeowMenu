// HologramLibAbi.h - HologramLib 原生 C++ 接口的 ABI 镜像（运行时连接, 非链接依赖）
//
// 为什么需要镜像而不是 #include "hologramlib/HologramLib.h":
//   HologramLib.h 里的 `HOLOGLIB_API static IHologramLib& getInstance()` 在非导出侧展开为
//   __declspec(dllimport)，一旦直接调用就会在 MeowMenu.dll 里留下导入项 —— HologramLib 会变成
//   **加载期硬前置**（缺库则 MeowMenu 整个加载失败）。本插件要求 HologramLib 是**可选前置**，
//   因此这里只镜像接口布局，入口符号在运行时用 GetProcAddress 解析。
//
// 契约:
//   · 虚函数**声明顺序**即 vtable 槽位顺序，必须与 HologramLib.h 完全一致（含析构函数）。
//     只在**尾部追加**是 HologramLib 的冻结契约（见其头文件 "冻结契约: 只在尾部追加"）。
//   · 结构体（NpcDialogSpec / NpcDialogButton / NpcDialogClickEvent / GhostInteractEvent /
//     PlayerNpcConfig / PlayerNpcSkin）按值/引用跨 DLL 传递，字段顺序与类型必须一致。
//   · 镜像来源: E:\111FWQ\xb26.40\plugins\HologramLib\include\hologramlib\HologramLib.h
//     （HologramLib 26.40.2 / HOLOGLIB_API_VERSION 0x011B00）
//   · 一致性由 tests/npc_abi/ 的探针测试校验（对比真实头文件与镜像的槽位顺序、结构体尺寸）。
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// 命名空间可改名: 仅供 tests/npc_abi 在同一编译单元里同时引用真实头文件与本镜像做静态校验。
// 生产代码不定义该宏, 因此就是 hologramlib。
#ifndef MEOW_HOLOGRAM_ABI_NAMESPACE
#define MEOW_HOLOGRAM_ABI_NAMESPACE hologramlib
#endif

namespace MEOW_HOLOGRAM_ABI_NAMESPACE {

// ── 只作为其他接口的返回类型出现，无需完整定义 ──
class IShapeDrawer;
class IHologramText;
class IItemDetail;
class IItemDisplay;
class ICustomEntity;
class IParticleShape;
class ITradeMenu;
class IContainerMenu;

// HologramLib.dll 导出的唯一 C++ 入口符号（dumpbin /exports 实测）
inline constexpr char const* kGetInstanceSymbol = "?getInstance@IHologramLib@hologramlib@@SAAEAV12@XZ";

// NPC 对话域所需的最低 API 版本。0x011B00 = 1.21.0 线（tradeMenus 加入线, npcDialogs 与之同线）
inline constexpr uint32_t kMinApiVersionForNpcDialog = 0x011B00;

// ─────────────────────────────────────────────
// 结构体（逐字段镜像）
// ─────────────────────────────────────────────

struct GhostInteractEvent {
    std::string playerName;   // 点击者（realName）
    int         action{0};    // 1=Interact 2=Attack 3=StopRiding 4=InteractUpdate 5=NpcOpen 6=OpenInventory
    std::string domain;       // "entity" / "itemDisplay" / "npc"
    int64_t     id{-1};       // 对应域的库内 id
    bool        hasPos{false};
    float       x{0}, y{0}, z{0};
};

struct PlayerNpcSkin {
    std::string pngPath{};
    std::string skinId{};
    std::string geometry{"geometry.humanoid.custom"};
    std::string armSize{"wide"};
    std::string geometryData{};
};

struct PlayerNpcConfig {
    std::string name{"NPC"};
    std::string skinId{"default"};
    float       x{0}, y{64}, z{0};
    int         dimension{0};
    float       yaw{0};
    double      viewDistance{96.0};
    float       scale{1.0f};
    bool        enabled{true};
};

struct NpcDialogButton {
    std::string              label;    // 按钮文本
    std::vector<std::string> commands; // 展示在按钮上的命令（合成 NPC 不由 BDS 代跑, 回传给调用方）
    std::string              actionId; // 服务器侧标识, 点击回传里原样带回
    int                      mode{0};  // 0=普通按钮 1=关闭 2=打开
};

struct NpcDialogSpec {
    std::string                  carrierIdentifier{"minecraft:npc"};
    std::string                  npcName{"NPC"};
    std::string                  sceneName{"main"};
    std::string                  dialogue;
    std::int64_t                 npcUniqueIdOverride{0};
    std::vector<NpcDialogButton> buttons;
    std::string                  rawActionJson;
};

struct NpcDialogClickEvent {
    std::string              playerName;
    int64_t                  dialogId{-1};
    std::string              sceneName;
    int                      buttonIndex{-1}; // -1 = 非按钮事件
    std::string              actionId;
    bool                     closed{false};
    std::vector<std::string> commands;
};

// ── 虚拟容器（箱子）界面（1.22.0 追加; 纯协议, 不依赖 GMLIB）──
struct ContainerMenuItem {
    std::string              type; // minecraft:xxx（空 = 该槽留空）
    int                      count{1};
    int                      damage{0};
    std::string              name; // 自定义名（空 = 无）
    std::vector<std::string> lore;
};

struct ContainerMenuSpec {
    int                            rows{3}; // 3 = 小箱子 27 格, 6 = 大箱子 54 格
    std::vector<ContainerMenuItem> items;   // 下标即槽位号
    // false（默认）= 客户端侧箱子方块 + 方块实体路径（实测客户端认这条）
    // true = 合成箱子矿车路径（实测客户端不弹界面, 仅对比排查用）
    bool                           useMinecart{false};
    // 箱子界面标题（方块实体的 CustomName）; 尾部追加字段
    std::string                    title{"虚拟容器"};
    // 摆好方块 + 方块实体之后, 等多少 tick 再发 ContainerOpen（客户端要先应用方块才绑得住）
    // 库实测: 7 能开、6 不行; 换设备/高负载可能要调大。尾部追加字段。
    int                            openDelayTicks{7};
};

struct ContainerClickEvent {
    std::string playerName;
    int64_t     menuId{-1};
    int         slot{-1};       // 被点击的槽位（= items 的下标）
    bool        closed{false};  // true = 界面被关闭（此时 slot = -1）
};

// ─────────────────────────────────────────────
// 接口（虚函数顺序即 vtable 槽位顺序）
// ─────────────────────────────────────────────

class INpcDialogue {
public:
    virtual ~INpcDialogue() = default;

    virtual int64_t open(std::string const& playerName, NpcDialogSpec const& spec) = 0;
    virtual bool    close(int64_t dialogId)                                       = 0;
    virtual void    closeAll()                                                    = 0;

    [[nodiscard]] virtual bool                 isOpen(int64_t dialogId) const = 0;
    [[nodiscard]] virtual std::vector<int64_t> getAllIds() const              = 0;

    virtual uint64_t addClickListener(std::function<void(NpcDialogClickEvent const&)> listener) = 0;
    virtual bool     removeClickListener(uint64_t token)                                        = 0;

    // ── 1.21.0 未发布线追加（冻结契约: 只在尾部追加）──
    // 就地换内容（翻页/进子菜单/返回）: 复用同一载体 NPC, 只重发一次 NpcDialoguePacket(Open),
    // 不删载体、不重建、不关界面 → 无闪烁无延迟。回调里调用即表示"本次点击已接管", 库不再自动拆。
    // 返回 false = 该对话已不在（调用方应改用 open）。
    virtual bool update(int64_t dialogId, NpcDialogSpec const& spec) = 0;
};

class IPlayerNpc {
public:
    virtual ~IPlayerNpc() = default;

    virtual bool                     registerSkin(PlayerNpcSkin const& skin)                       = 0;
    virtual bool                     captureSkin(std::string const& skinId, std::string const& playerName) = 0;
    virtual bool                     hasSkin(std::string const& skinId) const                      = 0;
    virtual bool                     unregisterSkin(std::string const& skinId)                     = 0;
    virtual std::vector<std::string> getSkinIds() const                                           = 0;

    virtual int64_t create(PlayerNpcConfig const& config)                        = 0;
    virtual int64_t createRandom(PlayerNpcConfig const& config)                  = 0;
    virtual int64_t createWithId(PlayerNpcConfig const& config, int64_t desiredId) = 0;
    virtual bool    destroy(int64_t id)                                          = 0;
    virtual void    destroyAll()                                                 = 0;
    virtual bool    exists(int64_t id) const                                     = 0;
    virtual bool    get(int64_t id, PlayerNpcConfig& out) const                   = 0;
    virtual bool    isIdUsed(int64_t id) const                                    = 0;
    virtual std::vector<int64_t> getAllIds() const                                = 0;

    virtual bool setPosition(int64_t id, float x, float y, float z, int dim) = 0;
    virtual bool setRotation(int64_t id, float yaw)                          = 0;
    virtual bool setNametag(int64_t id, std::string const& text)             = 0;
    virtual bool setSkin(int64_t id, std::string const& skinId)              = 0;
    virtual bool setViewDistance(int64_t id, double dist)                    = 0;
    virtual bool setScale(int64_t id, float scale)                           = 0;
    virtual bool setEnabled(int64_t id, bool enabled)                        = 0;

    virtual bool setVisiblePlayers(int64_t id, std::vector<std::string> const& playerNames) = 0;
    virtual bool clearVisiblePlayers(int64_t id)                                            = 0;
    virtual bool setVisiblePlayer(int64_t id, std::string const& playerName)                 = 0;

    virtual std::string getDebugInfo(int64_t id) const = 0;

    virtual int  importSkins(std::string const& dirPath)                                     = 0;
    virtual bool getSkinBlob(std::string const& skinId, std::string& out) const               = 0;
    virtual bool registerSkinFromBlob(std::string const& blob)                                = 0;

    virtual bool setRotationLight(int64_t id, float yaw)                                      = 0;
    virtual bool setPlayerRotation(int64_t id, std::string const& playerName, float yaw)       = 0;
    virtual bool clearPlayerRotation(int64_t id, std::string const& playerName)                = 0;
    virtual bool clearPlayerRotations(int64_t id)                                              = 0;
};

class IContainerMenu {
public:
    virtual ~IContainerMenu() = default;

    virtual int64_t open(std::string const& playerName, ContainerMenuSpec const& spec) = 0;
    virtual bool    close(int64_t menuId)                                             = 0;
    virtual void    closeAll()                                                        = 0;

    [[nodiscard]] virtual bool                 isOpen(int64_t menuId) const = 0;
    [[nodiscard]] virtual std::vector<int64_t> getAllIds() const             = 0;

    virtual uint64_t addClickListener(std::function<void(ContainerClickEvent const&)> listener) = 0;
    virtual bool     removeClickListener(uint64_t token)                                        = 0;

    // ── 1.21.0 未发布线追加（冻结契约: 只在尾部追加）──
    // 就地换内容（翻页/进子菜单/返回）: 复用同一个载体方块, 不拆界面、不重发方块、不等待 ——
    // 只重发一次方块实体 NBT（Items/标题）与 ContainerOpen。
    // 对比 open(): open 会先关旧菜单再重新摆方块 + 等 openDelayTicks, 每次都要付打开延迟。
    // 返回 false = 该菜单已不在（调用方应改用 open）。
    virtual bool update(int64_t menuId, ContainerMenuSpec const& spec) = 0;

    // 按槽动态刷新: 只改一个格子（发一条 InventorySlot）, 客户端就地换掉那一格 ——
    // 不重发方块实体、不重发 ContainerOpen、不重开界面, 无延迟无闪烁。
    // item.type 为空 = 清空该槽。返回 false = 菜单已不在或槽位越界。
    virtual bool setItem(int64_t menuId, int slot, ContainerMenuItem const& item) = 0;
};

// 库入口单例。getInstance 是 static（不占 vtable 槽位），由运行时 GetProcAddress 解析。
class IHologramLib {
public:
    virtual ~IHologramLib() = default;

    virtual IShapeDrawer&  shapes()      = 0;
    virtual IHologramText& holograms()   = 0;
    virtual IItemDetail&   itemDetails() = 0;

    virtual bool     isLseAvailable() = 0;
    virtual uint32_t version()        = 0;

    virtual IItemDisplay& itemDisplays() = 0;

    virtual int64_t findNearestItemDisplay(float x, float y, float z, int dim, double maxDist) = 0;

    virtual ICustomEntity& customEntities() = 0;

    virtual void                     setGhostInteractListener(std::function<void(GhostInteractEvent const&)> listener) = 0;
    virtual void                     clearGhostInteractListener()                                                     = 0;
    virtual std::vector<std::string> pollGhostInteractions()                                                          = 0;

    virtual IParticleShape& particleShapes() = 0;

    virtual IPlayerNpc& playerNpcs() = 0;

    virtual uint64_t addGhostInteractListener(std::function<void(GhostInteractEvent const&)> listener) = 0;
    virtual bool     removeGhostInteractListener(uint64_t token)                                       = 0;

    virtual ITradeMenu& tradeMenus() = 0;

    virtual INpcDialogue& npcDialogs() = 0;

    virtual IContainerMenu& containerMenus() = 0;
};

} // namespace MEOW_HOLOGRAM_ABI_NAMESPACE
