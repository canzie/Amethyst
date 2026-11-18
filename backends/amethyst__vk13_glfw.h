#ifndef AMETHYST__VK13_GLFW_H
#define AMETHYST__VK13_GLFW_H

#include "amethyst/amethyst_backend.h"
#include "components/common.h"
#include "rendering/frame_draw_list.h"

#include <GLFW/glfw3.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
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
    uint32_t framesInFlight = 2;
};

struct BufferRecord {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = nullptr;
    size_t capacity = 0;
    AmBufferDesc desc;
    bool alive = false;
};

/**
 * @brief Host visible buffer a frame copies its uploads from, filled front to back.
 */
struct StagingBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = nullptr;
    size_t capacity = 0;
    size_t used = 0;
};

/**
 * @brief Where an upload's data sits inside a staging buffer, the source of its GPU copy.
 */
struct StagingLocation {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
};

struct TextureRecord {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    bool uploaded = false; // false while the image is still in VK_IMAGE_LAYOUT_UNDEFINED
    AmTextureDesc desc;
};

class AmVulkanBackend : public AmethystBackend {
  public:
    void init(const AmVulkanInitInfo &config, const AmGlfwInitInfo &info);
    void shutdown();

  public:
    /**
     * @brief Call once per frame, after waiting on the fence of the frame slot about to be reused.
     *
     * Runs the deletions deferred framesInFlight frames ago and reuses this frame's staging buffer.
     */
    void beginFrame();
    void endFrame();
    void record(VkCommandBuffer cmd, const FrameDrawList &drawList, VkExtent2D extent);

    AmBufferId createBuffer(const AmBufferDesc &desc) override;
    bool growBuffer(AmBufferId id, size_t newCapacity) override;
    void uploadBufferRange(void *cmdBuffer, AmBufferId id, const void *data, size_t offsetBytes, size_t sizeBytes) override;
    void destroyBuffer(AmBufferId id) override;

    AmTextureId createTexture(const AmTextureDesc &desc) override;
    void uploadTexture(void *cmdBuffer, AmTextureId id, const uint8_t *pixels) override;
    void uploadTextureRegion(void *cmdBuffer, AmTextureId id, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                             const uint8_t *pixels) override;
    void destroyTexture(AmTextureId id) override;

    AmTextureId registerTexture(VkImageView imageView, VkSampler sampler);
    void unregisterTexture(AmTextureId id);

    void registerWindow(void *glfwWindow, Window *uiWindow);
    void unregisterWindow(void *glfwWindow);

  private:
    void createPipeline();
    void allocateDescriptorSet();
    void writeBufferDescriptor(VkBuffer buffer, uint32_t binding);
    void uploadDeviceLocal(VkBuffer dst, const void *data, size_t offsetBytes, size_t sizeBytes);
    void destroyBufferRecord(BufferRecord &record);
    void destroyTextureRecord(TextureRecord &record);

    /**
     * @brief Copy upload data into this frame's staging buffer, growing it when full.
     * @return Where the data now sits, or nullopt if a bigger staging buffer could not be created
     */
    std::optional<StagingLocation> copyToStaging(const void *data, size_t size);
    bool createStagingBuffer(size_t size, StagingBuffer &out);
    void destroyStagingBuffer(StagingBuffer &staging);

    /**
     * @brief Delete a resource once no frame in flight can still be using it.
     */
    void deferDeletion(std::function<void()> deletion);

    /**
     * @brief Run the deferred deletions whose frames have finished.
     */
    void runFinishedDeletions();

    /**
     * @brief Run every deferred deletion. Only valid once the device is idle.
     */
    void runAllDeletions();
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

    VkDeviceSize m_nonCoherentAtomSize = 1;

    std::vector<BufferRecord> m_buffers;
    std::vector<uint32_t> m_bufferFreeSlots;

    // keyed by bindless texture slot so AmTextureId doubles as the resource handle
    std::unordered_map<uint32_t, TextureRecord> m_textures;

    std::vector<uint32_t> m_textureFreeList;
    uint32_t m_nextTextureSlot = 0;

    struct DeferredDeletion {
        uint64_t frame = 0; // m_frameCount when it was deferred
        std::function<void()> run;
    };

    uint64_t m_frameCount = 0;
    std::vector<StagingBuffer> m_stagingBuffers; // one per frame in flight
    std::vector<DeferredDeletion> m_deferredDeletions;
    bool m_warnedNoBeginFrame = false;
};

} // namespace Amethyst

#endif // AMETHYST__VK13_GLFW_H
