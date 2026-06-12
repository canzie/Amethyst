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
    if (m_textAlloc != nullptr && m_textAlloc->isValid()) {
        if (m_glyphSlice.isValid()) {
            m_textAlloc->registry->glyphBuffer().destroySlice(m_glyphSlice);
        }
        m_textAlloc->registry->release(*m_textAlloc);
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
    if (ctx.textProcessor == nullptr || ctx.geometry == nullptr || ctx.glyphAtlas == nullptr || m_text.empty()) {
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
    if (m_textAlloc == nullptr) {
        return;
    }
    InstanceData *inst = ctx.geometry->getMutable(*m_textAlloc);
    if (inst != nullptr) {
        inst->translation = inst->translation + delta;
        inst->clipRect = clipRect;
        inst->setVisible(visible);
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

    BatchedText batched = ctx.textProcessor->layoutTextBatched(m_text, params);
    if (batched.glyphs.empty()) {
        releaseText(ctx);
        return;
    }

    GlyphBuffer &glyphBuffer = ctx.geometry->glyphBuffer();
    if (!m_glyphSlice.isValid()) {
        m_glyphSlice = glyphBuffer.createSlice();
    }
    glyphBuffer.updateSlice(m_glyphSlice, batched.glyphs.data(), static_cast<uint32_t>(batched.glyphs.size()), batched.lines.data(),
                            static_cast<uint32_t>(batched.lines.size()), batched.lineHeightPx);

    InstanceData inst{};
    inst.translation = batched.pos + batched.size * 0.5f;
    inst.scale = batched.size;
    inst.setFillColor(m_textStyle.textColor);
    inst.setPrimitiveType(PRIMITIVE_TEXT);
    inst.setGlyphSlice(m_glyphSlice.id);
    inst.textureId = ctx.glyphAtlas->getTextureId().id;
    inst.zIndex = zIndex;
    inst.clipRect = clipRect;
    inst.setVisible(visible);

    if (m_textAlloc == nullptr) {
        m_textAlloc = ctx.geometry->submit(inst);
    } else {
        ctx.geometry->update(*m_textAlloc, inst);
    }
}

void TextButton::releaseText(DrawContext &ctx)
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

} // namespace Amethyst
