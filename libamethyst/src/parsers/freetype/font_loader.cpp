#include "font_loader.h"
#include "logging/log.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

namespace Amethyst {

struct FontLoader::Impl {
    FT_Library library = nullptr;
    FT_Face face = nullptr;
};

FontLoader::FontLoader() : m_impl(std::make_unique<Impl>())
{
    FT_Error error = FT_Init_FreeType(&m_impl->library);
    if (error) {
        AM_LOG_ERROR("Failed to initialize FreeType library");
        m_impl.reset();
    }
}

FontLoader::~FontLoader()
{
    if (m_impl) {
        if (m_impl->face) {
            FT_Done_Face(m_impl->face);
        }
        if (m_impl->library) {
            FT_Done_FreeType(m_impl->library);
        }
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
    if (!m_impl || !m_impl->library) {
        AM_LOG_ERROR("FreeType library not initialized");
        return false;
    }

    if (m_impl->face) {
        FT_Done_Face(m_impl->face);
        m_impl->face = nullptr;
    }

    FT_Error error = FT_New_Face(m_impl->library, path.c_str(), 0, &m_impl->face);
    if (error) {
        AM_LOG_ERROR("Failed to load font from path: {}", path);
        return false;
    }

    AM_LOG_INFO("Successfully loaded font: {}", path);
    return true;
}

void FontLoader::setPixelSize(uint32_t size)
{
    if (!m_impl || !m_impl->face) {
        AM_LOG_WARN("Cannot set pixel size: font not loaded");
        return;
    }

    FT_Error error = FT_Set_Pixel_Sizes(m_impl->face, 0, size);
    if (error) {
        AM_LOG_ERROR("Failed to set pixel size: {}", size);
    }
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
    result.advance = m_impl->face->glyph->advance.x / 64.0f;

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

    result.ascender = m_impl->face->size->metrics.ascender / 64.0f;
    result.descender = m_impl->face->size->metrics.descender / 64.0f;
    result.lineHeight = m_impl->face->size->metrics.height / 64.0f;

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

    return delta.x / 64.0f;
}

bool FontLoader::isLoaded() const
{
    return m_impl && m_impl->face != nullptr;
}

} // namespace Amethyst
