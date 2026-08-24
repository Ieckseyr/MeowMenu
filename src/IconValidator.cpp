// IconValidator.cpp - 图标路径校验与修复实现
//
// 数据源：运行时读取 plugins/Meow/textures/texture_path.json（外部图标库）。
// 文件缺失时 mReady 保持 false，normalizeIcon 原样返回图标（菜单仍可用，仅跳过校验）。
#include "IconValidator.h"
#include "MeowMenu.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace meowmenu {

namespace {

// 去除字符串两端的空白字符
std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

// 判断字符串是否以指定前缀开头
bool startsWith(std::string const& str, std::string const& prefix) {
    if (str.size() < prefix.size()) return false;
    return str.compare(0, prefix.size(), prefix) == 0;
}

} // anonymous namespace

IconValidator& IconValidator::getInstance() {
    static IconValidator instance;
    return instance;
}

bool IconValidator::initialize() {
    if (mReady) return true;

    // 外部图标库路径（相对服务器工作目录）
    static constexpr const char* TEXTURE_PATH_FILE =
        "plugins/Meow/textures/texture_path.json";

    try {
        if (!std::filesystem::exists(TEXTURE_PATH_FILE)) {
            lib().getSelf().getLogger().error(
                "IconValidator: texture path file not found: {}", TEXTURE_PATH_FILE);
            return false;
        }

        // 读取文件内容
        std::ifstream ifs(TEXTURE_PATH_FILE);
        if (!ifs.is_open()) {
            lib().getSelf().getLogger().error(
                "IconValidator: cannot open {}", TEXTURE_PATH_FILE);
            return false;
        }
        std::stringstream ss;
        ss << ifs.rdbuf();
        auto j = nlohmann::json::parse(ss.str());

        if (!j.is_object()) {
            lib().getSelf().getLogger().error(
                "IconValidator: {} is not a JSON object", TEXTURE_PATH_FILE);
            return false;
        }

        size_t count = 0;
        for (auto it = j.begin(); it != j.end(); ++it) {
            std::string itemId   = it.key();
            std::string texPath  = it.value().is_string() ? it.value().get<std::string>() : "";

            // 去除纹理路径两端的空白（部分条目有前导空格，如 " spawn_egg_agent"）
            texPath = trim(texPath);
            if (texPath.empty()) continue;

            mItemToTexture.emplace(std::move(itemId), std::move(texPath));
            // 同时记录到纹理路径集合（用于快速校验）
            mTexturePaths.insert(it.value().is_string() ? it.value().get<std::string>() : "");
            count++;
        }

        mReady = true;
        lib().getSelf().getLogger().info(
            "IconValidator: loaded {} texture path entries (from {})", count, TEXTURE_PATH_FILE);
        return true;
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error(
            "IconValidator: failed to parse {}: {}", TEXTURE_PATH_FILE, e.what());
        return false;
    } catch (...) {
        lib().getSelf().getLogger().error(
            "IconValidator: unknown error parsing {}", TEXTURE_PATH_FILE);
        return false;
    }
}

bool IconValidator::isValidTexturePath(std::string const& path) const {
    if (!mReady || path.empty()) return false;
    return mTexturePaths.find(path) != mTexturePaths.end();
}

bool IconValidator::isValidItemId(std::string const& itemId) const {
    if (!mReady || itemId.empty()) return false;
    return mItemToTexture.find(itemId) != mItemToTexture.end();
}

std::string IconValidator::getTexturePathFor(std::string const& itemId) const {
    if (!mReady || itemId.empty()) return "";
    auto it = mItemToTexture.find(itemId);
    if (it == mItemToTexture.end()) return "";
    return it->second;
}

std::string IconValidator::normalizeIcon(std::string const& icon) const {
    // 未初始化时不修改，原样返回（避免误清除合法图标）
    if (!mReady) return icon;

    if (icon.empty()) return "";

    // 1. 若是合法纹理路径（在值集合中）→ 原样返回
    if (mTexturePaths.find(icon) != mTexturePaths.end()) {
        return icon;
    }

    // 2. 若是合法物品 ID（在键中）→ 转换为纹理路径
    auto it = mItemToTexture.find(icon);
    if (it != mItemToTexture.end()) {
        return it->second;
    }

    // 3. 若以 "textures/" 开头但不在已知值中 → 原样保留（可能是自定义纹理）
    if (startsWith(icon, "textures/")) {
        return icon;
    }

    // 4. 其他情况 → 非法图标，清空
    return "";
}

std::string IconValidator::resolveIcon(std::string const& icon, std::string const& iconItem) const {
    // 优先级：icon（文本路径）> iconItem（物品 ID）> 无图标

    // 1. 先尝试 icon 文本路径（规范化后非空则使用）
    if (!icon.empty()) {
        std::string normalized = normalizeIcon(icon);
        if (!normalized.empty()) {
            return normalized;
        }
    }

    // 2. icon 为空或非法，回退到 iconItem（物品 ID）
    if (!iconItem.empty()) {
        std::string texPath = getTexturePathFor(iconItem);
        if (!texPath.empty()) {
            return texPath;
        }
    }

    // 3. 两者都无效 → 无图标
    return "";
}

} // namespace meowmenu
