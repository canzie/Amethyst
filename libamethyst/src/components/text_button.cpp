#include "components/text_button.h"

#include "modules/text_processor.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "rendering/text_registry.h"

namespace Amethyst {

TextButton::~TextButton()
{
    if (m_textAlloc && m_textAlloc->isValid()) {
        m_textAlloc->registry->release(std::move(*m_textAlloc));
    }
}

void TextButton::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {

        InstanceData data = createInstanceData();
        data.primitiveType = PRIMITIVE_RECT;

        if (m_geometryAlloc == nullptr) {
            m_geometryAlloc = ctx.geometry->submit(data);
        } else {
            ctx.geometry->update(*m_geometryAlloc, data);
        }

        if (ctx.textProcessor && ctx.text && !text.empty()) {
            m_textSize = ctx.textProcessor->measureText(text);
            float effectiveFontSize = fontSize;

            if (textScaled) {
                if (m_textSize.x > 0.0f && m_textSize.y > 0.0f) {
                    float scaleX = absoluteSize.x / m_textSize.x;
                    float scaleY = absoluteSize.y / m_textSize.y;
                    effectiveFontSize = std::min(scaleX, scaleY);
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

            auto characters = ctx.textProcessor->layoutText(text, params);

            if (m_textAlloc == nullptr) {
                m_textAlloc = ctx.text->submit(characters);
            } else {
                ctx.text->update(*m_textAlloc, characters);
            }
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
