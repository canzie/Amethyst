#include "components/text_button.h"

#include "modules/style.h"
#include "modules/text_processor.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

static void applyStyle(TextButton &button)
{
    const auto &style = Style::instance();
    button.backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::TEXT_BUTTON);
    button.backgroundTransparency = style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::TEXT_BUTTON);
    button.borderColor = style.get<Color3>(StyleProperty::BORDER_COLOR, ComponentType::TEXT_BUTTON);
    button.borderTransparency = style.get<float>(StyleProperty::BORDER_TRANSPARENCY, ComponentType::TEXT_BUTTON);
    button.borderPixelSize = style.get<float>(StyleProperty::BORDER_PIXEL_SIZE, ComponentType::TEXT_BUTTON);
    button.cornerRadius = style.get<float>(StyleProperty::CORNER_RADIUS, ComponentType::TEXT_BUTTON);
    button.textColor = style.get<Color4>(StyleProperty::TEXT_COLOR, ComponentType::TEXT_BUTTON);
    button.fontSize = style.get<float>(StyleProperty::FONT_SIZE, ComponentType::TEXT_BUTTON);
}

TextButton::TextButton()
{
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

        if (ctx.textProcessor && ctx.geometry && !text.empty()) {
            uint32_t pixelSize = static_cast<uint32_t>(fontSize);
            m_textSize = ctx.textProcessor->measureTextAtlas(text, pixelSize);
            float effectiveFontSize = fontSize;

            if (textScaled) {
                if (m_textSize.x > 0.0f && m_textSize.y > 0.0f) {
                    float scaleX = absoluteSize.x / m_textSize.x;
                    float scaleY = absoluteSize.y / m_textSize.y;
                    effectiveFontSize = fontSize * std::min(scaleX, scaleY);
                }
            }

            TextLayoutParams params;
            params.position = absolutePosition;
            params.bounds = absoluteSize;
            params.fontSize = effectiveFontSize;
            params.color = textColor;
            params.lineHeight = lineHeight;
            params.strokeThickness = strokeThickness;
            params.strokeColor = strokeColor;
            params.xAlign = textXAlignment;
            params.yAlign = textYAlignment;
            params.truncate = textTruncate;
            params.wrap = textWrapped;

            auto glyphs = ctx.textProcessor->layoutTextAtlas(text, params);

            for (auto &glyphData : glyphs) {
                glyphData.zIndex = data.zIndex + 1;
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
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
