/**
 * @file frame_draw_list.h
 * @brief Backend-agnostic per-frame draw description built by GpuResourceHub::sync
 */

#ifndef AMETHYST__FRAME_DRAW_LIST_H
#define AMETHYST__FRAME_DRAW_LIST_H

#include "components/common.h"

#include <cstdint>
#include <vector>

namespace Amethyst {

/**
 * @brief One draw per visible registry. All bases are element indices into the shared
 *        arenas, never byte pointers, so a backend can map them onto any physical copy.
 */
struct FrameDrawEntry {
    uint32_t firstInstance = 0;
    uint32_t instanceCount = 0;
    uint32_t glyphBase = 0;
    uint32_t lineBase = 0;
    uint32_t sliceBase = 0;
};

struct FrameDrawList {
    AmBufferId indexBuffer;
    std::vector<FrameDrawEntry> entries;
};

} // namespace Amethyst

#endif // AMETHYST__FRAME_DRAW_LIST_H
