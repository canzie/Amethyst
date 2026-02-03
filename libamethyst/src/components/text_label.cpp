/*
 * TextLabel implementation
 */

#include "components/text_label.h"

#include "logging/log.h"
#include "modules/style.h"
#include "modules/text_processor.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "utils/profiling.h"

namespace Amethyst {

static void applyStyle(TextLabel &label)
{
    const auto &style = Style::instance();
    label.backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::TEXT_LABEL);
    label.backgroundTransparency = style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::TEXT_LABEL);
    label.borderColor = style.get<Color3>(StyleProperty::BORDER_COLOR, ComponentType::TEXT_LABEL);
    label.borderTransparency = style.get<float>(StyleProperty::BORDER_TRANSPARENCY, ComponentType::TEXT_LABEL);
    label.borderPixelSize = style.get<float>(StyleProperty::BORDER_PIXEL_SIZE, ComponentType::TEXT_LABEL);
    label.cornerRadius = style.get<float>(StyleProperty::CORNER_RADIUS, ComponentType::TEXT_LABEL);
    label.textColor = style.get<Color4>(StyleProperty::TEXT_COLOR, ComponentType::TEXT_LABEL);
    label.fontSize = style.get<float>(StyleProperty::FONT_SIZE, ComponentType::TEXT_LABEL);
}

TextLabel::TextLabel()
{
    applyStyle(*this);
}

TextLabel::TextLabel(Instance *parent)
{
    setParent(parent);
    applyStyle(*this);
}

TextLabel::~TextLabel()
{
    for (auto *alloc : m_textAllocations) {
        if (alloc && alloc->isValid()) {
            alloc->registry->release(*alloc);
        }
    }
}

void TextLabel::draw(DrawContext &ctx)
{
    AM_PROFILE_FUNCTION();
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
                glyphData.clipRect = clipRect;
                glyphData.zIndex = data.zIndex + 1;
                glyphData.setVisible(isVisible());
            }

            if (glyphs.size() != m_textAllocations.size()) {
                for (auto *alloc : m_textAllocations) {
                    if (alloc && alloc->isValid()) {
                        alloc->registry->release(*alloc);
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

    for (Instance *child : children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
