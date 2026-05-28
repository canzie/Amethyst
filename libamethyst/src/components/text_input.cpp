/*
 * Text input implementation
 */

#include "components/text_input.h"

#include "components/input_events.h"
#include "components/input_interface.h"
#include "components/window.h"
#include "modules/text_processor.h"
#include "rendering/draw_context.h"
#include "rendering/instance_data.h"

#include <algorithm>
#include <cstdint>

namespace Amethyst {

TextInput::TextInput()
{
    m_tiProps.placeholderColor = Color4{0.5f, 0.5f, 0.5f, 1.0f};
    m_tiProps.selectionColor = Color4{0.3f, 0.5f, 0.9f, 0.5f};
    m_tiProps.cursorColor = Color4{0.0f, 0.0f, 0.0f, 1.0f};
    m_tiProps.text.fontSize = 14.0f;
    m_tiProps.text.textColor = Color4{0.0f, 0.0f, 0.0f, 1.0f};
    m_tiProps.text.textXAlignment = TextXAlignment::LEFT;
    m_tiProps.text.textYAlignment = TextYAlignment::CENTER;
    m_tiProps.multiline = 0;
    m_tiProps.maxLength = -1;
    m_tiProps.readOnly = 0;
    m_tiProps.cursorBlinkRate = 0.5f;
}

bool TextInput::setTextInputProperties(const TextInputProperties &props)
{
    bool changed = false;
#define AM_APPLY(field) \
    if (propIsSet(props.field) && m_tiProps.field != props.field) { \
        m_tiProps.field = props.field; \
        changed = true; \
    }
    AM_APPLY(multiline)
    AM_APPLY(maxLength)
    AM_APPLY(readOnly)
    AM_APPLY(cursorBlinkRate)
#undef AM_APPLY
    if (!props.placeholderText.empty() && m_tiProps.placeholderText != props.placeholderText) {
        m_tiProps.placeholderText = props.placeholderText;
        changed = true;
    }
    if (propIsSet(props.placeholderColor) && m_tiProps.placeholderColor != props.placeholderColor) {
        m_tiProps.placeholderColor = props.placeholderColor;
        changed = true;
    }
    if (propIsSet(props.selectionColor) && m_tiProps.selectionColor != props.selectionColor) {
        m_tiProps.selectionColor = props.selectionColor;
        changed = true;
    }
    if (propIsSet(props.cursorColor) && m_tiProps.cursorColor != props.cursorColor) {
        m_tiProps.cursorColor = props.cursorColor;
        changed = true;
    }
    if (applyTextProperties(m_tiProps.text, props.text)) {
        changed = true;
    }
    if (changed) {
        markDirty();
    }
    return changed;
}

TextInput::~TextInput()
{
    for (auto *alloc : m_textAllocations) {
        if (alloc && alloc->isValid()) {
            alloc->registry->release(*alloc);
        }
    }
    if (m_selectionAlloc && m_selectionAlloc->isValid()) {
        m_selectionAlloc->registry->release(*m_selectionAlloc);
    }
    if (m_cursorAlloc && m_cursorAlloc->isValid()) {
        m_cursorAlloc->registry->release(*m_cursorAlloc);
    }
}

void TextInput::setText(const std::string &text)
{
    m_text = text;
    m_cursorPosition = std::min(m_cursorPosition, m_text.size());
    m_selectionStart = std::nullopt;
    markDirty();
    if (onTextChanged) {
        onTextChanged(m_text);
    }
}

void TextInput::clearText()
{
    m_text.clear();
    m_cursorPosition = 0;
    m_selectionStart = std::nullopt;
    markDirty();
    if (onTextChanged) {
        onTextChanged(m_text);
    }
}

EventResult TextInput::onMouseButton1Down(uint32_t x, uint32_t y)
{
    (void)y;
    if (!m_focused) {
        m_focused = true;
        InputInterface::clearKeyEvents();
        if (onFocusGained) {
            onFocusGained();
        }
    }

    size_t clickPos = getCursorFromMouseX(x);
    setCursorPosition(clickPos, false);
    m_draggingSelection = true;

    Window *window = getWindow();
    if (window) {
        window->captureMouse(this);
    }
    return EventResult::CONSUMED;
}

EventResult TextInput::onMouseButton1Up(uint32_t x, uint32_t y)
{
    (void)x;
    (void)y;
    m_draggingSelection = false;
    Window *window = getWindow();
    if (window) {
        window->releaseMouse(this);
    }
    return EventResult::CONSUMED;
}

EventResult TextInput::onMouseButton1Click()
{
    return EventResult::CONSUMED;
}

EventResult TextInput::onMouseMoved(uint32_t x, uint32_t y)
{
    (void)y;
    if (m_draggingSelection) {
        size_t newPos = getCursorFromMouseX(x);
        setCursorPosition(newPos, true);
    }
    return EventResult::CONSUMED;
}

EventResult TextInput::onMouseEnter()
{
    UIObject::onMouseEnter();
    InputInterface::setCursorShape(CURSOR_IBEAM);
    return EventResult::CONSUMED;
}

EventResult TextInput::onMouseLeave()
{
    UIObject::onMouseLeave();
    InputInterface::setCursorShape(CURSOR_ARROW);
    return EventResult::CONSUMED;
}

void TextInput::update(float deltaTime)
{
    if (!m_focused) {
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

void TextInput::processKeyboardInput()
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
            } else if (onEnterPressed) {
                onEnterPressed();
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

void TextInput::insertText(const std::string &text)
{
    if (m_tiProps.readOnly) {
        return;
    }

    if (m_selectionStart.has_value()) {
        deleteSelection();
    }

    if (m_tiProps.maxLength >= 0 && static_cast<int32_t>(m_text.size() + text.size()) > m_tiProps.maxLength) {
        return;
    }

    m_text.insert(m_cursorPosition, text);
    m_cursorPosition += text.size();
    markDirty();

    if (onTextChanged) {
        onTextChanged(m_text);
    }
}

void TextInput::deleteSelection()
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

void TextInput::moveCursor(int delta, bool select)
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

void TextInput::setCursorPosition(size_t pos, bool select)
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

size_t TextInput::getCursorFromMouseX(uint32_t mouseX)
{
    if (m_charPositions.empty() || m_text.empty()) {
        return 0;
    }

    float relativeX = static_cast<float>(mouseX) - absoluteContentPosition.x;

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

void TextInput::copy()
{
    if (!m_selectionStart.has_value()) {
        return;
    }

    size_t start = std::min(m_cursorPosition, *m_selectionStart);
    size_t end = std::max(m_cursorPosition, *m_selectionStart);
    std::string selectedText = m_text.substr(start, end - start);

    InputInterface::setClipboardText(selectedText);
}

void TextInput::paste()
{
    if (m_tiProps.readOnly) {
        return;
    }

    std::string clipboardText = InputInterface::getClipboardText();
    if (!clipboardText.empty()) {
        insertText(clipboardText);
    }
}

void TextInput::cut()
{
    if (m_tiProps.readOnly || !m_selectionStart.has_value()) {
        return;
    }

    copy();
    deleteSelection();
}

void TextInput::selectAll()
{
    m_selectionStart = 0;
    m_cursorPosition = m_text.size();
    markDirty();
}

void TextInput::releaseTextAllocations(GeometryRegistry *)
{
    for (auto *alloc : m_textAllocations) {
        if (alloc && alloc->isValid()) {
            alloc->registry->release(*alloc);
        }
    }
    m_textAllocations.clear();
}

void TextInput::drawText(DrawContext &ctx)
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

    bool shouldShowPlaceholder = m_text.empty() && !m_tiProps.placeholderText.empty();
    bool modeChanged = (shouldShowPlaceholder != m_showingPlaceholder);
    m_showingPlaceholder = shouldShowPlaceholder;

    const std::string &textToRender = m_showingPlaceholder ? m_tiProps.placeholderText : m_text;
    const Color4 &colorToUse = m_showingPlaceholder ? m_tiProps.placeholderColor : m_tiProps.text.textColor;

    if (textToRender.empty()) {
        releaseTextAllocations(ctx.geometry);
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

    auto glyphs = ctx.textProcessor->layoutTextAtlas(textToRender, params);
    int32_t desiredZIndex = getZIndex() + 1;
    for (auto &g : glyphs) {
        g.zIndex = desiredZIndex;
    }

    if (modeChanged || glyphs.size() != m_textAllocations.size()) {
        releaseTextAllocations(ctx.geometry);
        m_textAllocations.reserve(glyphs.size());
        for (const auto &glyphData : glyphs) {
            m_textAllocations.push_back(ctx.geometry->submit(glyphData));
        }
    } else {
        for (size_t i = 0; i < glyphs.size(); ++i) {
            ctx.geometry->update(*m_textAllocations[i], glyphs[i]);
        }
    }

    m_charPositions.clear();
    if (m_showingPlaceholder) {
        m_charPositions.push_back(0.0f);
    } else {
        m_charPositions.reserve(m_text.size() + 1);
        uint32_t pixelSize = static_cast<uint32_t>(m_tiProps.text.fontSize);
        float currentX = 0.0f;
        for (size_t i = 0; i <= m_text.size(); ++i) {
            m_charPositions.push_back(currentX);
            if (i < m_text.size()) {
                currentX += ctx.textProcessor->getCharAdvanceAtlas(static_cast<uint32_t>(m_text[i]), pixelSize);
            }
        }
    }
}

void TextInput::drawSelection(DrawContext &ctx)
{
    if (m_focused && m_selectionStart.has_value() && !m_text.empty() && m_charPositions.size() > 1) {
        size_t selStart = std::min(m_cursorPosition, *m_selectionStart);
        size_t selEnd = std::max(m_cursorPosition, *m_selectionStart);

        if (selEnd > selStart && selStart < m_charPositions.size() && selEnd < m_charPositions.size()) {
            glm::vec2 selPos = {absoluteContentPosition.x + m_charPositions[selStart], m_textBaselineY};
            glm::vec2 selSize = {m_charPositions[selEnd] - m_charPositions[selStart], m_tiProps.text.fontSize * 1.2f};
            glm::vec2 centerPos = selPos + selSize * 0.5f;

            InstanceData data{};
            data.translation = centerPos;
            data.scale = selSize;
            data.setFillColor(m_tiProps.selectionColor);
            data.setPrimitiveType(PRIMITIVE_RECT);
            data.zIndex = getZIndex();

            if (m_selectionAlloc == nullptr) {
                m_selectionAlloc = ctx.geometry->submit(data);
            } else {
                ctx.geometry->update(*m_selectionAlloc, data);
            }
            return;
        }
    }

    if (m_selectionAlloc && m_selectionAlloc->isValid()) {
        ctx.geometry->release(*m_selectionAlloc);
        m_selectionAlloc = nullptr;
    }
}

void TextInput::drawCursor(DrawContext &ctx)
{
    if (m_focused && m_cursorVisible) {
        float cursorX = 0.0f;
        if (!m_charPositions.empty()) {
            cursorX = (m_cursorPosition < m_charPositions.size()) ? m_charPositions[m_cursorPosition] : m_charPositions.back();
        }

        glm::vec2 cursorPos = {absoluteContentPosition.x + cursorX, m_textBaselineY};
        glm::vec2 cursorSize = {1.0f, m_tiProps.text.fontSize * 1.2f};
        glm::vec2 centerPos = cursorPos + cursorSize * 0.5f;

        InstanceData data{};
        data.translation = centerPos;
        data.scale = cursorSize;
        data.setFillColor(m_tiProps.cursorColor);
        data.setPrimitiveType(PRIMITIVE_RECT);
        data.zIndex = getZIndex();

        if (m_cursorAlloc == nullptr) {
            m_cursorAlloc = ctx.geometry->submit(data);
        } else {
            ctx.geometry->update(*m_cursorAlloc, data);
        }
        return;
    }

    if (m_cursorAlloc && m_cursorAlloc->isValid()) {
        ctx.geometry->release(*m_cursorAlloc);
        m_cursorAlloc = nullptr;
    }
}

void TextInput::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData bgData = createInstanceData();
        bgData.setPrimitiveType(PRIMITIVE_RECT);
        if (m_geometryAlloc == nullptr) {
            m_geometryAlloc = ctx.geometry->submit(bgData);
        } else {
            ctx.geometry->update(*m_geometryAlloc, bgData);
        }

        drawText(ctx);
        drawSelection(ctx);
        drawCursor(ctx);
    }

    glm::vec4 childClip = computeChildClipRect();

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteContentSize, absoluteContentPosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
