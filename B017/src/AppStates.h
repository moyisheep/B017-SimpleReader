#pragma once

struct AppStates {
    // ---- 取消令牌 ----
    std::shared_ptr<std::atomic_bool> cancelToken;

    // ---- 状态机 ----

    bool isLoaded = false;
    // 工具：生成新令牌，旧令牌立即失效
    void newCancelToken() {
        if (cancelToken) cancelToken->store(true);
        cancelToken = std::make_shared<std::atomic_bool>(false);
    }
};
