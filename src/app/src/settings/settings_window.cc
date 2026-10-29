#include "settings_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <format>

#include <raylib.h>

#include <ui/backend/raylib_input.h>

#include "area_geometry.h"
#include "pads.h"
#include "theme.h"
#include "widgets.h"

namespace settings {

namespace {

// Poppins, SIL Open Font License.
const unsigned char kPoppinsRegular[] = {
#embed "../../../../assets/fonts/Poppins-Regular.ttf"
};
const unsigned char kPoppinsSemiBold[] = {
#embed "../../../../assets/fonts/Poppins-SemiBold.ttf"
};

constexpr float kLineHeight = 1.4f;
constexpr double kStretchWarn = 0.02;
constexpr double kTooltipDelay = 0.7;
constexpr double kActiveAfterInput = 0.25;

struct TipPreset {
    const char *label;
    const char *tooltip;
    std::array<double, 2> anchor;
};

constexpr std::array<TipPreset, 3> kTipPresets{{
  {"Finger center", "No shift", {0.0, 0.0}},
  {"Front edge", "Front edge of the touch", {1.0, 0.0}},
  {"Fingertip", "Middle of the fingertip", {1.0, 1.0}},
}};

std::optional<double> parseNumber(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    const std::string copy(text);
    char *end = nullptr;
    const double value = std::strtod(copy.c_str(), &end);
    if (end != copy.c_str() + copy.size() || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

ui::Style column(int gap) {
    return ui::Style::empty().flexDir(ui::FlexDir::column).gap(ui::Value::pt(static_cast<float>(gap)));
}

ui::Style row(int gap) {
    return ui::Style::empty()
      .flexDir(ui::FlexDir::row)
      .alignItems(ui::AlignItems::center)
      .gap(ui::Value::pt(static_cast<float>(gap)));
}

ui::Style cardStyle(float grow) {
    return column(8)
      .padding(ui::Edges(14, 12, 14, 12))
      .flexGrow(grow)
      .flexShrink(1.0f)
      .flexBasis(ui::Value::pt(0))
      .minWidth(ui::Value::pt(0))
      .minHeight(ui::Value::pt(0));
}

int measure(std::string_view text, int font) {
    return ui::measurePainter()->measureText(text, font).size.x;
}

struct TooltipBox final : Control {
protected:
    void paint(ui::Painter &p, const ui::Rect &r) override {
        const auto &c = theme::colors();
        p.fillRect(ui::Rect(r.left(), r.top() + 2, r.right(), r.bottom() + 3), ui::Color(0, 0, 0, 36), 6.0f);
        p.fillRect(r, c.panel, 6.0f);
        p.drawRect(r, c.border, 6.0f, 1.0f);
    }
};

}  // namespace

struct SettingsWindow::Widgets {
    SegmentedControl *theme = nullptr;
    Button *toggle = nullptr;
    Banner *banner = nullptr;
    Group *areas = nullptr;
    Group *settings = nullptr;

    DisplayPad *displayPad = nullptr;
    NumberField *daWidth = nullptr, *daHeight = nullptr, *daX = nullptr, *daY = nullptr;
    Text *daMeta = nullptr;

    TrackpadPad *trackpadPad = nullptr;
    NumberField *taWidth = nullptr, *taHeight = nullptr, *taX = nullptr, *taY = nullptr, *taRotation = nullptr;
    Check *lockAspect = nullptr;
    Text *taMeta = nullptr;

    Select *tipPreset = nullptr;
    NumberField *tipFront = nullptr, *tipWidth = nullptr;
    Select *tipDirection = nullptr;

    Check *emitMouseEvents = nullptr;
    Check *hideCursor = nullptr;
    Check *multitouch = nullptr;
    Check *overlay = nullptr;

    Text *path = nullptr;
    Button *revert = nullptr;
    Button *save = nullptr;

    TooltipBox *tooltip = nullptr;
    Text *tooltipText = nullptr;
};

SettingsWindow::SettingsWindow(Requests requests)
    : _requests(std::move(requests)),
      _painter(GetWindowScaleDPI().x),
      _root(std::make_unique<ui::Root>()),
      _w(std::make_unique<Widgets>()) {
    namespace f = theme::fonts;
    const auto font = [&](std::span<const unsigned char> ttf, float px) {
        return _painter.addFont(ttf, px, std::round(px * kLineHeight));
    };
    f::body = font(kPoppinsRegular, 13.0f);
    f::small = font(kPoppinsRegular, 12.0f);
    f::tiny = font(kPoppinsRegular, 11.0f);
    f::tinyBold = font(kPoppinsSemiBold, 11.0f);
    f::heading = font(kPoppinsSemiBold, 13.0f);
    f::title = font(kPoppinsSemiBold, 15.0f);
    ui::setMeasurePainter(&_painter);

    _router.attachRoot(_root.get());
    host() = {_root.get(), &_router};

    auto &w = *_w;
    auto *body = box(
      _root->content(),
      column(12).padding(ui::Edges(18, 14, 18, 14)).flexGrow(1.0f).width(ui::Value::pct(100)).height(ui::Value::pct(100)));
    _router.pushFocusScope(body);

    const auto numberInput =
      [this](NumberField *field, std::function<bool(double)> commit, std::function<double()> current, double step) {
          field->onCommit = [this, commit](std::string_view text) {
              const auto value = parseNumber(text);
              const bool ok = value && commit(*value);
              render();
              if (ok) {
                  apply();
              }
          };
          field->onStep = [this, commit, current, step](int direction) {
              if (!_state) {
                  return;
              }
              const bool ok = commit(current() + direction * step);
              render();
              if (ok) {
                  apply();
              }
          };
      };
    const auto onClick = [this](Button *button, std::function<void()> change) {
        button->onClick = [this, change] {
            if (!_state) {
                return;
            }
            change();
            render();
            apply();
        };
    };
    const auto heading = [](ui::Widget *card, std::string title) {
        auto *h2 = box(card, row(8));
        add<Text>(h2, std::move(title), theme::fonts::heading, Tone::fg, Text::Mode::single);
    };
    const auto field = [](ui::Widget *fields, std::string label, std::string tooltip) {
        auto *container = add<Control>(fields);
        container->setStyle(column(2)
                              .justifyContent(ui::Justify::end)
                              .flexGrow(1.0f)
                              .flexShrink(1.0f)
                              .flexBasis(ui::Value::pt(0))
                              .minWidth(ui::Value::pt(0)));
        container->tooltip = std::move(tooltip);
        add<Text>(container, std::move(label), theme::fonts::tiny, Tone::muted, Text::Mode::ellipsis);
        return add<NumberField>(container);
    };
    const auto smallButton = [](ui::Widget *parent, std::string label, std::string tooltip) {
        auto *button = add<Button>(parent, std::move(label), Button::Kind::small);
        button->tooltip = std::move(tooltip);
        return button;
    };

    auto *header = box(body, row(12));
    add<Text>(header, "macOSu-pad", theme::fonts::title, Tone::fg, Text::Mode::single);
    w.theme = add<SegmentedControl>(header, std::vector<std::string>{"Dark", "Light"});
    w.theme->tooltip = "Window theme";
    spacer(header);
    w.toggle = add<Button>(header, "Start", Button::Kind::primary);
    w.toggle->tooltip = "Start or stop moving the cursor";
    w.toggle->onClick = [this] {
        if (_requests.toggle) {
            _requests.toggle();
        }
    };

    w.banner = add<Banner>(body);

    w.areas = add<Group>(body);
    w.areas->setStyle(row(14)
                        .alignItems(ui::AlignItems::stretch)
                        .flexGrow(1.0f)
                        .flexShrink(1.0f)
                        .flexBasis(ui::Value::pt(0))
                        .minHeight(ui::Value::pt(270)));

    auto *displayCard = add<Card>(w.areas);
    displayCard->setStyle(cardStyle(1.0f));
    heading(displayCard, "Screen");
    w.displayPad = add<DisplayPad>(displayCard);
    w.displayPad->tooltip = "Drag the blue box to move it";
    auto *daFields = box(displayCard, row(8).alignItems(ui::AlignItems::end));
    w.daWidth = field(daFields, "Width", "Pixels");
    w.daHeight = field(daFields, "Height", "Pixels");
    w.daX = field(daFields, "X", "Center, pixels from the left");
    w.daY = field(daFields, "Y", "Center, pixels from the top");
    auto *daButtons = box(displayCard, row(8));
    auto *daFull = smallButton(daButtons, "Full", "Whole screen");
    auto *daCenter = smallButton(daButtons, "Center", "Move to the middle");
    w.daMeta = add<Text>(displayCard, "", theme::fonts::tiny, Tone::muted);

    auto *trackpadCard = add<Card>(w.areas);
    trackpadCard->setStyle(cardStyle(1.0f));
    heading(trackpadCard, "Trackpad");
    w.trackpadPad = add<TrackpadPad>(trackpadCard);
    w.trackpadPad->tooltip = "Drag the blue box to move it. ▲ is the top of the screen box.";
    auto *taFields = box(trackpadCard, row(8).alignItems(ui::AlignItems::end));
    w.taWidth = field(taFields, "Width", "Millimetres");
    w.taHeight = field(taFields, "Height", "Millimetres");
    w.taX = field(taFields, "X", "Center, mm from the left");
    w.taY = field(taFields, "Y", "Center, mm from the top");
    w.taRotation = field(taFields, "Angle", "Degrees, clockwise");
    auto *taButtons = box(trackpadCard, row(8));
    auto *taFull = smallButton(taButtons, "Full", "Whole trackpad");
    auto *taCenter = smallButton(taButtons, "Center", "Move to the middle");
    spacer(taButtons);
    w.lockAspect = add<Check>(taButtons, "Keep ratio");
    w.lockAspect->tooltip = "Same shape as the screen box, so movement is not stretched";
    w.taMeta = add<Text>(trackpadCard, "", theme::fonts::tiny, Tone::muted);

    w.settings = add<Group>(body);
    w.settings->setStyle(row(14).alignItems(ui::AlignItems::stretch).flexShrink(0.0f));

    const auto labelWidth = [](std::initializer_list<const char *> labels) {
        int width = 0;
        for (const char *label : labels) {
            width = std::max(width, measure(label, theme::fonts::small));
        }
        return width;
    };
    const auto label = [&](ui::Widget *parent, std::string text, std::string tooltip, int width) {
        auto *l = add<Text>(parent, std::move(text), theme::fonts::small, Tone::fg, Text::Mode::single);
        l->tooltip = std::move(tooltip);
        l->setStyle(ui::Style::empty().width(ui::Value::pt(static_cast<float>(width))).flexShrink(0.0f));
        return l;
    };
    const auto grow =
      ui::Style::empty().flexGrow(1.0f).flexShrink(1.0f).flexBasis(ui::Value::pt(0)).minWidth(ui::Value::pt(40));

    auto *tipCard = add<Card>(w.settings);
    tipCard->setStyle(cardStyle(1.0f));
    heading(tipCard, "Aim");
    auto *tip = box(tipCard, column(6));
    const int tipLabels = labelWidth({"Point", "Forward", "Direction"});

    constexpr const char *kPointTip = "Which part of the finger points";
    auto *pointRow = box(tip, row(8));
    label(pointRow, "Point", kPointTip, tipLabels);
    std::vector<Select::Option> tipOptions;
    for (const auto &preset : kTipPresets) {
        tipOptions.push_back({preset.label, preset.tooltip});
    }
    tipOptions.push_back({"Custom", "Your own Forward and Back"});
    w.tipPreset = add<Select>(pointRow, std::move(tipOptions));
    w.tipPreset->tooltip = kPointTip;
    w.tipPreset->setStyle(grow);

    constexpr const char *kForwardTip = "Shift toward the fingertip; 1 = front edge";
    constexpr const char *kBackTip = "Shift back by half the finger width";
    auto *amountRow = box(tip, row(8));
    label(amountRow, "Forward", kForwardTip, tipLabels);
    w.tipFront = add<NumberField>(amountRow);
    w.tipFront->tooltip = kForwardTip;
    w.tipFront->setStyle(grow);
    label(amountRow, "Back", kBackTip, measure("Back", theme::fonts::small));
    w.tipWidth = add<NumberField>(amountRow);
    w.tipWidth->tooltip = kBackTip;
    w.tipWidth->setStyle(grow);

    constexpr const char *kDirectionTip = "Where the fingertip is";
    auto *directionRow = box(tip, row(8));
    label(directionRow, "Direction", kDirectionTip, tipLabels);
    w.tipDirection = add<Select>(
      directionRow, std::vector<Select::Option>{
                      {"Up", "Toward the top of the trackpad (steadiest)"},
                      {"Along finger", "Along the finger's shape"},
                    });
    w.tipDirection->tooltip = kDirectionTip;
    w.tipDirection->setStyle(grow);

    auto *outputCard = add<Card>(w.settings);
    outputCard->setStyle(cardStyle(1.0f));
    heading(outputCard, "Output");
    auto *checks = box(outputCard, column(8));
    const auto check = [&](std::string text, std::string tooltip) {
        auto *c = add<Check>(checks, std::move(text));
        c->tooltip = std::move(tooltip);
        return c;
    };
    w.emitMouseEvents = check("Send mouse events", "For games like osu! (raw input off). Needs Accessibility.");
    w.hideCursor = check("Hide cursor in fullscreen", "For games that draw their own cursor. Needs Accessibility.");
    w.multitouch = check("Move with 2+ fingers", "Keep moving while more fingers touch");
    w.overlay = check("Finger overlay", "Show your finger around the cursor, over all windows");

    auto *footer = box(body, row(12));
    w.path = add<Text>(footer, "", theme::fonts::tiny, Tone::muted, Text::Mode::ellipsis);
    w.path->setStyle(ui::Style::empty().flexShrink(1.0f).minWidth(ui::Value::pt(0)));
    spacer(footer);
    w.revert = add<Button>(footer, "Revert", Button::Kind::normal);
    w.revert->tooltip = "Reload the settings file";
    w.save = add<Button>(footer, "Save", Button::Kind::primary);
    w.save->tooltip = "Write the settings file";

    w.tooltip = add<TooltipBox>(_root->transientLayer());
    w.tooltip->setPickable(false);
    w.tooltip->setStyle(ui::Style::empty().position(ui::Position::absolute).display(ui::Display::none));
    w.tooltipText = add<Text>(w.tooltip, "", theme::fonts::tiny, Tone::fg);
    w.tooltipText->setPickable(false);

    w.displayPad->onDrag = [this](double x, double y) {
        _cfg.displayArea->x = x;
        _cfg.displayArea->y = y;
        fitDisplay(*_cfg.displayArea, _state->screen);
        render();
    };
    w.trackpadPad->onDrag = [this](double x, double y) {
        _cfg.trackpadArea->x = x;
        _cfg.trackpadArea->y = y;
        fitTrackpad(*_cfg.trackpadArea, _state->trackpadMm);
        render();
    };
    const auto dragEnd = [this](bool moved) {
        if (moved) {
            apply();
        }
    };
    w.displayPad->onDragEnd = dragEnd;
    w.trackpadPad->onDragEnd = dragEnd;

    auto &cfg = _cfg;
    const auto display = [&cfg]() -> macosu_pad::DisplayArea & {
        return *cfg.displayArea;
    };
    const auto trackpad = [&cfg]() -> macosu_pad::TrackpadArea & {
        return *cfg.trackpadArea;
    };
    numberInput(
      w.daWidth,
      [this, display, trackpad](double v) {
          if (roundTo(v, 0) < 1.0) {
              return false;
          }
          display().width = std::min(roundTo(v, 0), _state->screen.width);
          fitDisplay(display(), _state->screen);
          lockAspect(_cfg, _state->trackpadMm, false);
          fitTrackpad(trackpad(), _state->trackpadMm);
          return true;
      },
      [display] {
          return display().width;
      },
      1.0);
    numberInput(
      w.daHeight,
      [this, display, trackpad](double v) {
          if (roundTo(v, 0) < 1.0) {
              return false;
          }
          display().height = std::min(roundTo(v, 0), _state->screen.height);
          fitDisplay(display(), _state->screen);
          lockAspect(_cfg, _state->trackpadMm, false);
          fitTrackpad(trackpad(), _state->trackpadMm);
          return true;
      },
      [display] {
          return display().height;
      },
      1.0);
    numberInput(
      w.daX,
      [this, display](double v) {
          display().x = v;
          fitDisplay(display(), _state->screen);
          return true;
      },
      [display] {
          return display().x;
      },
      1.0);
    numberInput(
      w.daY,
      [this, display](double v) {
          display().y = v;
          fitDisplay(display(), _state->screen);
          return true;
      },
      [display] {
          return display().y;
      },
      1.0);
    numberInput(
      w.taWidth,
      [this, trackpad](double v) {
          if (roundTo(v, 2) <= 0.0) {
              return false;
          }
          trackpad().width = std::min(roundTo(v, 2), trackpadLimits(trackpad(), _state->trackpadMm).width);
          lockAspect(_cfg, _state->trackpadMm, false);
          fitTrackpad(trackpad(), _state->trackpadMm);
          return true;
      },
      [trackpad] {
          return trackpad().width;
      },
      0.1);
    numberInput(
      w.taHeight,
      [this, trackpad](double v) {
          if (roundTo(v, 2) <= 0.0) {
              return false;
          }
          trackpad().height = std::min(roundTo(v, 2), trackpadLimits(trackpad(), _state->trackpadMm).height);
          lockAspect(_cfg, _state->trackpadMm, true);
          fitTrackpad(trackpad(), _state->trackpadMm);
          return true;
      },
      [trackpad] {
          return trackpad().height;
      },
      0.1);
    numberInput(
      w.taX,
      [this, trackpad](double v) {
          trackpad().x = v;
          fitTrackpad(trackpad(), _state->trackpadMm);
          return true;
      },
      [trackpad] {
          return trackpad().x;
      },
      0.1);
    numberInput(
      w.taY,
      [this, trackpad](double v) {
          trackpad().y = v;
          fitTrackpad(trackpad(), _state->trackpadMm);
          return true;
      },
      [trackpad] {
          return trackpad().y;
      },
      0.1);
    numberInput(
      w.taRotation,
      [this, trackpad](double v) {
          trackpad().rotation = normalizeDegrees(roundTo(v, 2));
          fitTrackpad(trackpad(), _state->trackpadMm);
          return true;
      },
      [trackpad] {
          return trackpad().rotation;
      },
      1.0);

    onClick(daFull, [this] {
        const macosu_pad::Size s = _state->screen;
        _cfg.displayArea = macosu_pad::DisplayArea::full(s);
        lockAspect(_cfg, _state->trackpadMm, false);
        fitTrackpad(*_cfg.trackpadArea, _state->trackpadMm);
    });
    onClick(daCenter, [this] {
        _cfg.displayArea->x = _state->screen.width / 2.0;
        _cfg.displayArea->y = _state->screen.height / 2.0;
    });
    onClick(taFull, [this] {
        _cfg.trackpadArea = macosu_pad::TrackpadArea::full(_state->trackpadMm);
        lockAspect(_cfg, _state->trackpadMm, false);
    });
    onClick(taCenter, [this] {
        _cfg.trackpadArea->x = _state->trackpadMm.width / 2.0;
        _cfg.trackpadArea->y = _state->trackpadMm.height / 2.0;
    });
    w.lockAspect->onToggle = [this](bool on) {
        _cfg.lockAspectRatio = on;
        lockAspect(_cfg, _state->trackpadMm, false);
        fitTrackpad(*_cfg.trackpadArea, _state->trackpadMm);
        render();
        apply();
    };

    w.tipPreset->onChange = [this](size_t index) {
        if (index >= kTipPresets.size()) {
            _router.setFocus(_w->tipFront);
            return;
        }
        _cfg.tipAnchor = kTipPresets[index].anchor;
        render();
        apply();
    };
    for (size_t i = 0; i < 2; i++) {
        numberInput(
          i == 0 ? w.tipFront : w.tipWidth,
          [this, i](double v) {
              _cfg.tipAnchor[i] = roundTo(std::clamp(v, -macosu_pad::kMaxTipAnchor, macosu_pad::kMaxTipAnchor), 2);
              return true;
          },
          [this, i] {
              return _cfg.tipAnchor[i];
          },
          0.05);
    }
    w.tipDirection->onChange = [this](size_t index) {
        _cfg.tipDirection = index == 0 ? macosu_pad::TipDirection::up : macosu_pad::TipDirection::finger;
        render();
        apply();
    };
    w.theme->onChange = [this](size_t index) {
        _cfg.theme = index == 0 ? macosu_pad::Theme::dark : macosu_pad::Theme::light;
        render();
        apply();
    };

    const std::array<std::pair<Check *, bool macosu_pad::Config::*>, 3> flags{{
      {w.emitMouseEvents, &macosu_pad::Config::emitMouseEvents},
      {w.hideCursor, &macosu_pad::Config::hideCursorInFullscreen},
      {w.multitouch, &macosu_pad::Config::multitouchMovesCursor},
    }};
    for (const auto &[box, member] : flags) {
        box->onToggle = [this, member](bool on) {
            _cfg.*member = on;
            apply();
        };
    }
    w.overlay->onToggle = [this](bool on) {
        if (_requests.overlay) {
            _requests.overlay(on);
        }
    };
    w.save->onClick = [this] {
        if (_requests.save) {
            _requests.save(_cfg);
        }
    };
    w.revert->onClick = [this] {
        if (_requests.revert) {
            _requests.revert();
        }
    };
}

SettingsWindow::~SettingsWindow() {
    host() = {};
    ui::setMeasurePainter(nullptr);
}

void SettingsWindow::apply() {
    if (_state && _requests.apply) {
        _requests.apply(_cfg);
    }
}

void SettingsWindow::setState(State next) {
    auto &w = *_w;
    const bool dragging = w.displayPad->dragging() || w.trackpadPad->dragging();
    if (!dragging) {
        _cfg = next.config;
    }
    w.theme->setDisabled(!next.available);
    w.toggle->setLabel(next.tracking ? "Stop" : "Start");
    w.path->setText(next.configPath);
    w.path->tooltip = next.configPath;
    w.areas->setEnabled(next.available);
    w.settings->setEnabled(next.available);
    for (auto *button : {w.toggle, w.save, w.revert}) {
        button->setDisabled(!next.available);
    }
    w.displayPad->setCursor(std::nullopt, next.tracking);

    _noticeUntil = 0.0;
    if (next.error) {
        w.banner->show(Banner::Kind::error, *next.error);
    } else if (next.notice) {
        w.banner->show(Banner::Kind::notice, *next.notice);
        _noticeUntil = GetTime() + std::max(3.0, static_cast<double>(next.notice->size()) * 0.06);
    } else {
        w.banner->hide();
    }
    _state = std::move(next);
    render();
}

void SettingsWindow::onFrame(const macosu_pad::MonitorFrame &frame) {
    if (!_state) {
        return;
    }
    _w->trackpadPad->setTouches(frame.touches());
    _w->displayPad->setCursor(frame.cursor, _state->tracking);
}

void SettingsWindow::render() {
    if (!_state) {
        return;
    }
    auto &w = *_w;
    const macosu_pad::Size screen = _state->screen;
    const macosu_pad::Size trackpadMm = _state->trackpadMm;
    const auto &d = *_cfg.displayArea;
    const auto &t = *_cfg.trackpadArea;

    theme::setDark(_cfg.theme == macosu_pad::Theme::dark);
    w.theme->setSelected(_cfg.theme == macosu_pad::Theme::dark ? 0 : 1);

    w.displayPad->setSurface(screen);
    w.displayPad->setArea(d);
    w.trackpadPad->setSurface(trackpadMm);
    w.trackpadPad->setArea(t);

    w.daWidth->setShown(formatRounded(d.width, 2));
    w.daHeight->setShown(formatRounded(d.height, 2));
    w.daX->setShown(formatRounded(d.x, 2));
    w.daY->setShown(formatRounded(d.y, 2));
    w.taWidth->setShown(formatRounded(t.width, 2));
    w.taHeight->setShown(formatRounded(t.height, 2));
    w.taX->setShown(formatRounded(t.x, 2));
    w.taY->setShown(formatRounded(t.y, 2));
    w.taRotation->setShown(formatRounded(t.rotation, 2));
    w.lockAspect->setChecked(_cfg.lockAspectRatio);

    namespace f = theme::fonts;
    const double stretch = std::abs((t.width / t.height) / (d.width / d.height) - 1.0);
    w.daMeta->setSpans({
      {std::format("{} × {} px", formatRounded(d.width, 1), formatRounded(d.height, 1)), f::tinyBold, Tone::fg},
      {std::format(" of {} × {}", screen.width, screen.height), f::tiny, Tone::muted},
    });
    std::vector<Span> taMeta{
      {std::format("{} × {} mm", formatRounded(t.width, 2), formatRounded(t.height, 2)), f::tinyBold, Tone::fg},
      {std::format(" · {:.1f} px per mm", d.width / t.width), f::tiny, Tone::muted},
    };
    const bool stretched = !_cfg.lockAspectRatio && stretch > kStretchWarn;
    if (stretched) {
        taMeta.push_back({" · ", f::tiny, Tone::muted});
        taMeta.push_back({std::format("stretched {:.0f}%", stretch * 100.0), f::tinyBold, Tone::warn});
    }
    w.taMeta->setSpans(std::move(taMeta));
    w.taMeta->tooltip = stretched ? "Shapes differ, so one direction moves faster. Turn on Keep ratio." : "";

    const auto [front, width] = _cfg.tipAnchor;
    const auto preset = std::ranges::find_if(kTipPresets, [&](const TipPreset &p) {
        return p.anchor[0] == front && p.anchor[1] == width;
    });
    w.tipPreset->setSelected(static_cast<size_t>(preset - kTipPresets.begin()));
    w.tipFront->setShown(formatRounded(front, 2));
    w.tipWidth->setShown(formatRounded(width, 2));
    w.tipDirection->setSelected(_cfg.tipDirection == macosu_pad::TipDirection::up ? 0 : 1);

    w.emitMouseEvents->setChecked(_cfg.emitMouseEvents);
    w.hideCursor->setChecked(_cfg.hideCursorInFullscreen);
    w.multitouch->setChecked(_cfg.multitouchMovesCursor);
    w.overlay->setChecked(_state->overlay);
}

void SettingsWindow::updateTooltip(double now) {
    auto &w = *_w;
    ui::Widget *hovered = _router.hovered();
    const bool busy = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || w.displayPad->dragging() || w.trackpadPad->dragging() ||
                      _router.captured() != nullptr;
    if (hovered != _tipTarget || busy) {
        _tipTarget = busy ? nullptr : hovered;
        _tipSince = now;
        _tipAt = _router.pointerPos();
        w.tooltip->setStyle(ui::Style::empty().position(ui::Position::absolute).display(ui::Display::none));
        return;
    }
    const std::string *text = tooltipAt(hovered);
    const bool popupOpen = _root->transientLayer()->children().size() > 1;
    if (!text || popupOpen || now - _tipSince < kTooltipDelay) {
        return;
    }
    w.tooltipText->setText(*text);
    const int maxWidth = 320;
    const glm::ivec2 window(GetScreenWidth(), GetScreenHeight());
    w.tooltip->setStyle(
      ui::Style::empty()
        .position(ui::Position::absolute)
        .left(ui::Value::pt(static_cast<float>(std::clamp(_tipAt.x, 4, std::max(4, window.x - maxWidth - 4)))))
        .top(ui::Value::pt(static_cast<float>(_tipAt.y + 20)))
        .maxWidth(ui::Value::pt(static_cast<float>(maxWidth)))
        .padding(ui::Edges(8, 5, 8, 5)));
}

std::optional<double> SettingsWindow::frame() {
    const double now = GetTime();
    const float dt = static_cast<float>(_lastFrame > 0.0 ? std::min(now - _lastFrame, 0.1) : 0.0);
    _lastFrame = now;

    const bool input = ui::pumpRaylibInput(_router);
    _router.runDeferred();
    _painter.setBackingScale(GetWindowScaleDPI().x);

    if (_noticeUntil > 0.0 && now >= _noticeUntil) {
        _noticeUntil = 0.0;
        _w->banner->hide();
    }
    updateTooltip(now);

    const glm::ivec2 size(GetScreenWidth(), GetScreenHeight());
    _root->resize(size);
    _root->update(dt);
    _root->layout(static_cast<float>(size.x), static_cast<float>(size.y));
    _root->arrange({0, 0});
    if (const ui::Rect tip = _w->tooltip->frame(); !tip.empty() && tip.bottom() > size.y - 4) {
        const int top = std::max(4, _tipAt.y - 8 - tip.height());
        _w->tooltip->arrange({0, top - tip.top()});
    }
    _router.refreshHover();

    BeginDrawing();
    ClearBackground({theme::colors().bg.r(), theme::colors().bg.g(), theme::colors().bg.b(), 255});
    _painter.beginFrame();
    _root->draw(_painter);

    if (input) {
        _activeUntil = now + kActiveAfterInput;
    }
    std::optional<double> wake;
    const auto soon = [&](double seconds) {
        wake = std::min(wake.value_or(seconds), std::max(0.0, seconds));
    };
    if (now < _activeUntil) {
        soon(1.0 / 60.0);
    }
    if (_noticeUntil > 0.0) {
        soon(_noticeUntil - now);
    }
    if (_tipTarget && tooltipAt(_tipTarget) && now - _tipSince < kTooltipDelay) {
        soon(kTooltipDelay - (now - _tipSince));
    }
    if (_requests.wakeAfter && wake) {
        _requests.wakeAfter(*wake);
    }
    EndDrawing();
    return wake;
}

}  // namespace settings
