#include "components/text_button.h"

#include "modules/style.h"
#include "modules/text_processor.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

static void applyStyle(TextButton &button)
{
    const auto &style = Style::instance();
    BaseStyleProperties bs;
    bs.backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::TEXT_BUTTON);
    bs.backgroundTransparency = style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::TEXT_BUTTON);
    bs.borderColor = style.get<Color3>(StyleProperty::BORDER_COLOR, ComponentType::TEXT_BUTTON);
    bs.borderTransparency = style.get<float>(StyleProperty::BORDER_TRANSPARENCY, ComponentType::TEXT_BUTTON);
    bs.borderPixelSize = style.get<float>(StyleProperty::BORDER_PIXEL_SIZE, ComponentType::TEXT_BUTTON);
    bs.cornerRadius = style.get<float>(StyleProperty::CORNER_RADIUS, ComponentType::TEXT_BUTTON);
    button.setBaseStyleProperties(bs);

    TextStyleProperties tp;
    tp.textColor = style.get<Color4>(StyleProperty::TEXT_COLOR, ComponentType::TEXT_BUTTON);
    tp.fontSize = style.get<float>(StyleProperty::FONT_SIZE, ComponentType::TEXT_BUTTON);
    button.setTextStyleProperties(tp);
}

TextButton::TextButton()
{
    m_textStyle.textColor = Color4{0.0f, 0.0f, 0.0f, 1.0f};
    m_textStyle.fontSize = 14.0f;
    m_textStyle.textXAlignment = TextXAlignment::LEFT;
    m_textStyle.textYAlignment = TextYAlignment::TOP;
    m_textStyle.textTruncate = TextTruncate::OFF;
    m_textStyle.textWrapped = false;
    m_textStyle.textScaled = false;
    m_textStyle.lineHeight = 1.0f;
    m_textStyle.strokeThickness = 0.0f;
    m_textStyle.strokeColor = Color4{0.0f, 0.0f, 0.0f, 1.0f};

    applyStyle(*this);
}

TextButton::~TextButton()
{
    for (auto *alloc : m_textAllocations) {
        if (alloc && alloc->isValid()) {
            alloc->registry->release(*alloc);
        }
    }
}

bool TextButton::setTextStyleProperties(const TextStyleProperties &props)
{
    bool changed = m_textStyle.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

void TextButton::setText(std::string text)
{
    if (m_text != text) {
        m_text = std::move(text);
        markDirty();
    }
}

void TextButton::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {

        InstanceData data = createInstanceData();
        data.setPrimitiveType(PRIMITIVE_RECT);

        if (m_geometryAlloc == nullptr) {
            m_geometryAlloc = ctx.geometry->submit(data);
        } else {
            ctx.geometry->update(*m_geometryAlloc, data);
        }

        if (ctx.textProcessor && ctx.geometry && !m_text.empty()) {
            uint32_t pixelSize = static_cast<uint32_t>(m_textStyle.fontSize);
            m_textSize = ctx.textProcessor->measureTextAtlas(m_text, pixelSize);
            float effectiveFontSize = m_textStyle.fontSize;

            if (m_textStyle.textScaled) {
                if (m_textSize.x > 0.0f && m_textSize.y > 0.0f) {
                    float scaleX = absoluteContentSize.x / m_textSize.x;
                    float scaleY = absoluteContentSize.y / m_textSize.y;
                    effectiveFontSize = m_textStyle.fontSize * std::min(scaleX, scaleY);
                }
            }

            TextLayoutParams params;
            params.position = absoluteContentPosition;
            params.bounds = absoluteContentSize;
            params.fontSize = effectiveFontSize;
            params.color = m_textStyle.textColor;
            params.lineHeight = m_textStyle.lineHeight;
            params.strokeThickness = m_textStyle.strokeThickness;
            params.strokeColor = m_textStyle.strokeColor;
            params.xAlign = m_textStyle.textXAlignment;
            params.yAlign = m_textStyle.textYAlignment;
            params.truncate = m_textStyle.textTruncate;
            params.wrap = static_cast<bool>(m_textStyle.textWrapped);

            auto glyphs = ctx.textProcessor->layoutTextAtlas(m_text, params);

            for (auto &glyphData : glyphs) {
                glyphData.zIndex = getAbsoluteZIndex() + 1;
                glyphData.clipRect = clipRect;
                glyphData.setVisible(isVisible());
            }

            if (glyphs.size() != m_textAllocations.size()) {
                for (auto *alloc : m_textAllocations) {
                    if (alloc && alloc->isValid()) {
                        ctx.geometry->release(*alloc);
                    }
                }
                m_textAllocations.clear();
                m_textAllocations.reserve(glyphs.size());
                for (const auto &glyphData : glyphs) {
                    m_textAllocations.push_back(ctx.geometry->submit(glyphData));
                }
            } else {
                for (size_t i = 0; i < glyphs.size(); ++i) {
                    ctx.geometry->update(*m_textAllocations[i], glyphs[i]);
                }
            }
        }
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
