// NpcMenuBridge.cpp - HologramLib NPC 对话菜单桥接实现
//
// 三个关键点:
//   1. 前置是"可选"的: 只做 GetModuleHandle/LoadLibrary + GetProcAddress, 不链接 HologramLib,
//      因此 MeowMenu.dll 的导入表里没有 HologramLib —— 缺库时插件照常加载, 仅本模块禁用。
//   2. 菜单 → NPC 对话: 菜单按钮映射成 NpcDialogButton, actionId 里编码"菜单id#按钮下标";
//      点击回传后按 actionId 精确定位, 再走既有 ButtonHandler 执行（命令真执行、子菜单逐层推进）。
//   3. 持久化: HologramLib 明确不落盘（"持久化由消费方负责"）, 故这里保存 NPC 位置/绑定/皮肤快照。
#include "NpcMenuBridge.h"

#include "ButtonHandler.h"
#include "Config.h"
#include "MenuData.h"
#include "MenuRenderer.h"
#include "MenuStore.h"
#include "MeowMenu.h"
#include "PlayerUiPref.h"

#include "meowpapi/PlaceholderApi.h"

#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"

#include <mc/world/actor/player/Player.h>
#include <mc/world/level/Level.h>

#include <nlohmann/json.hpp>

#include <fmt/format.h>

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace meowmenu {

namespace fs = std::filesystem;

namespace {

constexpr char const* kDataDir       = "Meowdata/MeowMenu";
constexpr char const* kBindFile      = "Meowdata/MeowMenu/npc_menus.json";
constexpr char const* kActionBack    = "@back";
constexpr char const* kActionClose   = "@close";
constexpr char const* kMenuActionTag = "menu:";
constexpr size_t      kMaxBodyChars  = 700; // 对话框正文上限（过长客户端界面显示不全）
constexpr size_t      kMaxLabelChars = 60;  // 按钮文本上限
// 原版 NPC 对话框的按钮槽位上限。菜单自己的按钮最多这么多;
// 从上一层进入时若还有空槽才追加「返回上一级」。
constexpr size_t      kMaxDialogButtons = 6;
// NPC 对话框分页: 可点击按钮超过 6 个才分页。分页时每页内容项: 普通菜单 4, 主菜单 5
// （余下槽位放导航: 普通菜单 2 格 = 首格上一页 / 末格下一页; 主菜单只剩 1 格, 优先下一页,
//   最后一页显示上一页 —— 6 格里放不下 5 内容 + 双向导航, 这是取舍）
constexpr size_t      kPageSizeNormal = 4;
constexpr size_t      kPageSizeMain   = 5;
constexpr char const* kActionPrev     = "@prev";
constexpr char const* kActionNext     = "@next";

// 编辑器是单行输入, 写不了真换行 —— 把字面量 \n 当作换行处理
std::string expandEscapedNewlines(std::string const& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '\\' && i + 1 < in.size() && (in[i + 1] == 'n' || in[i + 1] == 'N')) {
            out.push_back('\n');
            ++i;
            continue;
        }
        out.push_back(in[i]);
    }
    return out;
}

// § 格式码在 UTF-8 文本里是两字节 (C2 A7); 也存在单字节 0xA7 的老写法。
// 返回需要跳过的字节数（0 = 当前位置不是格式码）: § 本身 + 其后的格式字符。
size_t formatCodeLength(unsigned char const* p, size_t remaining) {
    if (remaining >= 2 && p[0] == 0xC2 && p[1] == 0xA7) {
        return remaining >= 3 ? 3 : 2; // C2 A7 + 格式字符（结尾只剩 § 时只丢这两字节）
    }
    if (p[0] == 0xA7) {
        return remaining >= 2 ? 2 : 1;
    }
    return 0;
}

// NPC 对话框是原版界面, 不解析 § 颜色码; 保留会原样显示 "§l§6xxx", 故下发前剥离
std::string stripFormatCodes(std::string const& in) {
    std::string out;
    out.reserve(in.size());
    size_t i = 0;
    while (i < in.size()) {
        size_t skip = formatCodeLength(reinterpret_cast<unsigned char const*>(in.data()) + i, in.size() - i);
        if (skip) {
            i += skip;
            continue;
        }
        out.push_back(in[i]);
        ++i;
    }
    return out;
}

// 按字节上限截断, 但不切断 UTF-8 序列
// （截出半个多字节字符 → 客户端显示乱码, nlohmann 序列化抛 type_error.316, 存档失败）
std::string truncateUtf8(std::string const& s, size_t maxBytes) {
    if (s.size() <= maxBytes) return s;
    size_t cut = maxBytes;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut; // 10xxxxxx = 续字节
    return s.substr(0, cut);
}

// 与表单路径一致: 先走 MeowPAPI 占位符（%name% / {name}）, 再兜底替换 {player}
std::string translateForPlayer(std::string const& text, Player* player) {
    if (!player || text.empty()) return text;
    std::string out = meowpapi::PlaceholderApi::getInstance().translateStringWithPlayer(text, player);

    std::string playerName = player->getNameTag();
    std::string token      = "{player}";
    size_t      pos        = 0;
    while ((pos = out.find(token, pos)) != std::string::npos) {
        out.replace(pos, token.size(), playerName);
        pos += playerName.size();
    }
    return out;
}

// 压成单行并限长
std::string oneLine(std::string const& in, size_t maxChars) {
    std::string out;
    out.reserve(in.size());
    bool lastWasSpace = false;
    for (char c : in) {
        char ch = (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
        if (ch == ' ') {
            if (lastWasSpace) continue;
            lastWasSpace = true;
        } else {
            lastWasSpace = false;
        }
        out.push_back(ch);
    }
    // 去首尾空白
    size_t b = out.find_first_not_of(' ');
    if (b == std::string::npos) return "";
    size_t e = out.find_last_not_of(' ');
    out = out.substr(b, e - b + 1);

    if (out.size() > maxChars) {
        out = truncateUtf8(out, maxChars > 3 ? maxChars - 3 : maxChars);
        out += "...";
    }
    return out;
}

// actionId 编解码: "menu:<menuId>#<index>"
std::string makeMenuAction(std::string const& menuId, int index) {
    return std::string(kMenuActionTag) + menuId + "#" + std::to_string(index);
}

bool parseMenuAction(std::string const& action, std::string& outMenuId, int& outIndex) {
    if (action.rfind(kMenuActionTag, 0) != 0) return false;
    auto hash = action.rfind('#');
    if (hash == std::string::npos || hash <= std::string(kMenuActionTag).size()) return false;
    try {
        outIndex = std::stoi(action.substr(hash + 1));
    } catch (...) {
        return false;
    }
    outMenuId = action.substr(std::string(kMenuActionTag).size(), hash - std::string(kMenuActionTag).size());
    return !outMenuId.empty();
}

// ── base64: 皮肤 blob 是二进制, 存 JSON 必须先编码 ──
std::string base64Encode(std::string const& in) {
    static char const* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string        out;
    out.reserve(((in.size() + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        unsigned v = (static_cast<unsigned char>(in[i]) << 16) | (static_cast<unsigned char>(in[i + 1]) << 8)
                     | static_cast<unsigned char>(in[i + 2]);
        out.push_back(tbl[(v >> 18) & 0x3F]);
        out.push_back(tbl[(v >> 12) & 0x3F]);
        out.push_back(tbl[(v >> 6) & 0x3F]);
        out.push_back(tbl[v & 0x3F]);
    }
    if (i + 1 == in.size()) {
        unsigned v = static_cast<unsigned char>(in[i]) << 16;
        out.push_back(tbl[(v >> 18) & 0x3F]);
        out.push_back(tbl[(v >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (i + 2 == in.size()) {
        unsigned v = (static_cast<unsigned char>(in[i]) << 16) | (static_cast<unsigned char>(in[i + 1]) << 8);
        out.push_back(tbl[(v >> 18) & 0x3F]);
        out.push_back(tbl[(v >> 12) & 0x3F]);
        out.push_back(tbl[(v >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

bool base64Decode(std::string const& in, std::string& out) {
    auto decodeChar = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };

    out.clear();
    out.reserve((in.size() / 4) * 3);
    int  buffer = 0;
    int  bits   = 0;
    for (char c : in) {
        if (c == '=' ) break;
        if (c == '\n' || c == '\r' || c == ' ') continue;
        int v = decodeChar(c);
        if (v < 0) return false;
        buffer = (buffer << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buffer >> bits) & 0xFF));
        }
    }
    return true;
}

// NPC 名字清洗: 它会作为假玩家的 PlayerList 名字, 不能带 § 颜色码/换行
// （必须按 UTF-8 处理: § 是 C2 A7 两字节, 只按 0xA7 处理会留下半个字节 → 非法 UTF-8）
std::string sanitizeNpcName(std::string const& in) {
    std::string name;
    name.reserve(in.size());
    size_t i = 0;
    while (i < in.size()) {
        size_t skip = formatCodeLength(reinterpret_cast<unsigned char const*>(in.data()) + i, in.size() - i);
        if (skip) {
            i += skip;
            continue;
        }
        char c = in[i];
        if (c == '\n' || c == '\r' || c == '\t') {
            name.push_back(' ');
        } else {
            name.push_back(c);
        }
        ++i;
    }
    size_t b = name.find_first_not_of(' ');
    if (b == std::string::npos) return "";
    size_t e = name.find_last_not_of(' ');
    name     = name.substr(b, e - b + 1);
    return truncateUtf8(name, 24);
}

Player* findPlayerByName(std::string const& name) {
    auto level = ll::service::getLevel();
    if (!level.has_value()) return nullptr;
    return level->getPlayer(name);
}

// HologramLib 已卸载时不能再碰它的 vtable（插件卸载顺序不可控, 这里做最后一道保险）
bool hologramModuleLoaded() {
    return ::GetModuleHandleW(L"HologramLib.dll") != nullptr;
}

} // namespace

NpcMenuBridge& NpcMenuBridge::getInstance() {
    static NpcMenuBridge instance;
    return instance;
}

bool NpcMenuBridge::isAvailable() const { return mDialogs != nullptr; }

bool NpcMenuBridge::canManageNpc() const { return mNpcs != nullptr; }

//================================================================
// 前置连接（可选前置: 失败只记日志, 不影响插件其他功能）
//================================================================
void NpcMenuBridge::initialize() {
    if (mApi) return;

    connectHologramLib();
    if (!mApi) return;

    loadBindings();
    restoreNpcs();
}

void NpcMenuBridge::connectHologramLib() {
    auto& logger = lib().getSelf().getLogger();

    HMODULE module = ::GetModuleHandleW(L"HologramLib.dll");
    if (!module) {
        // 前置没装: 不是错误, 属于"可选前置缺失"的正常路径
        module = ::LoadLibraryW(L"HologramLib.dll");
    }
    if (!module) {
        logger.info("[MeowMenu] 未检测到 HologramLib → NPC 菜单功能已禁用（可选前置, 其余功能不受影响）");
        return;
    }

    auto getInstance = reinterpret_cast<hologramlib::IHologramLib& (*)()>(
        reinterpret_cast<void*>(::GetProcAddress(module, hologramlib::kGetInstanceSymbol))
    );
    if (!getInstance) {
        logger.warn(
            "[MeowMenu] HologramLib.dll 未导出 ABI 符号 {} → NPC 菜单功能已禁用",
            hologramlib::kGetInstanceSymbol
        );
        return;
    }

    hologramlib::IHologramLib* api = nullptr;
    try {
        api = &getInstance();
    } catch (...) {
        logger.error("[MeowMenu] 调用 HologramLib::getInstance 失败 → NPC 菜单功能已禁用");
        return;
    }
    if (!api) {
        logger.warn("[MeowMenu] HologramLib::getInstance 返回空 → NPC 菜单功能已禁用");
        return;
    }

    uint32_t version = 0;
    try {
        version = api->version();
    } catch (...) {
        logger.warn("[MeowMenu] 读取 HologramLib 版本失败 → NPC 菜单功能已禁用");
        return;
    }
    if (version < hologramlib::kMinApiVersionForNpcDialog) {
        logger.warn(
            "[MeowMenu] HologramLib API 0x{:06X} 低于 NPC 对话所需 0x{:06X} → 该功能已禁用（升级 HologramLib 即可启用）",
            version,
            hologramlib::kMinApiVersionForNpcDialog
        );
        return;
    }

    mApi        = api;
    mApiVersion = version;

    try {
        mDialogs = &api->npcDialogs();
    } catch (...) {
        mDialogs = nullptr;
    }
    if (!mDialogs) {
        logger.warn("[MeowMenu] 无法获取 HologramLib NPC 对话域 → NPC 菜单功能已禁用");
        mApi = nullptr;
        return;
    }
    try {
        mNpcs = &api->playerNpcs();
    } catch (...) {
        mNpcs = nullptr;
    }
    try {
        mContainers = &api->containerMenus(); // vtable 尾部槽位（1.22.0 追加）
    } catch (...) {
        mContainers = nullptr;
    }

    // 对话按钮点击回传
    try {
        mClickToken = mDialogs->addClickListener(
            [this](hologramlib::NpcDialogClickEvent const& event) { this->onDialogClick(event); }
        );
    } catch (...) {
        mClickToken = 0;
    }
    // ghost 交互（点击 NPC 打开其绑定的菜单）
    try {
        mGhostToken = mApi->addGhostInteractListener(
            [this](hologramlib::GhostInteractEvent const& event) { this->onGhostInteract(event); }
        );
    } catch (...) {
        mGhostToken = 0;
    }
    // 箱子界面点击/关闭回传
    if (mContainers) {
        try {
            mContainerClickToken = mContainers->addClickListener(
                [this](hologramlib::ContainerClickEvent const& event) { this->onContainerClick(event); }
            );
        } catch (...) {
            mContainerClickToken = 0;
        }
    }

    logger.info(
        "[MeowMenu] HologramLib 已连接 (ABI 0x{:06X}): 界面样式 NPC对话={}, 箱子={}; 监听: 对话点击={}, ghost={}, 箱子点击={}",
        version,
        mDialogs ? "可用" : "不可用",
        mContainers ? "可用" : "不可用",
        mClickToken,
        mGhostToken,
        mContainerClickToken
    );
}

void NpcMenuBridge::shutdown() {
    auto& logger = lib().getSelf().getLogger();

    // 前置已卸载时不能再解引用其对象（vtable 已失效）
    if (hologramModuleLoaded()) {
        try {
            if (mDialogs && mClickToken) mDialogs->removeClickListener(mClickToken);
        } catch (...) {}
        try {
            if (mApi && mGhostToken) mApi->removeGhostInteractListener(mGhostToken);
        } catch (...) {}
        try {
            if (mContainers && mContainerClickToken) mContainers->removeClickListener(mContainerClickToken);
        } catch (...) {}
        destroyOwnedNpcs();
    } else if (mApi) {
        logger.warn("[MeowMenu] HologramLib 已卸载, 跳过 NPC 清理（NPC 会随库卸载消失）");
    }

    mClickToken          = 0;
    mGhostToken          = 0;
    mContainerClickToken = 0;
    mDialogs             = nullptr;
    mNpcs                = nullptr;
    mContainers          = nullptr;
    mApi                 = nullptr;

    {
        std::unique_lock lock(mMutex);
        mScenes.clear();
        mChestScenes.clear();
    }
    if (mApiVersion) {
        logger.info("[MeowMenu] NPC 菜单桥接已断开（绑定数据保留, 下次加载自动重建）");
    }
}

//================================================================
// 状态与列表
//================================================================
std::string NpcMenuBridge::statusText() const {
    std::ostringstream oss;
    if (!mApi) {
        oss << "HologramLib: 未连接（NPC 菜单功能已禁用; 装好 HologramLib 后重启服务器即自动启用）";
        return oss.str();
    }
    oss << "HologramLib: 已连接, ABI 版本 0x" << std::hex << mApiVersion << std::dec << "\n";
    oss << "  NPC 对话域: " << (mDialogs ? "可用" : "不可用") << "\n";
    oss << "  NPC 生成域: " << (mNpcs ? "可用" : "不可用") << "\n";
    oss << "  点击监听 token: " << mClickToken << " / ghost 监听 token: " << mGhostToken << "\n";
    {
        std::shared_lock lock(mMutex);
        oss << "  已绑定 NPC: " << mBindings.size() << " 个\n";
        oss << "  进行中的对话: " << mScenes.size() << " 个";
    }
    return oss.str();
}

std::string NpcMenuBridge::listNpcsJson() const {
    nlohmann::json arr = nlohmann::json::array();
    {
        std::shared_lock lock(mMutex);
        for (auto const& b : mBindings) {
            arr.push_back(
                nlohmann::json{
                    {"id", b.id},
                    {"name", b.name},
                    {"menu", b.menuId},
                    {"x", b.x},
                    {"y", b.y},
                    {"z", b.z},
                    {"dim", b.dim},
                    {"skin", b.skinId},
                }
            );
        }
    }
    return arr.dump(2);
}

//================================================================
// 对话渲染
//================================================================
hologramlib::NpcDialogSpec NpcMenuBridge::buildSpec(
    Menu const&        menu,
    bool               withBack,
    std::string const& npcName,
    std::vector<int>&  outIndexMap,
    Player*            player,
    int&               page,
    int&               outPageCount
) const {
    hologramlib::NpcDialogSpec spec;
    spec.carrierIdentifier = "minecraft:npc";

    // NPC 名: 显式传入（NPC 绑定/调用方）> 菜单配置的 npc.name > 菜单标题
    std::string name = npcName;
    if (name.empty()) name = menu.npc.name;
    if (name.empty()) name = menu.title;
    spec.npcName = stripFormatCodes(oneLine(translateForPlayer(name, player), kMaxLabelChars));
    if (spec.npcName.empty()) spec.npcName = "菜单";
    spec.sceneName = menu.id;

    // 话语/正文: 菜单配置的 npc.dialogue 优先, 否则用 content;
    // 再并入信息类按钮（header/label）的文本（原版对话框没有不可点击的行）
    std::string body = expandEscapedNewlines(
        stripFormatCodes(translateForPlayer(menu.npc.dialogue.empty() ? menu.content : menu.npc.dialogue, player))
    );
    for (auto const& button : menu.buttons) {
        if (button.type != "header" && button.type != "label") continue;
        std::string line = stripFormatCodes(oneLine(translateForPlayer(button.text, player), kMaxLabelChars));
        if (line.empty()) continue;
        if (body.size() + line.size() + 1 > kMaxBodyChars) break;
        if (!body.empty()) body += "\n";
        body += line;
    }
    if (body.empty()) body = stripFormatCodes(oneLine(menu.title, kMaxBodyChars));
    body = truncateUtf8(body, kMaxBodyChars);
    spec.dialogue = body;

    outIndexMap.clear();

    // 收集可点击按钮（显示项不进按钮表）
    struct Entry {
        int           index;
        Button const* button;
    };
    std::vector<Entry> clickable;
    for (size_t i = 0; i < menu.buttons.size(); ++i) {
        auto const& b = menu.buttons[i];
        if (b.type == "spacer" || b.type == "divider" || b.type == "header" || b.type == "label") continue;
        clickable.push_back({static_cast<int>(i), &b});
    }
    size_t const total = clickable.size();

    outIndexMap.clear();
    outPageCount = 1;

    auto makeButton = [&](Button const& button, int buttonIndex) {
        std::string label = stripFormatCodes(oneLine(
            translateForPlayer(button.text.empty() ? button.value : button.text, player), kMaxLabelChars
        ));
        if (label.empty()) label = "按钮 " + std::to_string(buttonIndex);

        bool isSubmenu = button.type == "submenu" || button.type == "folder" || button.type == "admin_submenu";
        bool isAdmin   = button.type == "admin_command" || button.type == "admin_submenu";
        if (isAdmin) label = "[管理] " + label;
        if (isSubmenu) label += " >";

        hologramlib::NpcDialogButton dialogButton;
        dialogButton.label    = label;
        dialogButton.actionId = makeMenuAction(menu.id, buttonIndex);
        dialogButton.mode     = 0;
        return dialogButton;
    };
    auto makeNav = [](std::string const& label, char const* action) {
        hologramlib::NpcDialogButton b;
        b.label    = label;
        b.actionId = action;
        b.mode     = 0;
        return b;
    };

    // 不分页（≤6 个可点击按钮）: 保持原行为 —— 最多 6 个, 有空槽才补「返回上一级」
    if (total <= kMaxDialogButtons) {
        size_t shown = 0;
        for (auto const& entry : clickable) {
            if (shown >= kMaxDialogButtons) break;
            spec.buttons.push_back(makeButton(*entry.button, entry.index));
            outIndexMap.push_back(entry.index);
            ++shown;
        }
        if (withBack && shown < kMaxDialogButtons) {
            spec.buttons.push_back(makeNav("<< 返回上一级", kActionBack));
            outIndexMap.push_back(-1);
        }
        return spec;
    }

    // 分页模式（>6 个）: 每页内容项 普通菜单 4 / 主菜单 5, 其余槽位放导航
    //   普通菜单: 首格「上一页」+ 末格「下一页」（前后对应, 6 = 1 + 4 + 1）
    //   主菜单  : 只剩 1 格导航（6 = 5 + 1）→ 优先「下一页」, 最后一页显示「上一页」
    // 需要「返回上一级」时请在菜单里自带一个返回按钮（分页模式槽位被内容与导航占满）
    size_t const pageSize = (menu.id == ConfigManager::getInstance().getConfig().mainMenuId) ? kPageSizeMain
                                                                                            : kPageSizeNormal;
    size_t const navSlots = kMaxDialogButtons - pageSize; // 2 或 1

    int pageCount = static_cast<int>((total + pageSize - 1) / pageSize);
    if (pageCount < 1) pageCount = 1;
    if (page < 0) page = 0;
    if (page >= pageCount) page = pageCount - 1;
    outPageCount = pageCount;

    size_t       slot  = 0;
    size_t const begin = static_cast<size_t>(page) * pageSize;

    if (navSlots >= 2) {
        if (page > 0) {
            spec.buttons.push_back(makeNav("§7← 上一页", kActionPrev));
        } else {
            spec.buttons.push_back(makeNav("§8——", "@noop")); // 占位, 保持布局稳定
        }
        outIndexMap.push_back(-1);
        ++slot;
    }

    for (size_t k = 0; k < pageSize; ++k) {
        size_t idx = begin + k;
        if (idx >= total) break;
        spec.buttons.push_back(makeButton(*clickable[idx].button, clickable[idx].index));
        outIndexMap.push_back(clickable[idx].index);
        ++slot;
    }

    if (navSlots >= 2) {
        if (page + 1 < pageCount) {
            spec.buttons.push_back(makeNav("§7下一页 →", kActionNext));
        } else {
            spec.buttons.push_back(makeNav("§8——", "@noop"));
        }
        outIndexMap.push_back(-1);
        ++slot;
    } else {
        // 主菜单: 只有 1 格导航
        if (page + 1 < pageCount) {
            spec.buttons.push_back(makeNav("§7下一页 →", kActionNext));
            outIndexMap.push_back(-1);
        } else if (page > 0) {
            spec.buttons.push_back(makeNav("§7← 上一页", kActionPrev));
            outIndexMap.push_back(-1);
        }
    }

    lib().getSelf().getLogger().info(
        "NPC 对话分页: 菜单 {} 共 {} 项 → 第 {} / {} 页 (每页 {} 项, 导航 {} 格)",
        menu.id,
        total,
        page + 1,
        pageCount,
        pageSize,
        navSlots
    );
    return spec;
}

bool NpcMenuBridge::renderScene(Player& player, std::string const& menuId, std::string const& npcName) {
    if (!mDialogs) return false;

    auto menu = MenuStore::getInstance().getMenu(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在: " + menuId);
        return false;
    }

    std::string realName = player.getRealName();

    // 注意: 库调用不能持锁进行 —— 回调里也会取同一把锁
    hologramlib::NpcDialogSpec spec;
    std::vector<int>           indexMap;
    int64_t                    liveDialogId = -1;
    int                        page         = 0;
    int                        pageCount    = 1;
    {
        std::unique_lock lock(mMutex);
        auto&            scene = mScenes[realName];
        if (!npcName.empty()) scene.npcName = npcName;                     // 显式传入优先（NPC 绑定名）
        else if (!menu->npc.name.empty()) scene.npcName = menu->npc.name;  // 菜单自己配的 NPC 名
        bool withBack = !scene.ancestors.empty();
        page          = scene.page;
        spec          = buildSpec(*menu, withBack, scene.npcName, indexMap, &player, page, pageCount);
        scene.indexMap = indexMap;
        scene.page     = page;
        scene.menuId   = menuId;
        liveDialogId   = scene.dialogId;
    }

    // NPC 对话的换页/进子菜单/返回/命令后重推一律走 open()（重建载体）:
    //   原版 NPC 对话界面在**点击任意按钮时客户端就自行收起**了, 而 INpcDialogue::update 只重发
    //   一次 NpcDialoguePacket(Open) —— 客户端不会因此重新弹出界面（实测: 点了就关、没有新界面）。
    //   只有 open() 会"删旧载体 + 建新载体 + 发 Open", 客户端才会再弹一次界面。
    //   库侧已把点击后的清理改成"只清本次事件对应的对话", 所以在回调里同步重开不会被误删
    //   （这正是之前"点了直接关闭、新层没出来"的根因）。
    int64_t dialogId = -1;
    try {
        dialogId = mDialogs->open(realName, spec);
    } catch (...) {
        dialogId = -1;
    }
    if (dialogId < 0) {
        player.sendMessage("§c[MeowMenu] 打开 NPC 对话失败（玩家可能不在线）");
        return false;
    }
    {
        std::unique_lock lock(mMutex);
        mScenes[realName].dialogId = dialogId;
    }

    lib().getSelf().getLogger().info(
        "NPC 对话菜单已{}: {} → {} (dialog={}, 按钮={}{})",
        liveDialogId > 0 ? "重开" : "打开",
        realName,
        menuId,
        dialogId,
        spec.buttons.size(),
        pageCount > 1 ? fmt::format(", 第 {}/{} 页", page + 1, pageCount) : ""
    );
    return true;
}

// 对话分页跳转: 就地刷新, 界面不关不闪
void NpcMenuBridge::jumpDialoguePage(Player& player, int delta) {
    std::string realName = player.getRealName();
    std::string menuId;
    int         target = 0;
    {
        std::unique_lock lock(mMutex);
        auto             it = mScenes.find(realName);
        if (it == mScenes.end() || it->second.menuId.empty()) return;
        it->second.page = it->second.page + delta;
        target          = it->second.page;
        menuId          = it->second.menuId;
    }
    lib().getSelf().getLogger().info("NPC 对话翻页: {} → {} 第 {} 页", realName, menuId, target + 1);
    renderScene(player, menuId);
}

bool NpcMenuBridge::openDialogue(Player& player, std::string const& menuId, std::string const& npcName) {
    if (!mDialogs) {
        player.sendMessage("§c[MeowMenu] HologramLib 未安装或版本不足, NPC 菜单不可用");
        return false;
    }
    std::string id = menuId.empty() ? ConfigManager::getInstance().getConfig().mainMenuId : menuId;

    // 从根菜单进入: 重置导航状态（上级栈/页/NPC 名）, 但保留 dialogId —— 客户端那边对话还开着就就地换内容
    {
        std::unique_lock lock(mMutex);
        auto&            scene = mScenes[player.getRealName()];
        scene.ancestors.clear();
        scene.indexMap.clear();
        scene.page = 0;
        if (!npcName.empty()) scene.npcName = npcName;
    }
    return renderScene(player, id, npcName);
}

// 点按钮回调里的时序问题（必须延迟一 tick 的原因）:
//   HologramLib 处理 NpcRequestPacket 的顺序是  dispatch(事件) → 回调返回后 才按玩家拆掉
//   "当前对话记录 + 客户端载体实体"。如果我在回调里直接 open 下一层, 它会把我刚开的那层
//   当作旧对话一起删掉 —— 表现就是"界面直接关闭, 新的一层没出来"。
//   推迟一个 tick 后开, 清理已经完成, 新一层的载体和对话框包就不会被误删。
bool NpcMenuBridge::openDialogueDeferred(Player& player, std::string const& menuId, std::string const& npcName) {
    if (!mDialogs) {
        player.sendMessage("§c[MeowMenu] HologramLib 未安装或版本不足, NPC 菜单不可用");
        return false;
    }
    std::string realName = player.getRealName();
    std::string id       = menuId.empty() ? ConfigManager::getInstance().getConfig().mainMenuId : menuId;
    {
        std::unique_lock lock(mMutex);
        mScenes[realName] = SceneState{};
    }
    scheduleRenderScene(realName, id, npcName);
    return true;
}

void NpcMenuBridge::scheduleRenderScene(
    std::string const& playerName,
    std::string const& menuId,
    std::string const& npcName
) {
    auto& logger = lib().getSelf().getLogger();
    try {
        ll::thread::ServerThreadExecutor::getDefault().executeAfter(
            [this, playerName, menuId, npcName]() {
                try {
                    if (!mDialogs) return; // 期间前置被卸载
                    Player* player = findPlayerByName(playerName);
                    if (!player) return;   // 玩家已离线
                    renderScene(*player, menuId, npcName);
                } catch (std::exception const& e) {
                    lib().getSelf().getLogger().error("延迟打开 NPC 对话失败: {}", e.what());
                } catch (...) {}
            },
            std::chrono::milliseconds(50) // ≈1 tick, 足够让库完成上一步的拆对话
        );
    } catch (std::exception const& e) {
        // 排不进队列（极端情况）→ 直接开, 至少不静默失败
        logger.error("排入延迟打开任务失败, 改为立即打开: {}", e.what());
        Player* player = findPlayerByName(playerName);
        if (player) renderScene(*player, menuId, npcName);
    }
}

bool NpcMenuBridge::closeDialogue(Player& player) {
    if (!mDialogs) return false;

    std::string realName = player.getRealName();
    int64_t     dialogId = -1;
    {
        std::unique_lock lock(mMutex);
        auto             it = mScenes.find(realName);
        if (it != mScenes.end()) {
            dialogId = it->second.dialogId;
            it->second.dialogId = -1;
            it->second.ancestors.clear();
        }
    }

    bool closed = false;
    if (dialogId > 0) {
        try {
            closed = mDialogs->close(dialogId);
        } catch (...) {}
    }
    player.sendMessage(closed ? "§a[MeowMenu] 已关闭 NPC 对话" : "§7[MeowMenu] 当前没有打开的 NPC 对话");
    return closed;
}

bool NpcMenuBridge::backDialogue(Player& player) {
    std::string parentId;
    if (!popScene(player.getRealName(), parentId)) {
        player.sendMessage("§7[MeowMenu] 已经是第一级菜单");
        return false;
    }
    {
        std::unique_lock lock(mMutex);
        auto             it = mScenes.find(player.getRealName());
        if (it != mScenes.end()) it->second.page = 0; // 回到上级从第一页开始
    }
    renderScene(player, parentId); // 就地刷新（库已支持回调内不拆对话）
    return true;
}

//================================================================
// 按钮执行
//
// source = 这次执行是从哪来的:
//   NpcDialog / Container → 处于点击回调内, 重推界面必须延迟一个 tick 且用对应样式
//   Direct               → 脚本/API 触发, 不延迟
//================================================================
bool NpcMenuBridge::executeMenuButton(
    Player&            player,
    std::string const& menuId,
    int                buttonIndex,
    ClickSource        source
) {
    auto menu = MenuStore::getInstance().getMenu(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在: " + menuId);
        return false;
    }
    if (buttonIndex < 0 || buttonIndex >= static_cast<int>(menu->buttons.size())) return false;

    Button button = menu->buttons[buttonIndex];
    auto&  logger = lib().getSelf().getLogger();

    // 子菜单类: 在原地推进一层（不打开表单, 保持 NPC 菜单形态）
    if (button.type == "submenu" || button.type == "folder" || button.type == "admin_submenu") {
        if (button.type == "admin_submenu" && !ButtonHandler::isOp(player)) {
            player.sendMessage("§c[MeowMenu] 权限不足，无法访问此菜单");
            return false;
        }
        std::string childId = button.value;
        if (childId.empty()) {
            player.sendMessage("§c[MeowMenu] 子菜单ID为空");
            return false;
        }
        if (!MenuStore::getInstance().getMenu(childId)) {
            // 与表单路径一致: 目标不存在则建一个空的, 便于随后用编辑器补内容
            MenuStore::getInstance().createMenu(childId, childId, "");
        }
        auto child = MenuStore::getInstance().getMenu(childId);
        if (child) {
            // 目标菜单用哪种样式, 由"玩家自选样式 > 菜单自身配置"决定
            std::string childMode = MenuRenderer::resolveUiMode(player, *child);
            if (childMode == ui_mode::kNpc && mDialogs) {
                pushScene(player.getRealName(), menuId);
                {
                    std::unique_lock lock(mMutex);
                    auto             it = mScenes.find(player.getRealName());
                    if (it != mScenes.end()) it->second.page = 0; // 子菜单从第一页开始
                }
                logger.info("菜单进入子菜单(NPC对话): {} -[{}]-> {}", menuId, buttonIndex, childId);
                renderScene(player, childId); // 就地刷新: 复用同一载体, 不关界面
                return true;
            }
            if (childMode == ui_mode::kChest && mContainers) {
                pushChestScene(player.getRealName(), menuId);
                logger.info("菜单进入子菜单(箱子): {} -[{}]-> {}", menuId, buttonIndex, childId);
                if (source == ClickSource::NpcDialog) {
                    scheduleChestScene(player.getRealName(), childId, 0); // 对话点击会拆界面 → 延迟
                } else {
                    renderChestScene(player, childId, 0); // 箱子点击 → 直接换箱
                }
                return true;
            }
        }
        // 普通表单（或该样式当前不可用时的回退）→ 交给表单渲染器
        logger.info("菜单进入子菜单(表单): {} -[{}]-> {}", menuId, buttonIndex, childId);
        if (source == ClickSource::Direct) {
            MenuRenderer::getInstance().renderMenu(player, childId);
        } else {
            MenuRenderer::getInstance().renderMenuDeferred(player, childId); // 点击回调内延迟
        }
        return true;
    }

    // 其余类型复用既有执行链（command/admin_command 真执行; 未知类型静默忽略）
    logger.info(
        "菜单按钮执行: menu={} index={} type={} value={}",
        menuId,
        buttonIndex,
        button.type,
        button.value
    );
    ButtonHandler::handleButton(player, button);

    // 命令执行后重推当前层: 客户端点按钮后会自行收起界面, 且库在回调返回后才拆界面/载体,
    // 所以点按钮触发的一律延迟一个 tick; 用哪种样式重推取决于本次是从哪种界面点进来的。
    std::string realName = player.getRealName();
    switch (source) {
    case ClickSource::Container: {
        int page = 0;
        {
            std::shared_lock lock(mMutex);
            auto             it = mChestScenes.find(realName);
            if (it != mChestScenes.end()) page = it->second.page;
        }
        renderChestScene(player, menuId, page); // 容器点击不拆界面 → 直接重推, 无延迟
        break;
    }
    case ClickSource::NpcDialog:
        renderScene(player, menuId); // 就地刷新当前层（命令执行后界面不关不闪）
        break;
    case ClickSource::Direct:
    default:
        renderScene(player, menuId); // 非点击回调（脚本/API 触发）, 同步重推
        break;
    }
    return true;
}

//================================================================
// 每玩家对话状态
//================================================================
void NpcMenuBridge::pushScene(std::string const& playerName, std::string const& menuId) {
    std::unique_lock lock(mMutex);
    mScenes[playerName].ancestors.push_back(menuId);
}

bool NpcMenuBridge::popScene(std::string const& playerName, std::string& outMenuId) {
    std::unique_lock lock(mMutex);
    auto             it = mScenes.find(playerName);
    if (it == mScenes.end() || it->second.ancestors.empty()) return false;
    outMenuId = it->second.ancestors.back();
    it->second.ancestors.pop_back();
    return true;
}

//================================================================
// 监听回调
//================================================================
void NpcMenuBridge::onDialogClick(hologramlib::NpcDialogClickEvent const& event) {
    auto& logger = lib().getSelf().getLogger();
    try {
        if (event.closed) {
            // 客户端点了"关闭"(mode=1)或按 ESC: 清掉本层状态, 下次打开是干净的一级菜单
            {
                std::unique_lock lock(mMutex);
                auto             it = mScenes.find(event.playerName);
                if (it != mScenes.end()) {
                    it->second.dialogId = -1;
                    it->second.ancestors.clear();
                }
            }
            logger.info("NPC 对话已关闭: player={} scene={} dialog={}", event.playerName, event.sceneName, event.dialogId);
            return;
        }

        Player* player = findPlayerByName(event.playerName);
        if (!player) {
            logger.warn("NPC 对话点击: 玩家不在线 {}", event.playerName);
            return;
        }

        logger.info(
            "NPC 对话点击: player={} scene={} index={} action={}",
            event.playerName,
            event.sceneName,
            event.buttonIndex,
            event.actionId
        );

        if (event.actionId == kActionClose) {
            closeDialogue(*player);
            return;
        }
        if (event.actionId == kActionBack) {
            backDialogue(*player);
            return;
        }
        if (event.actionId == kActionNext) {
            jumpDialoguePage(*player, +1);
            return;
        }
        if (event.actionId == kActionPrev) {
            jumpDialoguePage(*player, -1);
            return;
        }
        if (event.actionId == "@noop") return; // 分页占位格

        std::string menuId;
        int         index = -1;
        if (!parseMenuAction(event.actionId, menuId, index)) {
            // 兼容: actionId 未回填时用 场景名 + 按钮下标, 并经下标映射还原到菜单按钮
            menuId = event.sceneName;
            index  = event.buttonIndex;
            std::shared_lock lock(mMutex);
            auto             it = mScenes.find(event.playerName);
            if (it != mScenes.end() && index >= 0 && index < static_cast<int>(it->second.indexMap.size())) {
                index = it->second.indexMap[index];
            }
        }
        if (menuId.empty() || index < 0) return;

        executeMenuButton(*player, menuId, index, ClickSource::NpcDialog);
    } catch (std::exception const& e) {
        logger.error("NPC 对话点击处理失败: {}", e.what());
    } catch (...) {
        logger.error("NPC 对话点击处理失败: 未知异常");
    }
}

void NpcMenuBridge::onGhostInteract(hologramlib::GhostInteractEvent const& event) {
    if (event.domain != "npc") return;
    if (event.action != 1 && event.action != 5) return; // 1=右键交互, 5=NpcOpen

    NpcBinding binding;
    if (!lookupBinding(event.id, binding)) return; // 未绑定菜单的 NPC 不处理

    Player* player = findPlayerByName(event.playerName);
    if (!player) return;

    lib().getSelf().getLogger().info(
        "点击 NPC {}({}) → 打开菜单 {} (player={})",
        binding.name,
        event.id,
        binding.menuId,
        event.playerName
    );
    openDialogue(*player, binding.menuId, binding.name);
}

//================================================================
// 箱子界面（HologramLib 的虚拟容器）
//
// 虚拟容器是"服务端没有真容器"的纯协议界面: 物品只是显示在那里, 点击/关闭由回包回调给出槽位号。
// 因此把每个可点击按钮映射成一个物品格:
//   槽位 → 菜单按钮下标（-1 = 返回上一级, -2 = 上一页, -3 = 下一页, -4 = 空槽/未使用）
// 分页: 3 行 = 27 格, 放不下自动升到 6 行 54 格, 仍放不下才分页（导航格固定占本页末尾）。
// 时序: 与 NPC 对话同理 —— 点击回调里重开界面必须延迟一 tick（见 scheduleRenderScene 的说明）。
//================================================================

bool NpcMenuBridge::isChestAvailable() const { return mContainers != nullptr; }

// 格子里的物品类型: 按钮的 item 字段（旧箱子UI字段, 现在启用）→ iconItem（物品 ID）→ 纸
static std::string chestItemType(Button const& button) {
    auto normalize = [](std::string s) {
        if (!s.empty() && s.find(':') == std::string::npos) s = "minecraft:" + s;
        return s;
    };
    if (!button.item.empty()) return normalize(button.item);
    if (!button.iconItem.empty() && button.iconItem.find(':') != std::string::npos) return button.iconItem;
    return "minecraft:paper";
}

hologramlib::ContainerMenuSpec
NpcMenuBridge::buildChestSpec(Menu const& menu, bool withBack, Player* player, ChestScene& out) const {
    std::vector<std::pair<int, Button const*>> clickable;
    for (size_t i = 0; i < menu.buttons.size(); ++i) {
        auto const& b = menu.buttons[i];
        if (b.type == "spacer" || b.type == "divider" || b.type == "header" || b.type == "label") continue;
        clickable.emplace_back(static_cast<int>(i), &b);
    }
    size_t total = clickable.size();

    size_t nav      = withBack ? 1 : 0;
    int    rows     = 3;
    size_t capacity = 27;
    if (total + nav > capacity) {
        rows     = 6;
        capacity = 54;
    }
    bool paging = false;
    if (total + nav > capacity) {
        paging = true;
        nav += 2; // 上一页 / 下一页
    }
    size_t pageSize  = capacity > nav ? capacity - nav : 1;
    int    pageCount = static_cast<int>((total + pageSize - 1) / pageSize);
    if (pageCount < 1) pageCount = 1;

    int page = out.page;
    if (page < 0) page = 0;
    if (page >= pageCount) page = pageCount - 1;

    hologramlib::ContainerMenuSpec spec;
    spec.rows        = rows;
    spec.useMinecart = false; // 方块实体路径（库注释: 实测客户端认这条）
    spec.title       = translateForPlayer(menu.title, player);
    if (spec.title.empty()) spec.title = menu.id;
    spec.items.assign(static_cast<size_t>(rows) * 9, hologramlib::ContainerMenuItem{});

    out.slotMap.assign(static_cast<size_t>(rows) * 9, -4);
    out.page   = page;
    out.rows   = rows;
    out.menuId = menu.id;

    size_t begin = static_cast<size_t>(page) * pageSize;
    for (size_t k = 0; k < pageSize; ++k) {
        size_t idx = begin + k;
        if (idx >= total) break;
        int           buttonIndex = clickable[idx].first;
        Button const& button      = *clickable[idx].second;

        hologramlib::ContainerMenuItem item;
        item.type  = chestItemType(button);
        item.count = 1;
        item.name  = oneLine(
            translateForPlayer(button.text.empty() ? button.value : button.text, player),
            kMaxLabelChars
        );
        if (item.name.empty()) item.name = "按钮 " + std::to_string(buttonIndex);
        bool isSubmenu = button.type == "submenu" || button.type == "folder" || button.type == "admin_submenu";
        bool isAdmin   = button.type == "admin_command" || button.type == "admin_submenu";
        if (isAdmin) item.lore.push_back("§c需要管理员");
        item.lore.push_back(isSubmenu ? "§7点击进入子菜单" : "§7点击执行");
        item.lore.push_back("§8" + button.type);

        spec.items[k]  = std::move(item);
        out.slotMap[k] = buttonIndex;
    }

    // 导航格: 固定放在本页内容之后（分页时槽位固定占位, 没有上一页/下一页就留空）
    size_t slot = pageSize;
    if (withBack && slot < spec.items.size()) {
        hologramlib::ContainerMenuItem back;
        back.type = "minecraft:barrier";
        back.name = "§c返回上一级";
        back.lore.push_back("§7回到上一层菜单");
        spec.items[slot]  = std::move(back);
        out.slotMap[slot] = -1;
        ++slot;
    }
    if (paging) {
        auto mkPage = [&](std::string const& name, std::string const& lore, int code) {
            if (slot >= spec.items.size()) return;
            hologramlib::ContainerMenuItem item;
            item.type = "minecraft:arrow";
            item.name = name;
            item.lore.push_back(lore);
            spec.items[slot]  = std::move(item);
            out.slotMap[slot] = code;
            ++slot;
        };
        std::string pageInfo = "§7第 " + std::to_string(page + 1) + " / " + std::to_string(pageCount) + " 页";
        if (page > 0) mkPage("§e← 上一页", pageInfo, -2);
        if (page + 1 < pageCount) mkPage("§e下一页 →", pageInfo, -3);
    }
    return spec;
}

bool NpcMenuBridge::renderChestScene(Player& player, std::string const& menuId, int page, bool inPlace) {
    if (!mContainers) return false;

    auto menu = MenuStore::getInstance().getMenu(menuId);
    if (!menu) {
        player.sendMessage("§c[MeowMenu] 菜单不存在: " + menuId);
        return false;
    }

    std::string                    realName = player.getRealName();
    hologramlib::ContainerMenuSpec spec;
    int64_t                        liveContainerId = -1;
    {
        std::unique_lock lock(mMutex);
        auto&            scene = mChestScenes[realName];
        scene.page             = page;
        bool withBack          = !scene.ancestors.empty();
        spec                   = buildChestSpec(*menu, withBack, &player, scene);
        // 就地刷新只在"格子数不变"时可用: 同一个方块容器的槽位数是打开时定的,
        // 行数变了（3 行 <-> 6 行, 例如翻页跨到另一个菜单）必须重新 open 才有正确格子数。
        if (scene.rows == spec.rows) {
            liveContainerId = scene.containerId;
        } else {
            liveContainerId = -1;
        }
    }

    // 库调用不持锁（回包回调会取同一把锁）
    //
    // 翻页 / 进子菜单 / 返回 / 命令后重推都走 update(): 复用同一个载体方块, 只重发方块实体 NBT
    // 与 ContainerOpen —— 不拆界面、不重发方块、不用等 openDelayTicks, 所以没有那 0.5 秒延迟,
    // 也没有闪烁。只有"这个玩家当前没有活着的箱子"（首次打开 / 被关过 / 样式切换）时才走 open()。
    // 配置可关闭"就地刷新"（回退整体重开, 更慢但兼容性最稳）
    if (inPlace && !ConfigManager::getInstance().getConfig().chestInPlaceRefresh) {
        inPlace = false;
    }

    bool   updated     = false;
    size_t pushedSlots = 0;
    if (inPlace && liveContainerId > 0) {
        try {
            updated = mContainers->update(liveContainerId, spec);
        } catch (...) {
            updated = false;
        }
        if (updated) {
            // update() 只重发方块实体 NBT + ContainerOpen, 而客户端对"已经打开的容器"不会因此重读
            // 方块实体 —— 实测结果是内容不变（服务端已换页, 客户端还是旧的一页, 点格子就对不上了）。
            // 所以这里再用 setItem 逐格推一遍: InventorySlot 是真实箱子同步内容用的同一种包,
            // 客户端会就地替换对应格, 于是整页内容无延迟、无闪烁地换掉。
            for (size_t slot = 0; slot < spec.items.size(); ++slot) {
                try {
                    if (mContainers->setItem(liveContainerId, static_cast<int>(slot), spec.items[slot])) ++pushedSlots;
                } catch (...) {}
            }
        }
    }

    int64_t containerId = liveContainerId;
    if (!updated) {
        try {
            containerId = mContainers->open(realName, spec);
        } catch (...) {
            containerId = -1;
        }
        if (containerId < 0) {
            player.sendMessage("§c[MeowMenu] 打开箱子界面失败（玩家可能不在线）");
            return false;
        }
    }

    {
        std::unique_lock lock(mMutex);
        mChestScenes[realName].containerId = containerId;
    }
    lib().getSelf().getLogger().info(
        "箱子界面已{}: {} → {} (container={}, 第 {} 页 / {} 行, 格子 {} 个{})",
        updated ? "就地刷新" : "打开",
        realName,
        menuId,
        containerId,
        page + 1,
        spec.rows,
        spec.items.size(),
        updated ? fmt::format(", 逐格推送 {} 格", pushedSlots) : ""
    );
    return true;
}

// 按槽刷新当前箱子界面的某一格（HologramLib 的 setItem: 只发一条 InventorySlot,
// 客户端就地换掉那一格 —— 不重发方块实体、不重开界面, 无延迟无闪烁）。
// itemType 为空 = 清空该格。供"任务打勾 / 数量变化 / 价格变化"这类单格改动使用。
bool NpcMenuBridge::refreshChestSlot(
    Player&            player,
    int                slot,
    std::string const& itemType,
    std::string const& name
) {
    if (!mContainers) return false;

    int64_t containerId = -1;
    {
        std::shared_lock lock(mMutex);
        auto             it = mChestScenes.find(player.getRealName());
        if (it == mChestScenes.end()) return false;
        containerId = it->second.containerId;
    }
    if (containerId <= 0) return false;

    hologramlib::ContainerMenuItem item;
    item.type  = itemType;
    item.count = itemType.empty() ? 0 : 1;
    item.name  = name;
    try {
        return mContainers->setItem(containerId, slot, item);
    } catch (...) {
        return false;
    }
}

void NpcMenuBridge::scheduleChestScene(std::string const& playerName, std::string const& menuId, int page) {
    try {
        ll::thread::ServerThreadExecutor::getDefault().executeAfter(
            [this, playerName, menuId, page]() {
                try {
                    if (!mContainers) return;
                    Player* player = findPlayerByName(playerName);
                    if (!player) return;
                    renderChestScene(*player, menuId, page);
                } catch (std::exception const& e) {
                    lib().getSelf().getLogger().error("延迟打开箱子界面失败: {}", e.what());
                } catch (...) {}
            },
            std::chrono::milliseconds(50)
        );
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error("排入延迟打开任务失败, 改为立即打开: {}", e.what());
        if (Player* player = findPlayerByName(playerName)) renderChestScene(*player, menuId, page);
    }
}

bool NpcMenuBridge::openChestMenu(Player& player, std::string const& menuId) {
    if (!mContainers) {
        player.sendMessage("§c[MeowMenu] HologramLib 未安装或版本不足, 箱子界面不可用");
        return false;
    }
    std::string id = menuId.empty() ? ConfigManager::getInstance().getConfig().mainMenuId : menuId;
    {
        std::unique_lock lock(mMutex);
        auto&            scene = mChestScenes[player.getRealName()];
        // 只重置导航状态, 保留 containerId: 客户端那边箱子还开着就就地换内容（无延迟）
        scene.ancestors.clear();
        scene.slotMap.clear();
        scene.menuId.clear();
        scene.page = 0;
    }
    return renderChestScene(player, id, 0);
}

bool NpcMenuBridge::openChestMenuDeferred(Player& player, std::string const& menuId) {
    if (!mContainers) {
        player.sendMessage("§c[MeowMenu] HologramLib 未安装或版本不足, 箱子界面不可用");
        return false;
    }
    std::string realName = player.getRealName();
    std::string id       = menuId.empty() ? ConfigManager::getInstance().getConfig().mainMenuId : menuId;
    {
        std::unique_lock lock(mMutex);
        auto&            scene = mChestScenes[realName];
        scene.ancestors.clear();
        scene.slotMap.clear();
        scene.menuId.clear();
        scene.page = 0;
    }
    scheduleChestScene(realName, id, 0);
    return true;
}

bool NpcMenuBridge::closeChestMenu(Player& player) {
    if (!mContainers) return false;

    std::string realName    = player.getRealName();
    int64_t     containerId = -1;
    {
        std::unique_lock lock(mMutex);
        auto             it = mChestScenes.find(realName);
        if (it != mChestScenes.end()) {
            containerId            = it->second.containerId;
            it->second.containerId = -1;
            it->second.ancestors.clear();
        }
    }
    bool closed = false;
    if (containerId > 0) {
        try {
            closed = mContainers->close(containerId);
        } catch (...) {}
    }
    player.sendMessage(closed ? "§a[MeowMenu] 已关闭箱子界面" : "§7[MeowMenu] 当前没有打开的箱子界面");
    return closed;
}

void NpcMenuBridge::pushChestScene(std::string const& playerName, std::string const& menuId) {
    std::unique_lock lock(mMutex);
    mChestScenes[playerName].ancestors.push_back(menuId);
}

bool NpcMenuBridge::popChestScene(std::string const& playerName, std::string& outMenuId) {
    std::unique_lock lock(mMutex);
    auto             it = mChestScenes.find(playerName);
    if (it == mChestScenes.end() || it->second.ancestors.empty()) return false;
    outMenuId = it->second.ancestors.back();
    it->second.ancestors.pop_back();
    return true;
}

void NpcMenuBridge::onContainerClick(hologramlib::ContainerClickEvent const& event) {
    auto& logger = lib().getSelf().getLogger();
    try {
        if (event.closed) {
            {
                std::unique_lock lock(mMutex);
                auto             it = mChestScenes.find(event.playerName);
                if (it != mChestScenes.end()) {
                    it->second.containerId = -1;
                    it->second.ancestors.clear();
                }
            }
            logger.info("箱子界面已关闭: player={} container={}", event.playerName, event.menuId);
            return;
        }

        Player* player = findPlayerByName(event.playerName);
        if (!player) return;

        std::string menuId;
        int         action = -4;
        int         page   = 0;
        {
            std::shared_lock lock(mMutex);
            auto             it = mChestScenes.find(event.playerName);
            if (it == mChestScenes.end()) return;
            auto const& scene = it->second;
            if (event.slot < 0 || event.slot >= static_cast<int>(scene.slotMap.size())) return;
            action = scene.slotMap[event.slot];
            menuId = scene.menuId;
            page   = scene.page;
        }

        std::string what = "空槽/未使用格";
        if (action >= 0) what = "按钮#" + std::to_string(action);
        else if (action == -1) what = "返回上一级";
        else if (action == -2) what = "上一页";
        else if (action == -3) what = "下一页";
        logger.info("箱子界面点击: player={} menu={} slot={} → {}", event.playerName, menuId, event.slot, what);

        // 容器点击不会拆界面（库只在"关闭"时才拆）, 所以这里直接渲染, 不必延迟;
        // 真正需要延迟的只有 NPC 对话那条线（详见 scheduleRenderScene 注释）。
        if (action == -4) return;
        if (action == -1) {
            std::string parent;
            if (popChestScene(event.playerName, parent)) renderChestScene(*player, parent, 0);
            return;
        }
        if (action == -2) {
            renderChestScene(*player, menuId, page - 1);
            return;
        }
        if (action == -3) {
            renderChestScene(*player, menuId, page + 1);
            return;
        }
        executeMenuButton(*player, menuId, action, ClickSource::Container);
    } catch (std::exception const& e) {
        logger.error("箱子界面点击处理失败: {}", e.what());
    } catch (...) {
        logger.error("箱子界面点击处理失败: 未知异常");
    }
}

//================================================================
// NPC 生成与绑定
//================================================================
int64_t NpcMenuBridge::spawnNpcNear(Player& opPlayer, std::string const& npcName, std::string const& menuId) {
    if (!canManageNpc()) {
        opPlayer.sendMessage("§c[MeowMenu] HologramLib 不可用或缺少 NPC 域, 无法生成菜单 NPC");
        return -1;
    }
    auto  pos  = opPlayer.getPosition();
    auto  view = opPlayer.getViewVector(1.0f);
    // 身前 2 格; 视线向量退化时（极端姿态）退回固定方向, 避免 NPC 生成在玩家身体里看不见
    float dx = static_cast<float>(view.x * 2.0);
    float dz = static_cast<float>(view.z * 2.0);
    if (std::fabs(dx) < 0.01f && std::fabs(dz) < 0.01f) {
        dx = 0.0f;
        dz = 2.0f;
    }
    float x   = static_cast<float>(pos.x) + dx;
    float y   = static_cast<float>(pos.y);
    float z   = static_cast<float>(pos.z) + dz;
    int   dim = static_cast<int>(opPlayer.getDimensionId());

    return spawnNpcAt(npcName, menuId, x, y, z, dim, opPlayer.getRealName());
}

int64_t NpcMenuBridge::spawnNpcAt(
    std::string const& npcName,
    std::string const& menuId,
    float              x,
    float              y,
    float              z,
    int                dim,
    std::string const& skinSourcePlayer
) {
    auto& logger = lib().getSelf().getLogger();
    if (!canManageNpc()) {
        logger.warn("生成菜单 NPC 失败: HologramLib NPC 域不可用");
        return -1;
    }
    std::string name = sanitizeNpcName(npcName);
    if (name.empty() || menuId.empty()) {
        logger.warn("生成菜单 NPC 失败: 名称或菜单为空");
        return -1;
    }
    if (!MenuStore::getInstance().getMenu(menuId)) {
        logger.warn("生成菜单 NPC 失败: 菜单不存在 {}", menuId);
        return -1;
    }

    // 皮肤: HologramLib 要求 create 时皮肤必须已注册（否则 -3）, 且库不落盘
    std::string skinId = "mnpc_" + menuId;
    if (!mNpcs->hasSkin(skinId)) {
        if (skinSourcePlayer.empty()) {
            logger.warn("生成菜单 NPC 失败: 皮肤 {} 未注册且没有可采集的玩家", skinId);
            return -1;
        }
        if (!mNpcs->captureSkin(skinId, skinSourcePlayer)) {
            logger.warn("生成菜单 NPC 失败: 采集玩家 {} 的皮肤失败（玩家不在线?）", skinSourcePlayer);
            return -1;
        }
        logger.info("已采集玩家 {} 的皮肤注册为 {}", skinSourcePlayer, skinId);
    }

    hologramlib::PlayerNpcConfig config;
    config.name         = name;
    config.skinId       = skinId;
    config.x            = x;
    config.y            = y;
    config.z            = z;
    config.dimension    = dim;
    config.yaw          = 0.0f;
    config.viewDistance = 96.0;
    config.enabled      = true;

    int64_t npcId = mNpcs->create(config);
    if (npcId < 0) {
        logger.warn("生成菜单 NPC 失败: HologramLib 返回 {} (-2 id 占用, -3 皮肤未注册)", npcId);
        return npcId;
    }

    NpcBinding binding;
    binding.id     = npcId;
    binding.name   = name;
    binding.menuId = menuId;
    binding.x      = x;
    binding.y      = y;
    binding.z      = z;
    binding.dim    = dim;
    binding.skinId = skinId;
    try {
        std::string blob;
        if (mNpcs->getSkinBlob(skinId, blob)) binding.skinBlobB64 = base64Encode(blob);
    } catch (...) {}

    {
        std::unique_lock lock(mMutex);
        mBindings.push_back(binding);
    }
    saveBindings();

    logger.info("菜单 NPC 已生成: id={} name={} menu={} pos=({:.1f},{:.1f},{:.1f}) dim={}", npcId, name, menuId, x, y, z, dim);
    return npcId;
}

bool NpcMenuBridge::removeNpc(int64_t npcId) {
    if (!canManageNpc()) return false;

    bool removed = false;
    {
        std::unique_lock lock(mMutex);
        auto it = std::find_if(mBindings.begin(), mBindings.end(), [npcId](NpcBinding const& b) { return b.id == npcId; });
        if (it != mBindings.end()) {
            mBindings.erase(it);
            removed = true;
        }
    }
    try {
        if (mNpcs->exists(npcId)) mNpcs->destroy(npcId);
        removed = true;
    } catch (...) {}

    if (removed) {
        saveBindings();
        lib().getSelf().getLogger().info("菜单 NPC 已删除: id={}", npcId);
    }
    return removed;
}

bool NpcMenuBridge::bindNpc(int64_t npcId, std::string const& menuId) {
    if (!canManageNpc() || menuId.empty()) return false;
    if (!MenuStore::getInstance().getMenu(menuId)) return false;
    bool exists = false;
    try {
        exists = mNpcs->exists(npcId);
    } catch (...) {}
    if (!exists) return false;

    std::string npcName = "NPC";
    try {
        hologramlib::PlayerNpcConfig config;
        if (mNpcs->get(npcId, config)) npcName = config.name;
    } catch (...) {}

    {
        std::unique_lock lock(mMutex);
        auto it = std::find_if(mBindings.begin(), mBindings.end(), [npcId](NpcBinding const& b) { return b.id == npcId; });
        if (it != mBindings.end()) {
            it->menuId = menuId;
        } else {
            NpcBinding binding;
            binding.id     = npcId;
            binding.name   = npcName;
            binding.menuId = menuId;
            // 外部创建的 NPC 只能绑定, 位置等交给对方维护
            mBindings.push_back(binding);
        }
    }
    saveBindings();
    lib().getSelf().getLogger().info("NPC {} 已绑定菜单 {}", npcId, menuId);
    return true;
}

bool NpcMenuBridge::reloadBindings() {
    if (!canManageNpc()) return false;
    destroyOwnedNpcs();
    loadBindings();
    restoreNpcs();
    return true;
}

//================================================================
// 持久化
//================================================================
void NpcMenuBridge::loadBindings() {
    std::vector<NpcBinding> loaded;
    try {
        if (!fs::exists(kBindFile)) {
            std::unique_lock lock(mMutex);
            mBindings.clear();
            return;
        }
        std::ifstream ifs(kBindFile);
        std::stringstream ss;
        ss << ifs.rdbuf();
        auto j = nlohmann::json::parse(ss.str());

        if (j.is_object() && j.contains("npcs") && j["npcs"].is_array()) {
            for (auto const& item : j["npcs"]) {
                if (!item.is_object()) continue;
                NpcBinding binding;
                binding.id          = item.value("id", static_cast<int64_t>(-1));
                binding.name        = item.value("name", std::string("NPC"));
                binding.menuId      = item.value("menu", std::string(""));
                binding.x           = item.value("x", 0.0f);
                binding.y           = item.value("y", 64.0f);
                binding.z           = item.value("z", 0.0f);
                binding.dim         = item.value("dim", 0);
                binding.skinId      = item.value("skin", std::string(""));
                binding.skinBlobB64 = item.value("skinBlob", std::string(""));
                if (binding.id > 0 && !binding.menuId.empty()) loaded.push_back(std::move(binding));
            }
        }
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error("读取 npc_menus.json 失败: {}", e.what());
    }

    std::unique_lock lock(mMutex);
    mBindings = std::move(loaded);
    lib().getSelf().getLogger().info("已加载 {} 个菜单 NPC 绑定", mBindings.size());
}

void NpcMenuBridge::saveBindings() {
    try {
        fs::create_directories(kDataDir);
        nlohmann::json j;
        j["version"] = 1;
        j["npcs"]    = nlohmann::json::array();
        {
            std::shared_lock lock(mMutex);
            for (auto const& b : mBindings) {
                j["npcs"].push_back(
                    nlohmann::json{
                        {"id", b.id},
                        {"name", b.name},
                        {"menu", b.menuId},
                        {"x", b.x},
                        {"y", b.y},
                        {"z", b.z},
                        {"dim", b.dim},
                        {"skin", b.skinId},
                        {"skinBlob", b.skinBlobB64},
                    }
                );
            }
        }
        std::ofstream ofs(kBindFile);
        ofs << j.dump(2);
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error("写入 npc_menus.json 失败: {}", e.what());
    }
}

void NpcMenuBridge::restoreNpcs() {
    if (!canManageNpc()) return;

    auto&                   logger = lib().getSelf().getLogger();
    std::vector<NpcBinding> snapshot;
    {
        std::shared_lock lock(mMutex);
        snapshot = mBindings;
    }
    if (snapshot.empty()) return;

    int ok = 0;
    int failed = 0;
    for (auto& binding : snapshot) {
        try {
            if (binding.id > 0 && mNpcs->exists(binding.id)) {
                ++ok; // 已存在（同一进程内重载）: 保留
                continue;
            }
            // 皮肤: 先用快照恢复; 快照缺失时只能靠玩家在线时重新采集
            if (!binding.skinId.empty() && !mNpcs->hasSkin(binding.skinId)) {
                std::string blob;
                if (!binding.skinBlobB64.empty() && base64Decode(binding.skinBlobB64, blob)) {
                    mNpcs->registerSkinFromBlob(blob);
                }
                if (!mNpcs->hasSkin(binding.skinId)) {
                    logger.warn("菜单 NPC {} 重建跳过: 皮肤 {} 不可用（请重新执行 /mnpc spawn）", binding.name, binding.skinId);
                    ++failed;
                    continue;
                }
            }
            if (binding.menuId.empty()) {
                ++failed;
                continue;
            }

            hologramlib::PlayerNpcConfig config;
            config.name         = binding.name;
            config.skinId       = binding.skinId;
            config.x            = binding.x;
            config.y            = binding.y;
            config.z            = binding.z;
            config.dimension    = binding.dim;
            config.viewDistance = 96.0;
            config.enabled      = true;

            int64_t newId = mNpcs->createWithId(config, binding.id);
            if (newId < 0) {
                // 目标 id 被占用或分段不符: 退化为随机 id, 并更新持久化记录
                newId = mNpcs->create(config);
                if (newId > 0) {
                    std::unique_lock lock(mMutex);
                    auto it = std::find_if(mBindings.begin(), mBindings.end(), [&](NpcBinding const& b) {
                        return b.skinId == binding.skinId && b.name == binding.name && b.menuId == binding.menuId;
                    });
                    if (it != mBindings.end()) it->id = newId;
                }
            }
            if (newId > 0) {
                ++ok;
            } else {
                ++failed;
                logger.warn("菜单 NPC {} 重建失败: HologramLib 返回 {}", binding.name, newId);
            }
        } catch (std::exception const& e) {
            ++failed;
            logger.error("菜单 NPC {} 重建异常: {}", binding.name, e.what());
        }
    }

    if (ok || failed) {
        saveBindings();
        logger.info("菜单 NPC 重建完成: 成功 {} 个, 失败 {} 个", ok, failed);
    }
}

void NpcMenuBridge::destroyOwnedNpcs() {
    if (!canManageNpc() || !hologramModuleLoaded()) return;

    std::vector<int64_t> ids;
    {
        std::shared_lock lock(mMutex);
        ids.reserve(mBindings.size());
        for (auto const& b : mBindings) {
            if (b.id > 0) ids.push_back(b.id);
        }
    }
    for (int64_t id : ids) {
        try {
            if (mNpcs->exists(id)) mNpcs->destroy(id);
        } catch (...) {}
    }
}

bool NpcMenuBridge::lookupBinding(int64_t npcId, NpcBinding& out) const {
    std::shared_lock lock(mMutex);
    auto const*      found = findBindingLocked(npcId);
    if (!found) return false;
    out = *found;
    return true;
}

NpcMenuBridge::NpcBinding* NpcMenuBridge::findBindingLocked(int64_t npcId) {
    auto it = std::find_if(mBindings.begin(), mBindings.end(), [npcId](NpcBinding const& b) { return b.id == npcId; });
    return it == mBindings.end() ? nullptr : &(*it);
}

NpcMenuBridge::NpcBinding const* NpcMenuBridge::findBindingLocked(int64_t npcId) const {
    auto it = std::find_if(mBindings.begin(), mBindings.end(), [npcId](NpcBinding const& b) { return b.id == npcId; });
    return it == mBindings.end() ? nullptr : &(*it);
}

} // namespace meowmenu
