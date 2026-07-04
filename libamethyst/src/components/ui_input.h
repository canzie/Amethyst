/*
 * Base class for editable text-input fields (text, number, color, ...)
 */

#ifndef AMETHYST__UI_INPUT_H
#define AMETHYST__UI_INPUT_H

#include "components/properties.h"
#include "components/ui_object.h"
#include "components/window.h"
#include "modules/event_signal.h"
#include "rendering/geometry_registry.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace Amethyst {

struct DrawContext;

/**
 * @brief Editable single-line text field: caret, selection, clipboard and rendering.
 *
 * Owns the full text-editing core. Concrete inputs subclass it and customise behaviour
 * through a few hooks: which theme type to resolve, which edits to accept, how to render
 * the buffer, and what to do when an edit is committed.
 */
class UIInput : public UIObject {
  public:
    virtual ~UIInput();

    void update(float deltaTime);

    std::string getText() const { return m_text; }
    void setText(const std::string &text);
    void clearText();

    void setPlaceholder(std::string placeholder);
    const std::string &getPlaceholder() const { return m_placeholder; }

    /**
     * @brief Give this field keyboard focus, as if it had been clicked.
     */
    void focus();

    /**
     * @brief Drop keyboard focus, committing the current edit.
     */
    void loseFocus();

    bool isFocused() const { return m_focused; }

    /**
     * @brief Select the entire buffer, as if the user pressed Ctrl+A.
     */
    void selectAll();

    bool setTextInputProperties(const TextInputStyleProperties &props);
    const TextInputStyleProperties &getTextInputProperties() const { return m_tiProps; }

    std::function<void(const std::string &)> onTextChanged;
    std::function<void()> onEnterPressed;
    std::function<void()> onFocusGained;
    std::function<void()> onFocusLost;

  protected:
    UIInput();

    /**
     * @brief Render the field: background, text, selection, caret and children.
     *
     * Subclasses call this from their own draw() override.
     */
    void drawInput(DrawContext &ctx);

    EventResult onInputBegan(const InputObject &input) override;
    EventResult onInputEnded(const InputObject &input) override;
    EventResult onMouseMoved(int32_t x, int32_t y) override;
    EventResult onMouseEnter() override;
    EventResult onMouseLeave() override;

    /**
     * @brief Decide whether a candidate buffer is allowed after an edit.
     * @param candidate The full text the buffer would hold once the edit is applied
     * @return True to accept the edit, false to reject it
     */
    virtual bool acceptText(std::string_view candidate) const
    {
        (void)candidate;
        return true;
    }

    /**
     * @brief Text actually drawn for the buffer; overridden for masking (e.g. passwords).
     * @return The string to render, one display char per buffer char
     */
    virtual std::string displayText() const { return m_text; }

    /**
     * @brief Invoked when an edit is committed (Enter pressed or focus lost).
     */
    virtual void onCommit() {}

    TextInputStyleProperties m_tiProps;

  private:
    void processKeyboardInput();
    void insertText(const std::string &text);
    void deleteSelection();
    void moveCursor(int delta, bool select);
    void setCursorPosition(size_t pos, bool select);
    size_t getCursorFromMouseX(int32_t mouseX);

    void copy();
    void paste();
    void cut();

    void releaseText(DrawContext &ctx);
    void drawText(DrawContext &ctx);
    void drawSelection(DrawContext &ctx);
    void drawCursor(DrawContext &ctx);

    std::string m_text;
    std::string m_placeholder;
    size_t m_cursorPosition = 0;
    std::optional<size_t> m_selectionStart;
    bool m_focused = false;
    bool m_hovered = false;
    float m_cursorBlinkTimer = 0.0f;
    bool m_cursorVisible = true;
    bool m_draggingSelection = false;

    EventConnection m_pressConn;

    GeometryAllocation *m_textAlloc = nullptr;
    GlyphSliceHandle m_glyphSlice;
    GeometryAllocation *m_selectionAlloc = nullptr;
    GeometryAllocation *m_cursorAlloc = nullptr;

    vec2 m_textSize = {0.0f, 0.0f};
    std::vector<float> m_charPositions;
    bool m_showingPlaceholder = false;
    float m_textBaselineY = 0.0f;
    float m_textStartX = 0.0f;

    TickHandle m_tick;
};

} // namespace Amethyst

#endif // AMETHYST__UI_INPUT_H
