#pragma once

// Private framework with no SDK stub: dlopen'd so the app can still start if macOS drops it.

#include <cstdint>

#include <CoreFoundation/CoreFoundation.h>

namespace macosu_pad::mt {

using DeviceRef = CFTypeRef;

struct Point {
    float x;
    float y;
};

struct Vector {
    Point position;
    Point velocity;
};

// Touching states; hover (1, 2), lift-off (5) and linger (6, 7) centres are not the press point.
inline constexpr uint32_t kMakeTouch = 3;
inline constexpr uint32_t kTouching = 4;

// MultitouchSupport contact layout (96 bytes on arm64 and x86_64).
struct Touch {
    int32_t frame;
    double timestamp;
    int32_t pathIndex;
    uint32_t state;
    int32_t fingerId;
    int32_t handId;
    // [0, 1], origin bottom-left.
    Vector normalized;
    float zTotal;
    float pressure;
    float angle;
    float majorAxis;
    float minorAxis;
    Vector absolute;
    int32_t field14;
    int32_t field15;
    float zDensity;
};

static_assert(sizeof(Touch) == 96);

using FrameCallback = int (*)(DeviceRef device, const Touch *touches, int touchCount, double timestamp, int frame);

struct Api {
    CFArrayRef (*createList)();
    int32_t (*getFamilyId)(DeviceRef, int *familyId);
    // Hundredths of a millimetre.
    int32_t (*getSensorSurfaceDimensions)(DeviceRef, int *width, int *height);
    void (*registerContactFrameCallback)(DeviceRef, FrameCallback);
    int32_t (*start)(DeviceRef, int mode);
    int32_t (*stop)(DeviceRef);
};

const Api *api(const char **error = nullptr);

}  // namespace macosu_pad::mt
