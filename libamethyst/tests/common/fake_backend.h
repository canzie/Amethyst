#ifndef AMETHYST__TESTS_FAKE_BACKEND_H
#define AMETHYST__TESTS_FAKE_BACKEND_H

#include "amethyst/amethyst_backend.h"

#include <cstdint>
#include <cstring>
#include <set>
#include <vector>

namespace AmethystTest {

using namespace Amethyst;

/**
 * @brief CPU-only AmethystBackend: buffers are byte vectors, textures are counted, and every
 * failure path can be forced so the core-side policy (arenas, hub) is testable without a GPU.
 */
class FakeBackend : public AmethystBackend {
  public:
    struct Buffer {
        AmBufferDesc desc;
        std::vector<uint8_t> bytes;
        uint32_t uploads = 0;
        bool alive = true;
    };

    AmBufferId createBuffer(const AmBufferDesc &desc) override
    {
        if (failBindings.count(desc.shaderBinding) != 0) {
            return {};
        }
        Buffer buffer;
        buffer.desc = desc;
        buffer.bytes.resize(desc.initialCapacity);
        buffers.push_back(buffer);
        return AmBufferId{static_cast<uint32_t>(buffers.size() - 1)};
    }

    bool growBuffer(AmBufferId id, size_t newCapacity) override
    {
        if (!allowGrow || id.id >= buffers.size()) {
            return false;
        }
        if (newCapacity > buffers[id.id].bytes.size()) {
            buffers[id.id].bytes.resize(newCapacity);
        }
        ++growCount;
        return true;
    }

    void uploadBufferRange(void *, AmBufferId id, const void *data, size_t offsetBytes, size_t sizeBytes) override
    {
        if (!id.isValid() || id.id >= buffers.size()) {
            return;
        }
        Buffer &buffer = buffers[id.id];
        // an upload past the end is a core-side bug; count it so a test can fail on it
        if (offsetBytes + sizeBytes > buffer.bytes.size()) {
            ++outOfBoundsUploads;
            return;
        }
        std::memcpy(buffer.bytes.data() + offsetBytes, data, sizeBytes);
        ++buffer.uploads;
    }

    void destroyBuffer(AmBufferId id) override
    {
        if (id.id < buffers.size()) {
            buffers[id.id].alive = false;
        }
    }

    AmTextureId createTexture(const AmTextureDesc &) override { return AmTextureId{nextTexture++}; }
    void uploadTexture(void *, AmTextureId, const uint8_t *) override { ++textureUploads; }
    void destroyTexture(AmTextureId) override {}

    /** @brief The buffer bound at a shader binding, or nullptr. */
    Buffer *byBinding(uint32_t binding)
    {
        for (Buffer &buffer : buffers) {
            if (buffer.desc.shaderBinding == binding) {
                return &buffer;
            }
        }
        return nullptr;
    }

    std::vector<Buffer> buffers;
    std::set<uint32_t> failBindings;
    bool allowGrow = true;
    uint32_t growCount = 0;
    uint32_t outOfBoundsUploads = 0;
    uint32_t textureUploads = 0;
    uint32_t nextTexture = 0;
};

} // namespace AmethystTest

#endif // AMETHYST__TESTS_FAKE_BACKEND_H
