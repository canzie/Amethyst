#include "components/ui_input.h"

#include "components/input_events.h"
#include "components/input_interface.h"
#include "components/overlay_layer.h"
#include "components/window.h"

#include <cstdint>

namespace Amethyst {

UIInput::UIInput() = default;

UIInput::~UIInput()
{
    m_tick.unregister();
    if (m_hovered) {
        InputInterface::setCursorShape(CURSOR_ARROW);
    }
}

void UIInput::focus()
{
    if (m_focused) {
        return;
    }
    m_focused = true;
    setGuiState(static_cast<uint16_t>(getGuiState() | GUI_STATE_FOCUSED));
    InputInterface::clearKeyEvents();

    Window *win = getWindow();
    OverlayLayer *overlay = win != nullptr ? win->getOverlayLayer() : nullptr;
    if (overlay != nullptr && !m_pressConn.connected()) {
        m_pressConn = overlay->onPressVote.connect([this](vec2 pos, PressVote &vote) {
            (void)vote;
            if (m_focused && !containsPoint(pos)) {
                loseFocus();
            }
        });
    }

    if (win != nullptr && !m_tick.active()) {
        m_tick = win->registerTick([this](float dt) { onUpdate(dt); });
    }

    if (onFocusGained) {
        onFocusGained();
    }
    markDirty();
}

void UIInput::loseFocus()
{
    if (!m_focused) {
        return;
    }
    m_focused = false;
    setGuiState(static_cast<uint16_t>(getGuiState() & ~GUI_STATE_FOCUSED));
    m_cursorVisible = false;
    m_pressConn.disconnect();
    m_tick.unregister();
    onCommit();
    markDirty();
    if (onFocusLost) {
        onFocusLost();
    }
}

EventResult UIInput::onInputBegan(const InputObject &input)
{
    if (input.type != InputType::MOUSE_BUTTON_1) {
        return UIObject::onInputBegan(input);
    }

    focus();
    moveCursorToPoint(input.position, false);
    m_draggingSelection = true;

    Window *window = getWindow();
    if (window) {
        window->captureMouse(this);
    }
    return EventResult::CONSUMED;
}

EventResult UIInput::onInputEnded(const InputObject &input)
{
    if (input.type != InputType::MOUSE_BUTTON_1) {
        return UIObject::onInputEnded(input);
    }

    m_draggingSelection = false;
    Window *window = getWindow();
    if (window) {
        window->releaseMouse(this);
    }
    return EventResult::CONSUMED;
}

EventResult UIInput::onMouseMoved(int32_t x, int32_t y)
{
    if (m_draggingSelection) {
        moveCursorToPoint(vec2{static_cast<float>(x), static_cast<float>(y)}, true);
    }
    return EventResult::CONSUMED;
}

EventResult UIInput::onMouseEnter()
{
    UIObject::onMouseEnter();
    m_hovered = true;
    InputInterface::setCursorShape(CURSOR_IBEAM);
    return EventResult::CONSUMED;
}

EventResult UIInput::onMouseLeave()
{
    UIObject::onMouseLeave();
    m_hovered = false;
    InputInterface::setCursorShape(CURSOR_ARROW);
    return EventResult::CONSUMED;
}

void UIInput::resetCursorBlink()
{
    m_cursorBlinkTimer = 0.0f;
    m_cursorVisible = true;
}

void UIInput::onUpdate(float deltaTime)
{
    if (!m_focused) {
        return;
    }

    if (!isVisible()) {
        loseFocus();
        return;
    }

    KeyEvent keyEvent;
    while (InputInterface::pollKeyEvent(keyEvent)) {
        if (keyEvent.action == KEY_ACTION_PRESS || keyEvent.action == KEY_ACTION_REPEAT) {
            onKeyPressed(keyEvent);
        }
    }

    uint32_t codepoint;
    while (InputInterface::pollCharEvent(codepoint)) {
        onCharacterTyped(codepoint);
    }

    m_cursorBlinkTimer += deltaTime;
    if (m_cursorBlinkTimer >= m_cursorBlinkRate) {
        m_cursorBlinkTimer = 0.0f;
        m_cursorVisible = !m_cursorVisible;
        markDirty();
    }
}

} // namespace Amethyst
