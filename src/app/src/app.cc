#include "app.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <raylib.h>

namespace macosu_pad {

namespace {

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 720;
constexpr int kMinWindowWidth = 760;
constexpr int kMinWindowHeight = 560;

void report(const std::string &message) {
    std::fprintf(stderr, "macOSu-pad: %s\n", message.c_str());
}

std::string displayPath(const std::filesystem::path &path) {
    const std::string text = path.string();
    const char *home = std::getenv("HOME");
    if (home && *home && text.starts_with(home) && (text.size() == std::strlen(home) || text[std::strlen(home)] == '/')) {
        return "~" + text.substr(std::strlen(home));
    }
    return text;
}

}  // namespace

App::App(std::filesystem::path configPath) : _configPath(std::move(configPath)) {
    Config config;
    if (auto loaded = load(_configPath)) {
        config = std::move(*loaded);
    } else {
        _startupError = loaded.error() + "\nUsing default settings.";
    }
    if (auto engine = Engine::create(std::move(config))) {
        _engine = std::move(*engine);
    } else {
        _engineError = engine.error();
    }
}

App::~App() {
    if (_engine) {
        _engine->stopTracking();
        _engine->setMonitor({});
    }
    _overlay.reset();
    _window.reset();
    if (IsWindowReady()) {
        CloseWindow();
    }
}

bool App::tracking() const {
    return _engine && _engine->isTracking();
}

std::expected<Engine *, std::string> App::engine() {
    if (!_engine) {
        return std::unexpected(_engineError);
    }
    return _engine.get();
}

int App::run() {
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    mac::prepareGlfw();
    InitWindow(kWindowWidth, kWindowHeight, "macOSu-pad");
    if (!IsWindowReady()) {
        report("cannot open the settings window");
        return 1;
    }
    SetWindowMinSize(kMinWindowWidth, kMinWindowHeight);
    // Escape belongs to the controls (closing popups), not to quitting.
    SetExitKey(KEY_NULL);
    // Sleep in the event wait instead of spinning; live frames and timers wake it.
    EnableEventWaiting();

    mac::install(
      [this](mac::MenuCommand command) {
          onCommand(command);
      },
      [this] {
          showWindow();
      });

    _window = std::make_unique<settings::SettingsWindow>(settings::Requests{
      .apply =
        [this](const Config &config) {
            apply(config);
        },
      .save =
        [this](const Config &config) {
            save(config);
        },
      .revert =
        [this] {
            revert();
        },
      .toggle =
        [this] {
            if (auto ok = toggle(); !ok) {
                pushState(&ok.error(), nullptr);
            }
        },
      .overlay =
        [this](bool on) {
            if (auto ok = setOverlay(on); !ok) {
                pushState(&ok.error(), nullptr);
            }
        },
      .wakeAfter =
        [](double seconds) {
            mac::wakeAfter(seconds);
        },
    });
    refreshTray();
    // Opening the app shows the window, in case the menu bar has no room for the icon.
    showWindow();

    while (!_quit) {
        std::optional<MonitorFrame> frame;
        {
            std::lock_guard lock(_frameMutex);
            frame = std::exchange(_pendingFrame, std::nullopt);
        }
        if (frame) {
            pushFrame(*frame);
        }

        if (_windowVisible && WindowShouldClose()) {
            hideWindow();
        }
        if (_windowVisible) {
            // Draws, then waits for the next event inside EndDrawing.
            _window->frame();
        } else {
            PollInputEvents();
        }
    }
    return 0;
}

void App::onCommand(mac::MenuCommand command) {
    switch (command) {
        case mac::MenuCommand::toggleTracking:
            if (auto ok = toggle(); !ok) {
                showWindow();
                pushState(&ok.error(), nullptr);
            }
            break;
        case mac::MenuCommand::showSettings:
            showWindow();
            break;
        case mac::MenuCommand::toggleOverlay:
            if (auto ok = setOverlay(!_overlayOn); !ok) {
                showWindow();
                pushState(&ok.error(), nullptr);
            }
            break;
        case mac::MenuCommand::hideSettings:
            hideWindow();
            break;
        case mac::MenuCommand::quit:
            if (_engine) {
                _engine->stopTracking();
                _engine->setMonitor({});
            }
            _quit = true;
            break;
    }
}

void App::refreshTray() {
    mac::setTrayState(_engine != nullptr, tracking(), _overlayOn);
}

std::expected<void, std::string> App::toggle() {
    auto e = engine();
    if (!e) {
        return std::unexpected(e.error());
    }
    std::expected<void, std::string> result;
    if ((*e)->isTracking()) {
        (*e)->stopTracking();
    } else {
        result = (*e)->startTracking();
    }
    refreshTray();
    pushState(nullptr, nullptr);
    return result;
}

void App::showWindow() {
    mac::setDockIconVisible(true);
    ClearWindowState(FLAG_WINDOW_HIDDEN);
    SetWindowFocused();
    _windowVisible = true;
    syncMonitor();
    const auto startupError = std::exchange(_startupError, std::nullopt);
    pushState(startupError ? &*startupError : nullptr, nullptr);
}

void App::hideWindow() {
    SetWindowState(FLAG_WINDOW_HIDDEN);
    mac::setDockIconVisible(false);
    _windowVisible = false;
    syncMonitor();
}

std::expected<void, std::string> App::setOverlay(bool on) {
    if (on && !_overlay) {
        _overlay = std::make_unique<mac::FingerOverlay>();
    }
    if (_overlay) {
        _overlay->setVisible(on);
    }
    _overlayOn = on;
    refreshTray();
    syncMonitor();
    pushState(nullptr, nullptr);
    return {};
}

void App::syncMonitor() {
    if (!_engine) {
        return;
    }
    if (_windowVisible || _overlayOn) {
        _engine->setMonitor([this](const MonitorFrame &frame) {
            {
                std::lock_guard lock(_frameMutex);
                _pendingFrame = frame;
            }
            mac::wake();
        });
    } else {
        _engine->setMonitor({});
    }
}

std::expected<void, std::string> App::setConfig(const Config &config) {
    if (auto valid = validate(config); !valid) {
        return valid;
    }
    auto e = engine();
    if (!e) {
        return std::unexpected(e.error());
    }
    auto result = (*e)->setConfig(config);
    refreshTray();
    return result;
}

void App::apply(const Config &config) {
    if (auto ok = setConfig(config); !ok) {
        pushState(&ok.error(), nullptr);
        return;
    }
    pushState(nullptr, nullptr);
}

void App::save(const Config &config) {
    auto ok = setConfig(config);
    if (ok) {
        ok = macosu_pad::save(_configPath, config);
    }
    if (!ok) {
        pushState(&ok.error(), nullptr);
        return;
    }
    const std::string notice = "Saved";
    pushState(nullptr, &notice);
}

void App::revert() {
    auto loaded = load(_configPath);
    std::expected<void, std::string> ok = loaded ? setConfig(*loaded) : std::unexpected(loaded.error());
    if (!ok) {
        pushState(&ok.error(), nullptr);
        return;
    }
    const std::string notice = "Reloaded";
    pushState(nullptr, &notice);
}

void App::pushState(const std::string *error, const std::string *notice) {
    const Size screen = mainDisplaySize();
    const Size trackpadMm = _engine ? _engine->trackpadSizeMm() : Size{128.0, 80.0};
    Config config = _engine ? _engine->config() : Config{};
    config.trackpadArea = config.trackpadArea.value_or(TrackpadArea::full(trackpadMm));
    config.displayArea = config.displayArea.value_or(DisplayArea::full(screen));
    mac::setDarkAppearance(config.theme == Theme::dark);

    if (_overlay) {
        _overlay->setConfig(config);
    }
    if (!_window) {
        return;
    }
    settings::State state{
      .available = _engine != nullptr,
      .config = std::move(config),
      .tracking = tracking(),
      .overlay = _overlayOn,
      .trackpadMm = trackpadMm,
      .screen = screen,
      .configPath = displayPath(_configPath),
    };
    if (error) {
        state.error = *error;
    } else if (!_engine) {
        state.error = _engineError;
    }
    if (notice) {
        state.notice = *notice;
    }
    _window->setState(std::move(state));
    mac::wake();
}

void App::pushFrame(const MonitorFrame &frame) {
    if (_windowVisible) {
        _window->onFrame(frame);
    }
    if (_overlayOn && _overlay) {
        _overlay->update(frame);
    }
}

}  // namespace macosu_pad
