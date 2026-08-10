/**
 * @file atlas_packer.h
 * @brief Rectangle packing for texture atlases: skyline and shelf policies
 *
 * Both packers implement the same interface and are selected at compile time, so the
 * two can be measured against each other in a real build. Shelf is the default;
 * defining AMETHYST_ATLAS_PACKER_SKYLINE selects skyline instead.
 *
 * They pack within roughly 10% of each other: skyline is denser, because a short rect
 * does not commit a full-width band, while shelf places in O(1) with no search.
 * Allocation only happens on a glyph cache miss, so neither difference is large in
 * absolute terms.
 */

#ifndef AMETHYST__ATLAS_PACKER_H
#define AMETHYST__ATLAS_PACKER_H

#include <cstdint>
#include <optional>
#include <vector>

namespace Amethyst {

/**
 * @brief A region allocated within an atlas
 */
struct AtlasRegion {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
};

/**
 * @brief Skyline bottom-left bin packing, tracking a per-column frontier.
 *
 * Denser than shelf because a short rect does not commit a full-width band, at the
 * cost of scanning the node list per allocation.
 */
class SkylinePacker {
  public:
    SkylinePacker(uint32_t width, uint32_t height);

    /**
     * @brief Pack a rectangle into the atlas
     * @return Packed position and size, or nullopt if no space remains
     */
    std::optional<AtlasRegion> packRect(uint32_t width, uint32_t height);

    void reset();

    uint32_t getWidth() const;
    uint32_t getHeight() const;

    /**
     * @brief Pixels consumed, including gaps under the frontier that can no longer be used.
     *
     * Distinct from the sum of packed rects: this is what determines when the atlas is
     * effectively full, so it is the number to compare between policies.
     */
    uint64_t footprint() const;

  private:
    struct SkylineNode {
        int32_t x;
        int32_t y;
        int32_t width;
    };

    void insertSkylineNode(int32_t index, const SkylineNode &node);
    void mergeSkylineNodes();

    std::vector<SkylineNode> m_skyline;
    uint32_t m_width;
    uint32_t m_height;
};

/**
 * @brief Shelf packing: rows filled left to right, each row as tall as its tallest rect.
 *
 * O(1) placement with no search. Wastes the slack above shorter rects in a row, since a
 * row is committed to one height and the cursor never moves back.
 */
class ShelfPacker {
  public:
    ShelfPacker(uint32_t width, uint32_t height);

    /**
     * @brief Pack a rectangle into the atlas
     * @return Packed position and size, or nullopt if no space remains
     */
    std::optional<AtlasRegion> packRect(uint32_t width, uint32_t height);

    void reset();

    uint32_t getWidth() const;
    uint32_t getHeight() const;

    /**
     * @brief Pixels consumed: every started row counts full width, slack included.
     */
    uint64_t footprint() const;

  private:
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_x = 0;
    uint32_t m_rowY = 0;
    uint32_t m_rowHeight = 0;
};

#if defined(AMETHYST_ATLAS_PACKER_SKYLINE)
using AtlasPacker = SkylinePacker;
#else
using AtlasPacker = ShelfPacker;
#endif

} // namespace Amethyst

#endif // AMETHYST__ATLAS_PACKER_H
