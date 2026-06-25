// FormManager.h - 表单ID分配与回调注册表
//
// 管理原始数据包方式发送的 SimpleForm 的 formId 分配和回调。
// 由于使用协议层原始数据包发送（不走 BDS 的 FormIdManager），
// 需要自行分配 formId 并在 ModalFormResponsePacket 到达时查找回调。
//
// formId 范围: 从 0x10000 (65536) 开始，避免与 BDS 内部 formId 冲突。
#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace meowmenu {

// 表单响应回调类型
// - buttonIndex: 玩家点击的按钮索引（从0开始），取消时为 nullopt
using FormCallback = std::function<void(uint32_t formId, std::optional<int> buttonIndex)>;

class FormManager {
public:
    static FormManager& getInstance();

    // 初始化
    void init();

    // 关闭清理
    void shutdown();

    // 分配 formId 并注册回调
    // Returns: 分配的 formId，0 表示失败
    uint32_t allocate(FormCallback callback);

    // 查找并消费回调（查找后从注册表移除）
    // Returns: 回调函数，未找到返回空
    FormCallback consume(uint32_t formId);

    // 查找回调（不移除）
    bool hasForm(uint32_t formId) const;

private:
    FormManager() = default;

    mutable std::mutex mMutex;
    uint32_t           mNextFormId = 0x10000; // 从 65536 开始，避免与 BDS 冲突
    std::unordered_map<uint32_t, FormCallback> mCallbacks;
};

} // namespace meowmenu
