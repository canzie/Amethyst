/*
 * Slider implementation
 */

#include "components/slider.h"

#include "components/extensions/ui_drag_detector.h"
#include "components/instance.h"
#include "logging/log.h"
#include "modules/style.h"
#include "rendering/draw_context.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace Amethyst {

static void s_setupTrack(Frame &track, float x, float y, float width, float height, const Slider *slider)
{
    track.setBaseStyleProperties(slider->getBaseStyleProperties());
    track.setBaseProperties({
        .interactable = true,
        .padding = slider->getBaseProperties().padding,
        .position = UDim2::fromOffset(x, y),
        .size = UDim2::fromOffset(width, height),
    });
}

static void s_setupFill(Frame &fill, float fillEnd, vec2 trackSize, const Slider *slider)
{
    // the fill sits inside the border and padding the track takes from the slider, so it ends at
    // the inner right edge and its corners round by what is left of the slider's radius
    const vec4 insets = slider->resolveContentInsets(trackSize);
    const float insetLeft = insets.w;
    const float insetRight = insets.y;
    const float fillWidth = std::max(0.0f, std::min(fillEnd, trackSize.x - insetRight) - insetLeft);
    const uvec4 insetRadius = uvec4(static_cast<uint32_t>(std::max(insetLeft, 0.0f)));
    const uvec4 cornerRadius = max(slider->getBaseStyleProperties().cornerRadius, insetRadius) - insetRadius;

    Color4 fillColor = slider->getSliderProperties().fillColor;
    fill.setBaseStyleProperties({
        .backgroundColor = Color3(fillColor),
        .backgroundTransparency = 1.0f - fillColor.a,
        .borderTransparency = 1.0f,
        .cornerRadius = cornerRadius,
    });
    fill.setBaseProperties({
        .interactable = false,
        .position = UDim2::fromOffset(0.0f, 0.0f),
        .size = UDim2(0.0f, fillWidth, 1.0f, 0.0f),
    });
}

static void s_setupThumb(Shape &thumb, float x, float y, float width, float height, const Slider *slider)
{
    thumb.setKind(slider->thumbShape);
    thumb.setBaseStyleProperties(slider->getSliderProperties().thumb);
    thumb.setBaseProperties({
        .interactable = false,
        .position = UDim2::fromOffset(x, y),
        .size = UDim2::fromOffset(width, height),
    });
}

static void s_setupValueLabel(TextLabel &label, const Slider *slider, const std::string &text)
{
    const TextStyleProperties &textStyle = slider->getSliderProperties().text;
    label.setTextStyleProperties(textStyle);
    label.setText(text);
    label.setBaseStyleProperties({
        .backgroundColor = Color3(0.0f),
        .backgroundTransparency = 1.0f,
    });

    float labelPadding = slider->getSliderProperties().labelPadding;
    UDim4 padding{};
    if (textStyle.textXAlignment == TextXAlignment::LEFT) {
        padding.left = UDim::fromOffset(labelPadding);
    } else if (textStyle.textXAlignment == TextXAlignment::RIGHT) {
        padding.right = UDim::fromOffset(labelPadding);
    }

    label.setBaseProperties({
        .interactable = false,
        .padding = padding,
        .position = UDim2::fromOffset(0, 0),
        .size = UDim2::fromScale(1.0f, 1.0f),
        .zIndex = 2,
    });
}

static float s_valueToNormalized(float value, float min, float max, ValueScale scale)
{
    if (max == min) {
        return 0.0f;
    }
    if (scale == ValueScale::LOGARITHMIC && min > 0.0f && max > 0.0f && value > 0.0f) {
        float logMin = std::log(min);
        float logMax = std::log(max);
        return (std::log(value) - logMin) / (logMax - logMin);
    }
    return (value - min) / (max - min);
}

static float s_normalizedToValue(float normalized, float min, float max, ValueScale scale)
{
    if (scale == ValueScale::LOGARITHMIC && min > 0.0f && max > 0.0f) {
        float logMin = std::log(min);
        float logMax = std::log(max);
        return std::exp(logMin + normalized * (logMax - logMin));
    }
    return min + normalized * (max - min);
}

// a NaN bound would poison every normalisation and an infinite one makes max - min overflow, so
// the range is made finite and ordered before use
static void s_normalizeRange(float &lo, float &hi)
{
    constexpr float LIMIT = std::numeric_limits<float>::max() / 4.0f;
    if (std::isnan(lo)) {
        lo = 0.0f;
    }
    if (std::isnan(hi)) {
        hi = lo;
    }
    lo = std::clamp(lo, -LIMIT, LIMIT);
    hi = std::clamp(hi, -LIMIT, LIMIT);
    if (lo > hi) {
        std::swap(lo, hi);
    }
}

Slider::Slider(NumberKind kind, std::string defaultFormat) : m_format(defaultFormat), m_defaultFormat(std::move(defaultFormat))
{
    if (auto parsed = NumberFormat::parse(m_defaultFormat, kind); parsed.has_value()) {
        m_numberFormat = *parsed;
    }
    m_track = add<Frame>();
    m_fill = m_track->add<Frame>();
    m_thumb = add<Shape>(thumbShape);
    m_valueLabel = add<TextLabel>();

    resolveStyle();
}

void Slider::resolveStyle()
{
    resolveBaseStyle(ComponentType::SLIDER);

    SliderStyleProperties resolved = Style::instance().getSliderStyle(ComponentType::SLIDER, getClasses(), effectiveGuiState());
    if (m_sProps.apply(resolved)) {
        markDirty();
    }
}

bool Slider::setSliderProperties(const SliderStylePropertiesArgs &props)
{
    bool changed = m_sProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

void Slider::setFormat(std::string format)
{
    if (m_format == format) {
        return;
    }
    std::optional<NumberFormat> parsed = NumberFormat::parse(format, m_numberFormat.kind());
    if (!parsed.has_value()) {
        AM_LOG_WARN("Slider format '{}' needs exactly one {} conversion; using '{}'", format,
                    m_numberFormat.kind() == NumberKind::INTEGER ? "integer" : "floating-point", m_defaultFormat);
        parsed = NumberFormat::parse(m_defaultFormat, m_numberFormat.kind());
    }
    if (parsed.has_value()) {
        m_numberFormat = *parsed;
    }
    m_format = std::move(format);
    markDirty();
}

void Slider::layoutTrack(float normalizedPos, float thumbWidth, const std::string &valueText,
                         const std::function<void(float)> &applyNormalized)
{
    float boxWidth = std::max(absoluteSize.x, 0.0f);
    float boxHeight = std::max(absoluteSize.y, 0.0f);

    float trackHeight = m_sProps.trackHeight.resolve(boxHeight);
    float trackY = (boxHeight - trackHeight) * 0.5f;
    s_setupTrack(*m_track, 0.0f, trackY, boxWidth, trackHeight, this);

    float thumbHeight = std::min(m_sProps.thumbHeight, boxHeight);
    float clampedThumbWidth = std::clamp(thumbWidth, 0.0f, boxWidth);
    float thumbX = normalizedPos * (boxWidth - clampedThumbWidth);
    float thumbY = (boxHeight - thumbHeight) * 0.5f;
    s_setupFill(*m_fill, thumbX, vec2(boxWidth, trackHeight), this);
    s_setupThumb(*m_thumb, thumbX, thumbY, clampedThumbWidth, thumbHeight, this);

    UIDragDetector *drag = m_track->getExtension<UIDragDetector>();
    if (!drag) {
        drag = m_track->addExtension<UIDragDetector>();
        drag->mode = DragMode::HORIZONTAL;
    }

    auto applyFromX = [this, clampedThumbWidth, applyNormalized](float absoluteX) {
        float relative = absoluteX - absolutePosition.x;
        float span = std::max(absoluteSize.x - clampedThumbWidth, 1.0f);
        float normalized = std::clamp((relative - clampedThumbWidth * 0.5f) / span, 0.0f, 1.0f);
        applyNormalized(normalized);
    };
    drag->onDragStart = [this, applyFromX](vec2 startPos) {
        applyFromX(startPos.x);
        m_thumb->setGuiState(static_cast<uint16_t>(m_thumb->getGuiState() | GUI_STATE_PRESSED));
    };
    drag->onDragUpdate = [applyFromX](vec2, vec2 position) { applyFromX(position.x); };
    drag->onDragEnd = [this](vec2) { m_thumb->setGuiState(static_cast<uint16_t>(m_thumb->getGuiState() & ~GUI_STATE_PRESSED)); };

    s_setupValueLabel(*m_valueLabel, this, valueText);
}

void Slider::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void Slider::arrange()
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }
    refreshStyleIfStale();

    if (flags & FLAG_DIRTY) {
        updateComponents();
    }

    vec4 childClip = computeChildClipRect();
    for (auto &child : m_children) {
        if (auto *obj = child->asUiObject()) {
            obj->clipRect = childClip;
            obj->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            obj->arrange();
        }
    }

}

SliderFloat::SliderFloat() : Slider(NumberKind::FLOATING, "%.3f") {}

void SliderFloat::updateComponents()
{
    float lo = min;
    float hi = max;
    s_normalizeRange(lo, hi);
    float current = value != nullptr ? *value : lo;
    float normalized = s_valueToNormalized(current, lo, hi, scale);
    normalized = std::isnan(normalized) ? 0.0f : std::clamp(normalized, 0.0f, 1.0f);

    layoutTrack(normalized, m_sProps.thumbWidth, formatValue(), [this](float n) {
        if (value == nullptr) {
            return;
        }
        float lo = min;
        float hi = max;
        s_normalizeRange(lo, hi);
        *value = std::clamp(s_normalizedToValue(n, lo, hi, scale), lo, hi);
        if (onValueChanged) {
            onValueChanged(*value);
        }
        markDirty();
    });
}

std::string SliderFloat::formatValue() const
{
    return m_numberFormat.format(static_cast<double>(value != nullptr ? *value : 0.0f));
}

SliderInt::SliderInt() : Slider(NumberKind::INTEGER, "%d") {}

void SliderInt::updateComponents()
{
    // int64 throughout: max - min + 1 overflows int for wide ranges, and std::clamp is UB when lo > hi
    int64_t lo = std::min(min, max);
    int64_t hi = std::max(min, max);
    int64_t steps = hi - lo + 1;
    int64_t current = value != nullptr ? std::clamp(static_cast<int64_t>(*value), lo, hi) : lo;
    float normalized = steps > 1 ? static_cast<float>(static_cast<double>(current - lo) / static_cast<double>(steps - 1)) : 0.0f;
    float thumbWidth = absoluteSize.x / static_cast<float>(steps);

    layoutTrack(normalized, thumbWidth, formatValue(), [this](float n) {
        if (value == nullptr) {
            return;
        }
        int64_t lo = std::min(min, max);
        int64_t hi = std::max(min, max);
        double mapped = static_cast<double>(lo) + static_cast<double>(n) * static_cast<double>(hi - lo);
        if (scale == ValueScale::LOGARITHMIC) {
            mapped = s_normalizedToValue(n, static_cast<float>(lo), static_cast<float>(hi), scale);
        }
        int64_t rounded =
            std::isnan(mapped)
                ? lo
                : static_cast<int64_t>(std::clamp(std::round(mapped), static_cast<double>(lo), static_cast<double>(hi)));
        *value = static_cast<int>(std::clamp(rounded, lo, hi));
        if (onValueChanged) {
            onValueChanged(*value);
        }
        markDirty();
    });
}

std::string SliderInt::formatValue() const
{
    return m_numberFormat.format(static_cast<long long>(value != nullptr ? *value : 0));
}

} // namespace Amethyst
