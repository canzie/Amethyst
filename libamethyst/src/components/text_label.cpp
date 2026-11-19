/*
 * TextLabel implementation
 */

#include "components/text_label.h"

#include "components/window.h"
#include "modules/style.h"
#include "modules/text_processor.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "utils/profiling.h"

namespace Amethyst {

// local draw layer of the glyphs, over the node's own quad (see UIObject::drawZ)
static constexpr uint32_t TEXT_DRAW_LAYER = 1;

static GeometryAllocation *s_pushData(GeometryRegistry *registry, GeometryAllocation *&alloc, const InstanceData &data)
{
    if (alloc == nullptr) {
        alloc = registry->submit(data);
    } else if (alloc->registry != registry) {
        if (alloc->isValid() && alloc->owning) {
            alloc->registry->release(*alloc);
        }
        alloc = registry->submit(data);
    } else {
        registry->update(*alloc, data);
    }
    return alloc;
}

TextLabel::TextLabel()
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

void TextLabel::resolveStyle()
{
    resolveBaseStyle(ComponentType::TEXT_LABEL);

    TextStyleProperties resolved =
        Style::instance().getTextStyle(ComponentType::TEXT_LABEL, getClasses(), effectiveGuiState(), m_part);
    if (m_textStyle.apply(resolved)) {
        m_font = FontRegistry::instance().findFont(m_textStyle.fontFamily);
        // a restyle can change size or family, and m_textSize is only remeasured on an invalid layout
        m_textLayout.invalidate();
        markDirty();
    }
}

TextLabel::~TextLabel()
{
    if (m_textAlloc != nullptr && m_textAlloc->isValid()) {
        if (m_glyphSlice.isValid()) {
            m_textAlloc->registry->glyphBuffer().destroySlice(m_glyphSlice);
        }
        m_textAlloc->registry->release(*m_textAlloc);
    }
}

bool TextLabel::setTextStyleProperties(const TextStylePropertiesArgs &props)
{
    bool changed = m_textStyle.apply(props);
    if (changed) {
        m_font = FontRegistry::instance().findFont(m_textStyle.fontFamily);
        m_textLayout.invalidate();
        markDirty();
    }
    return changed;
}

void TextLabel::setText(std::string text)
{
    if (m_text != text) {
        m_text = std::move(text);
        m_textLayout.invalidate();
        markDirty();
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

        pushData(ctx.geometry, data);

        updateTextGeometry(ctx);
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void TextLabel::updateTextGeometry(DrawContext &ctx)
{
    if (ctx.textProcessor == nullptr || ctx.geometry == nullptr || ctx.glyphAtlas == nullptr) {
        return;
    }

    // clearing the text has to drop the glyphs of the previous string, not just skip drawing
    if (m_text.empty()) {
        releaseText(ctx);
        m_textSize = {0.0f, 0.0f};
        m_textLayout.invalidate();
        return;
    }

    // Culled labels give their glyphs back: the slice arena is a fixed budget shared by
    // every label in the registry, so holding quads for text nobody can see is what
    // exhausts it on long documents. The instance stays alive and merely turns invisible,
    // so the registry's sort order is untouched.
    if (isRenderCulled()) {
        releaseGlyphSlice(ctx);
        if (m_textAlloc != nullptr) {
            if (InstanceData *inst = ctx.geometry->getMutable(*m_textAlloc)) {
                inst->setVisible(false);
            }
        }
        return;
    }

    if (!m_textLayout.isValid()) {
        // measured with the same line height the layout uses, so textScaled fits the real block
        TextLayoutParams measure;
        measure.font = m_font;
        measure.fontSize = m_textStyle.fontSize;
        measure.lineHeight = m_textStyle.lineHeight;
        measure.logicalToPhysicalScale = ctx.logicalToPhysicalScale;
        m_textSize = ctx.textProcessor->measureTextAtlas(m_text, measure);
    }

    float effectiveFontSize = m_textStyle.fontSize;
    if (m_textStyle.textScaled && m_textSize.x > 0.0f && m_textSize.y > 0.0f) {
        float scaleX = absoluteContentSize.x / m_textSize.x;
        float scaleY = absoluteContentSize.y / m_textSize.y;
        effectiveFontSize = m_textStyle.fontSize * std::min(scaleX, scaleY);
    }

    int32_t glyphZIndex = drawZ(TEXT_DRAW_LAYER);
    bool glyphVisible = isVisible();

    TextLayoutState next;
    next.font = m_font.index;
    next.fontSize = effectiveFontSize;
    next.bounds = absoluteContentSize;
    next.lineHeight = m_textStyle.lineHeight;
    next.color = packColor(m_textStyle.textColor);
    next.xAlign = m_textStyle.textXAlignment;
    next.yAlign = m_textStyle.textYAlignment;
    next.truncate = m_textStyle.textTruncate;
    next.wrap = static_cast<bool>(m_textStyle.textWrapped);
    next.logicalToPhysicalScale = ctx.logicalToPhysicalScale;
    // on the physical pixel grid, so a move by the difference of two origins keeps it there
    next.origin = snapToPhysicalPixel(absoluteContentPosition, ctx.logicalToPhysicalScale);

    bool registryChanged = m_textAlloc != nullptr && m_textAlloc->registry != ctx.geometry;
    if (m_textLayout.matches(next) && !registryChanged) {
        repositionGlyphs(ctx, next.origin - m_textLayout.origin, glyphZIndex, glyphVisible);
    } else {
        reshapeGlyphs(ctx, effectiveFontSize, glyphZIndex, glyphVisible);
    }

    m_textLayout = next;
}

void TextLabel::repositionGlyphs(DrawContext &ctx, vec2 delta, int32_t zIndex, bool visible)
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
    ctx.geometry->setZIndex(*m_textAlloc, zIndex);
}

void TextLabel::reshapeGlyphs(DrawContext &ctx, float effectiveFontSize, int32_t zIndex, bool visible)
{
    AM_PROFILE_FUNCTION();

    TextLayoutParams params;
    params.position = snapToPhysicalPixel(absoluteContentPosition, ctx.logicalToPhysicalScale);
    params.bounds = absoluteContentSize;
    params.logicalToPhysicalScale = ctx.logicalToPhysicalScale;
    params.font = m_font;
    params.fontSize = effectiveFontSize;
    params.color = m_textStyle.textColor;
    params.lineHeight = m_textStyle.lineHeight;
    params.strokeThickness = m_textStyle.strokeThickness;
    params.strokeColor = m_textStyle.strokeColor;
    params.xAlign = m_textStyle.textXAlignment;
    params.yAlign = m_textStyle.textYAlignment;
    params.truncate = m_textStyle.textTruncate;
    params.wrap = static_cast<bool>(m_textStyle.textWrapped);

    // TextLabel renders the whole string as one batched instance: a single quad whose glyph
    // rects and atlas UVs live in the registry's glyph slice. It deliberately does not emit
    // one instance per glyph, so per-glyph (rich) styling is not available here. The per-glyph
    // path still exists via TextProcessor::layoutTextAtlas and the shader's RICH branch, for a
    // future rich-text component.
    BatchedText batched = ctx.textProcessor->layoutTextBatched(m_text, params);
    if (batched.glyphs.empty()) {
        releaseText(ctx);
        return;
    }

    if (m_glyphSlice.isValid() && m_textAlloc != nullptr && m_textAlloc->registry != ctx.geometry) {
        if (GlyphBuffer *oldBuffer = m_textAlloc->registry->getGlyphBuffer()) {
            oldBuffer->destroySlice(m_glyphSlice);
        }
        m_glyphSlice = GlyphSliceHandle{};
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
    inst.zIndex = zIndex;
    inst.clipRect = clipRect;
    inst.setVisible(visible);

    s_pushData(ctx.geometry, m_textAlloc, inst);
}

void TextLabel::releaseGlyphSlice(DrawContext &ctx)
{
    if (!m_glyphSlice.isValid()) {
        return;
    }

    // The slice belongs to whichever registry created it, which is not necessarily the
    // one being drawn into.
    GeometryRegistry *owner = m_textAlloc != nullptr && m_textAlloc->registry != nullptr ? m_textAlloc->registry : ctx.geometry;
    if (GlyphBuffer *buffer = owner->getGlyphBuffer()) {
        buffer->destroySlice(m_glyphSlice);
    }
    m_glyphSlice = GlyphSliceHandle{};
    m_textLayout.invalidate();
}

void TextLabel::releaseText(DrawContext &ctx)
{
    if (m_glyphSlice.isValid()) {
        releaseGlyphSlice(ctx);
    }
    if (m_textAlloc != nullptr) {
        if (m_textAlloc->isValid()) {
            ctx.geometry->release(*m_textAlloc);
        }
        m_textAlloc = nullptr;
    }
}

vec2 TextLabel::automaticContentSize() const
{
    vec2 content = UIObject::automaticContentSize();
    const Window *window = getWindow();
    if (m_text.empty() || window == nullptr || window->getTextProcessor() == nullptr) {
        return content;
    }

    const vec2 text = window->getTextProcessor()->measureTextSize(m_text, FontRegistry::instance().findFont(m_textStyle.fontFamily),
                                                                  m_textStyle.fontSize, m_textStyle.lineHeight,
                                                                  window->getLogicalToPhysicalScale());
    return max(content, text);
}

} // namespace Amethyst
