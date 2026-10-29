#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "config.h"
#include "engine.h"

namespace mac {

enum struct MenuCommand {
    toggleTracking,
    showSettings,
    toggleOverlay,
    hideSettings,
    quit,
};

// Before raylib creates the window: stops GLFW installing its menu bar and Regular activation policy.
void prepareGlfw();

// `onCommand` runs on the main thread, inside the event wait.
void install(std::function<void(MenuCommand)> onCommand, std::function<void()> onReopen);

void setTrayState(bool available, bool tracking, bool overlay);

void setDockIconVisible(bool visible);

void wake();

// Main thread only.
void wakeAfter(double seconds);

void setDarkAppearance(bool dark);

// Native non-activating NSPanel: the only kind that may float over another app's fullscreen Space.
struct FingerOverlay {
    FingerOverlay();
    ~FingerOverlay();

    FingerOverlay(const FingerOverlay &) = delete;
    FingerOverlay &operator=(const FingerOverlay &) = delete;

    void setVisible(bool visible);

    // Both areas must be filled in.
    void setConfig(const macosu_pad::Config &config);

    void update(const macosu_pad::MonitorFrame &frame);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

}  // namespace mac
