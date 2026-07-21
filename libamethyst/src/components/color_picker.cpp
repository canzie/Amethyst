#include "components/color_picker.h"

#include "components/extensions/ui_drag_detector.h"
#include "components/frame.h"
#include "components/slider.h"
#include "logging/log.h"
#include "rendering/draw_context.h"

#include <algorithm>

namespace Amethyst {

// Linear gradient axis angles: 0deg fills left-to-right (t=0 at the left edge),
// 90deg fills top-to-bottom (t=0 at the top edge).
static constexpr float GRAD_LEFT_TO_RIGHT = 0.0f;
static constexpr float GRAD_TOP_TO_BOTTOM = 90.0f;

static constexpr float BAR_THICKNESS = 14.0f;
static constexpr float BAR_GAP = 6.0f;

static const std::shared_ptr<const Gradient> &s_rainbowGradient()
{
    static const std::shared_ptr<const Gradient> grad =
        Gradient::linear(GRAD_LEFT_TO_RIGHT, {
                                                 {0.0f / 6.0f, hsvToRgb(0.0f / 6.0f, 1.0f, 1.0f)},
                                                 {1.0f / 6.0f, hsvToRgb(1.0f / 6.0f, 1.0f, 1.0f)},
                                                 {2.0f / 6.0f, hsvToRgb(2.0f / 6.0f, 1.0f, 1.0f)},
                                                 {3.0f / 6.0f, hsvToRgb(3.0f / 6.0f, 1.0f, 1.0f)},
                                                 {4.0f / 6.0f, hsvToRgb(4.0f / 6.0f, 1.0f, 1.0f)},
                                                 {5.0f / 6.0f, hsvToRgb(5.0f / 6.0f, 1.0f, 1.0f)},
                                                 {6.0f / 6.0f, hsvToRgb(6.0f / 6.0f, 1.0f, 1.0f)},
                                             });
    return grad;
}

static const std::shared_ptr<const Gradient> &s_shadeGradient()
{
    static const std::shared_ptr<const Gradient> grad =
        Gradient::linear(GRAD_TOP_TO_BOTTOM, {{0.0f, Color4(0.0f, 0.0f, 0.0f, 0.0f)}, {1.0f, Color4(0.0f, 0.0f, 0.0f, 1.0f)}});
    return grad;
}

static void s_setupBarSlider(SliderFloat &bar, float *value, const std::function<void(float)> &onChanged)
{
    bar.value = value;
    bar.min = 0.0f;
    bar.max = 1.0f;
    bar.setFormat("");
    bar.onValueChanged = onChanged;
}

static void s_warnIfUnsupported(ColorModel model, ColorPickerShape shape)
{
    if (model != ColorModel::HSV) {
        AM_LOG_WARN("ColorPicker: only the HSV model is implemented, falling back to HSV");
    }
    if (shape != ColorPickerShape::SQUARE) {
        AM_LOG_WARN("ColorPicker: only the SQUARE shape is implemented, falling back to SQUARE");
    }
}

static void s_layoutField(Frame &field, Frame &shade, Frame &thumb, float hue, float saturation, float value, float thumbRadius,
                          float width, float height)
{
    Color3 pureHue = hsvToRgb(hue, 1.0f, 1.0f);
    auto satGradient = Gradient::linear(GRAD_LEFT_TO_RIGHT, {{0.0f, Color3(1.0f)}, {1.0f, pureHue}});
    field.setBaseStyleProperties({.backgroundColor = Color3::fromGradient(satGradient), .backgroundTransparency = 0.0f});
    field.setBaseProperties({
        .interactable = true,
        .position = UDim2::fromOffset(0.0f, 0.0f),
        .size = UDim2::fromOffset(width, height),
    });

    shade.setBaseStyleProperties({.backgroundColor = Color3::fromGradient(s_shadeGradient()), .backgroundTransparency = 0.0f});
    shade.setBaseProperties({
        .interactable = false,
        .position = UDim2::fromOffset(0.0f, 0.0f),
        .size = UDim2::fromOffset(width, height),
    });

    float thumbX = saturation * width;
    float thumbY = (1.0f - value) * height;
    thumb.setBaseStyleProperties({
        .backgroundColor = hsvToRgb(hue, saturation, value),
        .backgroundTransparency = 0.0f,
        .borderMode = BorderMode::OUTLINE,
        .borderPixelSize = 2.0f,
        .borderColor = Color3(1.0f),
        .borderTransparency = 0.0f,
        .cornerRadius = thumbRadius,
    });
    thumb.setBaseProperties({
        .interactable = false,
        .position = UDim2::fromOffset(thumbX - thumbRadius, thumbY - thumbRadius),
        .size = UDim2::fromOffset(thumbRadius * 2.0f, thumbRadius * 2.0f),
    });
}

static void s_layoutBar(SliderFloat &bar, std::shared_ptr<const Gradient> gradient, float y, float width, Color3 thumbColor,
                        float thumbTransparency)
{
    SliderStyleProperties sliderStyle;
    sliderStyle.trackHeight = UDim::fromOffset(BAR_THICKNESS);
    sliderStyle.thumb.backgroundColor = thumbColor;
    sliderStyle.thumb.backgroundTransparency = thumbTransparency;
    sliderStyle.thumb.borderMode = BorderMode::OUTLINE;
    sliderStyle.thumb.borderPixelSize = 2.0f;
    sliderStyle.thumb.borderColor = Color3(1.0f);
    sliderStyle.thumb.borderTransparency = 0.0f;
    sliderStyle.thumb.cornerRadius = 4.0f;
    bar.setSliderProperties(sliderStyle);
    bar.setBaseStyleProperties({.backgroundColor = Color3::fromGradient(std::move(gradient)), .backgroundTransparency = 0.0f});
    bar.setBaseProperties({
        .interactable = true,
        .position = UDim2::fromOffset(0.0f, y),
        .size = UDim2::fromOffset(width, BAR_THICKNESS),
    });
}

ColorPicker::ColorPicker()
{
    m_field = add<Frame>();
    m_fieldShade = add<Frame>();
    m_fieldThumb = add<Frame>();
    m_hueSlider = add<SliderFloat>();

    s_setupBarSlider(*m_hueSlider, &m_hue, [this](float) { onChannelsChanged(); });

    auto *drag = m_field->addExtension<UIDragDetector>();
    drag->mode = DragMode::FREE;
    auto applyFromAbs = [this](vec2 abs) {
        vec2 size = m_field->absoluteSize;
        if (size.x <= 0.0f || size.y <= 0.0f) {
            return;
        }
        m_saturation = std::clamp((abs.x - m_field->absolutePosition.x) / size.x, 0.0f, 1.0f);
        m_value = 1.0f - std::clamp((abs.y - m_field->absolutePosition.y) / size.y, 0.0f, 1.0f);
        onChannelsChanged();
    };
    drag->onDragStart = [applyFromAbs](vec2 start) { applyFromAbs(start); };
    drag->onDragUpdate = [applyFromAbs](vec2, vec2 position) { applyFromAbs(position); };
}

void ColorPicker::syncFromValue()
{
    pullValue();
    for (auto &child : m_children) {
        child->markDirty();
    }
    markDirty();
}

void ColorPicker::onChannelsChanged()
{
    pushValue();
    markDirty();
}

void ColorPicker::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.setPrimitiveType(PRIMITIVE_RECT);
        pushData(ctx.geometry, data);
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void ColorPicker::arrange()
{
    if (flags & FLAG_DIRTY) {
        updateComponents();
    }
    UIObject::arrange();
}

void Color3Picker::updateComponents()
{
    s_warnIfUnsupported(model, shape);

    float width = absoluteContentSize.x;
    float fieldHeight = std::max(absoluteContentSize.y - (BAR_THICKNESS + BAR_GAP), 1.0f);

    s_layoutField(*m_field, *m_fieldShade, *m_fieldThumb, m_hue, m_saturation, m_value, fieldThumbRadius, width, fieldHeight);
    s_layoutBar(*m_hueSlider, s_rainbowGradient(), fieldHeight + BAR_GAP, width, hsvToRgb(m_hue, 1.0f, 1.0f), 0.0f);
}

void Color3Picker::pullValue()
{
    if (value == nullptr) {
        return;
    }
    vec3 hsv = rgbToHsv(*value);
    m_hue = hsv.x;
    m_saturation = hsv.y;
    m_value = hsv.z;
}

void Color3Picker::pushValue()
{
    Color3 rgb = hsvToRgb(m_hue, m_saturation, m_value);
    if (value != nullptr) {
        *value = rgb;
    }
    if (onValueChanged) {
        onValueChanged(rgb);
    }
}

Color4Picker::Color4Picker()
{
    m_alphaSlider = add<SliderFloat>();
    s_setupBarSlider(*m_alphaSlider, &m_alpha, [this](float) { onChannelsChanged(); });
}

void Color4Picker::updateComponents()
{
    s_warnIfUnsupported(model, shape);

    float width = absoluteContentSize.x;
    float fieldHeight = std::max(absoluteContentSize.y - 2.0f * (BAR_THICKNESS + BAR_GAP), 1.0f);

    s_layoutField(*m_field, *m_fieldShade, *m_fieldThumb, m_hue, m_saturation, m_value, fieldThumbRadius, width, fieldHeight);

    float barY = fieldHeight + BAR_GAP;
    s_layoutBar(*m_hueSlider, s_rainbowGradient(), barY, width, hsvToRgb(m_hue, 1.0f, 1.0f), 0.0f);

    barY += BAR_THICKNESS + BAR_GAP;
    Color3 current = hsvToRgb(m_hue, m_saturation, m_value);
    auto alphaGradient = Gradient::linear(GRAD_LEFT_TO_RIGHT, {{0.0f, Color4(current, 0.0f)}, {1.0f, Color4(current, 1.0f)}});
    s_layoutBar(*m_alphaSlider, alphaGradient, barY, width, current, 1.0f - m_alpha);
}

void Color4Picker::pullValue()
{
    if (value == nullptr) {
        return;
    }
    vec3 hsv = rgbToHsv(Color3(*value));
    m_hue = hsv.x;
    m_saturation = hsv.y;
    m_value = hsv.z;
    m_alpha = value->a;
}

void Color4Picker::pushValue()
{
    Color4 rgba(hsvToRgb(m_hue, m_saturation, m_value), m_alpha);
    if (value != nullptr) {
        *value = rgba;
    }
    if (onValueChanged) {
        onValueChanged(rgba);
    }
}

} // namespace Amethyst
