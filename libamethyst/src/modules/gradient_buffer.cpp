#include "modules/gradient_buffer.h"

#include "amethyst/amethyst_backend.h"
#include "logging/log.h"
#include "math/math.h"

namespace Amethyst {

constexpr uint32_t INITIAL_GRADIENT_CAPACITY = 64;
constexpr uint32_t MAX_GRADIENT_CAPACITY = 4096;

#define GRADIENT_HEADER_PACK(type, stopCount) \
    ((static_cast<uint32_t>(type) & 0xFFu) | ((static_cast<uint32_t>(stopCount) & 0xFFu) << 8u))
#define GRADIENT_HEADER_TYPE(header)      ((header) & 0xFFu)
#define GRADIENT_HEADER_STOPCOUNT(header) (((header) >> 8u) & 0xFFu)

void GradientBuffer::init(AmethystBackend &backend)
{
    AmBufferDesc desc{INITIAL_GRADIENT_CAPACITY * sizeof(GpuGradient), AmBufferUsage::STORAGE, AmBufferMemory::HOST_VISIBLE, 5};
    m_arena.init(backend, desc, GrowthPolicy::doubleUntil(MAX_GRADIENT_CAPACITY * sizeof(GpuGradient)));
}

GradientBuffer::GpuGradient GradientBuffer::encode(const Gradient &grad)
{
    GpuGradient record;
    uint32_t count = grad.stopCount;
    record.header = GRADIENT_HEADER_PACK(grad.type, count);
    record.angle = grad.angleDegrees;

    float positions[MAX_GRADIENT_STOPS] = {};
    for (uint32_t i = 0; i < count; ++i) {
        positions[i] = grad.stops[i].t;
        record.stopColor[i] = grad.stops[i].color;
    }
    for (uint32_t j = 0; j < MAX_GRADIENT_STOPS / 2; ++j) {
        record.stopT[j] = packHalf2x16(vec2(positions[2 * j], positions[2 * j + 1]));
    }
    return record;
}

uint32_t GradientBuffer::resolveShared(const std::shared_ptr<const Gradient> &grad)
{
    if (grad == nullptr) {
        return Gradient::INVALID_SLOT;
    }

    if (grad->gpuSlot != Gradient::INVALID_SLOT) {
        return grad->gpuSlot;
    }

    sweep();

    ArenaBlock block = m_arena.alloc(sizeof(GpuGradient));
    if (!block.isValid()) {
        AM_LOG_ERROR("Gradient buffer exhausted");
        return Gradient::INVALID_SLOT;
    }

    uint32_t slot = static_cast<uint32_t>(block.offset / sizeof(GpuGradient));
    if (slot >= m_records.size()) {
        m_records.resize(slot + 1);
        m_blocks.resize(slot + 1);
        m_owners.resize(slot + 1);
    }

    m_records[slot] = encode(*grad);
    m_blocks[slot] = block;
    m_owners[slot] = grad;
    grad->gpuSlot = slot;
    m_dirty.add(slot, slot + 1);
    return slot;
}

void GradientBuffer::sweep()
{
    for (uint32_t slot = 0; slot < m_owners.size(); ++slot) {
        if (!m_owners[slot].expired() || !m_blocks[slot].isValid()) {
            continue;
        }
        m_arena.free(m_blocks[slot]);
        m_blocks[slot] = ArenaBlock{};
        m_owners[slot].reset();
    }
}

void GradientBuffer::sync(void *cmdBuffer)
{
    if (m_dirty.empty()) {
        return;
    }

    size_t loBytes = static_cast<size_t>(m_dirty.lo) * sizeof(GpuGradient);
    size_t hiBytes = static_cast<size_t>(m_dirty.hi) * sizeof(GpuGradient);
    m_arena.upload(cmdBuffer, reinterpret_cast<const uint8_t *>(m_records.data()) + loBytes, loBytes, hiBytes - loBytes);
    m_dirty.clear();
}

} // namespace Amethyst
