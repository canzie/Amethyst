#include "amethyst__vk13_glfw.h"

#include "components/input_interface.h"
#include "components/ui_layer.h"
#include "logging/log.h"
#include "utils/profiling.h"

#include <GLFW/glfw3.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include "math/math.h"
#include <iterator>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Amethyst {

/**
 * @brief Align a size up to the specified alignment
 */
static inline size_t alignUp(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

struct Amethyst_glfw_Data {
    GLFWwindow *window;
    GLFWmousebuttonfun prevMouseButtonCallback = nullptr;
    GLFWcursorposfun prevCursorPosCallback = nullptr;
    GLFWscrollfun prevScrollCallback = nullptr;
    GLFWkeyfun prevKeyCallback = nullptr;
    GLFWcharfun prevCharCallback = nullptr;
    GLFWwindowcontentscalefun prevContentScaleCallback = nullptr;
    float contentScaleX = 1.0f;
    float contentScaleY = 1.0f;
};

static GLFWcursor *CURSOR_SHAPE_MAP[CURSOR_COUNT];

static Amethyst_glfw_Data g_glfwData;

static GLFWcursor *createCursorFromMask(int size, const bool *fill)
{
    std::vector<unsigned char> pixels(size * size * 4, 0);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;
            if (fill[y * size + x]) {
                pixels[idx] = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 255;
                pixels[idx + 3] = 255;
            } else {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ny = y + dy, nx = x + dx;
                        if (ny >= 0 && ny < size && nx >= 0 && nx < size && fill[ny * size + nx]) {
                            pixels[idx] = 0;
                            pixels[idx + 1] = 0;
                            pixels[idx + 2] = 0;
                            pixels[idx + 3] = 255;
                            goto next;
                        }
                    }
                }
next:;
            }
        }
    }

    GLFWimage image;
    image.width = size;
    image.height = size;
    image.pixels = pixels.data();
    return glfwCreateCursor(&image, size / 2, size / 2);
}

static GLFWcursor *createCustomHorizontalResizeCursor()
{
    constexpr int size = 24;
    bool fill[size * size] = {};
    int cy = size / 2;

    for (int x = 5; x < size - 5; x++) {
        fill[(cy - 1) * size + x] = true;
        fill[cy * size + x] = true;
    }

    for (int i = 0; i < 6; i++) {
        int x = i;
        for (int dy = -i; dy <= i; dy++) {
            int y = cy + dy;
            if (y >= 0 && y < size) fill[y * size + x] = true;
        }
    }

    for (int i = 0; i < 6; i++) {
        int x = size - 1 - i;
        for (int dy = -i; dy <= i; dy++) {
            int y = cy + dy;
            if (y >= 0 && y < size) fill[y * size + x] = true;
        }
    }

    return createCursorFromMask(size, fill);
}

static GLFWcursor *createCustomVerticalResizeCursor()
{
    constexpr int size = 24;
    bool fill[size * size] = {};
    int cx = size / 2;

    for (int y = 5; y < size - 5; y++) {
        fill[y * size + (cx - 1)] = true;
        fill[y * size + cx] = true;
    }

    for (int i = 0; i < 6; i++) {
        int y = i;
        for (int dx = -i; dx <= i; dx++) {
            int x = cx + dx;
            if (x >= 0 && x < size) fill[y * size + x] = true;
        }
    }

    for (int i = 0; i < 6; i++) {
        int y = size - 1 - i;
        for (int dx = -i; dx <= i; dx++) {
            int x = cx + dx;
            if (x >= 0 && x < size) fill[y * size + x] = true;
        }
    }

    return createCursorFromMask(size, fill);
}

constexpr size_t INITIAL_ARENA_BYTES = 8 * 1024 * 1024;
constexpr size_t INDEX_COUNT_RECT = 6;

struct PushConstants {
    vec2 screenSize;
    uint32_t glyphOffset;
    uint32_t lineOffset;
    uint32_t sliceOffset;
};

void VkBackend::init(const VulkanInitInfo &config, const GLFWInitInfo &info)
{
    m_info = config;
    m_glfwInfo = info;

    CURSOR_SHAPE_MAP[CURSOR_ARROW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    CURSOR_SHAPE_MAP[CURSOR_HAND] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    CURSOR_SHAPE_MAP[CURSOR_IBEAM] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);

    CURSOR_SHAPE_MAP[CURSOR_CROSSHAIR] = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
    if (!CURSOR_SHAPE_MAP[CURSOR_CROSSHAIR]) {
        CURSOR_SHAPE_MAP[CURSOR_CROSSHAIR] = CURSOR_SHAPE_MAP[CURSOR_ARROW];
    }

    CURSOR_SHAPE_MAP[CURSOR_HORI_RESIZE] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    if (!CURSOR_SHAPE_MAP[CURSOR_HORI_RESIZE]) {
        CURSOR_SHAPE_MAP[CURSOR_HORI_RESIZE] = createCustomHorizontalResizeCursor();
    }

    CURSOR_SHAPE_MAP[CURSOR_VERT_RESIZE] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    if (!CURSOR_SHAPE_MAP[CURSOR_VERT_RESIZE]) {
        CURSOR_SHAPE_MAP[CURSOR_VERT_RESIZE] = createCustomVerticalResizeCursor();
    }

    setupGLFWCallbacks();
    allocateBufferArenas();
    allocateIndexBuffer();
    allocateInstanceBuffers();
    createPipeline();
    allocateDescriptorSet();

    InputInterface::onCursorShapeChanged = [](CursorShape shape) { glfwSetCursor(g_glfwData.window, CURSOR_SHAPE_MAP[shape]); };
    InputInterface::onSetClipboardText = [](const std::string &text) { glfwSetClipboardString(g_glfwData.window, text.c_str()); };
    InputInterface::onGetClipboardText = []() -> std::string {
        const char *text = glfwGetClipboardString(g_glfwData.window);
        return text ? std::string(text) : "";
    };

    GeometryRegistry::setDestroyCb([this](GeometryRegistry *reg) { freeGeometryAllocation(reg); });
}

void VkBackend::shutdown()
{
    VkDevice device = m_info.device;
    vkDeviceWaitIdle(device);

    if (m_dynamicArena.mappedMemory) {
        vkUnmapMemory(device, m_dynamicArena.memory);
    }
    if (m_dynamicArena.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_dynamicArena.buffer, nullptr);
        vkFreeMemory(device, m_dynamicArena.memory, nullptr);
    }
    if (m_glyphArena.mappedMemory) {
        vkUnmapMemory(device, m_glyphArena.memory);
    }
    if (m_glyphArena.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_glyphArena.buffer, nullptr);
        vkFreeMemory(device, m_glyphArena.memory, nullptr);
    }
    if (m_lineArena.mappedMemory) {
        vkUnmapMemory(device, m_lineArena.memory);
    }
    if (m_lineArena.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_lineArena.buffer, nullptr);
        vkFreeMemory(device, m_lineArena.memory, nullptr);
    }
    if (m_sliceArena.mappedMemory) {
        vkUnmapMemory(device, m_sliceArena.memory);
    }
    if (m_sliceArena.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_sliceArena.buffer, nullptr);
        vkFreeMemory(device, m_sliceArena.memory, nullptr);
    }
    if (m_staticArena.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_staticArena.buffer, nullptr);
        vkFreeMemory(device, m_staticArena.memory, nullptr);
    }
    // Atlas texture cleanup
    if (m_atlasSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_atlasSampler, nullptr);
    }
    if (m_atlasView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_atlasView, nullptr);
    }
    if (m_atlasImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_atlasImage, nullptr);
    }
    if (m_atlasMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_atlasMemory, nullptr);
    }
    if (m_atlasStagingMapped) {
        vkUnmapMemory(device, m_atlasStagingMemory);
    }
    if (m_atlasStagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_atlasStagingBuffer, nullptr);
        vkFreeMemory(device, m_atlasStagingMemory, nullptr);
    }
    // SVG atlas texture cleanup
    if (m_svgAtlasSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_svgAtlasSampler, nullptr);
    }
    if (m_svgAtlasView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_svgAtlasView, nullptr);
    }
    if (m_svgAtlasImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_svgAtlasImage, nullptr);
    }
    if (m_svgAtlasMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_svgAtlasMemory, nullptr);
    }
    if (m_svgAtlasStagingMapped) {
        vkUnmapMemory(device, m_svgAtlasStagingMemory);
    }
    if (m_svgAtlasStagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_svgAtlasStagingBuffer, nullptr);
        vkFreeMemory(device, m_svgAtlasStagingMemory, nullptr);
    }

    // UI pipeline cleanup
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
    }
    if (m_vertShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_vertShader, nullptr);
    }
    if (m_fragShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_fragShader, nullptr);
    }
}

void VkBackend::beginFrame() {}

void VkBackend::endFrame() {}

void VkBackend::onResize(vec2 extent)
{
    m_info.extent = VkExtent2D(extent.x, extent.y);
}

void VkBackend::record(VkCommandBuffer cmd)
{
    AM_PROFILE_FUNCTION();
    const auto &geometryRegistries = GeometryRegistry::getRegistries();

    for (auto *registry : geometryRegistries) {
        UILayer *layer = registry->getOwningLayer();
        if (!layer || !layer->isVisible()) continue;

        BufferAllocation *alloc = obtainGeometryAllocation(registry);
        if (alloc) {
            updateInstances(*alloc, *registry);
        }

        GlyphBuffer *gb = registry->getGlyphBuffer();
        if (gb != nullptr) {
            TextGpuAllocation *talloc = obtainTextAllocation(registry);
            if (talloc != nullptr) {
                updateTextBuffers(*talloc, *gb);
            }
        }
    }

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(m_info.extent.width),
        .height = static_cast<float>(m_info.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = m_info.extent,
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    PushConstants pc = {
        .screenSize = {static_cast<float>(m_info.extent.width), static_cast<float>(m_info.extent.height)},
        .glyphOffset = 0,
        .lineOffset = 0,
        .sliceOffset = 0,
    };

    bool pipelineBound = false;
    for (auto *registry : geometryRegistries) {
        UILayer *layer = registry->getOwningLayer();
        if (!layer || !layer->isVisible()) continue;

        auto it = m_geometryAllocations.find(registry);
        if (it != m_geometryAllocations.end() && it->second.size > 0) {
            if (!pipelineBound) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
                vkCmdBindIndexBuffer(cmd, m_indexBuffer.arena->buffer, m_indexBuffer.offset, VK_INDEX_TYPE_UINT32);
                pipelineBound = true;
            }

            pc.glyphOffset = 0;
            pc.lineOffset = 0;
            pc.sliceOffset = 0;
            auto tit = m_textAllocations.find(registry);
            if (tit != m_textAllocations.end()) {
                pc.glyphOffset = static_cast<uint32_t>(tit->second.glyph.offset / sizeof(GlyphQuad));
                pc.lineOffset = static_cast<uint32_t>(tit->second.line.offset / sizeof(GlyphLine));
                pc.sliceOffset = static_cast<uint32_t>(tit->second.slice.offset / sizeof(GlyphSlice));
            }
            vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);

            uint32_t firstInstance = static_cast<uint32_t>(it->second.offset / sizeof(InstanceData));
            vkCmdDrawIndexed(cmd, INDEX_COUNT_RECT, static_cast<uint32_t>(it->second.size), 0, 0, firstInstance);
        }
    }
}

static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}
static void s_createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, BufferArena &arena, VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags properties)
{
    // Buffer creation
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = static_cast<VkDeviceSize>(arena.capacity);
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &arena.buffer) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create buffer of size: {}", arena.capacity);
        return;
    }

    // Memory allocation
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, arena.buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &arena.memory) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create buffer memory");
        return;
    }

    vkBindBufferMemory(device, arena.buffer, arena.memory, 0);
}

void VkBackend::allocateBufferArenas()
{
    m_staticArena = {};
    m_staticArena.capacity = sizeof(uint32_t) * INDEX_COUNT_RECT;

    m_dynamicArena = {};
    m_dynamicArena.capacity = INITIAL_ARENA_BYTES;

    VkBufferUsageFlags staticUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    VkMemoryPropertyFlags staticProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    s_createBuffer(m_info.device, m_info.physicalDevice, m_staticArena, staticUsage, staticProperties);

    VkBufferUsageFlags dynamicUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VkMemoryPropertyFlags dynamicProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    s_createBuffer(m_info.device, m_info.physicalDevice, m_dynamicArena, dynamicUsage, dynamicProperties);

    vkMapMemory(m_info.device, m_dynamicArena.memory, 0, VK_WHOLE_SIZE, 0, &m_dynamicArena.mappedMemory);

    m_glyphArena = {};
    m_glyphArena.capacity = 4 * (GlyphBuffer::GLYPH_CAPACITY * sizeof(GlyphQuad));
    s_createBuffer(m_info.device, m_info.physicalDevice, m_glyphArena, dynamicUsage, dynamicProperties);
    vkMapMemory(m_info.device, m_glyphArena.memory, 0, VK_WHOLE_SIZE, 0, &m_glyphArena.mappedMemory);

    m_lineArena = {};
    m_lineArena.capacity = 8 * (GlyphBuffer::LINE_CAPACITY * sizeof(GlyphLine));
    s_createBuffer(m_info.device, m_info.physicalDevice, m_lineArena, dynamicUsage, dynamicProperties);
    vkMapMemory(m_info.device, m_lineArena.memory, 0, VK_WHOLE_SIZE, 0, &m_lineArena.mappedMemory);

    m_sliceArena = {};
    m_sliceArena.capacity = 8 * (GlyphBuffer::SLICE_CAPACITY * sizeof(GlyphSlice));
    s_createBuffer(m_info.device, m_info.physicalDevice, m_sliceArena, dynamicUsage, dynamicProperties);
    vkMapMemory(m_info.device, m_sliceArena.memory, 0, VK_WHOLE_SIZE, 0, &m_sliceArena.mappedMemory);
}

void VkBackend::allocateIndexBuffer()
{
    size_t neededSize = sizeof(uint32_t) * INDEX_COUNT_RECT;
    if (neededSize > (m_staticArena.capacity - m_staticArena.size)) {
        AM_LOG_ERROR("Failed to allocate index buffer {}bytes need but only {}bytes available.", neededSize,
                     (m_staticArena.capacity - m_staticArena.size));
        return;
    }

    m_indexBuffer = {};
    m_indexBuffer.arena = &m_staticArena;
    m_indexBuffer.capacity = neededSize;
    m_indexBuffer.offset = m_staticArena.size;
    m_staticArena.size += neededSize;

    constexpr uint32_t quadIndices[] = {0, 1, 2, 0, 2, 3};
    uploadToGpu(m_indexBuffer, quadIndices, sizeof(quadIndices), 0);
}

void VkBackend::allocateInstanceBuffers() {}

void VkBackend::updateInstances(BufferAllocation &alloc, GeometryRegistry &registry)
{
    AM_PROFILE_FUNCTION();
    registry.flush();
    const auto &allocations = registry.getAllocations();
    size_t requiredSize = allocations.size() * sizeof(InstanceData);
    bool fullDirty = registry.isFullDirty();
    const auto &dirtyIndices = registry.getDirtyIndices();
    bool hasChanges = fullDirty || !dirtyIndices.empty();

    if (requiredSize > alloc.capacity) {
        if (alloc.capacity > 0) {
            freeToArena(m_dynamicArenaFreeList, alloc);
        }
        AM_LOG_TRACE("Buffer needs to be resized");

        size_t newCapacity = requiredSize * 2;
        BufferAllocation newAlloc = allocateFromArena(m_dynamicArena, m_dynamicArenaFreeList, newCapacity);
        if (newAlloc.arena == nullptr) {
            AM_LOG_ERROR("Failed to reallocate geometry buffer");
            registry.clearDirtyState();
            return;
        }
        alloc = newAlloc;
        fullDirty = true;
        hasChanges = true;
    }

    if (hasChanges && !allocations.empty()) {
        auto *basePtr = static_cast<uint8_t *>(alloc.arena->mappedMemory);
        auto *instances = reinterpret_cast<InstanceData *>(basePtr + alloc.offset);

        if (fullDirty) {
            std::memcpy(instances, allocations.data(), allocations.size() * sizeof(InstanceData));
        } else {
            for (uint32_t idx : dirtyIndices) {
                instances[idx] = allocations[idx];
            }
        }

        VkMappedMemoryRange memoryRange{};
        memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        memoryRange.memory = alloc.arena->memory;
        size_t flushSize = allocations.size() * sizeof(InstanceData);
        size_t alignedOffset = (alloc.offset / 64) * 64;
        size_t alignedEnd = alignUp(alloc.offset + flushSize, 64);
        memoryRange.offset = alignedOffset;
        memoryRange.size = alignedEnd - alignedOffset;
        vkFlushMappedMemoryRanges(m_info.device, 1, &memoryRange);
    }

    registry.clearDirtyState();
    alloc.size = allocations.size();
}

void VkBackend::uploadToGpu(BufferAllocation &alloc, const void *data, size_t size, size_t offset)
{
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(m_info.device, &bufferInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_info.device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(m_info.physicalDevice, memReqs.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(m_info.device, &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(m_info.device, stagingBuffer, stagingMemory, 0);

    void *mapped;
    vkMapMemory(m_info.device, stagingMemory, 0, size, 0, &mapped);
    std::memcpy(mapped, data, size);
    vkUnmapMemory(m_info.device, stagingMemory);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = m_info.queueFamiliy;

    VkCommandPool cmdPool;
    vkCreateCommandPool(m_info.device, &poolInfo, nullptr, &cmdPool);

    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = cmdPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_info.device, &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = alloc.offset + offset;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, stagingBuffer, alloc.arena->buffer, 1, &copyRegion);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(m_info.queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_info.queue);

    vkDestroyCommandPool(m_info.device, cmdPool, nullptr);
    vkDestroyBuffer(m_info.device, stagingBuffer, nullptr);
    vkFreeMemory(m_info.device, stagingMemory, nullptr);
}

VkShaderModule VkBackend::loadShaderModule(const char *path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        AM_LOG_ERROR("Failed to open shader file: {}", path);
        return VK_NULL_HANDLE;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(buffer.data());

    VkShaderModule shaderModule;
    vkCreateShaderModule(m_info.device, &createInfo, nullptr, &shaderModule);
    return shaderModule;
}

void VkBackend::createPipeline()
{
    m_vertShader = loadShaderModule(m_info.vertexShaderPath);
    m_fragShader = loadShaderModule(m_info.fragmentShaderPath);

    VkDescriptorSetLayoutBinding bindings[5] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = MAX_BINDLESS_TEXTURES;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorBindingFlags bindingFlags[] = {
        0,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        0,
        0,
        0,
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = std::size(bindings);
    bindingFlagsInfo.pBindingFlags = bindingFlags;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = &bindingFlagsInfo;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = std::size(bindings);
    layoutInfo.pBindings = bindings;
    vkCreateDescriptorSetLayout(m_info.device, &layoutInfo, nullptr, &m_descriptorSetLayout);

    VkPushConstantRange pushConstant = {};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    vkCreatePipelineLayout(m_info.device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = m_vertShader;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = m_fragShader;
    shaderStages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment = {};
    blendAttachment.blendEnable = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = std::size(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &m_info.colorFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = std::size(shaderStages);
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;

    vkCreateGraphicsPipelines(m_info.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
}

void VkBackend::allocateDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_info.pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    vkAllocateDescriptorSets(m_info.device, &allocInfo, &m_descriptorSet);

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = m_dynamicArena.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(m_info.device, 1, &descriptorWrite, 0, nullptr);

    VkDescriptorBufferInfo textBufferInfos[3] = {};
    textBufferInfos[0].buffer = m_glyphArena.buffer;
    textBufferInfos[0].offset = 0;
    textBufferInfos[0].range = VK_WHOLE_SIZE;
    textBufferInfos[1].buffer = m_lineArena.buffer;
    textBufferInfos[1].offset = 0;
    textBufferInfos[1].range = VK_WHOLE_SIZE;
    textBufferInfos[2].buffer = m_sliceArena.buffer;
    textBufferInfos[2].offset = 0;
    textBufferInfos[2].range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet textWrites[3] = {};
    for (uint32_t i = 0; i < 3; i++) {
        textWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        textWrites[i].dstSet = m_descriptorSet;
        textWrites[i].dstBinding = 2 + i;
        textWrites[i].descriptorCount = 1;
        textWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        textWrites[i].pBufferInfo = &textBufferInfos[i];
    }
    vkUpdateDescriptorSets(m_info.device, 3, textWrites, 0, nullptr);
}

BufferAllocation VkBackend::allocateFromArena(BufferArena &arena, std::vector<FreeBlock> &freeList, size_t size)
{
    for (auto it = freeList.begin(); it != freeList.end(); ++it) {
        if (it->size >= size) {
            BufferAllocation alloc;
            alloc.arena = &arena;
            alloc.offset = it->offset;
            alloc.capacity = size;
            alloc.size = 0;

            if (it->size > size * 2) {
                it->offset += size;
                it->size -= size;
            } else {
                alloc.capacity = it->size;
                freeList.erase(it);
            }
            return alloc;
        }
    }

    if (arena.size + size > arena.capacity) {
        VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        if (!reallocBufferArena(arena, usage, properties)) {
            AM_LOG_ERROR("Arena out of memory and resize failed: size={}, capacity={}, requested={}", arena.size, arena.capacity,
                         size);
            return {};
        }

        if (arena.size + size > arena.capacity) {
            AM_LOG_ERROR("Arena still insufficient after resize: size={}, capacity={}, requested={}", arena.size, arena.capacity,
                         size);
            return {};
        }
    }

    BufferAllocation alloc;
    alloc.arena = &arena;
    alloc.offset = arena.size;
    alloc.capacity = size;
    alloc.size = 0;
    arena.size += size;
    return alloc;
}

void VkBackend::freeToArena(std::vector<FreeBlock> &freeList, const BufferAllocation &alloc)
{
    FreeBlock block;
    block.offset = alloc.offset;
    block.size = alloc.capacity;

    for (auto it = freeList.begin(); it != freeList.end(); ++it) {
        if (it->offset + it->size == block.offset) {
            it->size += block.size;
            auto next = std::next(it);
            if (next != freeList.end() && it->offset + it->size == next->offset) {
                it->size += next->size;
                freeList.erase(next);
            }
            return;
        }
        if (block.offset + block.size == it->offset) {
            it->offset = block.offset;
            it->size += block.size;
            return;
        }
    }

    freeList.push_back(block);
}

bool VkBackend::reallocBufferArena(BufferArena &arena, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    size_t newCapacity = arena.capacity * 2;
    if (newCapacity > MAX_BUFFER_ARENA_SIZE) {
        if (arena.capacity >= MAX_BUFFER_ARENA_SIZE) {
            AM_LOG_ERROR("Buffer arena already at max size (10MB), cannot resize");
            return false;
        }
        newCapacity = MAX_BUFFER_ARENA_SIZE;
    }

    AM_LOG_INFO("Resizing buffer arena from {} to {} bytes", arena.capacity, newCapacity);

    BufferArena newArena{};
    newArena.capacity = newCapacity;
    s_createBuffer(m_info.device, m_info.physicalDevice, newArena, usage, properties);

    if (newArena.buffer == VK_NULL_HANDLE) {
        AM_LOG_ERROR("Failed to create resized buffer");
        return false;
    }

    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        if (vkMapMemory(m_info.device, newArena.memory, 0, VK_WHOLE_SIZE, 0, &newArena.mappedMemory) != VK_SUCCESS) {
            AM_LOG_ERROR("Failed to map resized buffer memory");
            vkDestroyBuffer(m_info.device, newArena.buffer, nullptr);
            vkFreeMemory(m_info.device, newArena.memory, nullptr);
            return false;
        }

        if (arena.mappedMemory && arena.size > 0) {
            std::memcpy(newArena.mappedMemory, arena.mappedMemory, arena.size);
        }
    }

    newArena.size = arena.size;

    if (arena.buffer != VK_NULL_HANDLE) {
        if (arena.mappedMemory) {
            vkUnmapMemory(m_info.device, arena.memory);
        }
        vkDeviceWaitIdle(m_info.device);
        vkDestroyBuffer(m_info.device, arena.buffer, nullptr);
        vkFreeMemory(m_info.device, arena.memory, nullptr);
    }

    arena = newArena;

    uint32_t binding = 0;
    if (&arena == &m_glyphArena) {
        binding = 2;
    } else if (&arena == &m_lineArena) {
        binding = 3;
    } else if (&arena == &m_sliceArena) {
        binding = 4;
    }

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = arena.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_descriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(m_info.device, 1, &descriptorWrite, 0, nullptr);

    return true;
}

BufferAllocation *VkBackend::obtainGeometryAllocation(GeometryRegistry *registry)
{
    auto it = m_geometryAllocations.find(registry);
    if (it != m_geometryAllocations.end()) {
        return &it->second;
    }

    size_t initialSize = sizeof(InstanceData) * 256;
    BufferAllocation alloc = allocateFromArena(m_dynamicArena, m_dynamicArenaFreeList, initialSize);
    if (alloc.arena == nullptr) {
        return nullptr;
    }

    auto [inserted, _] = m_geometryAllocations.emplace(registry, alloc);
    return &inserted->second;
}

void VkBackend::freeGeometryAllocation(GeometryRegistry *registry)
{
    auto it = m_geometryAllocations.find(registry);
    if (it != m_geometryAllocations.end()) {
        freeToArena(m_dynamicArenaFreeList, it->second);
        m_geometryAllocations.erase(it);
    }
    freeTextAllocation(registry);
}

TextGpuAllocation *VkBackend::obtainTextAllocation(GeometryRegistry *registry)
{
    auto it = m_textAllocations.find(registry);
    if (it != m_textAllocations.end()) {
        return &it->second;
    }

    const size_t glyphBytes = GlyphBuffer::GLYPH_CAPACITY * sizeof(GlyphQuad);
    const size_t lineBytes = GlyphBuffer::LINE_CAPACITY * sizeof(GlyphLine);
    const size_t sliceBytes = GlyphBuffer::SLICE_CAPACITY * sizeof(GlyphSlice);

    TextGpuAllocation result;
    result.glyph = allocateFromArena(m_glyphArena, m_glyphArenaFreeList, glyphBytes);
    result.line = allocateFromArena(m_lineArena, m_lineArenaFreeList, lineBytes);
    result.slice = allocateFromArena(m_sliceArena, m_sliceArenaFreeList, sliceBytes);

    if (result.glyph.arena == nullptr || result.line.arena == nullptr || result.slice.arena == nullptr) {
        if (result.glyph.arena != nullptr) {
            freeToArena(m_glyphArenaFreeList, result.glyph);
        }
        if (result.line.arena != nullptr) {
            freeToArena(m_lineArenaFreeList, result.line);
        }
        if (result.slice.arena != nullptr) {
            freeToArena(m_sliceArenaFreeList, result.slice);
        }
        AM_LOG_ERROR("Failed to allocate text GPU buffers for registry");
        return nullptr;
    }

    auto [inserted, _] = m_textAllocations.emplace(registry, result);
    return &inserted->second;
}

void VkBackend::updateTextBuffers(TextGpuAllocation &alloc, GlyphBuffer &glyphBuffer)
{
    struct UploadSlot {
        BufferAllocation *target;
        const uint8_t *cpuData;
        size_t elemSize;
        DirtyRange dirty;
    };

    UploadSlot slots[3] = {
        {&alloc.glyph, reinterpret_cast<const uint8_t *>(glyphBuffer.glyphData()), sizeof(GlyphQuad), glyphBuffer.glyphDirty()},
        {&alloc.line, reinterpret_cast<const uint8_t *>(glyphBuffer.lineData()), sizeof(GlyphLine), glyphBuffer.lineDirty()},
        {&alloc.slice, reinterpret_cast<const uint8_t *>(glyphBuffer.sliceData()), sizeof(GlyphSlice), glyphBuffer.sliceDirty()},
    };

    for (UploadSlot &slot : slots) {
        if (slot.dirty.empty()) {
            continue;
        }

        size_t loBytes = static_cast<size_t>(slot.dirty.lo) * slot.elemSize;
        size_t hiBytes = static_cast<size_t>(slot.dirty.hi) * slot.elemSize;
        size_t rangeBytes = hiBytes - loBytes;

        auto *basePtr = static_cast<uint8_t *>(slot.target->arena->mappedMemory);
        std::memcpy(basePtr + slot.target->offset + loBytes, slot.cpuData + loBytes, rangeBytes);

        VkMappedMemoryRange memoryRange{};
        memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        memoryRange.memory = slot.target->arena->memory;
        size_t alignedOffset = ((slot.target->offset + loBytes) / 64) * 64;
        size_t alignedEnd = alignUp(slot.target->offset + hiBytes, 64);
        memoryRange.offset = alignedOffset;
        memoryRange.size = alignedEnd - alignedOffset;
        vkFlushMappedMemoryRanges(m_info.device, 1, &memoryRange);

        slot.target->size = hiBytes;
    }

    glyphBuffer.clearDirty();
}

void VkBackend::freeTextAllocation(GeometryRegistry *registry)
{
    auto it = m_textAllocations.find(registry);
    if (it != m_textAllocations.end()) {
        freeToArena(m_glyphArenaFreeList, it->second.glyph);
        freeToArena(m_lineArenaFreeList, it->second.line);
        freeToArena(m_sliceArenaFreeList, it->second.slice);
        m_textAllocations.erase(it);
    }
}

static int s_translateModifiers(int glfwMods)
{
    int result = 0;
    if (glfwMods & GLFW_MOD_SHIFT) {
        result |= MOD_SHIFT;
    }
    if (glfwMods & GLFW_MOD_CONTROL) {
        result |= MOD_CONTROL;
    }
    if (glfwMods & GLFW_MOD_ALT) {
        result |= MOD_ALT;
    }
    if (glfwMods & GLFW_MOD_SUPER) {
        result |= MOD_SUPER;
    }
    return result;
}

void VkBackend::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    if (g_glfwData.prevMouseButtonCallback) {
        g_glfwData.prevMouseButtonCallback(window, button, action, mods);
    }
    InputInterface::onMouseButton(button, action, s_translateModifiers(mods));
}

void VkBackend::cursorPosCallback(GLFWwindow *window, double x, double y)
{
    if (g_glfwData.prevCursorPosCallback) {
        g_glfwData.prevCursorPosCallback(window, x, y);
    }
    uint32_t scaledX = static_cast<uint32_t>(x * g_glfwData.contentScaleX);
    uint32_t scaledY = static_cast<uint32_t>(y * g_glfwData.contentScaleY);
    InputInterface::setMousePosition(scaledX, scaledY);
}

void VkBackend::scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    if (g_glfwData.prevScrollCallback) {
        g_glfwData.prevScrollCallback(window, xoffset, yoffset);
    }
    InputInterface::onMouseScroll(static_cast<float>(xoffset), static_cast<float>(yoffset));
}

void VkBackend::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (g_glfwData.prevKeyCallback) {
        g_glfwData.prevKeyCallback(window, key, scancode, action, mods);
    }
    InputInterface::onKey(key, scancode, action, s_translateModifiers(mods));
}

void VkBackend::charCallback(GLFWwindow *window, unsigned int codepoint)
{
    if (g_glfwData.prevCharCallback) {
        g_glfwData.prevCharCallback(window, codepoint);
    }
    InputInterface::onChar(codepoint);
}

void VkBackend::contentScaleCallback(GLFWwindow *window, float xscale, float yscale)
{
    if (g_glfwData.prevContentScaleCallback) {
        g_glfwData.prevContentScaleCallback(window, xscale, yscale);
    }
    g_glfwData.contentScaleX = xscale;
    g_glfwData.contentScaleY = yscale;
}

void VkBackend::setupGLFWCallbacks()
{
    GLFWwindow *window = static_cast<GLFWwindow *>(m_glfwInfo.window);
    g_glfwData.window = window;

    glfwGetWindowContentScale(window, &g_glfwData.contentScaleX, &g_glfwData.contentScaleY);

    g_glfwData.prevMouseButtonCallback = glfwSetMouseButtonCallback(window, mouseButtonCallback);
    g_glfwData.prevCursorPosCallback = glfwSetCursorPosCallback(window, cursorPosCallback);
    g_glfwData.prevScrollCallback = glfwSetScrollCallback(window, scrollCallback);
    g_glfwData.prevKeyCallback = glfwSetKeyCallback(window, keyCallback);
    g_glfwData.prevCharCallback = glfwSetCharCallback(window, charCallback);
    g_glfwData.prevContentScaleCallback = glfwSetWindowContentScaleCallback(window, contentScaleCallback);
}

AmTextureId VkBackend::registerTexture(VkImageView imageView, VkSampler sampler)
{
    uint32_t slot;
    if (!m_textureFreeList.empty()) {
        slot = m_textureFreeList.back();
        m_textureFreeList.pop_back();
    } else {
        if (m_nextTextureSlot >= MAX_BINDLESS_TEXTURES) {
            AM_LOG_ERROR("Exceeded maximum bindless textures ({})", MAX_BINDLESS_TEXTURES);
            return AM_INVALID_TEXTURE;
        }
        slot = m_nextTextureSlot++;
    }

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.sampler = sampler;
    imageInfo.imageView = imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 1;
    write.dstArrayElement = slot;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(m_info.device, 1, &write, 0, nullptr);

    return AmTextureId{slot};
}

void VkBackend::unregisterTexture(AmTextureId id)
{
    if (!id.isValid()) {
        return;
    }
    m_textureFreeList.push_back(id.id);
}

void VkBackend::createAtlasTexture(uint32_t width, uint32_t height)
{
    VkDevice device = m_info.device;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8_UNORM;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_atlasImage) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create atlas image");
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_atlasImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(m_info.physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_atlasMemory) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to allocate atlas memory");
        return;
    }

    vkBindImageMemory(device, m_atlasImage, m_atlasMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_atlasImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_atlasView) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create atlas image view");
        return;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_atlasSampler) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create atlas sampler");
        return;
    }

    size_t stagingSize = width * height;
    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = stagingSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &stagingBufferInfo, nullptr, &m_atlasStagingBuffer) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create atlas staging buffer");
        return;
    }

    VkMemoryRequirements stagingMemReqs;
    vkGetBufferMemoryRequirements(device, m_atlasStagingBuffer, &stagingMemReqs);

    VkMemoryAllocateInfo stagingAllocInfo{};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = stagingMemReqs.size;
    stagingAllocInfo.memoryTypeIndex = findMemoryType(m_info.physicalDevice, stagingMemReqs.memoryTypeBits,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &stagingAllocInfo, nullptr, &m_atlasStagingMemory) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to allocate atlas staging memory");
        return;
    }

    vkBindBufferMemory(device, m_atlasStagingBuffer, m_atlasStagingMemory, 0);
    vkMapMemory(device, m_atlasStagingMemory, 0, stagingSize, 0, &m_atlasStagingMapped);

    m_atlasWidth = width;
    m_atlasHeight = height;
    m_atlasTextureId = registerTexture(m_atlasView, m_atlasSampler);
}

void VkBackend::uploadAtlasData(void *cmdBuffer, const uint8_t *pixels, uint32_t width, uint32_t height)
{
    // TODO: This is inefficient - it uploads the entire atlas every time.
    // A better approach would be to track dirty regions and only upload changed areas.
    // For now, this works but should be optimized later.

    VkCommandBuffer cmd = static_cast<VkCommandBuffer>(cmdBuffer);

    if (!m_atlasStagingMapped) {
        return;
    }

    size_t imageSize = width * height;
    std::memcpy(m_atlasStagingMapped, pixels, imageSize);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_atlasImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, m_atlasStagingBuffer, m_atlasImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

void VkBackend::createSvgAtlasTexture(uint32_t width, uint32_t height)
{
    VkDevice device = m_info.device;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_svgAtlasImage) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create SVG atlas image");
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_svgAtlasImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(m_info.physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_svgAtlasMemory) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to allocate SVG atlas memory");
        return;
    }

    vkBindImageMemory(device, m_svgAtlasImage, m_svgAtlasMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_svgAtlasImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_svgAtlasView) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create SVG atlas image view");
        return;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_svgAtlasSampler) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create SVG atlas sampler");
        return;
    }

    size_t stagingSize = width * height * 4;
    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = stagingSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &stagingBufferInfo, nullptr, &m_svgAtlasStagingBuffer) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create SVG atlas staging buffer");
        return;
    }

    VkMemoryRequirements stagingMemReqs;
    vkGetBufferMemoryRequirements(device, m_svgAtlasStagingBuffer, &stagingMemReqs);

    VkMemoryAllocateInfo stagingAllocInfo{};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = stagingMemReqs.size;
    stagingAllocInfo.memoryTypeIndex = findMemoryType(m_info.physicalDevice, stagingMemReqs.memoryTypeBits,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &stagingAllocInfo, nullptr, &m_svgAtlasStagingMemory) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to allocate SVG atlas staging memory");
        return;
    }

    vkBindBufferMemory(device, m_svgAtlasStagingBuffer, m_svgAtlasStagingMemory, 0);
    vkMapMemory(device, m_svgAtlasStagingMemory, 0, stagingSize, 0, &m_svgAtlasStagingMapped);

    m_svgAtlasWidth = width;
    m_svgAtlasHeight = height;
    m_svgAtlasTextureId = registerTexture(m_svgAtlasView, m_svgAtlasSampler);
}

void VkBackend::uploadSvgAtlasData(void *cmdBuffer, const uint8_t *pixels, uint32_t width, uint32_t height)
{
    VkCommandBuffer cmd = static_cast<VkCommandBuffer>(cmdBuffer);

    if (!m_svgAtlasStagingMapped) {
        return;
    }

    size_t imageSize = width * height * 4;
    std::memcpy(m_svgAtlasStagingMapped, pixels, imageSize);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_svgAtlasImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, m_svgAtlasStagingBuffer, m_svgAtlasImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

} // namespace Amethyst
