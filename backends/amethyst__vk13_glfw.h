#ifndef AMETHYST__VK13_GLFW_H
#define AMETHYST__VK13_GLFW_H

#include "components/common.h"
#include "components/input_interface.h"
#include "parsers/ttf/ttf_types.h"
#include "rendering/geometry_registry.h"
#include "rendering/text_registry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

constexpr uint32_t MAX_BINDLESS_TEXTURES = 1024;

namespace Amethyst {

struct GLFWInitInfo {
    void *window;
};

struct VulkanInitInfo {
    VkDevice device;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkQueue queue;
    uint32_t queueFamiliy;
    VkDescriptorPool pool;
    uint32_t minImageCount;
    uint32_t imageCount;
    VkFormat colorFormat;
    VkExtent2D extent;
};

void setupGLFWCallbacks(const GLFWInitInfo &info);

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
    void record(VkCommandBuffer cmd, GeometryRegistry &geometryRegistry, TextRegistry &textRegistry);
    void onResize(glm::vec2 extent);

    void uploadFontData(const TTF::FontData &fontData);

    AmTextureId registerTexture(VkImageView imageView, VkSampler sampler);
    void unregisterTexture(AmTextureId id);

  private:
    void createPipeline();
    void createTextPipeline();
    void allocateDescriptorSet();
    void allocateTextDescriptorSet();
    void allocateBufferArenas();
    void allocateIndexBuffer();
    void allocateInstanceBuffers();
    void updateInstances(GeometryRegistry &registry);
    void updateTextCharacters(TextRegistry &registry);
    void uploadToGpu(BufferAllocation &alloc, const void *data, size_t size, size_t offset);
    VkShaderModule loadShaderModule(const char *path);

  private:
    VulkanInitInfo m_info;

    // UI geometry pipeline
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkShaderModule m_vertShader = VK_NULL_HANDLE;
    VkShaderModule m_fragShader = VK_NULL_HANDLE;

    // Text pipeline
    VkPipeline m_textPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_textPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_textDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_textDescriptorSet = VK_NULL_HANDLE;
    VkShaderModule m_textVertShader = VK_NULL_HANDLE;
    VkShaderModule m_textFragShader = VK_NULL_HANDLE;

    // used for indices
    BufferArena m_staticArena;
    // dynamic arena for geometry instances
    BufferArena m_dynamicArena;
    // stream arena for text (coherent, no flush)
    BufferArena m_streamArena;

    BufferAllocation m_indexBuffer;
    BufferAllocation m_instanceDataBuffer;

    BufferAllocation m_characterBuffer;
    BufferAllocation m_fontDataBuffer;
    bool m_fontDataUploaded = false;

    std::vector<uint32_t> m_textureFreeList;
    uint32_t m_nextTextureSlot = 0;
};

} // namespace Amethyst

#endif // AMETHYST__VK13_GLFW_H
