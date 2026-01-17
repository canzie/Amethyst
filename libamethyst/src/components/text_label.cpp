/*
 * TextLabel implementation
 */

#include "components/text_label.h"

#include "modules/text_processor.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "rendering/text_registry.h"

namespace Amethyst {

void TextLabel::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.primitiveType = PRIMITIVE_RECT;

        if (m_allocationIndex == UINT32_MAX) {
            m_allocationIndex = ctx.geometry->submit(data);
        } else {
            ctx.geometry->update(m_allocationIndex, data);
        }

        if (ctx.textProcessor && ctx.text && !text.empty()) {
            float effectiveFontSize = fontSize;

            if (textScaled) {
                glm::vec2 textSize = ctx.textProcessor->measureText(text);
                if (textSize.x > 0.0f && textSize.y > 0.0f) {
                    float scaleX = absoluteSize.x / textSize.x;
                    float scaleY = absoluteSize.y / textSize.y;
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

            if (m_textAllocationIndex == UINT32_MAX) {
                m_textAllocationIndex = ctx.text->submit(characters);
            } else {
                ctx.text->update(m_textAllocationIndex, characters);
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
