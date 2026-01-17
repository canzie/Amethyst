/**
 * @file ttf_types.h
 * @brief TTF data structures and macros for GPU-friendly font data
 */

#ifndef AMETHYST_TTF_TYPES_H
#define AMETHYST_TTF_TYPES_H

#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

namespace Amethyst {
namespace TTF {

/// @name Point flags (TrueType spec)
/// @{
#define TTF_POINT_ON_CURVE      (1 << 0)
#define TTF_POINT_X_SHORT       (1 << 1)
#define TTF_POINT_Y_SHORT       (1 << 2)
#define TTF_POINT_REPEAT        (1 << 3)
#define TTF_POINT_X_SAME_OR_POS (1 << 4)
#define TTF_POINT_Y_SAME_OR_POS (1 << 5)

#define TTF_IS_ON_CURVE(flags) ((flags) & TTF_POINT_ON_CURVE)
#define TTF_IS_CONTROL(flags)  (!TTF_IS_ON_CURVE(flags))
/// @}

/// @name Glyph flags (processed data)
/// @{
#define TTF_GLYPH_SIMPLE    (0)
#define TTF_GLYPH_COMPOSITE (1 << 0)
#define TTF_GLYPH_EMPTY     (1 << 1)

#define TTF_IS_COMPOSITE(flags) ((flags) & TTF_GLYPH_COMPOSITE)
#define TTF_IS_EMPTY(flags)     ((flags) & TTF_GLYPH_EMPTY)
/// @}

/// @name Table tags
/// @{
#define TTF_TAG(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))

#define TTF_TAG_HEAD TTF_TAG('h', 'e', 'a', 'd')
#define TTF_TAG_MAXP TTF_TAG('m', 'a', 'x', 'p')
#define TTF_TAG_LOCA TTF_TAG('l', 'o', 'c', 'a')
#define TTF_TAG_GLYF TTF_TAG('g', 'l', 'y', 'f')
#define TTF_TAG_CMAP TTF_TAG('c', 'm', 'a', 'p')
#define TTF_TAG_HHEA TTF_TAG('h', 'h', 'e', 'a')
#define TTF_TAG_HMTX TTF_TAG('h', 'm', 't', 'x')
#define TTF_TAG_KERN TTF_TAG('k', 'e', 'r', 'n')
#define TTF_TAG_NAME TTF_TAG('n', 'a', 'm', 'e')
/// @}

/**
 * @brief Single point in a glyph contour (GPU-aligned, 12 bytes)
 */
struct Point {
    glm::vec2 pos;
    uint32_t flags;
};
static_assert(sizeof(Point) == 12, "Point must be 12 bytes for GPU alignment");

/**
 * @brief Contour definition - references into points buffer (8 bytes)
 */
struct Contour {
    uint32_t pointStart;
    uint32_t pointCount;
};
static_assert(sizeof(Contour) == 8, "Contour must be 8 bytes");

/**
 * @brief Glyph metadata - references into contours buffer (48 bytes)
 */
struct Glyph {
    uint32_t contourStart;
    uint32_t contourCount;
    uint32_t flags;
    uint32_t _pad0;

    glm::vec2 bboxMin;
    glm::vec2 bboxMax;

    float advanceWidth;
    float leftSideBearing;
    float _pad1[2];
};
static_assert(sizeof(Glyph) == 48, "Glyph must be 48 bytes for GPU alignment");

/**
 * @brief Complete parsed font data ready for GPU upload
 */
struct FontData {
    std::vector<Point> points;
    std::vector<Contour> contours;
    std::vector<Glyph> glyphs;
    std::unordered_map<uint32_t, uint32_t> codepointMap;

    uint16_t unitsPerEm;
    int16_t ascender;
    int16_t descender;
    int16_t lineGap;

    uint32_t getGlyphIndex(uint32_t codepoint) const;
    const Glyph *getGlyph(uint32_t glyphIndex) const;
};

#pragma pack(push, 1)

/**
 * @brief TTF table directory entry
 */
struct TableRecord {
    uint32_t tag;
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;
};

/**
 * @brief TTF file header
 */
struct OffsetTable {
    uint32_t sfntVersion;
    uint16_t numTables;
    uint16_t searchRange;
    uint16_t entrySelector;
    uint16_t rangeShift;
};

/**
 * @brief TTF 'head' table
 */
struct HeadTable {
    uint16_t majorVersion;
    uint16_t minorVersion;
    int32_t fontRevision;
    uint32_t checksumAdjustment;
    uint32_t magicNumber;
    uint16_t flags;
    uint16_t unitsPerEm;
    int64_t created;
    int64_t modified;
    int16_t xMin;
    int16_t yMin;
    int16_t xMax;
    int16_t yMax;
    uint16_t macStyle;
    uint16_t lowestRecPPEM;
    int16_t fontDirectionHint;
    int16_t indexToLocFormat;
    int16_t glyphDataFormat;
};

/**
 * @brief TTF 'maxp' table
 */
struct MaxpTable {
    uint32_t version;
    uint16_t numGlyphs;
};

/**
 * @brief TTF 'hhea' table
 */
struct HheaTable {
    uint16_t majorVersion;
    uint16_t minorVersion;
    int16_t ascender;
    int16_t descender;
    int16_t lineGap;
    uint16_t advanceWidthMax;
    int16_t minLeftSideBearing;
    int16_t minRightSideBearing;
    int16_t xMaxExtent;
    int16_t caretSlopeRise;
    int16_t caretSlopeRun;
    int16_t caretOffset;
    int16_t reserved[4];
    int16_t metricDataFormat;
    uint16_t numberOfHMetrics;
};

/**
 * @brief TTF horizontal metrics entry
 */
struct LongHorMetric {
    uint16_t advanceWidth;
    int16_t leftSideBearing;
};

#pragma pack(pop)

} // namespace TTF
} // namespace Amethyst

#endif // AMETHYST_TTF_TYPES_H
