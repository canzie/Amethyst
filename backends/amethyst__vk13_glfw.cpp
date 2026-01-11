#include "amethyst__vk13_glfw.h"

#include "components/common.h"
#include "logging/log.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <iterator>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Amethyst {

constexpr size_t INITIAL_INSTANCE_CAPACITY = 64;
constexpr size_t INDEX_COUNT_RECT = 6;

struct PushConstants {
    glm::vec2 screenSize;
};

void VkBackend::init(const VulkanInitInfo &config)
{
    m_info = config;
    allocateBufferArenas();
    allocateIndexBuffer();
    allocateInstanceBuffers();
    createPipeline();
    allocateDescriptorSet();
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
    if (m_streamArena.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_streamArena.buffer, nullptr);
        vkFreeMemory(device, m_streamArena.memory, nullptr);
    }

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

void VkBackend::record(VkCommandBuffer cmd, GeometryRegistry &registry)
{
    updateInstances(registry);

    if (m_instanceDataBuffer.size == 0) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

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
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

    vkCmdBindIndexBuffer(cmd, m_indexBuffer.arena->buffer, m_indexBuffer.offset, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, INDEX_COUNT_RECT, static_cast<uint32_t>(m_instanceDataBuffer.size), 0, 0, 0);
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
    // 2 arena for now, streambuffer can stay empty
    // dynamic is host visible but not coherent
    // static is only gpu visible and needs a staging buffer to get the data there

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

void VkBackend::allocateInstanceBuffers()
{
    // Will be more dynamic and allow for adding/removing/updating data, that is why it will be dynamic for now
    // the size of the allocation will be large, but might need to grow over time, The buffer will also need ssbo flag
    // every allocation in it will be of the same size so defragmentation is not really a thing. we will let the core deal with
    // indices when data is to be removed, no problem we just wont ue it in the meantime and let the core fill in the gaps. we will
    // always prioritize filling in gaps over allocating after our farthest element because of the freelist

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
    allocInfo.memoryTypeIndex =
        findMemoryType(m_info.physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = buffer.size(),
        .pCode = reinterpret_cast<const uint32_t *>(buffer.data()),
    };

    VkShaderModule shaderModule;
    vkCreateShaderModule(m_info.device, &createInfo, nullptr, &shaderModule);
    return shaderModule;
}

void VkBackend::createPipeline()
{
    m_vertShader = loadShaderModule("/home/Thomas/dev/Amethyst/backends/shaders/spirv/ui.vs.spv");
    m_fragShader = loadShaderModule("/home/Thomas/dev/Amethyst/backends/shaders/spirv/ui.fs.spv");

    VkDescriptorSetLayoutBinding ssboBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &ssboBinding,
    };
    vkCreateDescriptorSetLayout(m_info.device, &layoutInfo, nullptr, &m_descriptorSetLayout);

    VkPushConstantRange pushConstant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(PushConstants),
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &m_descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstant,
    };
    vkCreatePipelineLayout(m_info.device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = m_vertShader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = m_fragShader,
            .pName = "main",
        },
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
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
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment,
    };

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = std::size(dynamicStates),
        .pDynamicStates = dynamicStates,
    };

    VkPipelineRenderingCreateInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &m_info.colorFormat,
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingInfo,
        .stageCount = std::size(shaderStages),
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = m_pipelineLayout,
    };

    vkCreateGraphicsPipelines(m_info.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
}

void VkBackend::allocateDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_info.pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descriptorSetLayout,
    };
    vkAllocateDescriptorSets(m_info.device, &allocInfo, &m_descriptorSet);

    VkDescriptorBufferInfo bufferInfo = {
        .buffer = m_instanceDataBuffer.arena->buffer,
        .offset = m_instanceDataBuffer.offset,
        .range = m_instanceDataBuffer.capacity,
    };

    VkWriteDescriptorSet descriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &bufferInfo,
    };
    vkUpdateDescriptorSets(m_info.device, 1, &descriptorWrite, 0, nullptr);
}

} // namespace Amethyst
