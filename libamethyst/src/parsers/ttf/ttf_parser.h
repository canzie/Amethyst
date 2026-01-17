/**
 * @file ttf_parser.h
 * @brief TTF font file parser
 */

#ifndef AMETHYST_TTF_PARSER_H
#define AMETHYST_TTF_PARSER_H

#include "ttf_types.h"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

namespace Amethyst {
namespace TTF {

/**
 * @brief Parses TrueType font files into GPU-ready data structures
 */
class Parser {
  public:
    Parser() = default;
    ~Parser() = default;

    /**
     * @brief Parse a TTF file from disk
     * @param filepath Path to the .ttf file
     * @return Parsed font data or nullopt on failure
     */
    std::optional<FontData> parse(const std::string &filepath);

    /**
     * @brief Parse TTF data from memory
     * @param data Raw TTF file bytes
     * @return Parsed font data or nullopt on failure
     */
    std::optional<FontData> parse(std::span<const uint8_t> data);

  private:
    std::span<const uint8_t> m_data;
    std::unordered_map<uint32_t, TableRecord> m_tables;
    float m_scale = 1.0f;

    bool parseOffsetTable();
    bool findTables();

    const uint8_t *getTable(uint32_t tag) const;
    uint32_t getTableLength(uint32_t tag) const;

    bool parseHead(FontData &out);
    bool parseMaxp(FontData &out);
    bool parseHhea(FontData &out);
    bool parseHmtx(FontData &out, uint16_t numGlyphs, uint16_t numHMetrics);
    bool parseLoca(std::vector<uint32_t> &offsets, uint16_t numGlyphs, bool longFormat);
    bool parseGlyf(FontData &out, const std::vector<uint32_t> &locaOffsets);
    bool parseCmap(FontData &out);

    bool parseSimpleGlyph(const uint8_t *glyphData, uint32_t length, FontData &out, uint32_t glyphIdx);
    bool parseCompositeGlyph(const uint8_t *glyphData, uint32_t length, FontData &out, uint32_t glyphIdx);
    bool parseCmapFormat4(const uint8_t *subtable, FontData &out);
    bool parseCmapFormat12(const uint8_t *subtable, FontData &out);

    uint16_t readU16(const uint8_t *ptr) const;
    int16_t readS16(const uint8_t *ptr) const;
    uint32_t readU32(const uint8_t *ptr) const;
};

} // namespace TTF
} // namespace Amethyst

#endif // AMETHYST_TTF_PARSER_H
