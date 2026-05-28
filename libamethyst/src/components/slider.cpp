/*
 * Slider implementation
 */

#include "components/slider.h"

#include "components/extensions/ui_drag_detector.h"
#include "components/frame.h"
#include "components/text_label.h"
#include "components/window.h"
#include "modules/style.h"
#include "rendering/draw_context.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Amethyst {

static void s_applyStyle(Slider &slider)
{
    const auto &style = Style::instance();
    slider.setBaseProperties({
        .backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::SLIDER),
        .backgroundTransparency = style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::SLIDER),
    });
    slider.setSliderProperties({
        .sliderColor = style.get<Color3>(StyleProperty::SLIDER_COLOR, ComponentType::SLIDER),
        .sliderTransparency = style.get<float>(StyleProperty::SLIDER_TRANSPARENCY, ComponentType::SLIDER),
        .thumbColor = style.get<Color3>(StyleProperty::THUMB_COLOR, ComponentType::SLIDER),
        .thumbTransparency = style.get<float>(StyleProperty::THUMB_TRANSPARENCY, ComponentType::SLIDER),
        .trackCornerRadius = style.get<float>(StyleProperty::TRACK_CORNER_RADIUS, ComponentType::SLIDER),
        .thumbCornerRadius = style.get<float>(StyleProperty::THUMB_CORNER_RADIUS, ComponentType::SLIDER),
        .labelColor = style.get<Color4>(StyleProperty::LABEL_COLOR, ComponentType::SLIDER),
        .labelPadding = style.get<UDim>(StyleProperty::LABEL_PADDING, ComponentType::SLIDER),
        .valueColor = style.get<Color4>(StyleProperty::VALUE_COLOR, ComponentType::SLIDER),
        .fontSize = style.get<float>(StyleProperty::FONT_SIZE, ComponentType::SLIDER),
    });
}

static void s_setupSideLabel(TextLabel &label, const Slider *slider, float &outLabelWidth, float &outLabelHeight)
{
    const auto &sp = slider->getSliderProperties();
    outLabelWidth = 0.0f;
    outLabelHeight = 0.0f;

    if (sp.label.empty()) {
        label.setBaseProperties({.visible = 0});
        return;
    }

    label.setTextProperties({
        .fontSize = sp.fontSize,
        .textColor = sp.labelColor,
        .textYAlignment = TextYAlignment::CENTER,
        .text = sp.label,
    });
    label.setBaseProperties({
        .backgroundColor = slider->getBaseProperties().backgroundColor,
        .backgroundTransparency = slider->getBaseProperties().backgroundTransparency,
        .visible = 1,
    });

    float padding = sp.labelPadding.resolve(
        sp.labelSide == LabelSide::LEFT || sp.labelSide == LabelSide::RIGHT ? slider->absoluteSize.x : slider->absoluteSize.y);

    if (sp.labelSide == LabelSide::LEFT || sp.labelSide == LabelSide::RIGHT) {
        outLabelWidth = sp.fontSize * sp.label.length() * 0.6f + padding;
        label.setBaseProperties({.size = UDim2::fromOffset(outLabelWidth, slider->absoluteSize.y)});
    } else {
        outLabelHeight = sp.fontSize * 1.5f + padding;
        label.setBaseProperties({.size = UDim2::fromOffset(slider->absoluteSize.x, outLabelHeight)});
    }

    if (sp.labelSide == LabelSide::LEFT || sp.labelSide == LabelSide::TOP) {
        label.setBaseProperties({.position = UDim2::fromOffset(0, 0)});
    }
}

static void s_setupTrack(Frame &track, float x, float y, float width, float height, const Slider *slider)
{
    const auto &sp = slider->getSliderProperties();
    track.setBaseProperties({
        .backgroundColor = sp.sliderColor,
        .backgroundTransparency = sp.sliderTransparency,
        .cornerRadius = sp.trackCornerRadius,
        .position = UDim2::fromOffset(x, y),
        .size = UDim2::fromOffset(width, height),
    });
}

static void s_setupThumb(Frame &thumb, float x, float y, float width, float height, const Slider *slider)
{
    const auto &sp = slider->getSliderProperties();
    thumb.setBaseProperties({
        .backgroundColor = sp.thumbColor,
        .backgroundTransparency = sp.thumbTransparency,
        .cornerRadius = sp.thumbCornerRadius,
        .position = UDim2::fromOffset(x, y),
        .size = UDim2::fromOffset(width, height),
    });
}

static void s_setupValueLabel(TextLabel &label, float x, float y, float width, float height, const Slider *slider,
                              const std::string &text)
{
    const auto &sp = slider->getSliderProperties();
    label.setTextProperties({
        .fontSize = sp.fontSize * 0.8f,
        .textColor = sp.valueColor,
        .textXAlignment = TextXAlignment::CENTER,
        .textYAlignment = TextYAlignment::CENTER,
        .text = text,
    });
    label.setBaseProperties({
        .backgroundColor = Color3(0.0f),
        .backgroundTransparency = 1.0f,
        .interactable = 0,
        .position = UDim2::fromOffset(x, y),
        .size = UDim2::fromOffset(width, height),
    });
}

Slider::Slider()
{
    m_sProps.sliderColor = Color3{0.5f, 0.5f, 0.5f};
    m_sProps.sliderTransparency = 0.0f;
    m_sProps.thumbColor = Color3{0.8f, 0.8f, 0.8f};
    m_sProps.thumbTransparency = 0.0f;
    m_sProps.trackCornerRadius = 0.0f;
    m_sProps.thumbCornerRadius = 0.0f;
    m_sProps.labelColor = Color4{0.0f, 0.0f, 0.0f, 1.0f};
    m_sProps.labelSide = LabelSide::LEFT;
    m_sProps.labelPadding = UDim::fromOffset(5.0f);
    m_sProps.valueColor = Color4{0.0f, 0.0f, 0.0f, 1.0f};
    m_sProps.fontSize = 14.0f;
    m_sProps.layout = ValueControlLayout::SIDE_BY_SIDE;

    m_sideLabel.parent = this;
    s_applyStyle(*this);
}

bool Slider::setSliderProperties(const SliderProperties &props)
{
    bool changed = false;
#define AM_APPLY(field)                                            \
    if (propIsSet(props.field) && m_sProps.field != props.field) { \
        m_sProps.field = props.field;                              \
        changed = true;                                            \
    }
    AM_APPLY(sliderColor)
    AM_APPLY(sliderTransparency)
    AM_APPLY(thumbColor)
    AM_APPLY(thumbTransparency)
    AM_APPLY(trackCornerRadius)
    AM_APPLY(thumbCornerRadius)
    AM_APPLY(labelColor)
    AM_APPLY(labelSide)
    AM_APPLY(labelPadding)
    AM_APPLY(valueColor)
    AM_APPLY(fontSize)
    AM_APPLY(layout)
#undef AM_APPLY
    if (!props.label.empty() && m_sProps.label != props.label) {
        m_sProps.label = props.label;
        changed = true;
    }
    if (!props.valueSuffix.empty() && m_sProps.valueSuffix != props.valueSuffix) {
        m_sProps.valueSuffix = props.valueSuffix;
        changed = true;
    }
    if (changed) {
        markDirty();
    }
    return changed;
}

SliderFloat::SliderFloat()
{
    m_track.parent = this;
    m_thumb.parent = this;
    m_valueLabel.parent = this;
}

void SliderFloat::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        updateComponents();
    }

    glm::vec4 childClip = computeChildClipRect();

    UIObject *parts[] = {&m_sideLabel, &m_track, &m_thumb, &m_valueLabel};
    for (auto *part : parts) {
        part->clipRect = childClip;
        part->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        part->draw(ctx);
    }

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void SliderFloat::updateComponents()
{
    float labelWidth, labelHeight;
    s_setupSideLabel(m_sideLabel, this, labelWidth, labelHeight);

    float trackX = (m_sProps.labelSide == LabelSide::LEFT) ? labelWidth : 0.0f;
    float trackY = (m_sProps.labelSide == LabelSide::TOP) ? labelHeight : 0.0f;
    float trackWidth =
        absoluteSize.x - ((m_sProps.labelSide == LabelSide::LEFT || m_sProps.labelSide == LabelSide::RIGHT) ? labelWidth : 0.0f);
    float trackHeight =
        absoluteSize.y - ((m_sProps.labelSide == LabelSide::TOP || m_sProps.labelSide == LabelSide::BOTTOM) ? labelHeight : 0.0f);

    s_setupTrack(m_track, trackX, trackY, trackWidth, trackHeight, this);

    float range = max - min;
    float thumbWidth = std::max(10.0f, std::min(20.0f, trackWidth / (range / speed)));
    float value = valueRef ? *valueRef : min;
    float normalizedValue = (value - min) / range;
    float thumbX = trackX + normalizedValue * (trackWidth - thumbWidth);

    s_setupThumb(m_thumb, thumbX, trackY, thumbWidth, trackHeight, this);

    UIDragDetector *dragDetector = m_thumb.getExtension<UIDragDetector>();
    if (!dragDetector) {
        dragDetector = m_thumb.addExtension<UIDragDetector>();
        dragDetector->mode = DragMode::HORIZONTAL;
    }

    dragDetector->onDragUpdate = [this, trackX, trackWidth, thumbWidth](glm::vec2, glm::vec2 position) {
        if (!valueRef) return;

        float mouseXRelative = position.x - absolutePosition.x - trackX;
        float normalizedPos = std::clamp((mouseXRelative - thumbWidth * 0.5f) / (trackWidth - thumbWidth), 0.0f, 1.0f);
        *valueRef = min + normalizedPos * (max - min);

        if (onValueChanged) {
            onValueChanged(*valueRef);
        }

        markDirty();
    };

    s_setupValueLabel(m_valueLabel, trackX, trackY, trackWidth, trackHeight, this, formatValue());
}

std::string SliderFloat::formatValue() const
{
    if (!valueRef) return "0" + m_sProps.valueSuffix;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << *valueRef << m_sProps.valueSuffix;
    return oss.str();
}

std::vector<Instance *> SliderFloat::getHittableInstances()
{
    auto result = Instance::getHittableInstances();
    result.push_back(&m_thumb);
    return result;
}

SliderInt::SliderInt()
{
    m_track.parent = this;
    m_thumb.parent = this;
    m_valueLabel.parent = this;
}

void SliderInt::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        updateComponents();
    }

    glm::vec4 childClip = computeChildClipRect();

    UIObject *parts[] = {&m_sideLabel, &m_track, &m_thumb, &m_valueLabel};
    for (auto *part : parts) {
        part->clipRect = childClip;
        part->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        part->draw(ctx);
    }

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void SliderInt::updateComponents()
{
    float labelWidth, labelHeight;
    s_setupSideLabel(m_sideLabel, this, labelWidth, labelHeight);

    float trackX = (m_sProps.labelSide == LabelSide::LEFT) ? labelWidth : 0.0f;
    float trackY = (m_sProps.labelSide == LabelSide::TOP) ? labelHeight : 0.0f;
    float trackWidth =
        absoluteSize.x - ((m_sProps.labelSide == LabelSide::LEFT || m_sProps.labelSide == LabelSide::RIGHT) ? labelWidth : 0.0f);
    float trackHeight =
        absoluteSize.y - ((m_sProps.labelSide == LabelSide::TOP || m_sProps.labelSide == LabelSide::BOTTOM) ? labelHeight : 0.0f);

    s_setupTrack(m_track, trackX, trackY, trackWidth, trackHeight, this);

    float range = static_cast<float>(max - min);
    float thumbWidth = std::max(10.0f, std::min(20.0f, trackWidth / (range / speed)));
    int value = valueRef ? *valueRef : min;
    float normalizedValue = (value - min) / range;
    float thumbX = trackX + normalizedValue * (trackWidth - thumbWidth);

    s_setupThumb(m_thumb, thumbX, trackY, thumbWidth, trackHeight, this);

    UIDragDetector *dragDetector = m_thumb.getExtension<UIDragDetector>();
    if (!dragDetector) {
        dragDetector = m_thumb.addExtension<UIDragDetector>();
        dragDetector->mode = DragMode::HORIZONTAL;
    }

    dragDetector->onDragUpdate = [this, trackX, trackWidth, thumbWidth](glm::vec2, glm::vec2 position) {
        if (!valueRef) return;

        float mouseXRelative = position.x - absolutePosition.x - trackX;
        float normalizedPos = std::clamp((mouseXRelative - thumbWidth * 0.5f) / (trackWidth - thumbWidth), 0.0f, 1.0f);
        float range = static_cast<float>(max - min);
        int newValue = static_cast<int>(std::round(min + normalizedPos * range));
        *valueRef = std::clamp(newValue, min, max);

        if (onValueChanged) {
            onValueChanged(*valueRef);
        }

        markDirty();
    };

    s_setupValueLabel(m_valueLabel, trackX, trackY, trackWidth, trackHeight, this, formatValue());
}

std::string SliderInt::formatValue() const
{
    if (!valueRef) return "0" + m_sProps.valueSuffix;

    return std::to_string(*valueRef) + m_sProps.valueSuffix;
}

std::vector<Instance *> SliderInt::getHittableInstances()
{
    auto result = Instance::getHittableInstances();
    result.push_back(&m_thumb);
    return result;
}

SliderVec2::SliderVec2()
{
    for (int i = 0; i < 2; i++) {
        m_track[i].parent = this;
        m_thumb[i].parent = this;
        m_valueLabel[i].parent = this;
    }
}

void SliderVec2::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        updateComponents();
    }

    glm::vec4 childClip = computeChildClipRect();

    for (int i = 0; i < 2; i++) {
        UIObject *parts[] = {&m_track[i], &m_thumb[i], &m_valueLabel[i]};
        for (auto *part : parts) {
            part->clipRect = childClip;
            part->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            part->draw(ctx);
        }
    }
    m_sideLabel.clipRect = childClip;
    m_sideLabel.computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
    m_sideLabel.draw(ctx);

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void SliderVec2::updateComponents()
{
    float labelWidth, labelHeight;
    s_setupSideLabel(m_sideLabel, this, labelWidth, labelHeight);

    float baseX = (m_sProps.labelSide == LabelSide::LEFT) ? labelWidth : 0.0f;
    float baseY = (m_sProps.labelSide == LabelSide::TOP) ? labelHeight : 0.0f;
    float availableWidth =
        absoluteSize.x - ((m_sProps.labelSide == LabelSide::LEFT || m_sProps.labelSide == LabelSide::RIGHT) ? labelWidth : 0.0f);
    float availableHeight =
        absoluteSize.y - ((m_sProps.labelSide == LabelSide::TOP || m_sProps.labelSide == LabelSide::BOTTOM) ? labelHeight : 0.0f);

    float trackWidth, trackHeight, spacing = 5.0f;

    if (m_sProps.layout == ValueControlLayout::STACKED) {
        trackWidth = availableWidth;
        trackHeight = (availableHeight - spacing) / 2.0f;
    } else {
        trackWidth = (availableWidth - spacing) / 2.0f;
        trackHeight = availableHeight;
    }

    for (int i = 0; i < 2; i++) {
        float trackX = (m_sProps.layout == ValueControlLayout::STACKED) ? baseX : baseX + i * (trackWidth + spacing);
        float trackY = (m_sProps.layout == ValueControlLayout::STACKED) ? baseY + i * (trackHeight + spacing) : baseY;

        s_setupTrack(m_track[i], trackX, trackY, trackWidth, trackHeight, this);

        float range = max[i] - min[i];
        float thumbWidth = std::max(10.0f, std::min(20.0f, trackWidth / (range / speed)));
        float value = valueRef ? (*valueRef)[i] : min[i];
        float normalizedValue = (value - min[i]) / range;
        float thumbXPos = trackX + normalizedValue * (trackWidth - thumbWidth);

        s_setupThumb(m_thumb[i], thumbXPos, trackY, thumbWidth, trackHeight, this);

        UIDragDetector *dragDetector = m_thumb[i].getExtension<UIDragDetector>();
        if (!dragDetector) {
            dragDetector = m_thumb[i].addExtension<UIDragDetector>();
            dragDetector->mode = DragMode::HORIZONTAL;
        }

        dragDetector->onDragUpdate = [this, i, trackX, trackWidth, thumbWidth](glm::vec2, glm::vec2 position) {
            if (!valueRef) return;

            float mouseXRelative = position.x - absolutePosition.x - trackX;
            float normalizedPos = std::clamp((mouseXRelative - thumbWidth * 0.5f) / (trackWidth - thumbWidth), 0.0f, 1.0f);
            (*valueRef)[i] = min[i] + normalizedPos * (max[i] - min[i]);

            if (onValueChanged) {
                onValueChanged(*valueRef);
            }

            markDirty();
        };

        s_setupValueLabel(m_valueLabel[i], trackX, trackY, trackWidth, trackHeight, this, formatValue(i));
    }
}

std::string SliderVec2::formatValue(int component) const
{
    if (!valueRef) return "0" + m_sProps.valueSuffix;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (*valueRef)[component] << m_sProps.valueSuffix;
    return oss.str();
}

std::vector<Instance *> SliderVec2::getHittableInstances()
{
    auto result = Instance::getHittableInstances();
    for (int i = 0; i < 2; i++) {
        result.push_back(&m_thumb[i]);
    }
    return result;
}

SliderVec3::SliderVec3()
{
    for (int i = 0; i < 3; i++) {
        m_track[i].parent = this;
        m_thumb[i].parent = this;
        m_valueLabel[i].parent = this;
    }
}

void SliderVec3::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        updateComponents();
    }

    glm::vec4 childClip = computeChildClipRect();

    for (int i = 0; i < 3; i++) {
        UIObject *parts[] = {&m_track[i], &m_thumb[i], &m_valueLabel[i]};
        for (auto *part : parts) {
            part->clipRect = childClip;
            part->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            part->draw(ctx);
        }
    }
    m_sideLabel.clipRect = childClip;
    m_sideLabel.computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
    m_sideLabel.draw(ctx);

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void SliderVec3::updateComponents()
{
    float labelWidth, labelHeight;
    s_setupSideLabel(m_sideLabel, this, labelWidth, labelHeight);

    float baseX = (m_sProps.labelSide == LabelSide::LEFT) ? labelWidth : 0.0f;
    float baseY = (m_sProps.labelSide == LabelSide::TOP) ? labelHeight : 0.0f;
    float availableWidth =
        absoluteSize.x - ((m_sProps.labelSide == LabelSide::LEFT || m_sProps.labelSide == LabelSide::RIGHT) ? labelWidth : 0.0f);
    float availableHeight =
        absoluteSize.y - ((m_sProps.labelSide == LabelSide::TOP || m_sProps.labelSide == LabelSide::BOTTOM) ? labelHeight : 0.0f);

    float trackWidth, trackHeight, spacing = 5.0f;

    if (m_sProps.layout == ValueControlLayout::STACKED) {
        trackWidth = availableWidth;
        trackHeight = (availableHeight - 2 * spacing) / 3.0f;
    } else {
        trackWidth = (availableWidth - 2 * spacing) / 3.0f;
        trackHeight = availableHeight;
    }

    for (int i = 0; i < 3; i++) {
        float trackX = (m_sProps.layout == ValueControlLayout::STACKED) ? baseX : baseX + i * (trackWidth + spacing);
        float trackY = (m_sProps.layout == ValueControlLayout::STACKED) ? baseY + i * (trackHeight + spacing) : baseY;

        s_setupTrack(m_track[i], trackX, trackY, trackWidth, trackHeight, this);

        float range = max[i] - min[i];
        float thumbWidth = std::max(10.0f, std::min(20.0f, trackWidth / (range / speed)));
        float value = valueRef ? (*valueRef)[i] : min[i];
        float normalizedValue = (value - min[i]) / range;
        float thumbXPos = trackX + normalizedValue * (trackWidth - thumbWidth);

        s_setupThumb(m_thumb[i], thumbXPos, trackY, thumbWidth, trackHeight, this);

        UIDragDetector *dragDetector = m_thumb[i].getExtension<UIDragDetector>();
        if (!dragDetector) {
            dragDetector = m_thumb[i].addExtension<UIDragDetector>();
            dragDetector->mode = DragMode::HORIZONTAL;
        }

        dragDetector->onDragUpdate = [this, i, trackX, trackWidth, thumbWidth](glm::vec2, glm::vec2 position) {
            if (!valueRef) return;

            float mouseXRelative = position.x - absolutePosition.x - trackX;
            float normalizedPos = std::clamp((mouseXRelative - thumbWidth * 0.5f) / (trackWidth - thumbWidth), 0.0f, 1.0f);
            (*valueRef)[i] = min[i] + normalizedPos * (max[i] - min[i]);

            if (onValueChanged) {
                onValueChanged(*valueRef);
            }

            markDirty();
        };

        s_setupValueLabel(m_valueLabel[i], trackX, trackY, trackWidth, trackHeight, this, formatValue(i));
    }
}

std::string SliderVec3::formatValue(int component) const
{
    if (!valueRef) return "0" + m_sProps.valueSuffix;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (*valueRef)[component] << m_sProps.valueSuffix;
    return oss.str();
}

std::vector<Instance *> SliderVec3::getHittableInstances()
{
    auto result = Instance::getHittableInstances();
    for (int i = 0; i < 3; i++) {
        result.push_back(&m_thumb[i]);
    }
    return result;
}

} // namespace Amethyst
