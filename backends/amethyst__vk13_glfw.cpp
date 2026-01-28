#include "amethyst__vk13_glfw.h"

#include "components/input_interface.h"
#include "components/ui_layer.h"
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

constexpr size_t INITIAL_INSTANCE_CAPACITY = 256 * 32; // 8192 instances, ~32 layers @ 256 each
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

void VkBackend::onResize(glm::vec2 extent)
{
    m_info.extent = VkExtent2D(extent.x, extent.y);
}

void VkBackend::record(VkCommandBuffer cmd)
{
    const auto &geometryRegistries = GeometryRegistry::getRegistries();

    for (auto *registry : geometryRegistries) {
        UILayer *layer = registry->getOwningLayer();
        if (!layer || !layer->visible) continue;

        BufferAllocation *alloc = obtainGeometryAllocation(registry);
        if (alloc) {
            updateInstances(*alloc, *registry);
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
    };

    bool pipelineBound = false;
    for (auto *registry : geometryRegistries) {
        UILayer *layer = registry->getOwningLayer();
        if (!layer || !layer->visible) continue;

        auto it = m_geometryAllocations.find(registry);
        if (it != m_geometryAllocations.end() && it->second.size > 0) {
            if (!pipelineBound) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
                vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
                vkCmdBindIndexBuffer(cmd, m_indexBuffer.arena->buffer, m_indexBuffer.offset, VK_INDEX_TYPE_UINT32);
                pipelineBound = true;
            }
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
    m_dynamicArena.capacity = sizeof(InstanceData) * INITIAL_INSTANCE_CAPACITY;

    VkBufferUsageFlags staticUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    VkMemoryPropertyFlags staticProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    s_createBuffer(m_info.device, m_info.physicalDevice, m_staticArena, staticUsage, staticProperties);

    VkBufferUsageFlags dynamicUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VkMemoryPropertyFlags dynamicProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    s_createBuffer(m_info.device, m_info.physicalDevice, m_dynamicArena, dynamicUsage, dynamicProperties);

    vkMapMemory(m_info.device, m_dynamicArena.memory, 0, VK_WHOLE_SIZE, 0, &m_dynamicArena.mappedMemory);
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
    const auto &allocations = registry.getAllocations();
    size_t requiredSize = allocations.size() * sizeof(InstanceData);

    if (requiredSize > alloc.capacity) {
        freeToArena(m_dynamicArenaFreeList, alloc);
        AM_LOG_TRACE("Buffer needs to be resized");

        size_t newCapacity = requiredSize * 2;
        BufferAllocation newAlloc = allocateFromArena(m_dynamicArena, m_dynamicArenaFreeList, newCapacity);
        if (newAlloc.arena == nullptr) {
            AM_LOG_ERROR("Failed to reallocate geometry buffer");
            return;
        }
        alloc = newAlloc;

        auto *basePtr = static_cast<uint8_t *>(alloc.arena->mappedMemory);
        auto *instances = reinterpret_cast<InstanceData *>(basePtr + alloc.offset);
        for (size_t i = 0; i < allocations.size(); ++i) {
            instances[i] = allocations[i];
        }

        registry.consumeDirtyIndices();
    } else {
        auto dirtyIndices = registry.consumeDirtyIndices();
        if (!dirtyIndices.empty()) {
            auto *basePtr = static_cast<uint8_t *>(alloc.arena->mappedMemory);
            auto *instances = reinterpret_cast<InstanceData *>(basePtr + alloc.offset);

            for (uint32_t idx : dirtyIndices) {
                instances[idx] = allocations[idx];
            }
        }
    }

    VkMappedMemoryRange memoryRange{};
    memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    memoryRange.memory = alloc.arena->memory;
    memoryRange.offset = alloc.offset;
    memoryRange.size = VK_WHOLE_SIZE;
    vkFlushMappedMemoryRanges(m_info.device, 1, &memoryRange);

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
        AM_LOG_ERROR("Arena out of memory: size={}, capacity={}, requested={}", arena.size, arena.capacity, size);
        return {};
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

void VkBackend::uploadAtlasData(VkCommandBuffer cmd, const uint8_t *pixels, uint32_t width, uint32_t height)
{
    // TODO: This is inefficient - it uploads the entire atlas every time.
    // A better approach would be to track dirty regions and only upload changed areas.
    // For now, this works but should be optimized later.

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

} // namespace Amethyst
