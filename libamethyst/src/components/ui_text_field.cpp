#include "components/ui_text_field.h"

#include "components/input_events.h"
#include "components/input_interface.h"
#include "modules/style.h"
#include "modules/text_processor.h"
#include "rendering/draw_context.h"
#include "rendering/instance_data.h"
#include "utils/utf8.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace Amethyst {

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

UITextField::UITextField()
{
    m_tiProps.placeholderColor = Color4{0.5f, 0.5f, 0.5f, 1.0f};
    m_tiProps.selectionColor = Color4{0.3f, 0.5f, 0.9f, 0.5f};
    m_tiProps.cursorColor = Color4{0.0f, 0.0f, 0.0f, 1.0f};
    m_tiProps.text.fontSize = 14.0f;
    m_tiProps.text.textColor = Color4{0.0f, 0.0f, 0.0f, 1.0f};
    m_tiProps.text.textXAlignment = TextXAlignment::LEFT;
    m_tiProps.text.textYAlignment = TextYAlignment::CENTER;
    m_tiProps.multiline = false;
    m_tiProps.maxLength = -1;
    m_tiProps.readOnly = false;
    m_tiProps.cursorBlinkRate = 0.5f;
    setCursorBlinkRate(m_tiProps.cursorBlinkRate);
}

UITextField::~UITextField()
{
    if (m_textAlloc != nullptr && m_textAlloc->isValid()) {
        if (m_glyphSlice.isValid()) {
            m_textAlloc->registry->glyphBuffer().destroySlice(m_glyphSlice);
        }
        m_textAlloc->registry->release(*m_textAlloc);
    }
    if (m_selectionAlloc && m_selectionAlloc->isValid()) {
        m_selectionAlloc->registry->release(*m_selectionAlloc);
    }
    if (m_cursorAlloc && m_cursorAlloc->isValid()) {
        m_cursorAlloc->registry->release(*m_cursorAlloc);
    }
}

bool UITextField::setTextInputProperties(const TextInputStylePropertiesArgs &props)
{
    bool changed = m_tiProps.apply(props);
    m_font = FontRegistry::instance().findFont(m_tiProps.text.fontFamily);
    setCursorBlinkRate(m_tiProps.cursorBlinkRate);
    if (changed) {
        markDirty();
    }
    return changed;
}

void UITextField::resolveTextInputStyle(ComponentType type)
{
    resolveBaseStyle(type);

    TextInputStyleProperties resolved = Style::instance().getTextInputStyle(type, getClasses(), effectiveGuiState());
    bool changed = m_tiProps.apply(resolved);
    m_font = FontRegistry::instance().findFont(m_tiProps.text.fontFamily);
    setCursorBlinkRate(m_tiProps.cursorBlinkRate);
    if (changed) {
        markDirty();
    }
}

void UITextField::setPlaceholder(std::string placeholder)
{
    if (m_placeholder != placeholder) {
        m_placeholder = std::move(placeholder);
        markDirty();
    }
}

void UITextField::setText(const std::string &text)
{
    m_text = text;
    m_cursorPosition = std::min(m_cursorPosition, m_text.size());
    m_selectionStart = std::nullopt;
    markDirty();
    if (onTextChanged) {
        onTextChanged(m_text);
    }
}

void UITextField::clearText()
{
    m_text.clear();
    m_cursorPosition = 0;
    m_selectionStart = std::nullopt;
    markDirty();
    if (onTextChanged) {
        onTextChanged(m_text);
    }
}

void UITextField::moveCursorToPoint(vec2 position, bool extendSelection)
{
    setCursorPosition(getCursorFromMouseX(static_cast<int32_t>(position.x)), extendSelection);
}

void UITextField::onKeyPressed(const KeyEvent &event)
{
    if (m_tiProps.readOnly) {
        return;
    }

    bool ctrl = event.mods & MOD_CONTROL;
    bool shift = event.mods & MOD_SHIFT;

    switch (event.key) {
    case KEY_BACKSPACE:
        if (m_selectionStart.has_value()) {
            deleteSelection();
        } else if (m_cursorPosition > 0) {
            // Erase a whole codepoint, not a byte: a multi-byte sequence would
            // otherwise be left truncated and render as a replacement box.
            size_t start = Utf8::prevBoundary(m_text, m_cursorPosition);
            m_text.erase(start, m_cursorPosition - start);
            m_cursorPosition = start;
            markDirty();
            if (onTextChanged) {
                onTextChanged(m_text);
            }
        }
        resetCursorBlink();
        break;

    case KEY_DELETE:
        if (m_selectionStart.has_value()) {
            deleteSelection();
        } else if (m_cursorPosition < m_text.size()) {
            size_t end = Utf8::nextBoundary(m_text, m_cursorPosition);
            m_text.erase(m_cursorPosition, end - m_cursorPosition);
            markDirty();
            if (onTextChanged) {
                onTextChanged(m_text);
            }
        }
        resetCursorBlink();
        break;

    case KEY_LEFT:
        moveCursor(-1, shift);
        resetCursorBlink();
        break;

    case KEY_RIGHT:
        moveCursor(1, shift);
        resetCursorBlink();
        break;

    case KEY_HOME:
        setCursorPosition(0, shift);
        resetCursorBlink();
        break;

    case KEY_END:
        setCursorPosition(m_text.size(), shift);
        resetCursorBlink();
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

    case KEY_ENTER:
        if (m_tiProps.multiline) {
            insertText("\n");
        } else {
            onCommit();
            if (onEnterPressed) {
                onEnterPressed();
            }
        }
        resetCursorBlink();
        break;

    default:
        break;
    }
}

void UITextField::onCharacterTyped(uint32_t codepoint)
{
    if (m_tiProps.readOnly) {
        return;
    }

    if (codepoint >= 32 || codepoint == '\n') {
        char utf8[Utf8::MAX_SEQUENCE];
        size_t bytes = Utf8::encode(codepoint, utf8);
        insertText(std::string(utf8, bytes));
        resetCursorBlink();
    }
}

void UITextField::insertText(const std::string &text)
{
    if (m_tiProps.readOnly) {
        return;
    }

    std::string candidate = m_text;
    size_t insertAt = m_cursorPosition;
    if (m_selectionStart.has_value()) {
        size_t start = std::min(m_cursorPosition, *m_selectionStart);
        size_t end = std::max(m_cursorPosition, *m_selectionStart);
        candidate.erase(start, end - start);
        insertAt = start;
    }
    candidate.insert(insertAt, text);

    if (m_tiProps.maxLength >= 0 && static_cast<int32_t>(candidate.size()) > m_tiProps.maxLength) {
        return;
    }
    if (!acceptText(candidate)) {
        return;
    }

    if (m_selectionStart.has_value()) {
        deleteSelection();
    }

    m_text.insert(m_cursorPosition, text);
    m_cursorPosition += text.size();
    markDirty();

    if (onTextChanged) {
        onTextChanged(m_text);
    }
}

void UITextField::deleteSelection()
{
    if (!m_selectionStart.has_value()) {
        return;
    }

    size_t start = std::min(m_cursorPosition, *m_selectionStart);
    size_t end = std::max(m_cursorPosition, *m_selectionStart);

    m_text.erase(start, end - start);
    m_cursorPosition = start;
    m_selectionStart = std::nullopt;
    markDirty();

    if (onTextChanged) {
        onTextChanged(m_text);
    }
}

void UITextField::moveCursor(int delta, bool select)
{
    // delta counts codepoints, not bytes, so the caret never lands inside a sequence.
    if (delta < 0) {
        if (m_cursorPosition > 0) {
            setCursorPosition(Utf8::prevBoundary(m_text, m_cursorPosition), select);
        }
    } else if (delta > 0) {
        if (m_cursorPosition < m_text.size()) {
            setCursorPosition(Utf8::nextBoundary(m_text, m_cursorPosition), select);
        }
    }
}

void UITextField::setCursorPosition(size_t pos, bool select)
{
    pos = Utf8::alignToBoundary(m_text, std::min(pos, m_text.size()));

    if (select) {
        if (!m_selectionStart.has_value()) {
            m_selectionStart = m_cursorPosition;
        }
    } else {
        m_selectionStart = std::nullopt;
    }

    m_cursorPosition = pos;
    markDirty();
}

size_t UITextField::getCursorFromMouseX(int32_t mouseX)
{
    if (m_charPositions.empty() || m_text.empty()) {
        return 0;
    }

    float relativeX = static_cast<float>(mouseX) - m_textStartX;

    // Walk codepoint boundaries and return the nearest one. m_charPositions is indexed by
    // byte so the lookups are direct, but only boundaries are valid caret positions.
    size_t boundary = 0;
    while (boundary < m_text.size()) {
        size_t next = Utf8::nextBoundary(m_text, boundary);
        if (next >= m_charPositions.size()) {
            break;
        }
        float midpoint = (m_charPositions[boundary] + m_charPositions[next]) * 0.5f;
        if (relativeX < midpoint) {
            return boundary;
        }
        boundary = next;
    }

    return boundary;
}

void UITextField::copy()
{
    if (!m_selectionStart.has_value()) {
        return;
    }

    size_t start = std::min(m_cursorPosition, *m_selectionStart);
    size_t end = std::max(m_cursorPosition, *m_selectionStart);
    std::string selectedText = m_text.substr(start, end - start);

    InputInterface::setClipboardText(selectedText);
}

void UITextField::paste()
{
    if (m_tiProps.readOnly) {
        return;
    }

    std::string clipboardText = InputInterface::getClipboardText();
    if (!clipboardText.empty()) {
        insertText(clipboardText);
    }
}

void UITextField::cut()
{
    if (m_tiProps.readOnly || !m_selectionStart.has_value()) {
        return;
    }

    copy();
    deleteSelection();
}

void UITextField::selectAll()
{
    m_selectionStart = 0;
    m_cursorPosition = m_text.size();
    markDirty();
}

void UITextField::releaseText(DrawContext &ctx)
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

void UITextField::drawText(DrawContext &ctx)
{
    switch (m_tiProps.text.textYAlignment) {
    case TextYAlignment::CENTER:
        m_textBaselineY = absoluteContentPosition.y + (absoluteContentSize.y - m_tiProps.text.fontSize) / 2.0f;
        break;
    case TextYAlignment::BOTTOM:
        m_textBaselineY = absoluteContentPosition.y + absoluteContentSize.y - m_tiProps.text.fontSize;
        break;
    default:
        m_textBaselineY = absoluteContentPosition.y;
        break;
    }

    auto alignStartX = [this](float textWidth) {
        float slack = absoluteContentSize.x - textWidth;
        switch (m_tiProps.text.textXAlignment) {
        case TextXAlignment::CENTER:
            return absoluteContentPosition.x + slack * 0.5f;
        case TextXAlignment::RIGHT:
            return absoluteContentPosition.x + slack;
        default:
            return absoluteContentPosition.x;
        }
    };
    m_textStartX = alignStartX(0.0f);

    bool shouldShowPlaceholder = m_text.empty() && !m_placeholder.empty();
    m_showingPlaceholder = shouldShowPlaceholder;

    std::string shown = displayText();
    const std::string &textToRender = m_showingPlaceholder ? m_placeholder : shown;
    const Color4 &colorToUse = m_showingPlaceholder ? m_tiProps.placeholderColor : m_tiProps.text.textColor;

    if (textToRender.empty()) {
        releaseText(ctx);
        m_charPositions.clear();
        m_renderedText.clear();
        m_textLayout.invalidate();
        m_textWidth = 0.0f;
        return;
    }

    TextLayoutParams params;
    params.position = absoluteContentPosition;
    params.bounds = absoluteContentSize;
    params.font = m_font;
    params.fontSize = m_tiProps.text.fontSize;
    params.color = colorToUse;
    params.xAlign = m_tiProps.text.textXAlignment;
    params.yAlign = m_tiProps.text.textYAlignment;
    params.wrap = m_tiProps.multiline;

    TextLayoutState next;
    next.fontSize = params.fontSize;
    next.bounds = params.bounds;
    next.lineHeight = params.lineHeight;
    next.color = packColor(params.color);
    next.zIndex = getZIndex() + 1;
    next.xAlign = params.xAlign;
    next.yAlign = params.yAlign;
    next.truncate = params.truncate;
    next.wrap = params.wrap;
    next.origin = params.position;

    bool textChanged = textToRender != m_renderedText;
    bool sizeChanged = m_textLayout.fontSize != next.fontSize;
    bool registryChanged = m_textAlloc != nullptr && m_textAlloc->registry != ctx.geometry;

    if (!textChanged && !registryChanged && m_textLayout.matches(next)) {
        if (m_textAlloc != nullptr) {
            if (InstanceData *inst = ctx.geometry->getMutable(*m_textAlloc)) {
                inst->translation = inst->translation + (next.origin - m_textLayout.origin);
                inst->clipRect = clipRect;
                inst->setVisible(isVisible());
            }
        }
        m_textLayout = next;
        m_textStartX = alignStartX(m_textWidth);
        return;
    }

    m_textLayout = next;

    BatchedText batched = ctx.textProcessor->layoutTextBatched(textToRender, params);
    if (batched.glyphs.empty()) {
        releaseText(ctx);
    } else {
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
        glyphBuffer.updateSlice(m_glyphSlice, batched.glyphs.data(), static_cast<uint32_t>(batched.glyphs.size()),
                                batched.lines.data(), static_cast<uint32_t>(batched.lines.size()), batched.lineHeightPx);

        InstanceData inst{};
        inst.translation = batched.pos + batched.size * 0.5f;
        inst.scale = batched.size;
        inst.setFillColor(colorToUse);
        inst.setPrimitiveType(PRIMITIVE_TEXT);
        inst.setGlyphSlice(m_glyphSlice.id);
        inst.zIndex = getZIndex() + 1;
        inst.clipRect = clipRect;
        inst.setVisible(isVisible());

        s_pushData(ctx.geometry, m_textAlloc, inst);
    }

    if (m_showingPlaceholder) {
        m_charPositions.assign(1, 0.0f);
        m_textWidth = 0.0f;
    } else if (textChanged || sizeChanged || m_charPositions.empty()) {
        // Indexed by byte so caret offsets can be looked up directly. A multi-byte
        // sequence stores its start x in every one of its byte slots, so a lookup at a
        // boundary is always the left edge of that codepoint.
        m_charPositions.assign(shown.size() + 1, 0.0f);
        uint32_t pixelSize = static_cast<uint32_t>(m_tiProps.text.fontSize);
        float currentX = 0.0f;
        size_t i = 0;
        while (i < shown.size()) {
            Utf8::Decoded decoded = Utf8::decode(shown, i);
            for (size_t byte = 0; byte < decoded.bytes && i + byte < shown.size(); ++byte) {
                m_charPositions[i + byte] = currentX;
            }
            currentX += ctx.textProcessor->getCharAdvanceAtlas(m_font, decoded.codepoint, pixelSize);
            i += decoded.bytes;
        }
        m_charPositions[shown.size()] = currentX;
        m_textWidth = currentX;
    }

    m_renderedText = m_showingPlaceholder ? m_placeholder : std::move(shown);

    // Glyphs are aligned within the content box, so the caret origin must shift by the
    // same amount; m_charPositions are measured from the text's own left edge.
    m_textStartX = alignStartX(m_textWidth);
}

void UITextField::drawSelection(DrawContext &ctx)
{
    vec2 selPos{};
    vec2 selSize{};
    bool visible = false;

    if (isFocused() && m_selectionStart.has_value() && !m_text.empty() && m_charPositions.size() > 1) {
        size_t selStart = std::min(m_cursorPosition, *m_selectionStart);
        size_t selEnd = std::max(m_cursorPosition, *m_selectionStart);

        if (selEnd > selStart && selStart < m_charPositions.size() && selEnd < m_charPositions.size()) {
            selPos = {m_textStartX + m_charPositions[selStart], m_textBaselineY};
            selSize = {m_charPositions[selEnd] - m_charPositions[selStart], m_tiProps.text.fontSize * 1.2f};
            visible = true;
        }
    }

    if (!visible && m_selectionAlloc == nullptr) {
        return;
    }

    InstanceData data{};
    data.translation = selPos + selSize * 0.5f;
    data.scale = selSize;
    data.setFillColor(m_tiProps.selectionColor);
    data.setPrimitiveType(PRIMITIVE_RECT);
    data.zIndex = getZIndex();
    data.setVisible(visible);

    s_pushData(ctx.geometry, m_selectionAlloc, data);
}

void UITextField::drawCursor(DrawContext &ctx)
{
    bool visible = isFocused() && isCursorVisible();
    if (!visible && m_cursorAlloc == nullptr) {
        return;
    }

    float cursorX = 0.0f;
    if (!m_charPositions.empty()) {
        cursorX = (m_cursorPosition < m_charPositions.size()) ? m_charPositions[m_cursorPosition] : m_charPositions.back();
    }

    vec2 cursorPos = {m_textStartX + cursorX, m_textBaselineY};
    vec2 cursorSize = {1.0f, m_tiProps.text.fontSize * 1.2f};
    vec2 centerPos = cursorPos + cursorSize * 0.5f;

    InstanceData data{};
    data.translation = centerPos;
    data.scale = cursorSize;
    data.setFillColor(m_tiProps.cursorColor);
    data.setPrimitiveType(PRIMITIVE_RECT);
    data.zIndex = getZIndex();
    data.setVisible(visible);

    s_pushData(ctx.geometry, m_cursorAlloc, data);
}

void UITextField::drawInput(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData bgData = createInstanceData();
        bgData.setPrimitiveType(PRIMITIVE_RECT);
        pushData(ctx.geometry, bgData);

        drawText(ctx);
        drawSelection(ctx);
        drawCursor(ctx);
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
