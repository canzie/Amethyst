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

static void applyStyle(Slider &slider)
{
    const auto &style = Style::instance();
    slider.backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::SLIDER);
    slider.backgroundTransparency = style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::SLIDER);
    slider.sliderColor = style.get<Color3>(StyleProperty::SLIDER_COLOR, ComponentType::SLIDER);
    slider.sliderTransparency = style.get<float>(StyleProperty::SLIDER_TRANSPARENCY, ComponentType::SLIDER);
    slider.thumbColor = style.get<Color3>(StyleProperty::THUMB_COLOR, ComponentType::SLIDER);
    slider.thumbTransparency = style.get<float>(StyleProperty::THUMB_TRANSPARENCY, ComponentType::SLIDER);
    slider.trackCornerRadius = style.get<float>(StyleProperty::TRACK_CORNER_RADIUS, ComponentType::SLIDER);
    slider.thumbCornerRadius = style.get<float>(StyleProperty::THUMB_CORNER_RADIUS, ComponentType::SLIDER);
    slider.labelColor = style.get<Color4>(StyleProperty::LABEL_COLOR, ComponentType::SLIDER);
    slider.labelPadding = style.get<UDim>(StyleProperty::LABEL_PADDING, ComponentType::SLIDER);
    slider.valueColor = style.get<Color4>(StyleProperty::VALUE_COLOR, ComponentType::SLIDER);
    slider.fontSize = style.get<float>(StyleProperty::FONT_SIZE, ComponentType::SLIDER);
}

static void setupSideLabel(TextLabel &label, const Slider *slider, float &outLabelWidth, float &outLabelHeight)
{
    outLabelWidth = 0.0f;
    outLabelHeight = 0.0f;

    if (slider->label.empty()) {
        label.visible = false;
        return;
    }

    label.text = slider->label;
    label.fontSize = slider->fontSize;
    label.textColor = slider->labelColor;
    label.textYAlignment = TextYAlignment::CENTER;
    label.backgroundColor = slider->backgroundColor;
    label.backgroundTransparency = slider->backgroundTransparency;
    label.visible = true;

    float padding = slider->labelPadding.resolve(slider->labelSide == LabelSide::LEFT || slider->labelSide == LabelSide::RIGHT
                                                     ? slider->absoluteSize.x
                                                     : slider->absoluteSize.y);

    if (slider->labelSide == LabelSide::LEFT || slider->labelSide == LabelSide::RIGHT) {
        outLabelWidth = slider->fontSize * slider->label.length() * 0.6f + padding;
        label.size = UDim2::fromOffset(outLabelWidth, slider->absoluteSize.y);
    } else {
        outLabelHeight = slider->fontSize * 1.5f + padding;
        label.size = UDim2::fromOffset(slider->absoluteSize.x, outLabelHeight);
    }

    if (slider->labelSide == LabelSide::LEFT || slider->labelSide == LabelSide::TOP) {
        label.position = UDim2::fromOffset(0, 0);
    }

    label.markDirty();
}

static void setupTrack(Frame &track, float x, float y, float width, float height, const Slider *slider)
{
    track.position = UDim2::fromOffset(x, y);
    track.size = UDim2::fromOffset(width, height);
    track.backgroundColor = slider->sliderColor;
    track.backgroundTransparency = slider->sliderTransparency;
    track.cornerRadius = slider->trackCornerRadius;
    track.markDirty();
}

static void setupThumb(Frame &thumb, float x, float y, float width, float height, const Slider *slider)
{
    thumb.position = UDim2::fromOffset(x, y);
    thumb.size = UDim2::fromOffset(width, height);
    thumb.backgroundColor = slider->thumbColor;
    thumb.backgroundTransparency = slider->thumbTransparency;
    thumb.cornerRadius = slider->thumbCornerRadius;
    thumb.markDirty();
}

static void setupValueLabel(TextLabel &label, float x, float y, float width, float height, const Slider *slider,
                            const std::string &text)
{
    label.text = text;
    label.fontSize = slider->fontSize * 0.8f;
    label.textColor = slider->valueColor;
    label.textXAlignment = TextXAlignment::CENTER;
    label.textYAlignment = TextYAlignment::CENTER;
    label.position = UDim2::fromOffset(x, y);
    label.size = UDim2::fromOffset(width, height);
    label.backgroundColor = Color3(0.0f);
    label.backgroundTransparency = 1.0f;
    label.interactable = false;
    label.markDirty();
}

Slider::Slider()
{
    m_sideLabel.parent = this;
    applyStyle(*this);
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
    setupSideLabel(m_sideLabel, this, labelWidth, labelHeight);

    float trackX = (labelSide == LabelSide::LEFT) ? labelWidth : 0.0f;
    float trackY = (labelSide == LabelSide::TOP) ? labelHeight : 0.0f;
    float trackWidth = absoluteSize.x - ((labelSide == LabelSide::LEFT || labelSide == LabelSide::RIGHT) ? labelWidth : 0.0f);
    float trackHeight = absoluteSize.y - ((labelSide == LabelSide::TOP || labelSide == LabelSide::BOTTOM) ? labelHeight : 0.0f);

    setupTrack(m_track, trackX, trackY, trackWidth, trackHeight, this);

    float range = max - min;
    float thumbWidth = std::max(10.0f, std::min(20.0f, trackWidth / (range / speed)));
    float value = valueRef ? *valueRef : min;
    float normalizedValue = (value - min) / range;
    float thumbX = trackX + normalizedValue * (trackWidth - thumbWidth);

    setupThumb(m_thumb, thumbX, trackY, thumbWidth, trackHeight, this);

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

    setupValueLabel(m_valueLabel, trackX, trackY, trackWidth, trackHeight, this, formatValue());
}

std::string SliderFloat::formatValue() const
{
    if (!valueRef) return "0" + valueSuffix;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << *valueRef << valueSuffix;
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
    setupSideLabel(m_sideLabel, this, labelWidth, labelHeight);

    float trackX = (labelSide == LabelSide::LEFT) ? labelWidth : 0.0f;
    float trackY = (labelSide == LabelSide::TOP) ? labelHeight : 0.0f;
    float trackWidth = absoluteSize.x - ((labelSide == LabelSide::LEFT || labelSide == LabelSide::RIGHT) ? labelWidth : 0.0f);
    float trackHeight = absoluteSize.y - ((labelSide == LabelSide::TOP || labelSide == LabelSide::BOTTOM) ? labelHeight : 0.0f);

    setupTrack(m_track, trackX, trackY, trackWidth, trackHeight, this);

    float range = static_cast<float>(max - min);
    float thumbWidth = std::max(10.0f, std::min(20.0f, trackWidth / (range / speed)));
    int value = valueRef ? *valueRef : min;
    float normalizedValue = (value - min) / range;
    float thumbX = trackX + normalizedValue * (trackWidth - thumbWidth);

    setupThumb(m_thumb, thumbX, trackY, thumbWidth, trackHeight, this);

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

    setupValueLabel(m_valueLabel, trackX, trackY, trackWidth, trackHeight, this, formatValue());
}

std::string SliderInt::formatValue() const
{
    if (!valueRef) return "0" + valueSuffix;

    return std::to_string(*valueRef) + valueSuffix;
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
    setupSideLabel(m_sideLabel, this, labelWidth, labelHeight);

    float baseX = (labelSide == LabelSide::LEFT) ? labelWidth : 0.0f;
    float baseY = (labelSide == LabelSide::TOP) ? labelHeight : 0.0f;
    float availableWidth = absoluteSize.x - ((labelSide == LabelSide::LEFT || labelSide == LabelSide::RIGHT) ? labelWidth : 0.0f);
    float availableHeight = absoluteSize.y - ((labelSide == LabelSide::TOP || labelSide == LabelSide::BOTTOM) ? labelHeight : 0.0f);

    float trackWidth, trackHeight, spacing = 5.0f;

    if (layout == ValueControlLayout::STACKED) {
        trackWidth = availableWidth;
        trackHeight = (availableHeight - spacing) / 2.0f;
    } else {
        trackWidth = (availableWidth - spacing) / 2.0f;
        trackHeight = availableHeight;
    }

    for (int i = 0; i < 2; i++) {
        float trackX = (layout == ValueControlLayout::STACKED) ? baseX : baseX + i * (trackWidth + spacing);
        float trackY = (layout == ValueControlLayout::STACKED) ? baseY + i * (trackHeight + spacing) : baseY;

        setupTrack(m_track[i], trackX, trackY, trackWidth, trackHeight, this);

        float range = max[i] - min[i];
        float thumbWidth = std::max(10.0f, std::min(20.0f, trackWidth / (range / speed)));
        float value = valueRef ? (*valueRef)[i] : min[i];
        float normalizedValue = (value - min[i]) / range;
        float thumbXPos = trackX + normalizedValue * (trackWidth - thumbWidth);

        setupThumb(m_thumb[i], thumbXPos, trackY, thumbWidth, trackHeight, this);

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

        setupValueLabel(m_valueLabel[i], trackX, trackY, trackWidth, trackHeight, this, formatValue(i));
    }
}

std::string SliderVec2::formatValue(int component) const
{
    if (!valueRef) return "0" + valueSuffix;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (*valueRef)[component] << valueSuffix;
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
    setupSideLabel(m_sideLabel, this, labelWidth, labelHeight);

    float baseX = (labelSide == LabelSide::LEFT) ? labelWidth : 0.0f;
    float baseY = (labelSide == LabelSide::TOP) ? labelHeight : 0.0f;
    float availableWidth = absoluteSize.x - ((labelSide == LabelSide::LEFT || labelSide == LabelSide::RIGHT) ? labelWidth : 0.0f);
    float availableHeight = absoluteSize.y - ((labelSide == LabelSide::TOP || labelSide == LabelSide::BOTTOM) ? labelHeight : 0.0f);

    float trackWidth, trackHeight, spacing = 5.0f;

    if (layout == ValueControlLayout::STACKED) {
        trackWidth = availableWidth;
        trackHeight = (availableHeight - 2 * spacing) / 3.0f;
    } else {
        trackWidth = (availableWidth - 2 * spacing) / 3.0f;
        trackHeight = availableHeight;
    }

    for (int i = 0; i < 3; i++) {
        float trackX = (layout == ValueControlLayout::STACKED) ? baseX : baseX + i * (trackWidth + spacing);
        float trackY = (layout == ValueControlLayout::STACKED) ? baseY + i * (trackHeight + spacing) : baseY;

        setupTrack(m_track[i], trackX, trackY, trackWidth, trackHeight, this);

        float range = max[i] - min[i];
        float thumbWidth = std::max(10.0f, std::min(20.0f, trackWidth / (range / speed)));
        float value = valueRef ? (*valueRef)[i] : min[i];
        float normalizedValue = (value - min[i]) / range;
        float thumbXPos = trackX + normalizedValue * (trackWidth - thumbWidth);

        setupThumb(m_thumb[i], thumbXPos, trackY, thumbWidth, trackHeight, this);

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

        setupValueLabel(m_valueLabel[i], trackX, trackY, trackWidth, trackHeight, this, formatValue(i));
    }
}

std::string SliderVec3::formatValue(int component) const
{
    if (!valueRef) return "0" + valueSuffix;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (*valueRef)[component] << valueSuffix;
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
