#include "components/text_button.h"

#include "modules/style.h"
#include "modules/text_processor.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "utils/profiling.h"

namespace Amethyst {

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

    resolveStyle();
}

void TextButton::resolveStyle()
{
    auto &style = Style::instance();
    setBaseStyleProperties(style.getBaseStyle(ComponentType::TEXT_BUTTON, getClasses()));
    setTextStyleProperties(style.getTextStyle(ComponentType::TEXT_BUTTON, getClasses()));
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
        m_textLayout.invalidate();
        markDirty();
    }
    return changed;
}

void TextButton::setText(std::string text)
{
    if (m_text != text) {
        m_text = std::move(text);
        m_textLayout.invalidate();
        markDirty();
    }
}

void TextButton::draw(DrawContext &ctx)
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

        updateTextGeometry(ctx);
    }

    vec4 childClip = computeChildClipRect();

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteContentSize, absoluteContentPosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void TextButton::updateTextGeometry(DrawContext &ctx)
{
    if (ctx.textProcessor == nullptr || ctx.geometry == nullptr || m_text.empty()) {
        return;
    }

    uint32_t pixelSize = static_cast<uint32_t>(m_textStyle.fontSize);
    if (!m_textLayout.isValid()) {
        m_textSize = ctx.textProcessor->measureTextAtlas(m_text, pixelSize);
    }

    float effectiveFontSize = m_textStyle.fontSize;
    if (m_textStyle.textScaled && m_textSize.x > 0.0f && m_textSize.y > 0.0f) {
        float scaleX = absoluteContentSize.x / m_textSize.x;
        float scaleY = absoluteContentSize.y / m_textSize.y;
        effectiveFontSize = m_textStyle.fontSize * std::min(scaleX, scaleY);
    }

    int32_t glyphZIndex = getAbsoluteZIndex() + 1;
    bool glyphVisible = isVisible();

    TextLayoutState next;
    next.fontSize = effectiveFontSize;
    next.bounds = absoluteContentSize;
    next.lineHeight = m_textStyle.lineHeight;
    next.color = packColor(m_textStyle.textColor);
    next.zIndex = glyphZIndex;
    next.xAlign = m_textStyle.textXAlignment;
    next.yAlign = m_textStyle.textYAlignment;
    next.truncate = m_textStyle.textTruncate;
    next.wrap = static_cast<bool>(m_textStyle.textWrapped);
    next.origin = absoluteContentPosition;

    if (m_textLayout.matches(next)) {
        repositionGlyphs(ctx, next.origin - m_textLayout.origin, glyphVisible);
    } else {
        reshapeGlyphs(ctx, effectiveFontSize, glyphZIndex, glyphVisible);
    }

    m_textLayout = next;
}

void TextButton::repositionGlyphs(DrawContext &ctx, vec2 delta, bool visible)
{
    AM_PROFILE_FUNCTION();
    for (auto *alloc : m_textAllocations) {
        if (alloc == nullptr) {
            continue;
        }
        InstanceData *inst = ctx.geometry->getMutable(*alloc);
        if (inst != nullptr) {
            inst->translation = inst->translation + delta;
            inst->clipRect = clipRect;
            inst->setVisible(visible);
        }
    }
}

void TextButton::reshapeGlyphs(DrawContext &ctx, float effectiveFontSize, int32_t zIndex, bool visible)
{
    AM_PROFILE_FUNCTION();

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
        glyphData.zIndex = zIndex;
        glyphData.clipRect = clipRect;
        glyphData.setVisible(visible);
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

} // namespace Amethyst
