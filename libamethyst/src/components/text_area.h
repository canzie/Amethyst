/*
 * Multi-line editable view over a TextSourceBase
 */

#ifndef AMETHYST__TEXT_AREA_H
#define AMETHYST__TEXT_AREA_H

#include "components/properties.h"
#include "components/ui_input.h"
#include "modules/text_processor.h"
#include "modules/text_source.h"
#include "rendering/geometry_registry.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Amethyst {

struct DrawContext;

/**
 * @brief Multi-line editable view of a TextSourceBase.
 *
 * Lays out only the lines that fit in its content box, so the source may hold far more text
 * than the view could ever shape. The view owns the cursor, the selection and the scroll
 * position; the text itself belongs to the source, and every edit goes through its replace().
 */
class TextArea : public UIInput {
  public:
    TextArea();
    ~TextArea() override;

    void draw(DrawContext &ctx) override;
    void resolveStyle() override;

    /**
     * @brief Point the view at the text it reads and edits.
     * @param source Text to show, or null for none; not owned, and must outlive the view
     */
    void setSource(TextSourceBase *source);
    TextSourceBase *getSource() const { return m_source; }

    TextPosition getCursorPosition() const { return m_cursor; }

    /**
     * @brief Move the cursor and drop any selection.
     * @param position Position to move to, clamped to the source
     */
    void setCursorPosition(TextPosition position);

    bool hasSelection() const { return m_anchor.has_value() && *m_anchor != m_cursor; }

    /**
     * @brief The selected span in order, or an empty range at the cursor when nothing is selected.
     */
    TextRange getSelection() const;

    /**
     * @brief Select the whole source.
     */
    void selectAll();

    void clearSelection();

    /**
     * @brief The selected text, with its lines joined by a newline.
     */
    std::string getSelectedText() const;

    uint64_t getFirstVisibleLine() const { return m_firstVisibleLine; }

    /**
     * @brief Put a line at the top of the view.
     * @param line Line to scroll to, clamped to the source
     */
    void scrollToLine(uint64_t line);

    /**
     * @brief Scroll until the cursor is inside the view.
     */
    void scrollToCursor();

    bool setTextAreaProperties(const TextAreaStylePropertiesArgs &props);
    const TextAreaStyleProperties &getTextAreaProperties() const { return m_taProps; }

  protected:
    void onKeyPressed(const KeyEvent &event) override;
    void onCharacterTyped(uint32_t codepoint) override;
    void moveCursorToPoint(vec2 position, bool extendSelection) override;
    EventResult onMouseScrollUp() override;
    EventResult onMouseScrollDown() override;

  private:
    /**
     * @brief Lines that fit in the content box, from the first visible one.
     */
    uint32_t visibleRowCount() const;

    /**
     * @brief Lines that fit in the content box whole, so a cursor on one is never half cut off.
     */
    uint32_t fullyVisibleRowCount() const;

    float lineHeightPx() const;

    /**
     * @brief Bytes on a line, without its terminator.
     */
    uint64_t lineLength(uint64_t line) const;

    /**
     * @brief Pull a position onto a real line and a real codepoint boundary within it.
     */
    TextPosition clampPosition(TextPosition position) const;

    /**
     * @brief Lay out a line, reusing the visible row already laid out for it when there is one.
     * @param line Line to lay out
     * @return The row layout, valid until the next call for a different line
     */
    const TextRowLayout &rowForLine(uint64_t line) const;

    float columnToX(TextPosition position) const;
    uint64_t xToColumn(uint64_t line, float x) const;

    void applyCursor(TextPosition position, bool extendSelection);
    void moveCursorHorizontal(int delta, bool extendSelection);
    void moveCursorVertical(int64_t delta, bool extendSelection);

    void insertText(std::string_view text);
    void deleteSelection();
    void deleteBackward();
    void deleteForward();

    void copy();
    void paste();
    void cut();

    /**
     * @brief Shape the rows the view can show, unless the ones it holds are still current.
     * @return True if the rows were reshaped, false if the cached ones were kept
     */
    bool layoutVisibleRows();

    /**
     * @brief Force the next draw to reshape, after something the shape state cannot see.
     */
    void invalidateRows();

    /**
     * @brief Turn the held rows into glyph quads and hand them to the glyph buffer.
     * @return True if there is anything to draw
     */
    bool buildRowQuads(DrawContext &ctx);

    /**
     * @brief Draw the visible rows.
     * @param reshaped Whether layoutVisibleRows rebuilt the rows this frame
     */
    void drawRows(DrawContext &ctx, bool reshaped);

    void drawSelection(DrawContext &ctx);
    void drawCursor(DrawContext &ctx);
    void releaseRows(DrawContext &ctx);

    /**
     * @brief The content box, narrowed by whatever an ancestor already clips to.
     */
    vec4 contentClipRect() const;

  private:
    /**
     * @brief The inputs the held rows were shaped from, so an unrelated repaint reuses them.
     *
     * A fontSize of 0 marks it unset, so it never matches a freshly built one.
     */
    struct RowShapeState {
        uint64_t revision = 0;
        uint64_t firstLine = 0;
        uint32_t rowCount = 0;
        uint16_t font = FontId::INVALID;
        float fontSize = 0.0f;
        float tabSize = 0.0f;
        uint32_t color = 0;

        bool operator==(const RowShapeState &) const = default;
    };

  private:
    TextSourceBase *m_source = nullptr;
    uint64_t m_sourceRevision = 0;

    TextPosition m_cursor;
    std::optional<TextPosition> m_anchor;

    uint64_t m_firstVisibleLine = 0;
    float m_scrollX = 0.0f;

    // Vertical movement holds the x it started from, so a run of Up/Down keeps its column
    // through short lines instead of collapsing onto them.
    float m_goalX = 0.0f;
    bool m_goalXValid = false;

    TextAreaStyleProperties m_taProps;
    FontId m_font;

    TextProcessor *m_textProcessor = nullptr;
    std::vector<TextRowLayout> m_rows;
    RowShapeState m_rowShape;
    uint64_t m_rowsFirstLine = 0;
    mutable TextRowLayout m_probeRow;
    mutable uint64_t m_probeLine = UINT64_MAX;

    std::vector<GlyphQuad> m_quads;
    std::vector<GlyphLine> m_quadLines;
    float m_rowsWidth = 0.0f;

    GeometryAllocation *m_textAlloc = nullptr;
    GlyphSliceHandle m_glyphSlice;
    std::vector<GeometryAllocation *> m_selectionAllocs;
    GeometryAllocation *m_cursorAlloc = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_AREA_H
