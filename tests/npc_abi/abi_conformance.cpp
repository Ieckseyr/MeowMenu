// abi_conformance.cpp - MeowMenu 的 HologramLib ABI 镜像一致性校验
//
// 为什么需要它: MeowMenu 不引用 HologramLib 的头文件、也不链接它（否则 HologramLib 就成了加载期
// 硬前置）。代价是镜像声明可能与真实接口漂移 —— 一旦漂移, 运行时按错误槽位调用虚函数会直接崩溃。
// 本文件把漂移变成**编译期错误**:
//   1. 结构体逐字段尺寸/偏移比对（跨 ABI 按值传递的结构必须完全一致）
//   2. 用镜像里的签名去实现真实接口, 每个方法都标 override —— 签名不一致则编译失败
// 另配套 check_abi.js 做"声明顺序"的文本比对（vtable 槽位顺序由声明顺序决定）。
//
// 编译运行: tests/npc_abi/run.bat

// 镜像先改名到 mirror 命名空间, 这样才能在同一编译单元里同时引用真实头文件
#define MEOW_HOLOGRAM_ABI_NAMESPACE mirror
#include "HologramLibAbi.h"

#include "hologramlib/HologramLib.h" // 真实头文件（仅测试用, 生产构建不引用）

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace real = hologramlib;

//================================================================
// 1. 结构体: 尺寸 + 关键字段偏移
//================================================================
static_assert(sizeof(mirror::GhostInteractEvent) == sizeof(real::GhostInteractEvent), "GhostInteractEvent 尺寸不一致");
static_assert(sizeof(mirror::PlayerNpcSkin) == sizeof(real::PlayerNpcSkin), "PlayerNpcSkin 尺寸不一致");
static_assert(sizeof(mirror::PlayerNpcConfig) == sizeof(real::PlayerNpcConfig), "PlayerNpcConfig 尺寸不一致");
static_assert(sizeof(mirror::NpcDialogButton) == sizeof(real::NpcDialogButton), "NpcDialogButton 尺寸不一致");
static_assert(sizeof(mirror::NpcDialogSpec) == sizeof(real::NpcDialogSpec), "NpcDialogSpec 尺寸不一致");
static_assert(sizeof(mirror::NpcDialogClickEvent) == sizeof(real::NpcDialogClickEvent), "NpcDialogClickEvent 尺寸不一致");

static_assert(
    offsetof(mirror::NpcDialogSpec, buttons) == offsetof(real::NpcDialogSpec, buttons),
    "NpcDialogSpec::buttons 偏移不一致"
);
static_assert(
    offsetof(mirror::NpcDialogSpec, dialogue) == offsetof(real::NpcDialogSpec, dialogue),
    "NpcDialogSpec::dialogue 偏移不一致"
);
static_assert(
    offsetof(mirror::NpcDialogSpec, sceneName) == offsetof(real::NpcDialogSpec, sceneName),
    "NpcDialogSpec::sceneName 偏移不一致"
);
static_assert(
    offsetof(mirror::NpcDialogSpec, npcName) == offsetof(real::NpcDialogSpec, npcName),
    "NpcDialogSpec::npcName 偏移不一致"
);
static_assert(
    offsetof(mirror::NpcDialogButton, label) == offsetof(real::NpcDialogButton, label),
    "NpcDialogButton::label 偏移不一致"
);
static_assert(
    offsetof(mirror::NpcDialogButton, actionId) == offsetof(real::NpcDialogButton, actionId),
    "NpcDialogButton::actionId 偏移不一致"
);
static_assert(
    offsetof(mirror::NpcDialogButton, mode) == offsetof(real::NpcDialogButton, mode),
    "NpcDialogButton::mode 偏移不一致"
);
static_assert(
    offsetof(mirror::NpcDialogClickEvent, actionId) == offsetof(real::NpcDialogClickEvent, actionId),
    "NpcDialogClickEvent::actionId 偏移不一致"
);
static_assert(
    offsetof(mirror::NpcDialogClickEvent, buttonIndex) == offsetof(real::NpcDialogClickEvent, buttonIndex),
    "NpcDialogClickEvent::buttonIndex 偏移不一致"
);
static_assert(
    offsetof(mirror::NpcDialogClickEvent, closed) == offsetof(real::NpcDialogClickEvent, closed),
    "NpcDialogClickEvent::closed 偏移不一致"
);
static_assert(
    offsetof(mirror::GhostInteractEvent, domain) == offsetof(real::GhostInteractEvent, domain),
    "GhostInteractEvent::domain 偏移不一致"
);
static_assert(
    offsetof(mirror::GhostInteractEvent, id) == offsetof(real::GhostInteractEvent, id),
    "GhostInteractEvent::id 偏移不一致"
);
static_assert(
    offsetof(mirror::PlayerNpcConfig, skinId) == offsetof(real::PlayerNpcConfig, skinId),
    "PlayerNpcConfig::skinId 偏移不一致"
);

// 虚拟容器（箱子 UI）
static_assert(sizeof(mirror::ContainerMenuItem) == sizeof(real::ContainerMenuItem), "ContainerMenuItem 尺寸不一致");
static_assert(sizeof(mirror::ContainerMenuSpec) == sizeof(real::ContainerMenuSpec), "ContainerMenuSpec 尺寸不一致");
static_assert(sizeof(mirror::ContainerClickEvent) == sizeof(real::ContainerClickEvent), "ContainerClickEvent 尺寸不一致");
static_assert(
    offsetof(mirror::ContainerMenuSpec, items) == offsetof(real::ContainerMenuSpec, items),
    "ContainerMenuSpec::items 偏移不一致"
);
static_assert(
    offsetof(mirror::ContainerMenuSpec, rows) == offsetof(real::ContainerMenuSpec, rows),
    "ContainerMenuSpec::rows 偏移不一致"
);
static_assert(
    offsetof(mirror::ContainerMenuSpec, useMinecart) == offsetof(real::ContainerMenuSpec, useMinecart),
    "ContainerMenuSpec::useMinecart 偏移不一致（尾部追加字段的契约被破坏）"
);
static_assert(
    offsetof(mirror::ContainerMenuSpec, title) == offsetof(real::ContainerMenuSpec, title),
    "ContainerMenuSpec::title 偏移不一致"
);
static_assert(
    offsetof(mirror::ContainerMenuSpec, openDelayTicks) == offsetof(real::ContainerMenuSpec, openDelayTicks),
    "ContainerMenuSpec::openDelayTicks 偏移不一致（尾部追加字段的契约被破坏）"
);
static_assert(
    offsetof(mirror::ContainerMenuItem, type) == offsetof(real::ContainerMenuItem, type),
    "ContainerMenuItem::type 偏移不一致"
);
static_assert(
    offsetof(mirror::ContainerMenuItem, lore) == offsetof(real::ContainerMenuItem, lore),
    "ContainerMenuItem::lore 偏移不一致"
);
static_assert(
    offsetof(mirror::ContainerClickEvent, slot) == offsetof(real::ContainerClickEvent, slot),
    "ContainerClickEvent::slot 偏移不一致"
);
static_assert(
    offsetof(mirror::ContainerClickEvent, closed) == offsetof(real::ContainerClickEvent, closed),
    "ContainerClickEvent::closed 偏移不一致"
);

//================================================================
// 2. 接口方法签名: 用镜像签名实现真实接口, 全部 override
//    —— 参数/返回类型/const 任一不一致都会编译失败
//================================================================

// INpcDialogue: MeowMenu 实际调用的接口, 逐方法校验
struct INpcDialogueProbe : real::INpcDialogue {
    int64_t open(std::string const& playerName, real::NpcDialogSpec const& spec) override;
    bool    close(int64_t dialogId) override;
    void    closeAll() override;
    bool                 isOpen(int64_t dialogId) const override;
    std::vector<int64_t> getAllIds() const override;
    uint64_t addClickListener(std::function<void(real::NpcDialogClickEvent const&)> listener) override;
    bool     removeClickListener(uint64_t token) override;
    bool     update(int64_t dialogId, real::NpcDialogSpec const& spec) override;
};

// IContainerMenu: 箱子 UI 模式调用的接口
struct IContainerMenuProbe : real::IContainerMenu {
    int64_t open(std::string const& playerName, real::ContainerMenuSpec const& spec) override;
    bool    close(int64_t menuId) override;
    void    closeAll() override;
    bool                 isOpen(int64_t menuId) const override;
    std::vector<int64_t> getAllIds() const override;
    uint64_t addClickListener(std::function<void(real::ContainerClickEvent const&)> listener) override;
    bool     removeClickListener(uint64_t token) override;
    bool     update(int64_t menuId, real::ContainerMenuSpec const& spec) override;
    bool     setItem(int64_t menuId, int slot, real::ContainerMenuItem const& item) override;
};

// IPlayerNpc: MeowMenu 生成/绑定 NPC 用到的方法
struct IPlayerNpcProbe : real::IPlayerNpc {
    bool                     registerSkin(real::PlayerNpcSkin const& skin) override;
    bool                     captureSkin(std::string const& skinId, std::string const& playerName) override;
    bool                     hasSkin(std::string const& skinId) const override;
    bool                     unregisterSkin(std::string const& skinId) override;
    std::vector<std::string> getSkinIds() const override;

    int64_t create(real::PlayerNpcConfig const& config) override;
    int64_t createRandom(real::PlayerNpcConfig const& config) override;
    int64_t createWithId(real::PlayerNpcConfig const& config, int64_t desiredId) override;
    bool    destroy(int64_t id) override;
    void    destroyAll() override;
    bool    exists(int64_t id) const override;
    bool    get(int64_t id, real::PlayerNpcConfig& out) const override;
    bool    isIdUsed(int64_t id) const override;
    std::vector<int64_t> getAllIds() const override;

    bool setPosition(int64_t id, float x, float y, float z, int dim) override;
    bool setRotation(int64_t id, float yaw) override;
    bool setNametag(int64_t id, std::string const& text) override;
    bool setSkin(int64_t id, std::string const& skinId) override;
    bool setViewDistance(int64_t id, double dist) override;
    bool setScale(int64_t id, float scale) override;
    bool setEnabled(int64_t id, bool enabled) override;

    bool setVisiblePlayers(int64_t id, std::vector<std::string> const& playerNames) override;
    bool clearVisiblePlayers(int64_t id) override;
    bool setVisiblePlayer(int64_t id, std::string const& playerName) override;

    std::string getDebugInfo(int64_t id) const override;

    int  importSkins(std::string const& dirPath) override;
    bool getSkinBlob(std::string const& skinId, std::string& out) const override;
    bool registerSkinFromBlob(std::string const& blob) override;

    bool setRotationLight(int64_t id, float yaw) override;
    bool setPlayerRotation(int64_t id, std::string const& playerName, float yaw) override;
    bool clearPlayerRotation(int64_t id, std::string const& playerName) override;
    bool clearPlayerRotations(int64_t id) override;
};

// IHologramLib: 入口接口 —— vtable 槽位顺序必须在 npcDialogs() 之前与真实接口逐一致
struct IHologramLibProbe : real::IHologramLib {
    real::IShapeDrawer&  shapes() override;
    real::IHologramText& holograms() override;
    real::IItemDetail&   itemDetails() override;

    bool     isLseAvailable() override;
    uint32_t version() override;

    real::IItemDisplay& itemDisplays() override;

    int64_t findNearestItemDisplay(float x, float y, float z, int dim, double maxDist) override;

    real::ICustomEntity& customEntities() override;

    void                     setGhostInteractListener(std::function<void(real::GhostInteractEvent const&)> listener) override;
    void                     clearGhostInteractListener() override;
    std::vector<std::string> pollGhostInteractions() override;

    real::IParticleShape& particleShapes() override;

    real::IPlayerNpc& playerNpcs() override;

    uint64_t addGhostInteractListener(std::function<void(real::GhostInteractEvent const&)> listener) override;
    bool     removeGhostInteractListener(uint64_t token) override;

    real::ITradeMenu& tradeMenus() override;

    real::INpcDialogue& npcDialogs() override;

    real::IContainerMenu& containerMenus() override;
};

int main() {
    // 走到这里说明上述 static_assert 全部成立、所有 override 都匹配真实接口
    return 0;
}
