/**
 * @file ttf_parser.cpp
 * @brief TTF font file parser implementation
 */

#include "ttf_parser.h"
#include "logging/log.h"
#include <cstring>
#include <fstream>

namespace Amethyst {
namespace TTF {

/// Big-endian read helpers
uint16_t Parser::readU16(const uint8_t *ptr) const
{
    return (uint16_t)(ptr[0] << 8 | ptr[1]);
}

int16_t Parser::readS16(const uint8_t *ptr) const
{
    return (int16_t)(ptr[0] << 8 | ptr[1]);
}

uint32_t Parser::readU32(const uint8_t *ptr) const
{
    return (uint32_t)(ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3]);
}

std::optional<FontData> Parser::parse(const std::string &filepath)
{
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        AM_LOG_ERROR("TTF: Failed to open file: {}", filepath);
        return std::nullopt;
    }

    auto size = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char *>(buffer.data()), size);

    return parse(std::span<const uint8_t>(buffer));
}

std::optional<FontData> Parser::parse(std::span<const uint8_t> data)
{
    m_data = data;
    m_tables.clear();

    if (!parseOffsetTable()) {
        return std::nullopt;
    }

    if (!findTables()) {
        return std::nullopt;
    }

    FontData fontData;

    if (!parseHead(fontData)) {
        AM_LOG_ERROR("TTF: Failed to parse 'head' table");
        return std::nullopt;
    }

    m_scale = 1.0f / fontData.unitsPerEm;

    uint16_t numGlyphs = 0;
    if (!parseMaxp(fontData)) {
        AM_LOG_ERROR("TTF: Failed to parse 'maxp' table");
        return std::nullopt;
    }
    numGlyphs = static_cast<uint16_t>(fontData.glyphs.capacity());

    uint16_t numHMetrics = 0;
    if (!parseHhea(fontData)) {
        AM_LOG_ERROR("TTF: Failed to parse 'hhea' table");
        return std::nullopt;
    }

    auto hheaTable = getTable(TTF_TAG_HHEA);
    if (hheaTable) {
        numHMetrics = readU16(hheaTable + 34);
    }

    fontData.glyphs.resize(numGlyphs);

    if (!parseHmtx(fontData, numGlyphs, numHMetrics)) {
        AM_LOG_ERROR("TTF: Failed to parse 'hmtx' table");
        return std::nullopt;
    }

    auto headTable = getTable(TTF_TAG_HEAD);
    bool longLoca = headTable ? readS16(headTable + 50) == 1 : false;

    std::vector<uint32_t> locaOffsets;
    if (!parseLoca(locaOffsets, numGlyphs, longLoca)) {
        AM_LOG_ERROR("TTF: Failed to parse 'loca' table");
        return std::nullopt;
    }

    if (!parseGlyf(fontData, locaOffsets)) {
        AM_LOG_ERROR("TTF: Failed to parse 'glyf' table");
        return std::nullopt;
    }

    if (!parseCmap(fontData)) {
        AM_LOG_ERROR("TTF: Failed to parse 'cmap' table");
        return std::nullopt;
    }

    return fontData;
}

bool Parser::parseOffsetTable()
{
    if (m_data.size() < sizeof(OffsetTable)) {
        AM_LOG_ERROR("TTF: File too small for offset table");
        return false;
    }

    uint32_t sfntVersion = readU32(m_data.data());

    // Check for valid TTF signature (0x00010000 or 'true' or 'typ1')
    if (sfntVersion != 0x00010000 && sfntVersion != TTF_TAG('t', 'r', 'u', 'e')) {
        AM_LOG_ERROR("TTF: Invalid sfnt version: 0x{:08X}", sfntVersion);
        return false;
    }

    return true;
}

bool Parser::findTables()
{
    uint16_t numTables = readU16(m_data.data() + 4);
    size_t tableOffset = sizeof(OffsetTable);

    if (m_data.size() < tableOffset + numTables * sizeof(TableRecord)) {
        AM_LOG_ERROR("TTF: File too small for table records");
        return false;
    }

    for (uint16_t i = 0; i < numTables; i++) {
        const uint8_t *entry = m_data.data() + tableOffset + i * 16;
        TableRecord record;
        record.tag = readU32(entry);
        record.checksum = readU32(entry + 4);
        record.offset = readU32(entry + 8);
        record.length = readU32(entry + 12);

        m_tables[record.tag] = record;
    }

    // Verify required tables exist
    uint32_t required[] = {TTF_TAG_HEAD, TTF_TAG_MAXP, TTF_TAG_LOCA, TTF_TAG_GLYF, TTF_TAG_CMAP, TTF_TAG_HHEA, TTF_TAG_HMTX};
    for (uint32_t tag : required) {
        if (m_tables.find(tag) == m_tables.end()) {
            char tagStr[5] = {(char)(tag >> 24), (char)(tag >> 16), (char)(tag >> 8), (char)tag, 0};
            AM_LOG_ERROR("TTF: Missing required table: {}", tagStr);
            return false;
        }
    }

    return true;
}

const uint8_t *Parser::getTable(uint32_t tag) const
{
    auto it = m_tables.find(tag);
    if (it == m_tables.end()) {
        return nullptr;
    }
    return m_data.data() + it->second.offset;
}

uint32_t Parser::getTableLength(uint32_t tag) const
{
    auto it = m_tables.find(tag);
    if (it == m_tables.end()) {
        return 0;
    }
    return it->second.length;
}

bool Parser::parseHead(FontData &out)
{
    const uint8_t *head = getTable(TTF_TAG_HEAD);
    if (!head) {
        return false;
    }

    uint32_t magic = readU32(head + 12);
    if (magic != 0x5F0F3CF5) {
        AM_LOG_ERROR("TTF: Invalid magic number in 'head' table");
        return false;
    }

    out.unitsPerEm = readU16(head + 18);
    return true;
}

bool Parser::parseMaxp(FontData &out)
{
    const uint8_t *maxp = getTable(TTF_TAG_MAXP);
    if (!maxp) {
        return false;
    }

    uint16_t numGlyphs = readU16(maxp + 4);
    out.glyphs.reserve(numGlyphs);
    return true;
}

bool Parser::parseHhea(FontData &out)
{
    const uint8_t *hhea = getTable(TTF_TAG_HHEA);
    if (!hhea) {
        return false;
    }

    out.ascender = readS16(hhea + 4);
    out.descender = readS16(hhea + 6);
    out.lineGap = readS16(hhea + 8);
    return true;
}

bool Parser::parseHmtx(FontData &out, uint16_t numGlyphs, uint16_t numHMetrics)
{
    const uint8_t *hmtx = getTable(TTF_TAG_HMTX);
    if (!hmtx) {
        return false;
    }

    uint16_t lastAdvance = 0;

    for (uint16_t i = 0; i < numGlyphs; i++) {
        if (i < numHMetrics) {
            const uint8_t *metric = hmtx + i * 4;
            lastAdvance = readU16(metric);
            out.glyphs[i].advanceWidth = lastAdvance * m_scale;
            out.glyphs[i].leftSideBearing = readS16(metric + 2) * m_scale;
        } else {
            // Monospaced glyphs after numHMetrics use last advance
            out.glyphs[i].advanceWidth = lastAdvance * m_scale;
            const uint8_t *lsb = hmtx + numHMetrics * 4 + (i - numHMetrics) * 2;
            out.glyphs[i].leftSideBearing = readS16(lsb) * m_scale;
        }
    }

    return true;
}

bool Parser::parseLoca(std::vector<uint32_t> &offsets, uint16_t numGlyphs, bool longFormat)
{
    const uint8_t *loca = getTable(TTF_TAG_LOCA);
    if (!loca) {
        return false;
    }

    offsets.resize(numGlyphs + 1);

    for (uint16_t i = 0; i <= numGlyphs; i++) {
        if (longFormat) {
            offsets[i] = readU32(loca + i * 4);
        } else {
            offsets[i] = readU16(loca + i * 2) * 2;
        }
    }

    return true;
}

bool Parser::parseGlyf(FontData &out, const std::vector<uint32_t> &locaOffsets)
{
    const uint8_t *glyf = getTable(TTF_TAG_GLYF);
    if (!glyf) {
        return false;
    }

    uint32_t glyfLength = getTableLength(TTF_TAG_GLYF);

    for (size_t i = 0; i < locaOffsets.size() - 1; i++) {
        uint32_t offset = locaOffsets[i];
        uint32_t nextOffset = locaOffsets[i + 1];
        uint32_t length = nextOffset - offset;

        if (length == 0) {
            // Empty glyph (space, etc.)
            out.glyphs[i].flags = TTF_GLYPH_EMPTY;
            out.glyphs[i].contourStart = 0;
            out.glyphs[i].contourCount = 0;
            out.glyphs[i].bboxMin = glm::vec2(0.0f);
            out.glyphs[i].bboxMax = glm::vec2(0.0f);
            continue;
        }

        if (offset + length > glyfLength) {
            AM_LOG_WARN("TTF: Glyph {} offset out of bounds", i);
            continue;
        }

        const uint8_t *glyphData = glyf + offset;
        int16_t numContours = readS16(glyphData);

        // Read bounding box
        out.glyphs[i].bboxMin.x = readS16(glyphData + 2) * m_scale;
        out.glyphs[i].bboxMin.y = readS16(glyphData + 4) * m_scale;
        out.glyphs[i].bboxMax.x = readS16(glyphData + 6) * m_scale;
        out.glyphs[i].bboxMax.y = readS16(glyphData + 8) * m_scale;

        if (numContours >= 0) {
            if (!parseSimpleGlyph(glyphData, length, out, static_cast<uint32_t>(i))) {
                AM_LOG_WARN("TTF: Failed to parse simple glyph {}", i);
            }
        } else {
            if (!parseCompositeGlyph(glyphData, length, out, static_cast<uint32_t>(i))) {
                AM_LOG_WARN("TTF: Failed to parse composite glyph {}", i);
            }
        }
    }

    return true;
}

bool Parser::parseSimpleGlyph(const uint8_t *glyphData, uint32_t length, FontData &out, uint32_t glyphIdx)
{
    (void)length;
    int16_t numContours = readS16(glyphData);
    if (numContours < 0) {
        return true;
    }

    const uint8_t *ptr = glyphData + 10; // Skip header

    // Read end points of each contour
    std::vector<uint16_t> endPoints(numContours);
    for (int16_t i = 0; i < numContours; i++) {
        endPoints[i] = readU16(ptr);
        ptr += 2;
    }

    uint16_t numPoints = endPoints[numContours - 1] + 1;

    // Skip instructions
    uint16_t instructionLength = readU16(ptr);
    ptr += 2 + instructionLength;

    // Read flags
    std::vector<uint8_t> flags(numPoints);
    for (uint16_t i = 0; i < numPoints;) {
        uint8_t flag = *ptr++;
        flags[i++] = flag;

        if (flag & TTF_POINT_REPEAT) {
            uint8_t repeatCount = *ptr++;
            for (uint8_t j = 0; j < repeatCount && i < numPoints; j++) {
                flags[i++] = flag;
            }
        }
    }

    // Read X coordinates
    std::vector<int16_t> xCoords(numPoints);
    int16_t x = 0;
    for (uint16_t i = 0; i < numPoints; i++) {
        if (flags[i] & TTF_POINT_X_SHORT) {
            int16_t delta = *ptr++;
            if (!(flags[i] & TTF_POINT_X_SAME_OR_POS)) {
                delta = -delta;
            }
            x += delta;
        } else if (!(flags[i] & TTF_POINT_X_SAME_OR_POS)) {
            x += readS16(ptr);
            ptr += 2;
        }
        xCoords[i] = x;
    }

    // Read Y coordinates
    std::vector<int16_t> yCoords(numPoints);
    int16_t y = 0;
    for (uint16_t i = 0; i < numPoints; i++) {
        if (flags[i] & TTF_POINT_Y_SHORT) {
            int16_t delta = *ptr++;
            if (!(flags[i] & TTF_POINT_Y_SAME_OR_POS)) {
                delta = -delta;
            }
            y += delta;
        } else if (!(flags[i] & TTF_POINT_Y_SAME_OR_POS)) {
            y += readS16(ptr);
            ptr += 2;
        }
        yCoords[i] = y;
    }

    // Store glyph contour info
    out.glyphs[glyphIdx].contourStart = static_cast<uint32_t>(out.contours.size());
    out.glyphs[glyphIdx].contourCount = numContours;
    out.glyphs[glyphIdx].flags = TTF_GLYPH_SIMPLE;

    uint16_t pointIdx = 0;
    for (int16_t c = 0; c < numContours; c++) {
        Contour contour;
        contour.pointStart = static_cast<uint32_t>(out.points.size());

        uint16_t startIdx = pointIdx;
        uint16_t endIdx = endPoints[c];
        uint16_t contourLen = endIdx - startIdx + 1;

        // Find first on-curve point to start from (rotate if needed)
        for (uint16_t i = 0; i < contourLen; i++) {
            uint16_t curr = startIdx + i;
            uint16_t next = startIdx + ((i + 1) % contourLen);

            Point pt;
            pt.pos.x = xCoords[curr] * m_scale;
            pt.pos.y = yCoords[curr] * m_scale;
            pt.flags = flags[curr] & TTF_POINT_ON_CURVE;
            out.points.push_back(pt);

            bool currOn = TTF_IS_ON_CURVE(flags[curr]);
            bool nextOn = TTF_IS_ON_CURVE(flags[next]);

            if (!currOn && !nextOn) {
                // Insert implied on-curve point between consecutive off-curve points
                Point mid;
                mid.pos.x = (xCoords[curr] + xCoords[next]) * 0.5f * m_scale;
                mid.pos.y = (yCoords[curr] + yCoords[next]) * 0.5f * m_scale;
                mid.flags = TTF_POINT_ON_CURVE;
                out.points.push_back(mid);
            } else if (currOn && nextOn) {
                // Insert off-curve control point between consecutive on-curve points
                // This converts line segments to degenerate beziers for uniform handling
                Point mid;
                mid.pos.x = (xCoords[curr] + xCoords[next]) * 0.5f * m_scale;
                mid.pos.y = (yCoords[curr] + yCoords[next]) * 0.5f * m_scale;
                mid.flags = 0; // off-curve
                out.points.push_back(mid);
            }
        }

        pointIdx = endIdx + 1;
        contour.pointCount = static_cast<uint32_t>(out.points.size()) - contour.pointStart;
        out.contours.push_back(contour);
    }

    return true;
}

bool Parser::parseCompositeGlyph(const uint8_t *glyphData, uint32_t length, FontData &out, uint32_t glyphIdx)
{
    (void)glyphData;
    (void)length;
    // TODO: Implement composite glyph parsing
    // For now, mark as composite and skip
    out.glyphs[glyphIdx].flags = TTF_GLYPH_COMPOSITE;
    out.glyphs[glyphIdx].contourStart = 0;
    out.glyphs[glyphIdx].contourCount = 0;
    return true;
}

bool Parser::parseCmap(FontData &out)
{
    const uint8_t *cmap = getTable(TTF_TAG_CMAP);
    if (!cmap) {
        return false;
    }

    uint16_t numSubtables = readU16(cmap + 2);
    const uint8_t *subtableEntries = cmap + 4;

    // Find a suitable subtable (prefer format 4 for BMP, format 12 for full Unicode)
    uint32_t bestOffset = 0;
    int bestPriority = -1;

    for (uint16_t i = 0; i < numSubtables; i++) {
        uint16_t platformId = readU16(subtableEntries + i * 8);
        uint16_t encodingId = readU16(subtableEntries + i * 8 + 2);
        uint32_t offset = readU32(subtableEntries + i * 8 + 4);

        uint16_t format = readU16(cmap + offset);
        int priority = -1;

        // Prefer Unicode platform (0) or Windows Unicode (3, 1)
        if (platformId == 0) {
            priority = (format == 12) ? 4 : (format == 4) ? 3 : 0;
        } else if (platformId == 3 && encodingId == 1) {
            priority = (format == 4) ? 2 : 0;
        } else if (platformId == 3 && encodingId == 10) {
            priority = (format == 12) ? 5 : 0;
        }

        if (priority > bestPriority) {
            bestPriority = priority;
            bestOffset = offset;
        }
    }

    if (bestOffset == 0) {
        AM_LOG_ERROR("TTF: No suitable cmap subtable found");
        return false;
    }

    const uint8_t *subtable = cmap + bestOffset;
    uint16_t format = readU16(subtable);

    if (format == 4) {
        return parseCmapFormat4(subtable, out);
    } else if (format == 12) {
        return parseCmapFormat12(subtable, out);
    }

    AM_LOG_ERROR("TTF: Unsupported cmap format: {}", format);
    return false;
}

bool Parser::parseCmapFormat4(const uint8_t *subtable, FontData &out)
{
    uint16_t segCount = readU16(subtable + 6) / 2;
    const uint8_t *endCodes = subtable + 14;
    const uint8_t *startCodes = endCodes + segCount * 2 + 2;
    const uint8_t *idDeltas = startCodes + segCount * 2;
    const uint8_t *idRangeOffsets = idDeltas + segCount * 2;

    for (uint16_t i = 0; i < segCount; i++) {
        uint16_t endCode = readU16(endCodes + i * 2);
        uint16_t startCode = readU16(startCodes + i * 2);
        int16_t idDelta = readS16(idDeltas + i * 2);
        uint16_t idRangeOffset = readU16(idRangeOffsets + i * 2);

        if (startCode == 0xFFFF) {
            break;
        }

        for (uint32_t c = startCode; c <= endCode; c++) {
            uint32_t glyphIndex;

            if (idRangeOffset == 0) {
                glyphIndex = (c + idDelta) & 0xFFFF;
            } else {
                const uint8_t *glyphIdPtr = idRangeOffsets + i * 2 + idRangeOffset + (c - startCode) * 2;
                glyphIndex = readU16(glyphIdPtr);
                if (glyphIndex != 0) {
                    glyphIndex = (glyphIndex + idDelta) & 0xFFFF;
                }
            }

            if (glyphIndex != 0 && glyphIndex < out.glyphs.size()) {
                out.codepointMap[c] = glyphIndex;
            }
        }
    }

    return true;
}

bool Parser::parseCmapFormat12(const uint8_t *subtable, FontData &out)
{
    uint32_t numGroups = readU32(subtable + 12);
    const uint8_t *groups = subtable + 16;

    for (uint32_t i = 0; i < numGroups; i++) {
        uint32_t startCode = readU32(groups + i * 12);
        uint32_t endCode = readU32(groups + i * 12 + 4);
        uint32_t startGlyph = readU32(groups + i * 12 + 8);

        for (uint32_t c = startCode; c <= endCode; c++) {
            uint32_t glyphIndex = startGlyph + (c - startCode);
            if (glyphIndex < out.glyphs.size()) {
                out.codepointMap[c] = glyphIndex;
            }
        }
    }

    return true;
}

uint32_t FontData::getGlyphIndex(uint32_t codepoint) const
{
    auto it = codepointMap.find(codepoint);
    return it != codepointMap.end() ? it->second : 0;
}

const Glyph *FontData::getGlyph(uint32_t glyphIndex) const
{
    if (glyphIndex >= glyphs.size()) {
        return nullptr;
    }
    return &glyphs[glyphIndex];
}

} // namespace TTF
} // namespace Amethyst
