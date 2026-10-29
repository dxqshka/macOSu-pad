#include "multitouch.h"

#include <dlfcn.h>

namespace macosu_pad::mt {

namespace {

constexpr const char *kFramework = "/System/Library/PrivateFrameworks/MultitouchSupport.framework/MultitouchSupport";

struct Loaded {
    Api api{};
    const char *error = nullptr;
};

template <typename F>
bool resolve(void *handle, const char *name, F &out) {
    out = reinterpret_cast<F>(dlsym(handle, name));
    return out != nullptr;
}

Loaded load() {
    Loaded loaded;
    void *handle = dlopen(kFramework, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        loaded.error = "the MultitouchSupport framework is not available on this macOS";
        return loaded;
    }
    auto &a = loaded.api;
    const bool ok = resolve(handle, "MTDeviceCreateList", a.createList) &&
                    resolve(handle, "MTDeviceGetFamilyID", a.getFamilyId) &&
                    resolve(handle, "MTDeviceGetSensorSurfaceDimensions", a.getSensorSurfaceDimensions) &&
                    resolve(handle, "MTRegisterContactFrameCallback", a.registerContactFrameCallback) &&
                    resolve(handle, "MTDeviceStart", a.start) && resolve(handle, "MTDeviceStop", a.stop);
    if (!ok) {
        loaded.error = "MultitouchSupport lacks an expected function on this macOS";
    }
    return loaded;
}

}  // namespace

const Api *api(const char **error) {
    static const Loaded loaded = load();
    if (error) {
        *error = loaded.error;
    }
    return loaded.error ? nullptr : &loaded.api;
}

}  // namespace macosu_pad::mt
