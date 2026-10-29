#pragma once

#include <cmath>

namespace ui {

using Easing = float (*)(float);

namespace easing {

constexpr float linear(float t) {
    return t;
}

constexpr float quadIn(float t) {
    return t * t;
}
constexpr float quadOut(float t) {
    return t * (2.0f - t);
}
constexpr float quadInOut(float t) {
    return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
}

constexpr float cubicIn(float t) {
    return t * t * t;
}
constexpr float cubicOut(float t) {
    const auto u = 1.0f - t;
    return 1.0f - u * u * u;
}
constexpr float cubicInOut(float t) {
    if (t < 0.5f)
        return 4.0f * t * t * t;
    const auto u = 1.0f - t;
    return 1.0f - 4.0f * u * u * u;
}

inline float expoOut(float t) {
    return t >= 1.0f ? 1.0f : 1.0f - std::exp2(-10.0f * t);
}

constexpr float backOut(float t) {
    constexpr float c1 = 1.70158f;
    const auto u = t - 1.0f;
    return 1.0f + (c1 + 1.0f) * u * u * u + c1 * u * u;
}

}  // namespace easing

// Duration scales with the distance travelled so partial retargets feel even.
struct Tween {
    void to(float target, float fullDuration, Easing easing) {
        if (target == _target) {
            return;
        }
        _start = _value;
        _target = target;
        _easing = easing;
        _elapsed = 0.0f;
        _duration = fullDuration * std::fabs(target - _start);
        if (_duration <= 0.0f)
            _value = _target;
    }

    void jump(float value) {
        _value = _start = _target = value;
        _elapsed = _duration = 0.0f;
    }

    void settle(float target, float fullDuration, Easing easing) {
        if (!_primed) {
            _primed = true;
            jump(target);
        } else {
            to(target, fullDuration, easing);
        }
    }

    void update(float dt) {
        if (_duration <= 0.0f) {
            _value = _target;
            return;
        }
        _elapsed += dt;
        if (_elapsed >= _duration) {
            _elapsed = _duration;
            _value = _target;
            return;
        }
        _value = _start + (_target - _start) * _easing(_elapsed / _duration);
    }

    float value() const {
        return _value;
    }
    bool done() const {
        return _elapsed >= _duration;
    }

private:
    float _value = 0.0f;
    float _start = 0.0f;
    float _target = 0.0f;
    float _elapsed = 0.0f;
    float _duration = 0.0f;
    bool _primed = false;
    Easing _easing = easing::linear;
};

}  // namespace ui
