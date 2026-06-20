#include "components/ui_object.h"
#include "components/common.h"
#include "components/extensions/ui_aspect_ratio_constraint.h"
#include "components/extensions/ui_drag_detector.h"
#include "components/extensions/ui_size_constraint.h"
#include "components/input_interface.h"
#include "components/ui_layer.h"
#include "components/window.h"
#include "rendering/instance_data.h"
#include "utils/profiling.h"
#include <algorithm>

#include <cstdint>

namespace Amethyst {

UIObject::UIObject()
{
    // sane defaults so we can compare to the default struct
    // otherwise we need to force the user to copy the current struct all the time, or use optionals everywhere?
    m_uiObjProps.active = false;
    m_uiObjProps.anchorPoint = vec2(0.0f);
    m_uiObjProps.automaticSize = AutomaticSize::OFF;
    m_uiObjProps.clipsDescendants = true;
    m_uiObjProps.guiState = GuiState::IDLE;
    m_uiObjProps.interactable = true;
    m_uiObjProps.layoutOrder = 0;
    m_uiObjProps.padding = UDim4{};
    m_uiObjProps.margin = UDim4{};
    m_uiObjProps.position = UDim2{};
    m_uiObjProps.size = UDim2{};
    m_uiObjProps.rotation = 0.0f;
    m_uiObjProps.visible = true;
    m_uiObjProps.zIndex = 1;
    m_uiObjProps.zindexBehavior = ZIndexBehavior::GLOBAL;

    m_baseStyle.backgroundColor = Color3{1.0f, 1.0f, 1.0f};
    m_baseStyle.backgroundTransparency = 0.0f;
    m_baseStyle.borderMode = BorderMode::OUTLINE;
    m_baseStyle.borderPixelSize = 0.0f;
    m_baseStyle.borderColor = Color3{0.0f, 0.0f, 0.0f};
    m_baseStyle.borderTransparency = 0.0f;
    m_baseStyle.cornerRadius = 0.0f;
}

UIObject::~UIObject() {}

bool UIObject::setBaseProperties(BaseProperties props)
{
    bool changed = m_uiObjProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

bool UIObject::setBaseStyleProperties(BaseStyleProperties style)
{
    bool changed = m_baseStyle.apply(style);
    if (changed) {
        markDirty();
    }
    return changed;
}

void UIObject::resolveStyle() {}

void UIObject::addClass(std::string_view name)
{
    StyleKey token = Style::classToken(name);
    if (std::ranges::find(m_classes, token) == m_classes.end()) {
        m_classes.push_back(token);
        Style::instance().registerClassName(token, name);
    }
    resolveStyle();
    markDirty();
}

void UIObject::removeClass(std::string_view name)
{
    StyleKey token = Style::classToken(name);
    auto it = std::ranges::find(m_classes, token);
    if (it != m_classes.end()) {
        m_classes.erase(it);
    }
    resolveStyle();
    markDirty();
}

bool UIObject::hasClass(std::string_view name) const
{
    StyleKey token = Style::classToken(name);
    return std::ranges::find(m_classes, token) != m_classes.end();
}

void UIObject::setClasses(std::span<const std::string> names)
{
    m_classes.clear();
    for (const auto &name : names) {
        StyleKey token = Style::classToken(name);
        if (std::ranges::find(m_classes, token) == m_classes.end()) {
            m_classes.push_back(token);
            Style::instance().registerClassName(token, name);
        }
    }
    resolveStyle();
    markDirty();
}

void UIObject::setClasses(std::initializer_list<std::string_view> names)
{
    m_classes.clear();
    for (std::string_view name : names) {
        StyleKey token = Style::classToken(name);
        if (std::ranges::find(m_classes, token) == m_classes.end()) {
            m_classes.push_back(token);
            Style::instance().registerClassName(token, name);
        }
    }
    resolveStyle();
    markDirty();
}

void UIObject::computeAbsolutes(vec2 parentSize, vec2 parentPos, Degrees parentRotation)
{
    AM_PROFILE_FUNCTION();
    absoluteSize = m_uiObjProps.size.resolve(parentSize);
    absolutePosition = parentPos + m_uiObjProps.position.resolve(parentSize) - m_uiObjProps.anchorPoint * absoluteSize;
    absoluteRotation = m_uiObjProps.rotation + parentRotation;

    vec4 m = m_uiObjProps.margin.resolve(parentSize);
    absolutePosition += vec2(m.w, m.x);
    absoluteSize -= vec2(m.w + m.y, m.x + m.z);

    vec4 p = m_uiObjProps.padding.resolve(absoluteSize);
    absoluteContentPosition = absolutePosition + vec2(p.w, p.x);
    absoluteContentSize = absoluteSize - vec2(p.w + p.y, p.x + p.z);

    if (auto *sizeConstraint = getExtension<UISizeConstraint>()) {
        sizeConstraint->apply();
    }
    if (auto *arConstraint = getExtension<UIAspectRatioConstraint>()) {
        arConstraint->apply();
    }
}

InstanceData UIObject::createInstanceData() const
{
    AM_PROFILE_FUNCTION();
    vec2 centerPos = absolutePosition + absoluteSize * 0.5f;

    InstanceData data{};
    data.translation = centerPos;
    data.scale = absoluteSize;
    data.clipRect = clipRect;
    data.setFillColor(Color4(m_baseStyle.backgroundColor, 1.0f - m_baseStyle.backgroundTransparency));
    data.setBorderColor(Color4(m_baseStyle.borderColor, 1.0f - m_baseStyle.borderTransparency));
    data.setRotation(radians(absoluteRotation));
    data.setBorderThickness(m_baseStyle.borderPixelSize);
    data.setCornerRadius(m_baseStyle.cornerRadius);
    data.setPrimitiveType(PRIMITIVE_TRIANGLE);
    data.setBorderMode(m_baseStyle.borderMode);
    data.zIndex = getZIndex();
    data.setVisible(isVisible());
    return data;
}

vec4 UIObject::computeChildClipRect() const
{
    if (!m_uiObjProps.clipsDescendants) {
        return clipRect;
    }
    vec4 myBounds = {absolutePosition.x, absolutePosition.y, absolutePosition.x + absoluteSize.x,
                     absolutePosition.y + absoluteSize.y};
    if (clipRect == vec4(0.0f)) {
        return myBounds;
    }
    return {max(clipRect.x, myBounds.x), max(clipRect.y, myBounds.y), min(clipRect.z, myBounds.z), min(clipRect.w, myBounds.w)};
}

Window *UIObject::getWindow()
{
    for (Instance *current = parent; current != nullptr; current = current->parent) {
        if (auto *window = current->as<Window>()) {
            return window;
        }
    }
    return nullptr;
}

EventResult UIObject::onMouseEnter()
{
    if (onHoverChanged) {
        onHoverChanged(true);
        markDirty();
    }
    return EventResult::CONSUMED;
}

bool UIObject::isVisible() const
{
    if (!m_uiObjProps.visible) return false;
    if (!parent) return true;
    if (auto *obj = parent->as<UIObject>()) {
        return obj->isVisible();
    } else if (auto *layer = parent->as<UILayer>()) {
        return layer->isVisible();
    }
    return true;
}

int32_t UIObject::getAbsoluteZIndex() const
{
    if (parent == nullptr) {
        return m_uiObjProps.zIndex;
    }

    if (auto *obj = parent->as<UIObject>()) {
        return obj->getAbsoluteZIndex() + m_uiObjProps.zIndex;
    } else if (auto *layer = parent->as<UILayer>()) {
        return layer->getDisplayOrder() + m_uiObjProps.zIndex;
    }

    return m_uiObjProps.zIndex;
}

int32_t UIObject::getZIndex() const
{
    if (m_uiObjProps.zindexBehavior == ZIndexBehavior::GLOBAL) {
        return getAbsoluteZIndex();
    }
    return m_uiObjProps.zIndex;
}

EventResult UIObject::onMouseLeave()
{
    if (onHoverChanged) {
        onHoverChanged(false);
        markDirty();
    }
    return EventResult::CONSUMED;
}

EventResult UIObject::onMouseMoved(int32_t x, int32_t y)
{
    if (auto *drag = getExtension<UIDragDetector>()) {
        drag->handleMouseMove(x, y);
    }

    InputObject input{};
    input.type = InputType::MOUSE_MOVEMENT;
    input.state = InputState::CHANGE;
    input.position = {static_cast<float>(x), static_cast<float>(y), 0.0f};
    input.modifiers = InputInterface::getModifiers();
    return onInputChanged(input);
}

EventResult UIObject::onInputBegan(const InputObject &input)
{
    if (input.type == InputType::MOUSE_BUTTON_1) {
        if (auto *drag = getExtension<UIDragDetector>()) {
            drag->handleMouseDown(static_cast<int32_t>(input.position.x), static_cast<int32_t>(input.position.y));
        }
    }
    if (onInputBeganCb) {
        return onInputBeganCb(input);
    }
    return EventResult::CONSUMED;
}

EventResult UIObject::onInputChanged(const InputObject &input)
{
    if (onInputChangedCb) {
        return onInputChangedCb(input);
    }
    return EventResult::CONSUMED;
}

EventResult UIObject::onInputEnded(const InputObject &input)
{
    if (input.type == InputType::MOUSE_BUTTON_1) {
        if (auto *drag = getExtension<UIDragDetector>()) {
            drag->handleMouseUp(static_cast<int32_t>(input.position.x), static_cast<int32_t>(input.position.y));
        }
    }
    if (onInputEndedCb) {
        return onInputEndedCb(input);
    }
    return EventResult::CONSUMED;
}

} // namespace Amethyst
