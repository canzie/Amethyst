/**
 * @file atlas_packer.h
 * @brief Skyline bottom-left rectangle packing for texture atlases
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
 * @brief Skyline bottom-left bin packing algorithm for 2D rectangle atlases
 */
class AtlasPacker {
  public:
    AtlasPacker(uint32_t width, uint32_t height);

    /**
     * @brief Pack a rectangle into the atlas
     * @return Packed position and size, or nullopt if no space remains
     */
    std::optional<AtlasRegion> packRect(uint32_t width, uint32_t height);

    void reset();

    uint32_t getWidth() const;
    uint32_t getHeight() const;

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

} // namespace Amethyst

#endif // AMETHYST__ATLAS_PACKER_H
