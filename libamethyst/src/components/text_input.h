/*
 * Text input field with cursor and selection
 */

#ifndef AMETHYST__TEXT_INPUT_H
#define AMETHYST__TEXT_INPUT_H

#include "components/properties.h"
#include "components/ui_object.h"
#include "rendering/geometry_registry.h"

#include <functional>
#include <optional>
#include <string>

namespace Amethyst {

struct DrawContext;

class TextInput : public UIObject {
  public:
    TextInput() = default;
    virtual ~TextInput();

    void draw(DrawContext &ctx) override;
    void update(float deltaTime);

    std::string getText() const { return m_text; }
    void setText(const std::string &text);
    void clearText();

  protected:
    EventResult onMouseButton1Down(uint32_t x, uint32_t y) override;
    EventResult onMouseButton1Up(uint32_t x, uint32_t y) override;
    EventResult onMouseButton1Click() override;
    EventResult onMouseMoved(uint32_t x, uint32_t y) override;
    EventResult onMouseEnter() override;
    EventResult onMouseLeave() override;

  private:
    void processKeyboardInput();
    void insertText(const std::string &text);
    void deleteSelection();
    void moveCursor(int delta, bool select);
    void setCursorPosition(size_t pos, bool select);
    size_t getCursorFromMouseX(uint32_t mouseX);

    void copy();
    void paste();
    void cut();
    void selectAll();

    void releaseTextAllocations(GeometryRegistry *geometry);
    void drawText(DrawContext &ctx);
    void drawSelection(DrawContext &ctx);
    void drawCursor(DrawContext &ctx);

  public:
    bool setTextInputProperties(const TextInputProperties &props);
    const TextInputProperties &getTextInputProperties() const { return m_tiProps; }

    std::function<void(const std::string &)> onTextChanged;
    std::function<void()> onEnterPressed;
    std::function<void()> onFocusGained;
    std::function<void()> onFocusLost;

  protected:
    TextInputProperties m_tiProps;

  private:
    std::string m_text;
    size_t m_cursorPosition = 0;
    std::optional<size_t> m_selectionStart;
    bool m_focused = false;
    float m_cursorBlinkTimer = 0.0f;
    bool m_cursorVisible = true;
    bool m_draggingSelection = false;

    std::vector<GeometryAllocation *> m_textAllocations;
    GeometryAllocation *m_selectionAlloc = nullptr;
    GeometryAllocation *m_cursorAlloc = nullptr;

    glm::vec2 m_textSize = {0.0f, 0.0f};
    std::vector<float> m_charPositions;
    bool m_showingPlaceholder = false;
    float m_textBaselineY = 0.0f;
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_INPUT_H
