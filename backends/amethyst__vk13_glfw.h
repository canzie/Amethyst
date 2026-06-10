#ifndef AMETHYST__VK13_GLFW_H
#define AMETHYST__VK13_GLFW_H

#include "amethyst/amethyst_backend.h"
#include "components/common.h"
#include "components/input_interface.h"
#include "rendering/instance_data.h"
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
constexpr size_t MAX_BUFFER_ARENA_SIZE = 10 * 1024 * 1024; // 10MB hard limit

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

struct TextGpuAllocation {
    BufferAllocation glyph;
    BufferAllocation line;
    BufferAllocation slice;
};

class VkBackend : public AmethystBackend {
  public:
    void init(const VulkanInitInfo &config, const GLFWInitInfo &info);
    void shutdown();

  public:
    void beginFrame();
    void endFrame();
    void record(VkCommandBuffer cmd);
    void onResize(Amethyst::vec2 extent);

    void createAtlasTexture(uint32_t width, uint32_t height) override;
    void uploadAtlasData(void *cmdBuffer, const uint8_t *pixels, uint32_t width, uint32_t height) override;
    AmTextureId getAtlasTextureId() const override { return m_atlasTextureId; }

    void createSvgAtlasTexture(uint32_t width, uint32_t height) override;
    void uploadSvgAtlasData(void *cmdBuffer, const uint8_t *pixels, uint32_t width, uint32_t height) override;
    AmTextureId getSvgAtlasTextureId() const override { return m_svgAtlasTextureId; }

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
    TextGpuAllocation* obtainTextAllocation(GeometryRegistry* registry);
    void updateTextBuffers(TextGpuAllocation& alloc, GlyphBuffer& glyphBuffer);
    void freeTextAllocation(GeometryRegistry* registry);
    BufferAllocation allocateFromArena(BufferArena& arena, std::vector<FreeBlock>& freeList, size_t size);
    void freeToArena(std::vector<FreeBlock>& freeList, const BufferAllocation& alloc);
    bool reallocBufferArena(BufferArena& arena, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
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

    // SVG atlas texture
    VkImage m_svgAtlasImage = VK_NULL_HANDLE;
    VkDeviceMemory m_svgAtlasMemory = VK_NULL_HANDLE;
    VkImageView m_svgAtlasView = VK_NULL_HANDLE;
    VkSampler m_svgAtlasSampler = VK_NULL_HANDLE;
    AmTextureId m_svgAtlasTextureId = AM_INVALID_TEXTURE;
    VkBuffer m_svgAtlasStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_svgAtlasStagingMemory = VK_NULL_HANDLE;
    void *m_svgAtlasStagingMapped = nullptr;
    uint32_t m_svgAtlasWidth = 0;
    uint32_t m_svgAtlasHeight = 0;

    // used for indices
    BufferArena m_staticArena;
    // dynamic arena for geometry instances
    BufferArena m_dynamicArena;
    // stream arena for text (coherent, no flush)
    BufferArena m_streamArena;

    // pooled storage buffers for batched text
    BufferArena m_glyphArena;
    BufferArena m_lineArena;
    BufferArena m_sliceArena;

    BufferAllocation m_indexBuffer;

    std::unordered_map<GeometryRegistry*, BufferAllocation> m_geometryAllocations;
    std::unordered_map<GeometryRegistry*, TextGpuAllocation> m_textAllocations;

    std::vector<FreeBlock> m_dynamicArenaFreeList;
    std::vector<FreeBlock> m_glyphArenaFreeList;
    std::vector<FreeBlock> m_lineArenaFreeList;
    std::vector<FreeBlock> m_sliceArenaFreeList;

    std::vector<uint32_t> m_textureFreeList;
    uint32_t m_nextTextureSlot = 0;
};

} // namespace Amethyst

#endif // AMETHYST__VK13_GLFW_H
