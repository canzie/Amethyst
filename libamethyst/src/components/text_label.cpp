/*
 * TextLabel implementation
 */

#include "components/text_label.h"

#include "modules/text_processor.h"
#include "rendering/draw_context.h"
#include "rendering/text_registry.h"

namespace Amethyst {

void TextLabel::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        if (ctx.textProcessor && ctx.text && !text.empty()) {
            TextLayoutParams params;
            params.position = absolutePosition;
            params.fontSize = fontSize;
            params.color = textColor;
            params.lineHeight = lineHeight;

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
