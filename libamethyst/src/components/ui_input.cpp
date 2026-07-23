#include "components/ui_input.h"

#include "components/input_events.h"
#include "components/input_interface.h"
#include "components/overlay_layer.h"
#include "components/window.h"
#include "modules/text_processor.h"
#include "rendering/draw_context.h"
#include "rendering/instance_data.h"

#include <algorithm>
#include <cstdint>

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

UIInput::UIInput()
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
}

UIInput::~UIInput()
{
    m_tick.unregister();
    if (m_hovered) {
        InputInterface::setCursorShape(CURSOR_ARROW);
    }
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

bool UIInput::setTextInputProperties(const TextInputStylePropertiesArgs &props)
{
    bool changed = m_tiProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

void UIInput::setPlaceholder(std::string placeholder)
{
    if (m_placeholder != placeholder) {
        m_placeholder = std::move(placeholder);
        markDirty();
    }
}

void UIInput::setText(const std::string &text)
{
    m_text = text;
    m_cursorPosition = std::min(m_cursorPosition, m_text.size());
    m_selectionStart = std::nullopt;
    markDirty();
    if (onTextChanged) {
        onTextChanged(m_text);
    }
}

void UIInput::clearText()
{
    m_text.clear();
    m_cursorPosition = 0;
    m_selectionStart = std::nullopt;
    markDirty();
    if (onTextChanged) {
        onTextChanged(m_text);
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
        m_tick = win->registerTick([this](float dt) { update(dt); });
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

    size_t clickPos = getCursorFromMouseX(static_cast<int32_t>(input.position.x));
    setCursorPosition(clickPos, false);
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
    (void)y;
    if (m_draggingSelection) {
        size_t newPos = getCursorFromMouseX(x);
        setCursorPosition(newPos, true);
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

void UIInput::update(float deltaTime)
{
    if (!m_focused) {
        return;
    }

    if (!isVisible()) {
        loseFocus();
        return;
    }

    processKeyboardInput();

    m_cursorBlinkTimer += deltaTime;
    if (m_cursorBlinkTimer >= m_tiProps.cursorBlinkRate) {
        m_cursorBlinkTimer = 0.0f;
        m_cursorVisible = !m_cursorVisible;
        markDirty();
    }
}

void UIInput::processKeyboardInput()
{
    if (!m_focused || m_tiProps.readOnly) {
        return;
    }

    KeyEvent keyEvent;
    while (InputInterface::pollKeyEvent(keyEvent)) {
        if (keyEvent.action != KEY_ACTION_PRESS && keyEvent.action != KEY_ACTION_REPEAT) {
            continue;
        }

        bool ctrl = keyEvent.mods & MOD_CONTROL;
        bool shift = keyEvent.mods & MOD_SHIFT;

        switch (keyEvent.key) {
        case KEY_BACKSPACE:
            if (m_selectionStart.has_value()) {
                deleteSelection();
            } else if (m_cursorPosition > 0) {
                m_text.erase(m_cursorPosition - 1, 1);
                m_cursorPosition--;
                markDirty();
                if (onTextChanged) {
                    onTextChanged(m_text);
                }
            }
            m_cursorBlinkTimer = 0.0f;
            m_cursorVisible = true;
            break;

        case KEY_DELETE:
            if (m_selectionStart.has_value()) {
                deleteSelection();
            } else if (m_cursorPosition < m_text.size()) {
                m_text.erase(m_cursorPosition, 1);
                markDirty();
                if (onTextChanged) {
                    onTextChanged(m_text);
                }
            }
            m_cursorBlinkTimer = 0.0f;
            m_cursorVisible = true;
            break;

        case KEY_LEFT:
            moveCursor(-1, shift);
            m_cursorBlinkTimer = 0.0f;
            m_cursorVisible = true;
            break;

        case KEY_RIGHT:
            moveCursor(1, shift);
            m_cursorBlinkTimer = 0.0f;
            m_cursorVisible = true;
            break;

        case KEY_HOME:
            setCursorPosition(0, shift);
            m_cursorBlinkTimer = 0.0f;
            m_cursorVisible = true;
            break;

        case KEY_END:
            setCursorPosition(m_text.size(), shift);
            m_cursorBlinkTimer = 0.0f;
            m_cursorVisible = true;
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
            m_cursorBlinkTimer = 0.0f;
            m_cursorVisible = true;
            break;

        default:
            break;
        }
    }

    uint32_t codepoint;
    while (InputInterface::pollCharEvent(codepoint)) {
        if (codepoint >= 32 || codepoint == '\n') {
            char utf8[5] = {0};
            if (codepoint < 0x80) {
                utf8[0] = static_cast<char>(codepoint);
            } else if (codepoint < 0x800) {
                utf8[0] = static_cast<char>(0xC0 | (codepoint >> 6));
                utf8[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
            } else if (codepoint < 0x10000) {
                utf8[0] = static_cast<char>(0xE0 | (codepoint >> 12));
                utf8[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                utf8[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
            } else {
                utf8[0] = static_cast<char>(0xF0 | (codepoint >> 18));
                utf8[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                utf8[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                utf8[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
            }
            insertText(utf8);
            m_cursorBlinkTimer = 0.0f;
            m_cursorVisible = true;
        }
    }
}

void UIInput::insertText(const std::string &text)
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

void UIInput::deleteSelection()
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

void UIInput::moveCursor(int delta, bool select)
{
    if (delta < 0) {
        if (m_cursorPosition > 0) {
            setCursorPosition(m_cursorPosition - 1, select);
        }
    } else if (delta > 0) {
        if (m_cursorPosition < m_text.size()) {
            setCursorPosition(m_cursorPosition + 1, select);
        }
    }
}

void UIInput::setCursorPosition(size_t pos, bool select)
{
    pos = std::min(pos, m_text.size());

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

size_t UIInput::getCursorFromMouseX(int32_t mouseX)
{
    if (m_charPositions.empty() || m_text.empty()) {
        return 0;
    }

    float relativeX = static_cast<float>(mouseX) - m_textStartX;

    if (relativeX <= m_charPositions[0]) {
        return 0;
    }

    for (size_t i = 1; i < m_charPositions.size(); ++i) {
        float midpoint = (m_charPositions[i - 1] + m_charPositions[i]) / 2.0f;
        if (relativeX < midpoint) {
            return i - 1;
        }
    }

    return std::min(m_text.size(), m_charPositions.size() - 1);
}

void UIInput::copy()
{
    if (!m_selectionStart.has_value()) {
        return;
    }

    size_t start = std::min(m_cursorPosition, *m_selectionStart);
    size_t end = std::max(m_cursorPosition, *m_selectionStart);
    std::string selectedText = m_text.substr(start, end - start);

    InputInterface::setClipboardText(selectedText);
}

void UIInput::paste()
{
    if (m_tiProps.readOnly) {
        return;
    }

    std::string clipboardText = InputInterface::getClipboardText();
    if (!clipboardText.empty()) {
        insertText(clipboardText);
    }
}

void UIInput::cut()
{
    if (m_tiProps.readOnly || !m_selectionStart.has_value()) {
        return;
    }

    copy();
    deleteSelection();
}

void UIInput::selectAll()
{
    m_selectionStart = 0;
    m_cursorPosition = m_text.size();
    markDirty();
}

void UIInput::releaseText(DrawContext &ctx)
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

void UIInput::drawText(DrawContext &ctx)
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
        return;
    }

    TextLayoutParams params;
    params.position = absoluteContentPosition;
    params.bounds = absoluteContentSize;
    params.fontSize = m_tiProps.text.fontSize;
    params.color = colorToUse;
    params.xAlign = m_tiProps.text.textXAlignment;
    params.yAlign = m_tiProps.text.textYAlignment;
    params.wrap = m_tiProps.multiline;

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
        inst.textureId = ctx.glyphAtlas->getTextureId().id;
        inst.zIndex = getZIndex() + 1;
        inst.clipRect = clipRect;
        inst.setVisible(isVisible());

        s_pushData(ctx.geometry, m_textAlloc, inst);
    }

    m_charPositions.clear();
    if (m_showingPlaceholder) {
        m_charPositions.push_back(0.0f);
    } else {
        m_charPositions.reserve(shown.size() + 1);
        uint32_t pixelSize = static_cast<uint32_t>(m_tiProps.text.fontSize);
        float currentX = 0.0f;
        for (size_t i = 0; i <= shown.size(); ++i) {
            m_charPositions.push_back(currentX);
            if (i < shown.size()) {
                currentX += ctx.textProcessor->getCharAdvanceAtlas(static_cast<uint32_t>(shown[i]), pixelSize);
            }
        }

        // Glyphs are aligned within the content box, so the caret origin must shift by the
        // same amount; m_charPositions are measured from the text's own left edge.
        m_textStartX = alignStartX(currentX);
    }
}

void UIInput::drawSelection(DrawContext &ctx)
{
    if (m_focused && m_selectionStart.has_value() && !m_text.empty() && m_charPositions.size() > 1) {
        size_t selStart = std::min(m_cursorPosition, *m_selectionStart);
        size_t selEnd = std::max(m_cursorPosition, *m_selectionStart);

        if (selEnd > selStart && selStart < m_charPositions.size() && selEnd < m_charPositions.size()) {
            vec2 selPos = {m_textStartX + m_charPositions[selStart], m_textBaselineY};
            vec2 selSize = {m_charPositions[selEnd] - m_charPositions[selStart], m_tiProps.text.fontSize * 1.2f};
            vec2 centerPos = selPos + selSize * 0.5f;

            InstanceData data{};
            data.translation = centerPos;
            data.scale = selSize;
            data.setFillColor(m_tiProps.selectionColor);
            data.setPrimitiveType(PRIMITIVE_RECT);
            data.zIndex = getZIndex();

            s_pushData(ctx.geometry, m_selectionAlloc, data);
            return;
        }
    }

    if (m_selectionAlloc && m_selectionAlloc->isValid()) {
        ctx.geometry->release(*m_selectionAlloc);
        m_selectionAlloc = nullptr;
    }
}

void UIInput::drawCursor(DrawContext &ctx)
{
    if (m_focused && m_cursorVisible) {
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

        s_pushData(ctx.geometry, m_cursorAlloc, data);
        return;
    }

    if (m_cursorAlloc && m_cursorAlloc->isValid()) {
        ctx.geometry->release(*m_cursorAlloc);
        m_cursorAlloc = nullptr;
    }
}

void UIInput::drawInput(DrawContext &ctx)
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
