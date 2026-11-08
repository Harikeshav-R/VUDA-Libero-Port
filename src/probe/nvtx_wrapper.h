#pragma once

#include <dlfcn.h>
#include <cstdint>

// Dynamic loader wrapper for NVTX so binary does not strictly require -lnvToolsExt at link time.
// If libnvToolsExt.so is available at runtime, functions will be called. Otherwise, no-op.
struct NvtxApi {
    using PushFn = int (*)(const char*);
    using PopFn = int (*)();
    PushFn push = nullptr;
    PopFn pop = nullptr;
};

inline const NvtxApi& nvtx_api() {
    static const NvtxApi api = [] {
        NvtxApi loaded;
        void* handle = dlopen("libnvToolsExt.so", RTLD_NOW | RTLD_GLOBAL);
        if (!handle) handle = dlopen("libnvToolsExt.so.1", RTLD_NOW | RTLD_GLOBAL);
        if (handle) {
            loaded.push = reinterpret_cast<NvtxApi::PushFn>(dlsym(handle, "nvtxRangePushA"));
            loaded.pop = reinterpret_cast<NvtxApi::PopFn>(dlsym(handle, "nvtxRangePop"));
        }
        return loaded;
    }();
    return api;
}

inline void nvtx_push(const char* name) {
    const auto& api = nvtx_api();
    if (api.push && name) api.push(name);
}

inline void nvtx_pop() {
    const auto& api = nvtx_api();
    if (api.pop) api.pop();
}

class NvtxScope {
public:
    explicit NvtxScope(const char* name) {
        nvtx_push(name);
        active_ = true;
    }

    void pop() {
        if (active_) {
            nvtx_pop();
            active_ = false;
        }
    }

    ~NvtxScope() {
        pop();
    }

    NvtxScope(const NvtxScope&) = delete;
    NvtxScope& operator=(const NvtxScope&) = delete;

private:
    bool active_ = false;
};
