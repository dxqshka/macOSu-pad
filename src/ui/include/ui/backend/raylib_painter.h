#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <raylib.h>

#include <ui/core/painter.h>

namespace ui {

// Group opacity is approximated by multiplying every color's alpha with the layer stack.
struct RaylibPainter final : Painter {
    explicit RaylibPainter(float backingScale);
    ~RaylibPainter() override;

    RaylibPainter(const RaylibPainter &) = delete;
    RaylibPainter &operator=(const RaylibPainter &) = delete;

    // `size` is in points; the bytes must outlive the painter.
    int addFont(std::span<const uint8_t> ttf, float size, float lineHeight);

    void setBackingScale(float scale);

    void beginFrame();

    void pushClip(const Rect &) override;
    void popClip() override;

    void beginLayer(float opacity) override;
    void endLayer() override;

    void fillRect(const Rect &, Color, float radius, Corners) override;
    void drawRect(const Rect &, Color, float radius, float thickness, Corners) override;
    void maskRoundedCorners(const Rect &, float radius, Color, Corners) override;

    void drawLine(glm::vec2 from, glm::vec2 to, Color, float thickness) override;
    void fillTriangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, Color) override;
    void fillCircle(glm::vec2 center, float radius, Color) override;
    void drawCircle(glm::vec2 center, float radius, Color, float thickness) override;
    void fillConvex(std::span<const glm::vec2> points, Color) override;
    void strokeClosed(std::span<const glm::vec2> points, Color, float thickness) override;

    void drawText(std::string_view utf8, glm::ivec2 pos, Color, int font) override;
    TextMetrics measureText(std::string_view utf8, int font) const override;
    int lineHeight(int font) const override;

private:
    struct FontSlot {
        std::span<const uint8_t> ttf;
        float size;
        float lineHeight;
        Font font;
    };

    void load(FontSlot &slot) const;
    const FontSlot &slotAt(int index) const;
    ::Color convert(Color) const;
    const char *terminated(std::string_view utf8) const;
    void applyClip();

    float _scale;
    std::vector<FontSlot> _fonts;
    std::vector<Rect> _clips;
    std::vector<float> _layers;
    float _opacity = 1.0f;
    mutable std::string _textBuffer;
};

}  // namespace ui
