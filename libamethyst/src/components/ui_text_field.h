/*
 * Base class for single-line editable text fields (text, number, color, ...)
 */

#ifndef AMETHYST__UI_TEXT_FIELD_H
#define AMETHYST__UI_TEXT_FIELD_H

#include "components/properties.h"
#include "components/ui_input.h"
#include "modules/text_processor.h"
#include "rendering/geometry_registry.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Amethyst {

struct DrawContext;

/**
 * @brief Editable single-line text field: caret, selection, clipboard and rendering.
 *
 * Owns one string and the editing state over it. Concrete fields subclass it and customise
 * behaviour through a few hooks: which theme type to resolve, which edits to accept, how to
 * render the buffer, and what to do when an edit is committed.
 */
class UITextField : public UIInput {
  public:
    virtual ~UITextField();

    std::string getText() const { return m_text; }
    void setText(const std::string &text);
    void clearText();

    void setPlaceholder(std::string placeholder);
    const std::string &getPlaceholder() const { return m_placeholder; }

    /**
     * @brief Select the entire buffer, as if the user pressed Ctrl+A.
     */
    void selectAll();

    bool setTextInputProperties(const TextInputStylePropertiesArgs &props);
    const TextInputStyleProperties &getTextInputProperties() const { return m_tiProps; }

    /**
     * @brief Pull the base and text input style for a component type out of the theme.
     * @param type Component type to resolve against
     */
    void resolveTextInputStyle(ComponentType type);

  public:
    std::function<void(const std::string &)> onTextChanged;
    std::function<void()> onEnterPressed;

  protected:
    UITextField();

    /**
     * @brief Render the field: background, text, selection, caret and children.
     *
     * Subclasses call this from their own draw() override.
     */
    void drawInput(DrawContext &ctx);

    void onKeyPressed(const KeyEvent &event) override;
    void onCharacterTyped(uint32_t codepoint) override;
    void moveCursorToPoint(vec2 position, bool extendSelection) override;

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

  protected:
    TextInputStyleProperties m_tiProps;

  private:
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

  private:
    std::string m_text;
    std::string m_placeholder;
    size_t m_cursorPosition = 0;
    std::optional<size_t> m_selectionStart;

    GeometryAllocation *m_textAlloc = nullptr;
    GlyphSliceHandle m_glyphSlice;
    GeometryAllocation *m_selectionAlloc = nullptr;
    GeometryAllocation *m_cursorAlloc = nullptr;

    vec2 m_textSize = {0.0f, 0.0f};
    std::vector<float> m_charPositions;
    bool m_showingPlaceholder = false;
    float m_textBaselineY = 0.0f;
    float m_textStartX = 0.0f;

    TextLayoutState m_textLayout;
    std::string m_renderedText;
    float m_textWidth = 0.0f;
    FontId m_font;
};

} // namespace Amethyst

#endif // AMETHYST__UI_TEXT_FIELD_H
