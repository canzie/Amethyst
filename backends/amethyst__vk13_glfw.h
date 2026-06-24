#ifndef AMETHYST__VK13_GLFW_H
#define AMETHYST__VK13_GLFW_H

#include "amethyst/amethyst_backend.h"
#include "components/common.h"
#include "rendering/frame_draw_list.h"

#include <GLFW/glfw3.h>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

constexpr uint32_t MAX_BINDLESS_TEXTURES = 1024;

namespace Amethyst {

class Window;

struct AmGlfwInitInfo {
    void *window;
    Window *uiWindow = nullptr;
};

struct AmVulkanInitInfo {
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
    const char *vertexShaderPath;
    const char *fragmentShaderPath;
};

struct BufferRecord {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = nullptr;
    size_t capacity = 0;
    AmBufferDesc desc;
    bool alive = false;
};

struct TextureRecord {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    void *stagingMapped = nullptr;
    AmTextureDesc desc;
};

class AmVulkanBackend : public AmethystBackend {
  public:
    void init(const AmVulkanInitInfo &config, const AmGlfwInitInfo &info);
    void shutdown();

  public:
    void beginFrame();
    void endFrame();
    void record(VkCommandBuffer cmd, const FrameDrawList &drawList);
    void onResize(Amethyst::vec2 extent);

    AmBufferId createBuffer(const AmBufferDesc &desc) override;
    bool growBuffer(AmBufferId id, size_t newCapacity) override;
    void uploadBufferRange(void *cmdBuffer, AmBufferId id, const void *data, size_t offsetBytes, size_t sizeBytes) override;
    void destroyBuffer(AmBufferId id) override;

    AmTextureId createTexture(const AmTextureDesc &desc) override;
    void uploadTexture(void *cmdBuffer, AmTextureId id, const uint8_t *pixels) override;
    void destroyTexture(AmTextureId id) override;

    AmTextureId registerTexture(VkImageView imageView, VkSampler sampler);
    void unregisterTexture(AmTextureId id);

  private:
    void createPipeline();
    void allocateDescriptorSet();
    void writeBufferDescriptor(VkBuffer buffer, uint32_t binding);
    void uploadDeviceLocal(VkBuffer dst, const void *data, size_t offsetBytes, size_t sizeBytes);
    void destroyBufferRecord(BufferRecord &record);
    void destroyTextureRecord(TextureRecord &record);
    VkShaderModule loadShaderModule(const char *path);
    void setupGLFWCallbacks();
    static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow *window, double x, double y);
    static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow *window, unsigned int codepoint);
    static void contentScaleCallback(GLFWwindow *window, float xscale, float yscale);

  private:
    AmVulkanInitInfo m_info;
    AmGlfwInitInfo m_glfwInfo;

    // UI geometry pipeline
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkShaderModule m_vertShader = VK_NULL_HANDLE;
    VkShaderModule m_fragShader = VK_NULL_HANDLE;

    std::vector<BufferRecord> m_buffers;
    std::vector<uint32_t> m_bufferFreeSlots;

    // keyed by bindless texture slot so AmTextureId doubles as the resource handle
    std::unordered_map<uint32_t, TextureRecord> m_textures;

    std::vector<uint32_t> m_textureFreeList;
    uint32_t m_nextTextureSlot = 0;

    // Per-window GLFW input state: chained-from callbacks and this window's content scale.
    GLFWmousebuttonfun m_prevMouseButtonCallback = nullptr;
    GLFWcursorposfun m_prevCursorPosCallback = nullptr;
    GLFWscrollfun m_prevScrollCallback = nullptr;
    GLFWkeyfun m_prevKeyCallback = nullptr;
    GLFWcharfun m_prevCharCallback = nullptr;
    GLFWwindowcontentscalefun m_prevContentScaleCallback = nullptr;
    float m_contentScaleX = 1.0f;
    float m_contentScaleY = 1.0f;
};

} // namespace Amethyst

#endif // AMETHYST__VK13_GLFW_H
