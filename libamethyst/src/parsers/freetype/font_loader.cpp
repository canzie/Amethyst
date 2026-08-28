#include "font_loader.h"
#include "logging/log.h"

#include <ft2build.h>
#include FT_ADVANCES_H
#include FT_FREETYPE_H
#include FT_GLYPH_H

namespace Amethyst {

// FreeType reports FT_Pos metrics in 26.6 and FT_Fixed in 16.16 fixed point.
static constexpr float FT_POS_TO_PIXELS = 1.0f / 64.0f;
static constexpr float FT_FIXED_TO_PIXELS = 1.0f / 65536.0f;

struct FontLoader::Impl {
    FT_Face face = nullptr;
    uint32_t pixelSize = 0;
};

// FT_Library owns the module list and memory manager, so every face shares one.
static FT_Library s_freetype()
{
    static FT_Library library = [] {
        FT_Library created = nullptr;
        if (FT_Init_FreeType(&created) != 0) {
            AM_LOG_ERROR("Failed to initialize FreeType library");
            return static_cast<FT_Library>(nullptr);
        }
        return created;
    }();
    return library;
}

FontLoader::FontLoader() : m_impl(std::make_unique<Impl>()) {}

FontLoader::~FontLoader()
{
    if (m_impl && m_impl->face) {
        FT_Done_Face(m_impl->face);
    }
}

FontLoader::FontLoader(FontLoader &&other) noexcept : m_impl(std::move(other.m_impl)) {}

FontLoader &FontLoader::operator=(FontLoader &&other) noexcept
{
    if (this != &other) {
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

bool FontLoader::loadFont(const std::string &path)
{
    FT_Library library = s_freetype();
    if (!m_impl || library == nullptr) {
        return false;
    }

    if (m_impl->face) {
        FT_Done_Face(m_impl->face);
        m_impl->face = nullptr;
    }

    FT_Error error = FT_New_Face(library, path.c_str(), 0, &m_impl->face);
    if (error) {
        AM_LOG_ERROR("Failed to load font from path: {}", path);
        return false;
    }

    AM_LOG_INFO("Successfully loaded font: {}", path);
    return true;
}

FontDescription FontLoader::getDescription() const
{
    FontDescription description;
    if (!m_impl || !m_impl->face) {
        return description;
    }

    if (m_impl->face->family_name != nullptr) {
        description.family = m_impl->face->family_name;
    }
    if (m_impl->face->style_name != nullptr) {
        description.style = m_impl->face->style_name;
    }
    description.bold = (m_impl->face->style_flags & FT_STYLE_FLAG_BOLD) != 0;
    description.italic = (m_impl->face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
    return description;
}

void FontLoader::setPixelSize(uint32_t size)
{
    if (!m_impl || !m_impl->face) {
        AM_LOG_WARN("Cannot set pixel size: font not loaded");
        return;
    }

    // FT_Set_Pixel_Sizes resets the face's size state, discarding FreeType's own caches.
    if (m_impl->pixelSize == size) {
        return;
    }

    FT_Error error = FT_Set_Pixel_Sizes(m_impl->face, 0, size);
    if (error) {
        AM_LOG_ERROR("Failed to set pixel size: {}", size);
        return;
    }
    m_impl->pixelSize = size;
}

float FontLoader::getAdvance(uint32_t codepoint) const
{
    if (!m_impl || !m_impl->face) {
        return 0.0f;
    }

    FT_UInt glyphIndex = FT_Get_Char_Index(m_impl->face, codepoint);

    FT_Fixed advance = 0;
    if (FT_Get_Advance(m_impl->face, glyphIndex, FT_LOAD_DEFAULT, &advance) != 0) {
        return 0.0f;
    }

    return static_cast<float>(advance) * FT_FIXED_TO_PIXELS;
}

GlyphBitmap FontLoader::rasterizeGlyph(uint32_t codepoint)
{
    GlyphBitmap result;

    if (!m_impl || !m_impl->face) {
        AM_LOG_WARN("Cannot rasterize glyph: font not loaded");
        return result;
    }

    FT_UInt glyphIndex = FT_Get_Char_Index(m_impl->face, codepoint);
    if (glyphIndex == 0) {
        AM_LOG_WARN("Glyph not found for codepoint: {}, falling back to .notdef", codepoint);
    }

    FT_Error error = FT_Load_Glyph(m_impl->face, glyphIndex, FT_LOAD_DEFAULT);
    if (error) {
        AM_LOG_ERROR("Failed to load glyph for codepoint: {}", codepoint);
        return result;
    }

    error = FT_Render_Glyph(m_impl->face->glyph, FT_RENDER_MODE_NORMAL);
    if (error) {
        AM_LOG_ERROR("Failed to render glyph for codepoint: {}", codepoint);
        return result;
    }

    FT_Bitmap &bitmap = m_impl->face->glyph->bitmap;
    result.width = bitmap.width;
    result.height = bitmap.rows;
    result.bearingX = static_cast<float>(m_impl->face->glyph->bitmap_left);
    result.bearingY = static_cast<float>(m_impl->face->glyph->bitmap_top);
    result.advance = m_impl->face->glyph->advance.x * FT_POS_TO_PIXELS;

    if (bitmap.width > 0 && bitmap.rows > 0) {
        result.buffer.resize(bitmap.width * bitmap.rows);
        std::copy(bitmap.buffer, bitmap.buffer + (bitmap.width * bitmap.rows), result.buffer.begin());
    }

    return result;
}

FontMetrics FontLoader::getMetrics() const
{
    FontMetrics result;

    if (!m_impl || !m_impl->face) {
        AM_LOG_WARN("Cannot get metrics: font not loaded");
        return result;
    }

    result.ascender = m_impl->face->size->metrics.ascender * FT_POS_TO_PIXELS;
    result.descender = m_impl->face->size->metrics.descender * FT_POS_TO_PIXELS;
    result.lineHeight = m_impl->face->size->metrics.height * FT_POS_TO_PIXELS;

    return result;
}

uint32_t FontLoader::getGlyphIndex(uint32_t codepoint) const
{
    if (!m_impl || !m_impl->face) {
        AM_LOG_WARN("Cannot get glyph index: font not loaded");
        return 0;
    }

    return FT_Get_Char_Index(m_impl->face, codepoint);
}

float FontLoader::getKerning(uint32_t leftCodepoint, uint32_t rightCodepoint) const
{
    if (!m_impl || !m_impl->face) {
        return 0.0f;
    }

    if (!FT_HAS_KERNING(m_impl->face)) {
        return 0.0f;
    }

    FT_UInt leftIndex = FT_Get_Char_Index(m_impl->face, leftCodepoint);
    FT_UInt rightIndex = FT_Get_Char_Index(m_impl->face, rightCodepoint);

    if (leftIndex == 0 || rightIndex == 0) {
        return 0.0f;
    }

    FT_Vector delta;
    FT_Error error = FT_Get_Kerning(m_impl->face, leftIndex, rightIndex, FT_KERNING_DEFAULT, &delta);
    if (error) {
        return 0.0f;
    }

    return delta.x * FT_POS_TO_PIXELS;
}

bool FontLoader::isLoaded() const
{
    return m_impl && m_impl->face != nullptr;
}

} // namespace Amethyst
