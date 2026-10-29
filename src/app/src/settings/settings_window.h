#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <ui/backend/raylib_painter.h>
#include <ui/core/root.h>
#include <ui/core/router.h>

#include "config.h"
#include "engine.h"

namespace settings {

struct State {
    bool available = false;
    macosu_pad::Config config;
    bool tracking = false;
    bool overlay = false;
    macosu_pad::Size trackpadMm;
    // Points.
    macosu_pad::Size screen;
    std::string configPath;
    std::optional<std::string> error;
    std::optional<std::string> notice;
};

struct Requests {
    std::function<void(const macosu_pad::Config &)> apply;
    std::function<void(const macosu_pad::Config &)> save;
    std::function<void()> revert;
    std::function<void()> toggle;
    std::function<void(bool)> overlay;
    // Called from the frame, on the main thread.
    std::function<void(double)> wakeAfter;
};

struct SettingsWindow {
    // Needs the raylib window (and its GL context) to exist.
    explicit SettingsWindow(Requests requests);
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow &) = delete;
    SettingsWindow &operator=(const SettingsWindow &) = delete;

    void setState(State state);

    void onFrame(const macosu_pad::MonitorFrame &frame);

    // Seconds until the next frame is needed without input; nothing when idle.
    std::optional<double> frame();

private:
    struct Widgets;

    void render();
    void apply();
    void updateTooltip(double now);

    Requests _requests;
    ui::RaylibPainter _painter;
    ui::Router _router;
    std::unique_ptr<ui::Root> _root;
    std::unique_ptr<Widgets> _w;

    std::optional<State> _state;
    macosu_pad::Config _cfg;
    double _noticeUntil = 0.0;
    double _lastFrame = 0.0;
    double _activeUntil = 0.0;

    ui::Widget *_tipTarget = nullptr;
    double _tipSince = 0.0;
    glm::ivec2 _tipAt{0, 0};
};

}  // namespace settings
