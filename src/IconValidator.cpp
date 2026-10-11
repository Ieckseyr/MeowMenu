// IconValidator.cpp - 图标路径校验/修复（数据源 = 编译期嵌入的 embedded_texture_path.h）
#include "IconValidator.h"
#include "MeowMenu.h"
#include "embedded_texture_path.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>

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

    try {
        // 分块嵌入的 JSON 先拼接还原，再交给 nlohmann 解析
        auto j = nlohmann::json::parse(embedded_texture_path::text());

        if (!j.is_object()) {
            lib().getSelf().getLogger().error(
                "IconValidator: embedded texture data is not a JSON object");
            return false;
        }

        size_t count = 0;
        for (auto it = j.begin(); it != j.end(); ++it) {
            std::string itemId   = it.key();
            std::string texPath  = it.value().is_string() ? it.value().get<std::string>() : "";

            // 去除纹理路径两端的空白（部分条目有前导空格，如 " spawn_egg_agent"）
            texPath = trim(texPath);
            if (texPath.empty()) continue;

            mItemToTexture.emplace(itemId, texPath);
            // 同时记录到纹理路径集合（用于快速校验）
            mTexturePaths.insert(texPath);
            count++;
        }

        mReady = true;
        lib().getSelf().getLogger().info(
            "IconValidator: loaded {} texture path entries (embedded at build time)", count);
        return true;
    } catch (std::exception const& e) {
        lib().getSelf().getLogger().error(
            "IconValidator: failed to parse embedded texture data: {}", e.what());
        return false;
    } catch (...) {
        lib().getSelf().getLogger().error(
            "IconValidator: unknown error parsing embedded texture data");
        return false;
    }
}

std::vector<std::string> IconValidator::getAllTexturePaths() const {
    std::unordered_set<std::string> unique;
    for (auto const& entry : mItemToTexture) {
        std::string const& path = entry.second;
        if (path.rfind("textures/", 0) == 0) unique.insert(path);
    }
    std::vector<std::string> out(unique.begin(), unique.end());
    std::sort(out.begin(), out.end());
    return out;
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
