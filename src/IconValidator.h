// IconValidator.h - 图标路径校验/修复（基于编译期嵌入的图标库, 运行时不读外部文件）
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

    // 规范化图标路径: 合法纹理/未知 textures/ 路径原样返回, 物品 ID 转纹理路径, 其余清空
    std::string normalizeIcon(std::string const& icon) const;

    // 解析最终图标: icon > iconItem, 都没有则空
    std::string resolveIcon(std::string const& icon, std::string const& iconItem) const;

private:
    IconValidator() = default;

    bool                                               mReady = false;
    std::unordered_map<std::string, std::string>       mItemToTexture; // key: item ID, value: texture path
    std::unordered_set<std::string>                    mTexturePaths;  // set of all known texture paths
};

} // namespace meowmenu
