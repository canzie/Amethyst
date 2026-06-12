#include "amethyst__vk13_glfw.h"

#include "components/input_interface.h"
#include "logging/log.h"
#include "utils/am_assert.h"
#include "utils/profiling.h"

#include <GLFW/glfw3.h>

#include "math/math.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
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

static GLFWcursor *s_createCustomDiagonalResizeCursor(bool nesw)
{
    constexpr int size = 24;
    bool fill[size * size] = {};

    for (int i = 6; i < size - 7; i++) {
        fill[i * size + i] = true;
        fill[i * size + (i + 1)] = true;
        fill[(i + 1) * size + i] = true;
    }

    for (int y = 2; y <= 8; y++) {
        for (int x = 2; x <= 8; x++) {
            if (x + y <= 10) {
                fill[y * size + x] = true;
            }
        }
    }

    for (int y = size - 9; y <= size - 3; y++) {
        for (int x = size - 9; x <= size - 3; x++) {
            if (x + y >= 2 * size - 12) {
                fill[y * size + x] = true;
            }
        }
    }

    if (nesw) {
        bool mirrored[size * size] = {};
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                mirrored[y * size + (size - 1 - x)] = fill[y * size + x];
            }
        }
        return createCursorFromMask(size, mirrored);
    }

    return createCursorFromMask(size, fill);
}

constexpr size_t INDEX_COUNT_RECT = 6;

struct PushConstants {
    vec2 screenSize;
    uint32_t glyphOffset;
    uint32_t lineOffset;
    uint32_t sliceOffset;
};

void AmVulkanBackend::init(const AmVulkanInitInfo &config, const AmGlfwInitInfo &info)
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

    CURSOR_SHAPE_MAP[CURSOR_NWSE_RESIZE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
    if (CURSOR_SHAPE_MAP[CURSOR_NWSE_RESIZE] == nullptr) {
        CURSOR_SHAPE_MAP[CURSOR_NWSE_RESIZE] = s_createCustomDiagonalResizeCursor(false);
    }

    CURSOR_SHAPE_MAP[CURSOR_NESW_RESIZE] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
    if (CURSOR_SHAPE_MAP[CURSOR_NESW_RESIZE] == nullptr) {
        CURSOR_SHAPE_MAP[CURSOR_NESW_RESIZE] = s_createCustomDiagonalResizeCursor(true);
    }

    CURSOR_SHAPE_MAP[CURSOR_ALL_RESIZE] = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
    if (CURSOR_SHAPE_MAP[CURSOR_ALL_RESIZE] == nullptr) {
        CURSOR_SHAPE_MAP[CURSOR_ALL_RESIZE] = CURSOR_SHAPE_MAP[CURSOR_ARROW];
    }

    CURSOR_SHAPE_MAP[CURSOR_NOT_ALLOWED] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
    if (CURSOR_SHAPE_MAP[CURSOR_NOT_ALLOWED] == nullptr) {
        CURSOR_SHAPE_MAP[CURSOR_NOT_ALLOWED] = CURSOR_SHAPE_MAP[CURSOR_ARROW];
    }

    setupGLFWCallbacks();
    createPipeline();
    allocateDescriptorSet();

    InputInterface::onCursorShapeChanged = [](CursorShape shape) { glfwSetCursor(g_glfwData.window, CURSOR_SHAPE_MAP[shape]); };
    InputInterface::onSetClipboardText = [](const std::string &text) { glfwSetClipboardString(g_glfwData.window, text.c_str()); };
    InputInterface::onGetClipboardText = []() -> std::string {
        const char *text = glfwGetClipboardString(g_glfwData.window);
        return text ? std::string(text) : "";
    };
}

void AmVulkanBackend::shutdown()
{
    VkDevice device = m_info.device;
    vkDeviceWaitIdle(device);

    for (BufferRecord &record : m_buffers) {
        if (record.alive) {
            destroyBufferRecord(record);
        }
    }
    m_buffers.clear();
    m_bufferFreeSlots.clear();

    for (auto &[slot, record] : m_textures) {
        destroyTextureRecord(record);
    }
    m_textures.clear();

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

void AmVulkanBackend::beginFrame() {}

void AmVulkanBackend::endFrame() {}

void AmVulkanBackend::onResize(vec2 extent)
{
    m_info.extent = VkExtent2D(extent.x, extent.y);
}

void AmVulkanBackend::record(VkCommandBuffer cmd, const FrameDrawList &drawList)
{
    AM_PROFILE_FUNCTION();

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

    if (drawList.entries.empty() || !drawList.indexBuffer.isValid()) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    vkCmdBindIndexBuffer(cmd, m_buffers[drawList.indexBuffer.id].buffer, 0, VK_INDEX_TYPE_UINT32);

    PushConstants pc = {
        .screenSize = {static_cast<float>(m_info.extent.width), static_cast<float>(m_info.extent.height)},
        .glyphOffset = 0,
        .lineOffset = 0,
        .sliceOffset = 0,
    };

    for (const FrameDrawEntry &entry : drawList.entries) {
        pc.glyphOffset = entry.glyphBase;
        pc.lineOffset = entry.lineBase;
        pc.sliceOffset = entry.sliceBase;
        vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
        vkCmdDrawIndexed(cmd, INDEX_COUNT_RECT, entry.instanceCount, 0, 0, entry.firstInstance);
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
static bool s_createDeviceBuffer(VkDevice device, VkPhysicalDevice physicalDevice, size_t capacity, VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &memory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = static_cast<VkDeviceSize>(capacity);
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create buffer of size: {}", capacity);
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create buffer memory");
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(device, buffer, memory, 0);
    return true;
}

static VkBufferUsageFlags s_translateBufferUsage(AmBufferUsage usage, AmBufferMemory memory)
{
    VkBufferUsageFlags flags = 0;
    if (usage == AmBufferUsage::INDEX) {
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    } else {
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if (memory == AmBufferMemory::DEVICE_LOCAL) {
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    return flags;
}

AmBufferId AmVulkanBackend::createBuffer(const AmBufferDesc &desc)
{
    BufferRecord record;
    record.desc = desc;
    record.capacity = desc.initialCapacity;

    VkBufferUsageFlags usage = s_translateBufferUsage(desc.usage, desc.memory);
    VkMemoryPropertyFlags properties =
        (desc.memory == AmBufferMemory::DEVICE_LOCAL) ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    if (!s_createDeviceBuffer(m_info.device, m_info.physicalDevice, record.capacity, usage, properties, record.buffer,
                              record.memory)) {
        return {};
    }

    if (desc.memory == AmBufferMemory::HOST_VISIBLE) {
        if (vkMapMemory(m_info.device, record.memory, 0, VK_WHOLE_SIZE, 0, &record.mapped) != VK_SUCCESS) {
            AM_LOG_ERROR("Failed to map buffer memory");
            vkDestroyBuffer(m_info.device, record.buffer, nullptr);
            vkFreeMemory(m_info.device, record.memory, nullptr);
            return {};
        }
    }
    record.alive = true;

    uint32_t slot;
    if (!m_bufferFreeSlots.empty()) {
        slot = m_bufferFreeSlots.back();
        m_bufferFreeSlots.pop_back();
        m_buffers[slot] = record;
    } else {
        slot = static_cast<uint32_t>(m_buffers.size());
        m_buffers.push_back(record);
    }

    if (desc.shaderBinding != UINT32_MAX) {
        writeBufferDescriptor(record.buffer, desc.shaderBinding);
    }

    return AmBufferId{slot};
}

bool AmVulkanBackend::growBuffer(AmBufferId id, size_t newCapacity)
{
    if (!id.isValid() || id.id >= m_buffers.size() || !m_buffers[id.id].alive) {
        return false;
    }

    BufferRecord &record = m_buffers[id.id];
    if (record.desc.memory == AmBufferMemory::DEVICE_LOCAL) {
        AM_LOG_ERROR("growBuffer is not supported for DEVICE_LOCAL buffers");
        return false;
    }
    if (newCapacity <= record.capacity) {
        return true;
    }

    AM_LOG_INFO("Resizing buffer from {} to {} bytes", record.capacity, newCapacity);

    VkBufferUsageFlags usage = s_translateBufferUsage(record.desc.usage, record.desc.memory);
    VkBuffer newBuffer = VK_NULL_HANDLE;
    VkDeviceMemory newMemory = VK_NULL_HANDLE;
    if (!s_createDeviceBuffer(m_info.device, m_info.physicalDevice, newCapacity, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                              newBuffer, newMemory)) {
        return false;
    }

    void *newMapped = nullptr;
    if (vkMapMemory(m_info.device, newMemory, 0, VK_WHOLE_SIZE, 0, &newMapped) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to map resized buffer memory");
        vkDestroyBuffer(m_info.device, newBuffer, nullptr);
        vkFreeMemory(m_info.device, newMemory, nullptr);
        return false;
    }

    if (record.mapped != nullptr && record.capacity > 0) {
        std::memcpy(newMapped, record.mapped, record.capacity);

        VkMappedMemoryRange memoryRange{};
        memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        memoryRange.memory = newMemory;
        memoryRange.offset = 0;
        memoryRange.size = VK_WHOLE_SIZE;
        vkFlushMappedMemoryRanges(m_info.device, 1, &memoryRange);
    }

    vkDeviceWaitIdle(m_info.device);
    if (record.mapped != nullptr) {
        vkUnmapMemory(m_info.device, record.memory);
    }
    vkDestroyBuffer(m_info.device, record.buffer, nullptr);
    vkFreeMemory(m_info.device, record.memory, nullptr);

    record.buffer = newBuffer;
    record.memory = newMemory;
    record.mapped = newMapped;
    record.capacity = newCapacity;

    if (record.desc.shaderBinding != UINT32_MAX) {
        writeBufferDescriptor(record.buffer, record.desc.shaderBinding);
    }

    return true;
}

void AmVulkanBackend::uploadBufferRange(void *cmdBuffer, AmBufferId id, const void *data, size_t offsetBytes, size_t sizeBytes)
{
    (void)cmdBuffer;
    if (!id.isValid() || id.id >= m_buffers.size() || !m_buffers[id.id].alive || sizeBytes == 0) {
        return;
    }

    BufferRecord &record = m_buffers[id.id];
    if (record.desc.memory == AmBufferMemory::DEVICE_LOCAL) {
        uploadDeviceLocal(record.buffer, data, offsetBytes, sizeBytes);
        return;
    }

    std::memcpy(static_cast<uint8_t *>(record.mapped) + offsetBytes, data, sizeBytes);

    VkMappedMemoryRange memoryRange{};
    memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    memoryRange.memory = record.memory;
    size_t alignedOffset = (offsetBytes / 64) * 64;
    size_t alignedEnd = alignUp(offsetBytes + sizeBytes, 64);
    memoryRange.offset = alignedOffset;
    memoryRange.size = alignedEnd - alignedOffset;
    vkFlushMappedMemoryRanges(m_info.device, 1, &memoryRange);
}

void AmVulkanBackend::destroyBuffer(AmBufferId id)
{
    if (!id.isValid() || id.id >= m_buffers.size() || !m_buffers[id.id].alive) {
        return;
    }

    vkDeviceWaitIdle(m_info.device);
    destroyBufferRecord(m_buffers[id.id]);
    m_bufferFreeSlots.push_back(id.id);
}

void AmVulkanBackend::destroyBufferRecord(BufferRecord &record)
{
    if (record.mapped != nullptr) {
        vkUnmapMemory(m_info.device, record.memory);
    }
    if (record.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_info.device, record.buffer, nullptr);
        vkFreeMemory(m_info.device, record.memory, nullptr);
    }
    record = {};
}

void AmVulkanBackend::writeBufferDescriptor(VkBuffer buffer, uint32_t binding)
{
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = buffer;
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
}

void AmVulkanBackend::uploadDeviceLocal(VkBuffer dst, const void *data, size_t offsetBytes, size_t sizeBytes)
{
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeBytes;
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
    vkMapMemory(m_info.device, stagingMemory, 0, sizeBytes, 0, &mapped);
    std::memcpy(mapped, data, sizeBytes);
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
    copyRegion.dstOffset = offsetBytes;
    copyRegion.size = sizeBytes;
    vkCmdCopyBuffer(cmd, stagingBuffer, dst, 1, &copyRegion);

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

VkShaderModule AmVulkanBackend::loadShaderModule(const char *path)
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

void AmVulkanBackend::createPipeline()
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
        0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, 0, 0, 0,
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

void AmVulkanBackend::allocateDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_info.pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    vkAllocateDescriptorSets(m_info.device, &allocInfo, &m_descriptorSet);

    // Buffer descriptors are written by createBuffer/growBuffer once core creates the buffers.
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

void AmVulkanBackend::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    if (g_glfwData.prevMouseButtonCallback) {
        g_glfwData.prevMouseButtonCallback(window, button, action, mods);
    }
    InputInterface::onMouseButton(button, action, s_translateModifiers(mods));
}

void AmVulkanBackend::cursorPosCallback(GLFWwindow *window, double x, double y)
{
    if (g_glfwData.prevCursorPosCallback) {
        g_glfwData.prevCursorPosCallback(window, x, y);
    }
    uint32_t scaledX = static_cast<uint32_t>(x * g_glfwData.contentScaleX);
    uint32_t scaledY = static_cast<uint32_t>(y * g_glfwData.contentScaleY);
    InputInterface::setMousePosition(scaledX, scaledY);
}

void AmVulkanBackend::scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    if (g_glfwData.prevScrollCallback) {
        g_glfwData.prevScrollCallback(window, xoffset, yoffset);
    }
    InputInterface::onMouseScroll(static_cast<float>(xoffset), static_cast<float>(yoffset));
}

void AmVulkanBackend::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (g_glfwData.prevKeyCallback) {
        g_glfwData.prevKeyCallback(window, key, scancode, action, mods);
    }
    InputInterface::onKey(key, scancode, action, s_translateModifiers(mods));
}

void AmVulkanBackend::charCallback(GLFWwindow *window, unsigned int codepoint)
{
    if (g_glfwData.prevCharCallback) {
        g_glfwData.prevCharCallback(window, codepoint);
    }
    InputInterface::onChar(codepoint);
}

void AmVulkanBackend::contentScaleCallback(GLFWwindow *window, float xscale, float yscale)
{
    if (g_glfwData.prevContentScaleCallback) {
        g_glfwData.prevContentScaleCallback(window, xscale, yscale);
    }
    g_glfwData.contentScaleX = xscale;
    g_glfwData.contentScaleY = yscale;
}

void AmVulkanBackend::setupGLFWCallbacks()
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

AmTextureId AmVulkanBackend::registerTexture(VkImageView imageView, VkSampler sampler)
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

void AmVulkanBackend::unregisterTexture(AmTextureId id)
{
    if (!id.isValid()) {
        return;
    }
    m_textureFreeList.push_back(id.id);
}

static size_t s_bytesPerPixel(AmTextureFormat format)
{
    switch (format) {
    case AmTextureFormat::R8:
        return 1;
    case AmTextureFormat::RGBA8:
        return 4;
    }
    AM_UNREACHABLE();
}

static VkFormat s_translateTextureFormat(AmTextureFormat format)
{
    switch (format) {
    case AmTextureFormat::R8:
        return VK_FORMAT_R8_UNORM;
    case AmTextureFormat::RGBA8:
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
    AM_UNREACHABLE();
}

AmTextureId AmVulkanBackend::createTexture(const AmTextureDesc &desc)
{
    VkDevice device = m_info.device;
    VkFormat format = s_translateTextureFormat(desc.format);

    TextureRecord record;
    record.desc = desc;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent.width = desc.width;
    imageInfo.extent.height = desc.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &record.image) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create texture image");
        return AM_INVALID_TEXTURE;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, record.image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(m_info.physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &record.memory) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to allocate texture memory");
        destroyTextureRecord(record);
        return AM_INVALID_TEXTURE;
    }

    vkBindImageMemory(device, record.image, record.memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = record.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &record.view) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create texture image view");
        destroyTextureRecord(record);
        return AM_INVALID_TEXTURE;
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

    if (vkCreateSampler(device, &samplerInfo, nullptr, &record.sampler) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create texture sampler");
        destroyTextureRecord(record);
        return AM_INVALID_TEXTURE;
    }

    size_t stagingSize = desc.width * desc.height * s_bytesPerPixel(desc.format);
    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = stagingSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &stagingBufferInfo, nullptr, &record.stagingBuffer) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to create texture staging buffer");
        destroyTextureRecord(record);
        return AM_INVALID_TEXTURE;
    }

    VkMemoryRequirements stagingMemReqs;
    vkGetBufferMemoryRequirements(device, record.stagingBuffer, &stagingMemReqs);

    VkMemoryAllocateInfo stagingAllocInfo{};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = stagingMemReqs.size;
    stagingAllocInfo.memoryTypeIndex = findMemoryType(m_info.physicalDevice, stagingMemReqs.memoryTypeBits,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &stagingAllocInfo, nullptr, &record.stagingMemory) != VK_SUCCESS) {
        AM_LOG_ERROR("Failed to allocate texture staging memory");
        destroyTextureRecord(record);
        return AM_INVALID_TEXTURE;
    }

    vkBindBufferMemory(device, record.stagingBuffer, record.stagingMemory, 0);
    vkMapMemory(device, record.stagingMemory, 0, stagingSize, 0, &record.stagingMapped);

    AmTextureId id = registerTexture(record.view, record.sampler);
    if (!id.isValid()) {
        destroyTextureRecord(record);
        return AM_INVALID_TEXTURE;
    }

    m_textures.emplace(id.id, record);
    return id;
}

void AmVulkanBackend::uploadTexture(void *cmdBuffer, AmTextureId id, const uint8_t *pixels)
{
    // TODO: This is inefficient - it uploads the entire texture every time.
    // A better approach would be to track dirty regions and only upload changed areas.
    // For now, this works but should be optimized later.

    VkCommandBuffer cmd = static_cast<VkCommandBuffer>(cmdBuffer);

    auto it = m_textures.find(id.id);
    if (it == m_textures.end()) {
        return;
    }
    TextureRecord &record = it->second;
    if (record.stagingMapped == nullptr) {
        return;
    }

    size_t imageSize = record.desc.width * record.desc.height * s_bytesPerPixel(record.desc.format);
    std::memcpy(record.stagingMapped, pixels, imageSize);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = record.image;
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
    region.imageExtent = {record.desc.width, record.desc.height, 1};

    vkCmdCopyBufferToImage(cmd, record.stagingBuffer, record.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);
}

void AmVulkanBackend::destroyTexture(AmTextureId id)
{
    auto it = m_textures.find(id.id);
    if (it == m_textures.end()) {
        return;
    }

    vkDeviceWaitIdle(m_info.device);
    unregisterTexture(id);
    destroyTextureRecord(it->second);
    m_textures.erase(it);
}

void AmVulkanBackend::destroyTextureRecord(TextureRecord &record)
{
    VkDevice device = m_info.device;

    if (record.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, record.sampler, nullptr);
    }
    if (record.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, record.view, nullptr);
    }
    if (record.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, record.image, nullptr);
    }
    if (record.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, record.memory, nullptr);
    }
    if (record.stagingMapped != nullptr) {
        vkUnmapMemory(device, record.stagingMemory);
    }
    if (record.stagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, record.stagingBuffer, nullptr);
        vkFreeMemory(device, record.stagingMemory, nullptr);
    }
    record = {};
}

} // namespace Amethyst
