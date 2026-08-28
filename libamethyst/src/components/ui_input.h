/*
 * Base class for keyboard-editable components (single-line fields, text areas, ...)
 */

#ifndef AMETHYST__UI_INPUT_H
#define AMETHYST__UI_INPUT_H

#include "components/input_events.h"
#include "components/ui_object.h"
#include "components/window.h"
#include "modules/event_signal.h"

#include <cstdint>
#include <functional>

namespace Amethyst {

/**
 * @brief Focusable component that receives keyboard text input and runs a blinking cursor.
 *
 * Covers what every editable text surface shares: taking and dropping keyboard focus,
 * dismissing itself when a press lands elsewhere, blinking the cursor, delivering key and
 * character events while focused, and showing an I-beam under the mouse. Subclasses own the
 * text itself and decide what each key does.
 */
class UIInput : public UIObject {
  public:
    virtual ~UIInput();

    /**
     * @brief Give this component keyboard focus, as if it had been clicked.
     */
    void focus();

    /**
     * @brief Drop keyboard focus, committing the current edit.
     */
    void loseFocus();

    bool isFocused() const { return m_focused; }

  public:
    std::function<void()> onFocusGained;
    std::function<void()> onFocusLost;

  protected:
    UIInput();

    EventResult onInputBegan(const InputObject &input) override;
    EventResult onInputEnded(const InputObject &input) override;
    EventResult onMouseMoved(int32_t x, int32_t y) override;
    EventResult onMouseEnter() override;
    EventResult onMouseLeave() override;

    /**
     * @brief A key was pressed or auto-repeated while this component holds focus.
     * @param event The key, its action and the active modifiers
     */
    virtual void onKeyPressed(const KeyEvent &event) { (void)event; }

    /**
     * @brief A character was committed by the keyboard while this component holds focus.
     * @param codepoint The Unicode codepoint typed
     */
    virtual void onCharacterTyped(uint32_t codepoint) { (void)codepoint; }

    /**
     * @brief Put the cursor at a window-space point, from a click or a selection drag.
     * @param position Point in window space
     * @param extendSelection True to keep the existing anchor and select up to the point
     */
    virtual void moveCursorToPoint(vec2 position, bool extendSelection) = 0;

    /**
     * @brief Invoked when an edit is committed (Enter pressed or focus lost).
     */
    virtual void onCommit() {}

    /**
     * @brief Show the cursor and restart its blink, so it stays solid while typing.
     */
    void resetCursorBlink();

    bool isCursorVisible() const { return m_cursorVisible; }

    void setCursorBlinkRate(float seconds) { m_cursorBlinkRate = seconds; }

  private:
    void onUpdate(float deltaTime);

  private:
    bool m_focused = false;
    bool m_hovered = false;
    bool m_cursorVisible = true;
    bool m_draggingSelection = false;
    float m_cursorBlinkTimer = 0.0f;
    float m_cursorBlinkRate = 0.5f;

    EventConnection m_pressConn;
    TickHandle m_tick;
};

} // namespace Amethyst

#endif // AMETHYST__UI_INPUT_H
