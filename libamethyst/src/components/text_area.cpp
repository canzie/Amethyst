#include "components/text_area.h"

#include "components/input_events.h"
#include "components/input_interface.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "rendering/instance_data.h"
#include "utils/packing.h"
#include "utils/utf8.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace Amethyst {

static constexpr float CURSOR_WIDTH = 1.0f;
static constexpr uint64_t WHEEL_LINES = 3;

static GeometryAllocation *s_pushData(GeometryRegistry *registry, GeometryAllocation *&alloc, const InstanceData &data)
{
    if (alloc == nullptr) {
        alloc = registry->submit(data);
    } else if (alloc->registry != registry) {
        if (alloc->isValid() && alloc->owning) {
            alloc->registry->release(*alloc);
        }
        alloc = registry->submit(data);
    } else {
        registry->update(*alloc, data);
    }
    return alloc;
}

// Where the cursor ends up once `text` has been inserted at `start`.
static TextPosition s_positionAfterInsert(TextPosition start, std::string_view text)
{
    TextPosition end = start;
    size_t lastBreak = text.npos;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\n') {
            end.line++;
            lastBreak = i;
        }
    }

    if (lastBreak == text.npos) {
        end.column = start.column + text.size();
    } else {
        end.column = text.size() - lastBreak - 1;
    }
    return end;
}

TextArea::TextArea()
{
    m_taProps.text.fontSize = 14.0f;
    m_taProps.text.textColor = Color4{0.0f, 0.0f, 0.0f, 1.0f};
    m_taProps.text.lineHeight = 1.0f;
    m_taProps.selectionColor = Color4{0.2f, 0.4f, 0.8f, 0.4f};
    m_taProps.cursorColor = Color4{1.0f, 1.0f, 1.0f, 1.0f};
    m_taProps.cursorBlinkRate = 0.5f;
    m_taProps.readOnly = false;
    m_taProps.tabSize = 4.0f;
    resolveStyle();
}

TextArea::~TextArea()
{
    if (m_textAlloc != nullptr && m_textAlloc->isValid()) {
        if (m_glyphSlice.isValid()) {
            m_textAlloc->registry->glyphBuffer().destroySlice(m_glyphSlice);
        }
        m_textAlloc->registry->release(*m_textAlloc);
    }
    for (GeometryAllocation *alloc : m_selectionAllocs) {
        if (alloc != nullptr && alloc->isValid()) {
            alloc->registry->release(*alloc);
        }
    }
    if (m_cursorAlloc != nullptr && m_cursorAlloc->isValid()) {
        m_cursorAlloc->registry->release(*m_cursorAlloc);
    }
}

void TextArea::resolveStyle()
{
    resolveBaseStyle(ComponentType::TEXT_AREA);

    TextAreaStyleProperties resolved = Style::instance().getTextAreaStyle(ComponentType::TEXT_AREA, getClasses(), effectiveGuiState());
    bool changed = m_taProps.apply(resolved);
    m_font = FontRegistry::instance().findFont(m_taProps.text.fontFamily);
    setCursorBlinkRate(m_taProps.cursorBlinkRate);
    if (changed) {
        markDirty();
    }
}

bool TextArea::setTextAreaProperties(const TextAreaStylePropertiesArgs &props)
{
    bool changed = m_taProps.apply(props);
    m_font = FontRegistry::instance().findFont(m_taProps.text.fontFamily);
    setCursorBlinkRate(m_taProps.cursorBlinkRate);
    if (changed) {
        markDirty();
    }
    return changed;
}

void TextArea::invalidateRows()
{
    m_rowShape = RowShapeState{};
    m_probeLine = UINT64_MAX;
}

void TextArea::setSource(TextSourceBase *source)
{
    m_source = source;
    m_sourceRevision = source != nullptr ? source->revision() : 0;
    m_cursor = TextPosition{};
    m_anchor.reset();
    m_firstVisibleLine = 0;
    m_scrollX = 0.0f;
    m_goalXValid = false;
    invalidateRows();
    markDirty();
}

uint64_t TextArea::lineLength(uint64_t line) const
{
    if (m_source == nullptr) {
        return 0;
    }
    return m_source->line(line).size();
}

TextPosition TextArea::clampPosition(TextPosition position) const
{
    if (m_source == nullptr) {
        return TextPosition{};
    }

    uint64_t lineCount = m_source->lineCount();
    if (lineCount == 0) {
        return TextPosition{};
    }
    if (position.line >= lineCount) {
        position.line = lineCount - 1;
    }

    std::string_view text = m_source->line(position.line);
    if (position.column > text.size()) {
        position.column = text.size();
    } else {
        position.column = Utf8::alignToBoundary(text, position.column);
    }
    return position;
}

TextRange TextArea::getSelection() const
{
    if (!m_anchor.has_value()) {
        return TextRange{m_cursor, m_cursor};
    }
    if (*m_anchor <= m_cursor) {
        return TextRange{*m_anchor, m_cursor};
    }
    return TextRange{m_cursor, *m_anchor};
}

void TextArea::clearSelection()
{
    if (m_anchor.has_value()) {
        m_anchor.reset();
        markDirty();
    }
}

void TextArea::selectAll()
{
    if (m_source == nullptr) {
        return;
    }
    uint64_t lineCount = m_source->lineCount();
    if (lineCount == 0) {
        return;
    }

    m_anchor = TextPosition{0, 0};
    m_cursor = TextPosition{lineCount - 1, lineLength(lineCount - 1)};
    m_goalXValid = false;
    scrollToCursor();
    markDirty();
}

std::string TextArea::getSelectedText() const
{
    TextRange range = getSelection();
    if (m_source == nullptr || range.isEmpty()) {
        return {};
    }

    if (range.start.line == range.end.line) {
        std::string_view text = m_source->line(range.start.line);
        return std::string(text.substr(range.start.column, range.end.column - range.start.column));
    }

    std::string out;
    std::string_view first = m_source->line(range.start.line);
    out.append(first.substr(range.start.column));
    for (uint64_t line = range.start.line + 1; line < range.end.line; line++) {
        out.push_back('\n');
        out.append(m_source->line(line));
    }
    out.push_back('\n');
    out.append(m_source->line(range.end.line).substr(0, range.end.column));
    return out;
}

float TextArea::lineHeightPx() const
{
    if (m_textProcessor == nullptr) {
        return 0.0f;
    }
    FontMetrics metrics = m_textProcessor->getMetricsAtlas(m_font, static_cast<uint32_t>(m_taProps.text.fontSize));
    return metrics.lineHeight * m_taProps.text.lineHeight;
}

uint32_t TextArea::visibleRowCount() const
{
    float lineHeight = lineHeightPx();
    if (lineHeight <= 0.0f || absoluteContentSize.y <= 0.0f) {
        return 0;
    }
    return static_cast<uint32_t>(std::ceil(absoluteContentSize.y / lineHeight));
}

uint32_t TextArea::fullyVisibleRowCount() const
{
    float lineHeight = lineHeightPx();
    if (lineHeight <= 0.0f || absoluteContentSize.y <= 0.0f) {
        return 0;
    }
    return static_cast<uint32_t>(std::floor(absoluteContentSize.y / lineHeight));
}

const TextRowLayout &TextArea::rowForLine(uint64_t line) const
{
    if (line >= m_rowsFirstLine && line - m_rowsFirstLine < m_rows.size()) {
        return m_rows[line - m_rowsFirstLine];
    }

    if (line != m_probeLine) {
        m_probeLine = line;
        if (m_textProcessor != nullptr && m_source != nullptr) {
            TextRowLayoutParams params;
            params.font = m_font;
            params.fontSize = m_taProps.text.fontSize;
            params.tabSize = m_taProps.tabSize;
            m_textProcessor->layoutTextRow(m_source->line(line), params, m_probeRow);
        }
    }
    return m_probeRow;
}

float TextArea::columnToX(TextPosition position) const
{
    const TextRowLayout &row = rowForLine(position.line);
    if (row.columnX.empty()) {
        return 0.0f;
    }
    size_t column = std::min<size_t>(position.column, row.columnX.size() - 1);
    return row.columnX[column];
}

uint64_t TextArea::xToColumn(uint64_t line, float x) const
{
    if (m_source == nullptr) {
        return 0;
    }

    const TextRowLayout &row = rowForLine(line);
    std::string_view text = m_source->line(line);
    if (row.columnX.empty() || text.empty()) {
        return 0;
    }

    // Only codepoint boundaries are valid cursor columns, so walk them and take the nearest.
    size_t boundary = 0;
    while (boundary < text.size()) {
        size_t next = Utf8::nextBoundary(text, boundary);
        if (next >= row.columnX.size()) {
            break;
        }
        float midpoint = (row.columnX[boundary] + row.columnX[next]) * 0.5f;
        if (x < midpoint) {
            return boundary;
        }
        boundary = next;
    }
    return boundary;
}

void TextArea::setCursorPosition(TextPosition position)
{
    m_anchor.reset();
    m_cursor = clampPosition(position);
    m_goalXValid = false;
    scrollToCursor();
    markDirty();
}

void TextArea::applyCursor(TextPosition position, bool extendSelection)
{
    if (extendSelection) {
        if (!m_anchor.has_value()) {
            m_anchor = m_cursor;
        }
    } else {
        m_anchor.reset();
    }

    m_cursor = clampPosition(position);
    resetCursorBlink();
    scrollToCursor();
    markDirty();
}

void TextArea::scrollToLine(uint64_t line)
{
    if (m_source != nullptr) {
        uint64_t lineCount = m_source->lineCount();
        if (lineCount > 0 && line >= lineCount) {
            line = lineCount - 1;
        }
    }
    if (m_firstVisibleLine != line) {
        m_firstVisibleLine = line;
        markDirty();
    }
}

void TextArea::scrollToCursor()
{
    uint32_t rows = fullyVisibleRowCount();
    if (rows > 0) {
        if (m_cursor.line < m_firstVisibleLine) {
            m_firstVisibleLine = m_cursor.line;
        } else if (m_cursor.line >= m_firstVisibleLine + rows) {
            m_firstVisibleLine = m_cursor.line - rows + 1;
        }
    }

    float width = absoluteContentSize.x;
    if (width > CURSOR_WIDTH) {
        float x = columnToX(m_cursor);
        if (x < m_scrollX) {
            m_scrollX = x;
        } else if (x > m_scrollX + width - CURSOR_WIDTH) {
            m_scrollX = x - width + CURSOR_WIDTH;
        }
    }
    if (m_scrollX < 0.0f) {
        m_scrollX = 0.0f;
    }
}

void TextArea::moveCursorHorizontal(int delta, bool extendSelection)
{
    if (m_source == nullptr) {
        return;
    }

    TextPosition target = m_cursor;
    std::string_view text = m_source->line(m_cursor.line);

    if (delta < 0) {
        if (m_cursor.column > 0) {
            target.column = Utf8::prevBoundary(text, m_cursor.column);
        } else if (m_cursor.line > 0) {
            target.line = m_cursor.line - 1;
            target.column = lineLength(target.line);
        }
    } else {
        if (m_cursor.column < text.size()) {
            target.column = Utf8::nextBoundary(text, m_cursor.column);
        } else if (m_cursor.line + 1 < m_source->lineCount()) {
            target.line = m_cursor.line + 1;
            target.column = 0;
        }
    }

    applyCursor(target, extendSelection);
    m_goalXValid = false;
}

void TextArea::moveCursorVertical(int64_t delta, bool extendSelection)
{
    if (m_source == nullptr) {
        return;
    }

    if (!m_goalXValid) {
        m_goalX = columnToX(m_cursor);
        m_goalXValid = true;
    }

    int64_t target = static_cast<int64_t>(m_cursor.line) + delta;
    if (target < 0) {
        target = 0;
    }
    uint64_t lineCount = m_source->lineCount();
    if (lineCount > 0 && static_cast<uint64_t>(target) >= lineCount) {
        target = static_cast<int64_t>(lineCount - 1);
    }

    TextPosition position;
    position.line = static_cast<uint64_t>(target);
    position.column = xToColumn(position.line, m_goalX);
    applyCursor(position, extendSelection);
}

void TextArea::moveCursorToPoint(vec2 position, bool extendSelection)
{
    if (m_source == nullptr) {
        return;
    }

    float lineHeight = lineHeightPx();
    if (lineHeight <= 0.0f) {
        return;
    }

    int64_t row = static_cast<int64_t>(std::floor((position.y - absoluteContentPosition.y) / lineHeight));
    int64_t line = static_cast<int64_t>(m_firstVisibleLine) + row;
    if (line < 0) {
        line = 0;
    }
    uint64_t lineCount = m_source->lineCount();
    if (lineCount > 0 && static_cast<uint64_t>(line) >= lineCount) {
        line = static_cast<int64_t>(lineCount - 1);
    }

    TextPosition target;
    target.line = static_cast<uint64_t>(line);
    target.column = xToColumn(target.line, position.x - absoluteContentPosition.x + m_scrollX);

    applyCursor(target, extendSelection);
    m_goalXValid = false;
}

EventResult TextArea::onMouseScrollUp()
{
    scrollToLine(m_firstVisibleLine > WHEEL_LINES ? m_firstVisibleLine - WHEEL_LINES : 0);
    return EventResult::CONSUMED;
}

EventResult TextArea::onMouseScrollDown()
{
    scrollToLine(m_firstVisibleLine + WHEEL_LINES);
    return EventResult::CONSUMED;
}

void TextArea::insertText(std::string_view text)
{
    if (m_source == nullptr || m_taProps.readOnly) {
        return;
    }

    TextRange range = getSelection();
    m_source->replace(range, text);
    m_sourceRevision = m_source->revision();

    m_cursor = s_positionAfterInsert(range.start, text);
    m_anchor.reset();
    m_goalXValid = false;
    invalidateRows();
    resetCursorBlink();
    scrollToCursor();
    markDirty();
}

void TextArea::deleteSelection()
{
    if (m_source == nullptr || m_taProps.readOnly || !hasSelection()) {
        return;
    }

    TextRange range = getSelection();
    m_source->replace(range, {});
    m_sourceRevision = m_source->revision();

    m_cursor = range.start;
    m_anchor.reset();
    m_goalXValid = false;
    invalidateRows();
    resetCursorBlink();
    scrollToCursor();
    markDirty();
}

void TextArea::deleteBackward()
{
    if (m_source == nullptr || m_taProps.readOnly) {
        return;
    }
    if (hasSelection()) {
        deleteSelection();
        return;
    }

    TextPosition from = m_cursor;
    if (m_cursor.column > 0) {
        from.column = Utf8::prevBoundary(m_source->line(m_cursor.line), m_cursor.column);
    } else if (m_cursor.line > 0) {
        from.line = m_cursor.line - 1;
        from.column = lineLength(from.line);
    } else {
        return;
    }

    m_source->replace(TextRange{from, m_cursor}, {});
    m_sourceRevision = m_source->revision();

    m_cursor = from;
    m_anchor.reset();
    m_goalXValid = false;
    invalidateRows();
    resetCursorBlink();
    scrollToCursor();
    markDirty();
}

void TextArea::deleteForward()
{
    if (m_source == nullptr || m_taProps.readOnly) {
        return;
    }
    if (hasSelection()) {
        deleteSelection();
        return;
    }

    std::string_view text = m_source->line(m_cursor.line);
    TextPosition to = m_cursor;
    if (m_cursor.column < text.size()) {
        to.column = Utf8::nextBoundary(text, m_cursor.column);
    } else if (m_cursor.line + 1 < m_source->lineCount()) {
        to.line = m_cursor.line + 1;
        to.column = 0;
    } else {
        return;
    }

    m_source->replace(TextRange{m_cursor, to}, {});
    m_sourceRevision = m_source->revision();

    m_goalXValid = false;
    invalidateRows();
    resetCursorBlink();
    markDirty();
}

void TextArea::copy()
{
    std::string selected = getSelectedText();
    if (!selected.empty()) {
        InputInterface::setClipboardText(selected);
    }
}

void TextArea::paste()
{
    std::string clipboard = InputInterface::getClipboardText();
    if (clipboard.empty()) {
        return;
    }

    // Text copied out of another application can carry CRLF, and a carriage return left in
    // the buffer is a byte on the line rather than a break.
    std::erase(clipboard, '\r');
    insertText(clipboard);
}

void TextArea::cut()
{
    if (m_taProps.readOnly || !hasSelection()) {
        return;
    }
    copy();
    deleteSelection();
}

void TextArea::onKeyPressed(const KeyEvent &event)
{
    bool ctrl = (event.mods & MOD_CONTROL) != 0;
    bool shift = (event.mods & MOD_SHIFT) != 0;

    switch (event.key) {
    case KEY_LEFT:
        moveCursorHorizontal(-1, shift);
        break;

    case KEY_RIGHT:
        moveCursorHorizontal(1, shift);
        break;

    case KEY_UP:
        moveCursorVertical(-1, shift);
        break;

    case KEY_DOWN:
        moveCursorVertical(1, shift);
        break;

    case KEY_PAGE_UP:
        moveCursorVertical(-static_cast<int64_t>(std::max(fullyVisibleRowCount(), 1u)), shift);
        break;

    case KEY_PAGE_DOWN:
        moveCursorVertical(static_cast<int64_t>(std::max(fullyVisibleRowCount(), 1u)), shift);
        break;

    case KEY_HOME:
        applyCursor(ctrl ? TextPosition{0, 0} : TextPosition{m_cursor.line, 0}, shift);
        m_goalXValid = false;
        break;

    case KEY_END:
        if (ctrl && m_source != nullptr && m_source->lineCount() > 0) {
            uint64_t last = m_source->lineCount() - 1;
            applyCursor(TextPosition{last, lineLength(last)}, shift);
        } else {
            applyCursor(TextPosition{m_cursor.line, lineLength(m_cursor.line)}, shift);
        }
        m_goalXValid = false;
        break;

    case KEY_BACKSPACE:
        deleteBackward();
        break;

    case KEY_DELETE:
        deleteForward();
        break;

    case KEY_ENTER:
        insertText("\n");
        break;

    case KEY_TAB:
        insertText("\t");
        break;

    case KEY_A:
        if (ctrl) {
            selectAll();
        }
        break;

    case KEY_C:
        if (ctrl) {
            copy();
        }
        break;

    case KEY_V:
        if (ctrl) {
            paste();
        }
        break;

    case KEY_X:
        if (ctrl) {
            cut();
        }
        break;

    default:
        break;
    }
}

void TextArea::onCharacterTyped(uint32_t codepoint)
{
    if (codepoint < 32) {
        return;
    }

    char utf8[Utf8::MAX_SEQUENCE];
    size_t bytes = Utf8::encode(codepoint, utf8);
    insertText(std::string_view(utf8, bytes));
}

bool TextArea::layoutVisibleRows()
{
    if (m_source == nullptr || m_textProcessor == nullptr) {
        bool had = !m_rows.empty();
        m_rows.clear();
        m_rowShape = RowShapeState{};
        return had;
    }

    uint64_t lineCount = m_source->lineCount();
    if (m_firstVisibleLine >= lineCount) {
        m_firstVisibleLine = lineCount > 0 ? lineCount - 1 : 0;
    }

    uint64_t rows = visibleRowCount();
    uint64_t available = lineCount > m_firstVisibleLine ? lineCount - m_firstVisibleLine : 0;
    if (rows > available) {
        rows = available;
    }

    RowShapeState next;
    next.revision = m_source->revision();
    next.firstLine = m_firstVisibleLine;
    next.rowCount = static_cast<uint32_t>(rows);
    next.font = m_font.index;
    next.fontSize = m_taProps.text.fontSize;
    next.tabSize = m_taProps.tabSize;
    next.color = packColor(m_taProps.text.textColor);

    if (next == m_rowShape) {
        return false;
    }
    m_rowShape = next;
    m_probeLine = UINT64_MAX;

    TextRowLayoutParams params;
    params.font = m_font;
    params.fontSize = m_taProps.text.fontSize;
    params.tabSize = m_taProps.tabSize;

    // resize() rather than clear() + resize(): a row keeps the buffers it shaped into last
    // frame, so a steady viewport reshapes without touching the allocator.
    m_rowsFirstLine = m_firstVisibleLine;
    m_rows.resize(rows);
    for (uint64_t i = 0; i < rows; i++) {
        m_textProcessor->layoutTextRow(m_source->line(m_firstVisibleLine + i), params, m_rows[i]);
    }
    return true;
}

vec4 TextArea::contentClipRect() const
{
    vec4 box = {absoluteContentPosition.x, absoluteContentPosition.y, absoluteContentPosition.x + absoluteContentSize.x,
                absoluteContentPosition.y + absoluteContentSize.y};

    if (clipRect.z > 0.0f && clipRect.w > 0.0f) {
        box.x = std::max(box.x, clipRect.x);
        box.y = std::max(box.y, clipRect.y);
        box.z = std::min(box.z, clipRect.z);
        box.w = std::min(box.w, clipRect.w);
    }
    return box;
}

void TextArea::releaseRows(DrawContext &ctx)
{
    if (m_glyphSlice.isValid()) {
        ctx.geometry->glyphBuffer().destroySlice(m_glyphSlice);
        m_glyphSlice = GlyphSliceHandle{};
    }
    if (m_textAlloc != nullptr) {
        if (m_textAlloc->isValid()) {
            ctx.geometry->release(*m_textAlloc);
        }
        m_textAlloc = nullptr;
    }
}

bool TextArea::buildRowQuads(DrawContext &ctx)
{
    m_quads.clear();
    m_quadLines.clear();

    float lineHeight = lineHeightPx();
    uint32_t color = packColor(m_taProps.text.textColor);
    float maxWidth = 0.0f;

    for (size_t row = 0; row < m_rows.size(); row++) {
        const TextRowLayout &layout = m_rows[row];
        maxWidth = std::max(maxWidth, layout.width);

        GlyphLine line;
        line.glyphStart = static_cast<uint32_t>(m_quads.size());
        line.glyphCount = 0;

        float rowTop = static_cast<float>(row) * lineHeight;
        for (const TextRowGlyph &glyph : layout.glyphs) {
            const GlyphInfo *info = glyph.info;
            float y = rowTop + glyph.y;

            GlyphQuad quad;
            quad.posMin = packU16x2(clampToU16(glyph.x), clampToU16(y));
            quad.posMax = packU16x2(clampToU16(glyph.x + info->width), clampToU16(y + info->height));
            quad.uvMin = packU16x2(info->x, info->y);
            quad.uvMax = packU16x2(static_cast<uint16_t>(info->x + info->width),
                                   static_cast<uint16_t>(info->y + info->height));
            quad.color = color;
            quad.textureId = ctx.glyphAtlas->getTextureId(info->page).id;

            m_quads.push_back(quad);
            line.glyphCount++;
        }

        m_quadLines.push_back(line);
    }

    m_rowsWidth = maxWidth;

    if (m_quads.empty()) {
        return false;
    }

    if (m_glyphSlice.isValid() && m_textAlloc != nullptr && m_textAlloc->registry != ctx.geometry) {
        if (GlyphBuffer *oldBuffer = m_textAlloc->registry->getGlyphBuffer()) {
            oldBuffer->destroySlice(m_glyphSlice);
        }
        m_glyphSlice = GlyphSliceHandle{};
    }

    GlyphBuffer &glyphBuffer = ctx.geometry->glyphBuffer();
    if (!m_glyphSlice.isValid()) {
        m_glyphSlice = glyphBuffer.createSlice();
    }
    glyphBuffer.updateSlice(m_glyphSlice, m_quads.data(), static_cast<uint32_t>(m_quads.size()), m_quadLines.data(),
                            static_cast<uint32_t>(m_quadLines.size()), lineHeight);
    return true;
}

void TextArea::drawRows(DrawContext &ctx, bool reshaped)
{
    if (m_rows.empty() || ctx.glyphAtlas == nullptr) {
        releaseRows(ctx);
        return;
    }

    bool registryChanged = m_textAlloc != nullptr && m_textAlloc->registry != ctx.geometry;
    if (reshaped || registryChanged || m_textAlloc == nullptr || !m_glyphSlice.isValid()) {
        if (!buildRowQuads(ctx)) {
            releaseRows(ctx);
            return;
        }
    }

    // The quad starts at the unscrolled text origin so no glyph needs a negative offset, and
    // the clip rect takes back everything left of the content box.
    vec2 pos = {absoluteContentPosition.x - m_scrollX, absoluteContentPosition.y};
    vec2 size = {std::max(m_rowsWidth, 1.0f), static_cast<float>(m_rows.size()) * lineHeightPx()};

    InstanceData inst{};
    inst.translation = pos + size * 0.5f;
    inst.scale = size;
    inst.setFillColor(m_taProps.text.textColor);
    inst.setPrimitiveType(PRIMITIVE_TEXT);
    inst.setGlyphSlice(m_glyphSlice.id);
    inst.zIndex = getZIndex() + 1;
    inst.clipRect = contentClipRect();
    inst.setVisible(isVisible());

    s_pushData(ctx.geometry, m_textAlloc, inst);
}

void TextArea::drawSelection(DrawContext &ctx)
{
    if (!m_selectionAllocs.empty() && m_selectionAllocs.front()->registry != ctx.geometry) {
        for (GeometryAllocation *alloc : m_selectionAllocs) {
            if (alloc->isValid() && alloc->owning) {
                alloc->registry->release(*alloc);
            }
        }
        m_selectionAllocs.clear();
    }

    float lineHeight = lineHeightPx();
    vec4 clip = contentClipRect();
    size_t used = 0;

    auto pushRow = [&](float x0, float x1, float top) {
        vec2 size = {std::max(x1 - x0, 1.0f), lineHeight};
        vec2 pos = {x0, top};

        InstanceData data{};
        data.translation = pos + size * 0.5f;
        data.scale = size;
        data.setFillColor(m_taProps.selectionColor);
        data.setPrimitiveType(PRIMITIVE_RECT);
        data.zIndex = getZIndex();
        data.clipRect = clip;
        data.setVisible(isVisible());

        if (used < m_selectionAllocs.size()) {
            ctx.geometry->update(*m_selectionAllocs[used], data);
        } else {
            m_selectionAllocs.push_back(ctx.geometry->submit(data));
        }
        used++;
    };

    if (hasSelection() && !m_rows.empty() && lineHeight > 0.0f) {
        TextRange range = getSelection();
        uint64_t lastVisible = m_rowsFirstLine + m_rows.size() - 1;

        if (range.start.line <= lastVisible && range.end.line >= m_rowsFirstLine) {
            uint64_t first = std::max(range.start.line, m_rowsFirstLine);
            uint64_t last = std::min(range.end.line, lastVisible);
            float originX = absoluteContentPosition.x - m_scrollX;

            for (uint64_t line = first; line <= last; line++) {
                const TextRowLayout &layout = m_rows[line - m_rowsFirstLine];
                if (layout.columnX.empty()) {
                    continue;
                }

                uint64_t startColumn = (line == range.start.line) ? range.start.column : 0;
                uint64_t endColumn = (line == range.end.line) ? range.end.column : lineLength(line);
                size_t lastIndex = layout.columnX.size() - 1;

                float x0 = originX + layout.columnX[std::min<size_t>(startColumn, lastIndex)];
                float x1 = originX + layout.columnX[std::min<size_t>(endColumn, lastIndex)];

                // A line whose break is inside the selection shows that break as trailing space.
                if (line < range.end.line) {
                    x1 += m_taProps.text.fontSize * 0.5f;
                }

                pushRow(x0, x1, absoluteContentPosition.y + static_cast<float>(line - m_rowsFirstLine) * lineHeight);
            }
        }
    }

    // A drag changes the selected row count on every mouse move, so the slots stay put and
    // the spare ones go invisible. The pool cannot outgrow what the view can show.
    while (m_selectionAllocs.size() > m_rows.size()) {
        ctx.geometry->release(*m_selectionAllocs.back());
        m_selectionAllocs.pop_back();
    }
    for (size_t i = used; i < m_selectionAllocs.size(); i++) {
        InstanceData hidden{};
        hidden.setVisible(false);
        ctx.geometry->update(*m_selectionAllocs[i], hidden);
    }
}

void TextArea::drawCursor(DrawContext &ctx)
{
    float lineHeight = lineHeightPx();
    bool visible = isFocused() && isCursorVisible() && lineHeight > 0.0f && m_cursor.line >= m_rowsFirstLine &&
                   m_cursor.line - m_rowsFirstLine < m_rows.size();

    if (!visible && m_cursorAlloc == nullptr) {
        return;
    }

    vec2 size = {CURSOR_WIDTH, lineHeight};
    vec2 pos{};
    if (visible) {
        pos = {absoluteContentPosition.x - m_scrollX + columnToX(m_cursor),
               absoluteContentPosition.y + static_cast<float>(m_cursor.line - m_rowsFirstLine) * lineHeight};
    }

    InstanceData data{};
    data.translation = pos + size * 0.5f;
    data.scale = size;
    data.setFillColor(m_taProps.cursorColor);
    data.setPrimitiveType(PRIMITIVE_RECT);
    data.zIndex = getZIndex() + 2;
    data.clipRect = contentClipRect();
    data.setVisible(visible && isVisible());

    s_pushData(ctx.geometry, m_cursorAlloc, data);
}

void TextArea::draw(DrawContext &ctx)
{
    m_textProcessor = ctx.textProcessor;

    if (m_source != nullptr && m_source->revision() != m_sourceRevision) {
        m_sourceRevision = m_source->revision();
        m_cursor = clampPosition(m_cursor);
        if (m_anchor.has_value()) {
            m_anchor = clampPosition(*m_anchor);
        }
        markDirty();
    }

    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData background = createInstanceData();
        background.setPrimitiveType(PRIMITIVE_RECT);
        pushData(ctx.geometry, background);

        bool reshaped = layoutVisibleRows();
        drawRows(ctx, reshaped);
        drawSelection(ctx);
        drawCursor(ctx);
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
