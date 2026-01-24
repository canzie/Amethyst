/*
 * Text input implementation
 */

#include "components/text_input.h"

#include "components/input_events.h"
#include "components/input_interface.h"
#include "components/window.h"
#include "modules/text_processor.h"
#include "rendering/draw_context.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace Amethyst {

TextInput::~TextInput()
{
    if (m_textAlloc && m_textAlloc->isValid()) {
        m_textAlloc->registry->release(std::move(*m_textAlloc));
    }
    if (m_selectionAlloc && m_selectionAlloc->isValid()) {
        m_selectionAlloc->registry->release(std::move(*m_selectionAlloc));
    }
    if (m_cursorAlloc && m_cursorAlloc->isValid()) {
        m_cursorAlloc->registry->release(std::move(*m_cursorAlloc));
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

void TextInput::onMouseButton1Down(uint32_t x, uint32_t y)
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
}

void TextInput::onMouseButton1Up(uint32_t x, uint32_t y)
{
    (void)x;
    (void)y;
    m_draggingSelection = false;
    Window *window = getWindow();
    if (window) {
        window->releaseMouse(this);
    }
}

void TextInput::onMouseButton1Click()
{
}

void TextInput::onMouseMoved(uint32_t x, uint32_t y)
{
    (void)y;
    if (m_draggingSelection) {
        size_t newPos = getCursorFromMouseX(x);
        setCursorPosition(newPos, true);
    }
}

void TextInput::onMouseEnter()
{
    InputInterface::setCursorShape(CURSOR_IBEAM);
}

void TextInput::onMouseLeave()
{
    InputInterface::setCursorShape(CURSOR_ARROW);
}

void TextInput::update(float deltaTime)
{
    if (!m_focused) {
        return;
    }

    processKeyboardInput();

    m_cursorBlinkTimer += deltaTime;
    if (m_cursorBlinkTimer >= cursorBlinkRate) {
        m_cursorBlinkTimer = 0.0f;
        m_cursorVisible = !m_cursorVisible;
        markDirty();
    }
}

void TextInput::processKeyboardInput()
{
    if (!m_focused || readOnly) {
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
            if (multiline) {
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
    if (readOnly) {
        return;
    }

    if (m_selectionStart.has_value()) {
        deleteSelection();
    }

    if (maxLength >= 0 && static_cast<int32_t>(m_text.size() + text.size()) > maxLength) {
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

    float relativeX = static_cast<float>(mouseX) - absolutePosition.x;

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
    if (readOnly) {
        return;
    }

    std::string clipboardText = InputInterface::getClipboardText();
    if (!clipboardText.empty()) {
        insertText(clipboardText);
    }
}

void TextInput::cut()
{
    if (readOnly || !m_selectionStart.has_value()) {
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

void TextInput::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData bgData = createInstanceData();
        bgData.primitiveType = PRIMITIVE_RECT;
        if (m_geometryAlloc == nullptr) {
            m_geometryAlloc = ctx.geometry->submit(bgData);
        } else {
            ctx.geometry->update(*m_geometryAlloc, bgData);
        }

        const std::string &displayText = m_text.empty() ? placeholderText : m_text;
        const Color4 &displayColor = m_text.empty() ? placeholderColor : textColor;

        if (!displayText.empty()) {
            TextLayoutParams params;
            params.position = absolutePosition;
            params.bounds = absoluteSize;
            params.fontSize = fontSize;
            params.color = displayColor;
            params.xAlign = textXAlignment;
            params.yAlign = TextYAlignment::TOP;
            params.wrap = multiline;

            std::vector<CharacterInstance> chars = ctx.textProcessor->layoutText(displayText, params);

            m_charPositions.clear();
            m_charPositions.reserve(displayText.size() + 1);

            float currentX = 0.0f;
            for (size_t i = 0; i <= displayText.size(); ++i) {
                m_charPositions.push_back(currentX);

                if (i < displayText.size()) {
                    currentX += ctx.textProcessor->getCharAdvance(displayText[i], fontSize);
                }
            }

            if (m_textAlloc == nullptr) {
                m_textAlloc = ctx.text->submit(chars);
            } else {
                ctx.text->update(*m_textAlloc, chars);
            }
        } else {
            if (m_textAlloc && m_textAlloc->isValid()) {
                m_textAlloc->registry->release(std::move(*m_textAlloc));
                m_textAlloc = nullptr;
            }
            m_charPositions.clear();
        }

        if (m_focused && m_selectionStart.has_value() && !m_text.empty() && m_charPositions.size() > 1) {
            size_t selStart = std::min(m_cursorPosition, *m_selectionStart);
            size_t selEnd = std::max(m_cursorPosition, *m_selectionStart);

            if (selEnd > selStart && selStart < m_charPositions.size() && selEnd < m_charPositions.size()) {
                float startX = m_charPositions[selStart];
                float endX = m_charPositions[selEnd];

                glm::vec2 selPos = glm::vec2(absolutePosition.x + startX, absolutePosition.y);
                glm::vec2 selSize = glm::vec2(endX - startX, fontSize * 1.2f);

                glm::vec2 centerPos = selPos + selSize * 0.5f;
                glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(centerPos, 0.0f)) *
                                      glm::scale(glm::mat4(1.0f), glm::vec3(selSize, 1.0f));

                InstanceData selectionData{.transform = transform,
                                           .fillColor = selectionColor,
                                           .borderColor = Color4(0.0f, 0.0f, 0.0f, 0.0f),
                                           .borderThickness = 0.0f,
                                           .cornerRadius = 0.0f,
                                           .primitiveType = PRIMITIVE_RECT,
                                           .borderMode = 0,
                                           .textureId = UINT32_MAX,
                                           .zIndex = zIndex};

                if (m_selectionAlloc == nullptr) {
                    m_selectionAlloc = ctx.geometry->submit(selectionData);
                } else {
                    ctx.geometry->update(*m_selectionAlloc, selectionData);
                }
            }
        } else if (m_selectionAlloc && m_selectionAlloc->isValid()) {
            m_selectionAlloc->registry->release(std::move(*m_selectionAlloc));
            m_selectionAlloc = nullptr;
        }

        if (m_focused && m_cursorVisible) {
            float cursorX = 0.0f;
            if (!m_charPositions.empty()) {
                if (m_cursorPosition < m_charPositions.size()) {
                    cursorX = m_charPositions[m_cursorPosition];
                } else {
                    cursorX = m_charPositions.back();
                }
            }

            glm::vec2 cursorPos = glm::vec2(absolutePosition.x + cursorX, absolutePosition.y);
            glm::vec2 cursorSize = glm::vec2(1.0f, fontSize * 1.2f);

            glm::vec2 centerPos = cursorPos + cursorSize * 0.5f;
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(centerPos, 0.0f)) *
                                  glm::scale(glm::mat4(1.0f), glm::vec3(cursorSize, 1.0f));

            InstanceData cursorData{.transform = transform,
                                    .fillColor = cursorColor,
                                    .borderColor = Color4(0.0f, 0.0f, 0.0f, 0.0f),
                                    .borderThickness = 0.0f,
                                    .cornerRadius = 0.0f,
                                    .primitiveType = PRIMITIVE_RECT,
                                    .borderMode = 0,
                                    .textureId = UINT32_MAX,
                                    .zIndex = zIndex};

            if (m_cursorAlloc == nullptr) {
                m_cursorAlloc = ctx.geometry->submit(cursorData);
            } else {
                ctx.geometry->update(*m_cursorAlloc, cursorData);
            }
        } else if (m_cursorAlloc && m_cursorAlloc->isValid()) {
            m_cursorAlloc->registry->release(std::move(*m_cursorAlloc));
            m_cursorAlloc = nullptr;
        }
    }

    for (Instance *child : children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
