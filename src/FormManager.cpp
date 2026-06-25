// FormManager.cpp - 表单ID分配与回调注册表实现
#include "FormManager.h"

namespace meowmenu {

FormManager& FormManager::getInstance() {
    static FormManager instance;
    return instance;
}

void FormManager::init() {
    std::lock_guard lock(mMutex);
    mNextFormId = 0x10000;
    mCallbacks.clear();
}

void FormManager::shutdown() {
    std::lock_guard lock(mMutex);
    mCallbacks.clear();
}

uint32_t FormManager::allocate(FormCallback callback) {
    std::lock_guard lock(mMutex);
    uint32_t id = mNextFormId++;
    // 防止回绕到 BDS 的 formId 范围
    if (mNextFormId < 0x10000) {
        mNextFormId = 0x10000;
    }
    mCallbacks[id] = std::move(callback);
    return id;
}

FormCallback FormManager::consume(uint32_t formId) {
    std::lock_guard lock(mMutex);
    auto it = mCallbacks.find(formId);
    if (it == mCallbacks.end()) return {};
    auto cb = std::move(it->second);
    mCallbacks.erase(it);
    return cb;
}

bool FormManager::hasForm(uint32_t formId) const {
    std::lock_guard lock(mMutex);
    return mCallbacks.find(formId) != mCallbacks.end();
}

} // namespace meowmenu
