#include "amethyst__vk13_glfw.h"

#include "components/input_interface.h"
#include "logging/log.h"

#include <GLFW/glfw3.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <iterator>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Amethyst {

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

static GLFWcursor *createCustomHorizontalResizeCursor()
{
    const int width = 16;
    const int height = 16;
    unsigned char pixels[width * height * 4];
    memset(pixels, 0, sizeof(pixels));

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            if ((x == 0 || x == width - 1) && y >= 6 && y <= 9) {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 255;
                pixels[idx + 3] = 255;
            } else if (y == 7 || y == 8) {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 255;
                pixels[idx + 3] = 255;
            }
        }
    }

    GLFWimage image;
    image.width = width;
    image.height = height;
    image.pixels = pixels;

    return glfwCreateCursor(&image, width / 2, height / 2);
}

static GLFWcursor *createCustomVerticalResizeCursor()
{
    const int width = 16;
    const int height = 16;
    unsigned char pixels[width * height * 4];
    memset(pixels, 0, sizeof(pixels));

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            if ((y == 0 || y == height - 1) && x >= 6 && x <= 9) {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 255;
                pixels[idx + 3] = 255;
            } else if (x == 7 || x == 8) {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 255;
                pixels[idx + 3] = 255;
            }
        }
    }

    GLFWimage image;
    image.width = width;
    image.height = height;
    image.pixels = pixels;

    return glfwCreateCursor(&image, width / 2, height / 2);
}

constexpr size_t INITIAL_INSTANCE_CAPACITY = 64;
constexpr size_t INITIAL_CHARACTER_CAPACITY = 1024;
constexpr size_t INITIAL_FONT_DATA_CAPACITY = 512 * 1024; // 512KB for font data
constexpr size_t INDEX_COUNT_RECT = 6;

struct PushConstants {
    glm::vec2 screenSize;
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
    createTextPipeline();
    allocateTextDescriptorSet();

    InputInterface::onCursorShapeChanged = [](CursorShape shape) { glfwSetCursor(g_glfwData.window, CURSOR_SHAPE_MAP[shape]); };
    InputInterface::onSetClipboardText = [](const std::string &text) { glfwSetClipboardString(g_glfwData.window, text.c_str()); };
    InputInterface::onGetClipboardText = []() -> std::string {
        const char *text = glfwGetClipboardString(g_glfwData.window);
        return text ? std::string(text) : "";
    };
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
    if (m_staticArena.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_staticArena.buffer, nullptr);
        vkFreeMemory(device, m_staticArena.memory, nullptr);
    }
    if (m_streamArena.mappedMemory) {
        vkUnmapMemory(device, m_streamArena.memory);
    }
    if (m_streamArena.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_streamArena.buffer, nullptr);
        vkFreeMemory(device, m_streamArena.memory, nullptr);
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

    // Text pipeline cleanup
    if (m_textPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_textPipeline, nullptr);
    }
    if (m_textPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_textPipelineLayout, nullptr);
    }
    if (m_textDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_textDescriptorSetLayout, nullptr);
    }
    if (m_textVertShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_textVertShader, nullptr);
    }
    if (m_textFragShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_textFragShader, nullptr);
    }
}

void VkBackend::beginFrame() {}

void VkBackend::endFrame() {}

void VkBackend::onResize(glm::vec2 extent)
{
    m_info.extent = VkExtent2D(extent.x, extent.y);
}

void VkBackend::record(VkCommandBuffer cmd, GeometryRegistry &geometryRegistry, TextRegistry &textRegistry)
{
    updateInstances(geometryRegistry);
    updateTextCharacters(textRegistry);

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
    };

    // Draw UI geometry
    if (m_instanceDataBuffer.size > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
        vkCmdBindIndexBuffer(cmd, m_indexBuffer.arena->buffer, m_indexBuffer.offset, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, INDEX_COUNT_RECT, static_cast<uint32_t>(m_instanceDataBuffer.size), 0, 0, 0);
    }

    // Draw text
    if (m_characterBuffer.size > 0 && m_fontDataUploaded) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_textPipeline);
        vkCmdPushConstants(cmd, m_textPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_textPipelineLayout, 0, 1, &m_textDescriptorSet, 0, nullptr);
        vkCmdBindIndexBuffer(cmd, m_indexBuffer.arena->buffer, m_indexBuffer.offset, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, INDEX_COUNT_RECT, static_cast<uint32_t>(m_characterBuffer.size), 0, 0, 0);
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
    // static: gpu only, for indices
    // dynamic: host visible, for geometry instances
    // stream: host visible + coherent, for text (no flush needed)

    m_staticArena = {};
    m_staticArena.capacity = sizeof(uint32_t) * INDEX_COUNT_RECT;

    m_dynamicArena = {};
    m_dynamicArena.capacity = sizeof(InstanceData) * INITIAL_INSTANCE_CAPACITY;

    m_streamArena = {};
    m_streamArena.capacity = sizeof(CharacterInstance) * INITIAL_CHARACTER_CAPACITY + INITIAL_FONT_DATA_CAPACITY;

    VkBufferUsageFlags staticUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    VkMemoryPropertyFlags staticProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    s_createBuffer(m_info.device, m_info.physicalDevice, m_staticArena, staticUsage, staticProperties);

    VkBufferUsageFlags dynamicUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VkMemoryPropertyFlags dynamicProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    s_createBuffer(m_info.device, m_info.physicalDevice, m_dynamicArena, dynamicUsage, dynamicProperties);

    VkBufferUsageFlags streamUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VkMemoryPropertyFlags streamProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    s_createBuffer(m_info.device, m_info.physicalDevice, m_streamArena, streamUsage, streamProperties);

    vkMapMemory(m_info.device, m_dynamicArena.memory, 0, VK_WHOLE_SIZE, 0, &m_dynamicArena.mappedMemory);
    vkMapMemory(m_info.device, m_streamArena.memory, 0, VK_WHOLE_SIZE, 0, &m_streamArena.mappedMemory);
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

void VkBackend::allocateInstanceBuffers()
{
    // Geometry instance buffer (dynamic arena)
    size_t neededSize = sizeof(InstanceData) * INITIAL_INSTANCE_CAPACITY;
    if (neededSize > (m_dynamicArena.capacity - m_dynamicArena.size)) {
        AM_LOG_ERROR("Failed to allocate instance buffer");
        return;
    }

    m_instanceDataBuffer = {};
    m_instanceDataBuffer.arena = &m_dynamicArena;
    m_instanceDataBuffer.capacity = neededSize;
    m_instanceDataBuffer.offset = m_dynamicArena.size;
    m_dynamicArena.size += neededSize;

    // Character buffer (stream arena)
    size_t charSize = sizeof(CharacterInstance) * INITIAL_CHARACTER_CAPACITY;
    m_characterBuffer = {};
    m_characterBuffer.arena = &m_streamArena;
    m_characterBuffer.capacity = charSize;
    m_characterBuffer.offset = m_streamArena.size;
    m_streamArena.size += charSize;

    // Font data buffer (stream arena)
    m_fontDataBuffer = {};
    m_fontDataBuffer.arena = &m_streamArena;
    m_fontDataBuffer.capacity = INITIAL_FONT_DATA_CAPACITY;
    m_fontDataBuffer.offset = m_streamArena.size;
    m_streamArena.size += INITIAL_FONT_DATA_CAPACITY;
}

void VkBackend::updateInstances(GeometryRegistry &registry)
{
    auto dirtyIndices = registry.consumeDirtyIndices();
    const auto &allocations = registry.getAllocations();

    if (allocations.size() > m_instanceDataBuffer.capacity) {
        // grow
    }

    if (!dirtyIndices.empty()) {
        auto *basePtr = static_cast<uint8_t *>(m_instanceDataBuffer.arena->mappedMemory);
        auto *instances = reinterpret_cast<InstanceData *>(basePtr + m_instanceDataBuffer.offset);

        for (uint32_t idx : dirtyIndices) {
            instances[idx] = allocations[idx];
        }

        VkMappedMemoryRange memoryRange{};
        memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        memoryRange.memory = m_instanceDataBuffer.arena->memory;
        memoryRange.offset = m_instanceDataBuffer.offset;
        memoryRange.size = VK_WHOLE_SIZE;
        vkFlushMappedMemoryRanges(m_info.device, 1, &memoryRange);
    }

    m_instanceDataBuffer.size = allocations.size();
}

void VkBackend::updateTextCharacters(TextRegistry &registry)
{
    if (!registry.consumeDirty()) {
        return;
    }

    const auto &characters = registry.getCharacters();
    size_t dataSize = characters.size() * sizeof(CharacterInstance);

    if (dataSize > m_characterBuffer.capacity) {
        AM_LOG_WARN("Character buffer overflow, truncating");
        dataSize = m_characterBuffer.capacity;
    }

    if (!characters.empty()) {
        auto *basePtr = static_cast<uint8_t *>(m_characterBuffer.arena->mappedMemory);
        auto *dst = basePtr + m_characterBuffer.offset;
        std::memcpy(dst, characters.data(), dataSize);
    }

    m_characterBuffer.size = characters.size();
}

void VkBackend::uploadFontData(const TTF::FontData &fontData)
{
    // Pack font data: header (offsets) + points + contours + glyphs
    struct FontDataHeader {
        uint32_t pointsOffset;
        uint32_t contoursOffset;
        uint32_t glyphsOffset;
        uint32_t _pad;
    };

    size_t headerSize = sizeof(FontDataHeader);
    size_t pointsSize = fontData.points.size() * sizeof(TTF::Point);
    size_t contoursSize = fontData.contours.size() * sizeof(TTF::Contour);
    size_t glyphsSize = fontData.glyphs.size() * sizeof(TTF::Glyph);
    size_t totalSize = headerSize + pointsSize + contoursSize + glyphsSize;

    if (totalSize > m_fontDataBuffer.capacity) {
        AM_LOG_ERROR("Font data too large: {} bytes, capacity: {}", totalSize, m_fontDataBuffer.capacity);
        return;
    }

    auto *basePtr = static_cast<uint8_t *>(m_fontDataBuffer.arena->mappedMemory);
    auto *dst = basePtr + m_fontDataBuffer.offset;

    // Write header (offsets are in uint32_t units after header)
    FontDataHeader header;
    header.pointsOffset = 0;
    header.contoursOffset = static_cast<uint32_t>(fontData.points.size() * 3);                         // Point is 3 uint32_t
    header.glyphsOffset = header.contoursOffset + static_cast<uint32_t>(fontData.contours.size() * 2); // Contour is 2 uint32_t
    header._pad = 0;

    std::memcpy(dst, &header, headerSize);
    dst += headerSize;

    std::memcpy(dst, fontData.points.data(), pointsSize);
    dst += pointsSize;

    std::memcpy(dst, fontData.contours.data(), contoursSize);
    dst += contoursSize;

    std::memcpy(dst, fontData.glyphs.data(), glyphsSize);

    m_fontDataBuffer.size = totalSize;
    m_fontDataUploaded = true;

    AM_LOG_INFO("Uploaded font data: {} points, {} contours, {} glyphs", fontData.points.size(), fontData.contours.size(),
                fontData.glyphs.size());
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
    m_vertShader = loadShaderModule("/home/Thomas/dev/Amethyst/backends/shaders/spirv/ui.vs.spv");
    m_fragShader = loadShaderModule("/home/Thomas/dev/Amethyst/backends/shaders/spirv/ui.fs.spv");

    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = MAX_BINDLESS_TEXTURES;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorBindingFlags bindingFlags[] = {
        0,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
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
    bufferInfo.buffer = m_instanceDataBuffer.arena->buffer;
    bufferInfo.offset = m_instanceDataBuffer.offset;
    bufferInfo.range = m_instanceDataBuffer.capacity;

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(m_info.device, 1, &descriptorWrite, 0, nullptr);
}

void VkBackend::createTextPipeline()
{
    m_textVertShader = loadShaderModule("/home/Thomas/dev/Amethyst/backends/shaders/spirv/text.vs.spv");
    m_textFragShader = loadShaderModule("/home/Thomas/dev/Amethyst/backends/shaders/spirv/text.fs.spv");

    // Two bindings: character buffer (binding 0) and font data buffer (binding 1)
    VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr,
        },
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = std::size(bindings),
        .pBindings = bindings,
    };
    vkCreateDescriptorSetLayout(m_info.device, &layoutInfo, nullptr, &m_textDescriptorSetLayout);

    VkPushConstantRange pushConstant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(PushConstants),
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &m_textDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstant,
    };
    vkCreatePipelineLayout(m_info.device, &pipelineLayoutInfo, nullptr, &m_textPipelineLayout);

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = m_textVertShader,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = m_textFragShader,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        },
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState blendAttachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = std::size(dynamicStates),
        .pDynamicStates = dynamicStates,
    };

    VkPipelineRenderingCreateInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &m_info.colorFormat,
        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingInfo,
        .flags = 0,
        .stageCount = std::size(shaderStages),
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pTessellationState = nullptr,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = m_textPipelineLayout,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    vkCreateGraphicsPipelines(m_info.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_textPipeline);
}

void VkBackend::allocateTextDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = m_info.pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_textDescriptorSetLayout,
    };
    vkAllocateDescriptorSets(m_info.device, &allocInfo, &m_textDescriptorSet);

    VkDescriptorBufferInfo characterBufferInfo = {
        .buffer = m_characterBuffer.arena->buffer,
        .offset = m_characterBuffer.offset,
        .range = m_characterBuffer.capacity,
    };

    VkDescriptorBufferInfo fontDataBufferInfo = {
        .buffer = m_fontDataBuffer.arena->buffer,
        .offset = m_fontDataBuffer.offset,
        .range = m_fontDataBuffer.capacity,
    };

    VkWriteDescriptorSet descriptorWrites[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_textDescriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &characterBufferInfo,
            .pTexelBufferView = nullptr,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = m_textDescriptorSet,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &fontDataBufferInfo,
            .pTexelBufferView = nullptr,
        },
    };
    vkUpdateDescriptorSets(m_info.device, std::size(descriptorWrites), descriptorWrites, 0, nullptr);
}

void VkBackend::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    if (g_glfwData.prevMouseButtonCallback) {
        g_glfwData.prevMouseButtonCallback(window, button, action, mods);
    }
    InputInterface::onMouseButton(button, action, mods);
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
    InputInterface::onKey(key, scancode, action, mods);
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

} // namespace Amethyst
