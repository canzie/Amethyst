/*
 * Slider implementation
 */

#include "components/slider.h"

#include "components/extensions/ui_drag_detector.h"
#include "components/instance.h"
#include "modules/style.h"
#include "rendering/draw_context.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

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

static void s_setupFill(Frame &fill, float fillWidth, const Slider *slider)
{
    Color4 fillColor = slider->getSliderProperties().fillColor;
    fill.setBaseStyleProperties({
        .backgroundColor = Color3(fillColor),
        .backgroundTransparency = 1.0f - fillColor.a,
        .cornerRadius = slider->getBaseStyleProperties().cornerRadius,
    });
    fill.setBaseProperties({
        .interactable = false,
        .position = UDim2::fromOffset(0.0f, 0.0f),
        .size = UDim2({0.0f, 1.0f}, {fillWidth, 0.0f}),
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

template <typename T> static std::string s_formatNumber(const std::string &format, T value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), format.c_str(), value);
    return std::string(buffer);
}

Slider::Slider()
{
    m_track = add<Frame>();
    m_fill = m_track->add<Frame>();
    m_thumb = add<Shape>(thumbShape);
    m_valueLabel = add<TextLabel>();
    resolveStyle();
}

void Slider::resolveStyle()
{
    auto &style = Style::instance();
    setBaseStyleProperties(style.getBaseStyle(ComponentType::SLIDER, getClasses()));
    setSliderProperties(style.getSliderStyle(ComponentType::SLIDER, getClasses()));
}

bool Slider::setSliderProperties(const SliderStyleProperties &props)
{
    bool changed = m_sProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

void Slider::setFormat(std::string format)
{
    if (m_format != format) {
        m_format = std::move(format);
        markDirty();
    }
}

void Slider::layoutTrack(float normalizedPos, float thumbWidth, const std::string &valueText,
                         const std::function<void(float)> &applyNormalized)
{
    float boxWidth = absoluteSize.x;
    float boxHeight = absoluteSize.y;

    float trackHeight = m_sProps.trackHeight.resolve(boxHeight);
    float trackY = (boxHeight - trackHeight) * 0.5f;
    s_setupTrack(*m_track, 0.0f, trackY, boxWidth, trackHeight, this);

    float thumbHeight = std::min(m_sProps.thumbHeight, boxHeight);
    float clampedThumbWidth = std::clamp(thumbWidth, 0.0f, boxWidth);
    float thumbX = normalizedPos * (boxWidth - clampedThumbWidth);
    float thumbY = (boxHeight - thumbHeight) * 0.5f;
    s_setupFill(*m_fill, thumbX, this);
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
    drag->onDragStart = [applyFromX](vec2 startPos) { applyFromX(startPos.x); };
    drag->onDragUpdate = [applyFromX](vec2, vec2 position) { applyFromX(position.x); };

    s_setupValueLabel(*m_valueLabel, this, valueText);
}

void Slider::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        updateComponents();
    }

    vec4 childClip = computeChildClipRect();

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

std::vector<Instance *> Slider::getHittableInstances()
{
    auto result = Instance::getHittableInstances();
    result.push_back(m_track);
    return result;
}

SliderFloat::SliderFloat()
{
    m_format = "%.3f";
}

void SliderFloat::updateComponents()
{
    float current = value ? *value : min;
    float normalized = std::clamp(s_valueToNormalized(current, min, max, scale), 0.0f, 1.0f);

    layoutTrack(normalized, m_sProps.thumbWidth, formatValue(), [this](float n) {
        if (!value) {
            return;
        }
        *value = s_normalizedToValue(n, min, max, scale);
        if (onValueChanged) {
            onValueChanged(*value);
        }
        markDirty();
    });
}

std::string SliderFloat::formatValue() const
{
    return s_formatNumber(m_format, value ? *value : 0.0f);
}

SliderInt::SliderInt()
{
    m_format = "%d";
}

void SliderInt::updateComponents()
{
    int steps = std::max(max - min + 1, 1);
    int current = value ? std::clamp(*value, min, max) : min;
    float normalized = steps > 1 ? static_cast<float>(current - min) / static_cast<float>(steps - 1) : 0.0f;
    float thumbWidth = absoluteSize.x / static_cast<float>(steps);

    layoutTrack(normalized, thumbWidth, formatValue(), [this](float n) {
        if (!value) {
            return;
        }
        float mapped = s_normalizedToValue(n, static_cast<float>(min), static_cast<float>(max), scale);
        *value = std::clamp(static_cast<int>(std::lround(mapped)), min, max);
        if (onValueChanged) {
            onValueChanged(*value);
        }
        markDirty();
    });
}

std::string SliderInt::formatValue() const
{
    return s_formatNumber(m_format, value ? *value : 0);
}

} // namespace Amethyst
