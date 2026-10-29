#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "config.h"
#include "engine.h"
#include "mac/shell.h"
#include "settings/settings_window.h"

namespace macosu_pad {

struct App {
    explicit App(std::filesystem::path configPath);
    ~App();

    App(const App &) = delete;
    App &operator=(const App &) = delete;

    int run();

private:
    bool tracking() const;
    std::expected<Engine *, std::string> engine();

    void onCommand(mac::MenuCommand command);
    void refreshTray();
    std::expected<void, std::string> toggle();
    void showWindow();
    void hideWindow();
    std::expected<void, std::string> setOverlay(bool on);
    void syncMonitor();
    std::expected<void, std::string> setConfig(const Config &config);

    void apply(const Config &config);
    void save(const Config &config);
    void revert();

    void pushState(const std::string *error, const std::string *notice);
    void pushFrame(const MonitorFrame &frame);

    std::filesystem::path _configPath;
    std::unique_ptr<Engine> _engine;
    std::string _engineError;
    std::optional<std::string> _startupError;

    std::unique_ptr<settings::SettingsWindow> _window;
    bool _windowVisible = false;
    std::unique_ptr<mac::FingerOverlay> _overlay;
    bool _overlayOn = false;
    bool _quit = false;

    // Latest live frame from the MultitouchSupport thread, picked up by the loop.
    std::mutex _frameMutex;
    std::optional<MonitorFrame> _pendingFrame;
};

}  // namespace macosu_pad
