// IconValidator.h - 图标路径校验与修复
//
// 基于编译期嵌入的图标库（src/embedded_texture_path.h，由 gen_texture_path_embed.ps1
// 从 texture_path.json 生成）提供：
// - 图标路径校验：判断纹理路径是否合法
// - 图标修复：将物品 ID（如 minecraft:dirt）转换为对应的纹理路径
// - 物品图标解析：根据物品类型 ID 获取纹理路径
//
// 运行时不读取外部文件，texture_path.json 缺失不影响本插件。
//
// 修复规则：
// - 若 icon 是合法纹理路径（在嵌入图标库的值中）→ 原样保留
// - 若 icon 是合法物品 ID（在嵌入图标库的键中）→ 转换为对应的纹理路径
// - 若 icon 以 "textures/" 开头但不在已知值中 → 原样保留（可能是自定义纹理）
// - 否则 → 清空（非法图标）
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace meowmenu {

class IconValidator {
public:
    static IconValidator& getInstance();

    // 初始化：解析嵌入的图标库数据构建查找表（不访问外部文件）
    // 返回是否初始化成功
    bool initialize();

    // 是否已成功初始化
    bool isReady() const { return mReady; }

    // 获取已加载的条目数量
    size_t getEntryCount() const { return mItemToTexture.size(); }

    // 判断是否为合法的纹理路径（存在于嵌入图标库的值集合中）
    bool isValidTexturePath(std::string const& path) const;

    // 判断是否为合法的物品 ID（存在于嵌入图标库的键集合中）
    bool isValidItemId(std::string const& itemId) const;

    // 供图标选择器浏览: 全部合法纹理路径（去重、按路径排序, 仅含 textures/ 开头的规范形式）
    std::vector<std::string> getAllTexturePaths() const;

    // 根据物品 ID 获取对应的纹理路径
    // 找不到返回空字符串
    std::string getTexturePathFor(std::string const& itemId) const;

    // 规范化图标路径（修复非法图标）
    // - 合法纹理路径 → 原样返回
    // - 合法物品 ID  → 转换为纹理路径
    // - 以 "textures/" 开头的未知路径 → 原样返回（可能是自定义纹理）
    // - 其他 → 返回空字符串（非法，应清除）
    std::string normalizeIcon(std::string const& icon) const;

    // 解析最终使用的图标
    // 优先级：icon（文本路径）> iconItem（物品 ID）> 无图标
    // - icon 非空（规范化后）→ 返回 icon
    // - iconItem 非空 → 返回其对应的纹理路径
    // - 两者都空 → 返回空字符串
    std::string resolveIcon(std::string const& icon, std::string const& iconItem) const;

private:
    IconValidator() = default;

    bool                                               mReady = false;
    std::unordered_map<std::string, std::string>       mItemToTexture; // key: item ID, value: texture path
    std::unordered_set<std::string>                    mTexturePaths;  // set of all known texture paths
};

} // namespace meowmenu
