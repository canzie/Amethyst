#ifndef AMETHYST__VK13_GLFW_H
#define AMETHYST__VK13_GLFW_H

#include "components/common.h"
#include "rendering/geometry_registry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Amethyst {

struct VulkanInitInfo {
    VkDevice device;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkQueue queue;
    uint32_t queueFamiliy;
    VkDescriptorPool pool;
    uint32_t minImageCount;
    uint32_t imageCount;
    // callback for results
    VkFormat colorFormat;
    VkExtent2D extent;
};

struct BufferArena {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mappedMemory = nullptr;
    size_t size = 0;
    size_t capacity = 0;
};

struct BufferAllocation {
    BufferArena *arena = nullptr;
    size_t offset = 0;   // start of the allocation
    size_t size = 0;     // current amount of bytes used out of capacity
    size_t capacity = 0; // the amount of bytes allocated from the arena
};

class VkBackend {
  public:
    void init(const VulkanInitInfo &config);
    void shutdown();

    void beginFrame();
    void endFrame();
    void record(VkCommandBuffer cmd, GeometryRegistry &registry);

  private:
    void createPipeline();
    void allocateDescriptorSet();
    void allocateBufferArenas();
    void allocateIndexBuffer();
    void allocateInstanceBuffers();
    void updateInstances(GeometryRegistry &registry);
    void uploadToGpu(BufferAllocation &alloc, const void *data, size_t size, size_t offset);
    VkShaderModule loadShaderModule(const char *path);

  private:
    VulkanInitInfo m_info;

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkShaderModule m_vertShader = VK_NULL_HANDLE;
    VkShaderModule m_fragShader = VK_NULL_HANDLE;

    // used for indices
    BufferArena m_staticArena;
    // perhaps stream buffer can be used for very frequently updated data, like animated stuff
    // while the normal shapes will be usingthe dynamic arena, as they need fast updates but not all on a frame to frame basis
    BufferArena m_dynamicArena;
    BufferArena m_streamArena;

    BufferAllocation m_indexBuffer;
    BufferAllocation m_instanceDataBuffer;
};

} // namespace Amethyst

#endif // AMETHYST__VK13_GLFW_H
