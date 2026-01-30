#ifndef AMETHYST__VK13_GLFW_H
#define AMETHYST__VK13_GLFW_H

#include "components/common.h"
#include "components/input_interface.h"
#include "rendering/geometry_registry.h"

#include <GLFW/glfw3.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
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
    const char* vertexShaderPath;
    const char* fragmentShaderPath;
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

struct FreeBlock {
    size_t offset;
    size_t size;
};

class VkBackend {
  public:
    void init(const VulkanInitInfo &config, const GLFWInitInfo &info);
    void shutdown();

  public:
    void beginFrame();
    void endFrame();
    void record(VkCommandBuffer cmd);
    void onResize(glm::vec2 extent);

    void createAtlasTexture(uint32_t width, uint32_t height);
    void uploadAtlasData(VkCommandBuffer cmd, const uint8_t *pixels, uint32_t width, uint32_t height);
    AmTextureId getAtlasTextureId() const { return m_atlasTextureId; }

    AmTextureId registerTexture(VkImageView imageView, VkSampler sampler);
    void unregisterTexture(AmTextureId id);

  private:
    void createPipeline();
    void allocateDescriptorSet();
    void allocateBufferArenas();
    void allocateIndexBuffer();
    void allocateInstanceBuffers();
    void updateInstances(BufferAllocation &alloc, GeometryRegistry &registry);
    void uploadToGpu(BufferAllocation &alloc, const void *data, size_t size, size_t offset);
    VkShaderModule loadShaderModule(const char *path);
    void setupGLFWCallbacks();
    BufferAllocation* obtainGeometryAllocation(GeometryRegistry* registry);
    void freeGeometryAllocation(GeometryRegistry* registry);
    BufferAllocation allocateFromArena(BufferArena& arena, std::vector<FreeBlock>& freeList, size_t size);
    void freeToArena(std::vector<FreeBlock>& freeList, const BufferAllocation& alloc);
    static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow *window, double x, double y);
    static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow *window, unsigned int codepoint);
    static void contentScaleCallback(GLFWwindow *window, float xscale, float yscale);

  private:
    VulkanInitInfo m_info;
    GLFWInitInfo m_glfwInfo;

    // UI geometry pipeline
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkShaderModule m_vertShader = VK_NULL_HANDLE;
    VkShaderModule m_fragShader = VK_NULL_HANDLE;

    // Glyph atlas texture
    VkImage m_atlasImage = VK_NULL_HANDLE;
    VkDeviceMemory m_atlasMemory = VK_NULL_HANDLE;
    VkImageView m_atlasView = VK_NULL_HANDLE;
    VkSampler m_atlasSampler = VK_NULL_HANDLE;
    AmTextureId m_atlasTextureId = AM_INVALID_TEXTURE;
    VkBuffer m_atlasStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_atlasStagingMemory = VK_NULL_HANDLE;
    void *m_atlasStagingMapped = nullptr;
    uint32_t m_atlasWidth = 0;
    uint32_t m_atlasHeight = 0;

    // used for indices
    BufferArena m_staticArena;
    // dynamic arena for geometry instances
    BufferArena m_dynamicArena;
    // stream arena for text (coherent, no flush)
    BufferArena m_streamArena;

    BufferAllocation m_indexBuffer;

    std::unordered_map<GeometryRegistry*, BufferAllocation> m_geometryAllocations;

    std::vector<FreeBlock> m_dynamicArenaFreeList;

    std::vector<uint32_t> m_textureFreeList;
    uint32_t m_nextTextureSlot = 0;
};

} // namespace Amethyst

#endif // AMETHYST__VK13_GLFW_H
