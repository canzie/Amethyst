#include "vk_context.h"

#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"

int main()
{
    Amethyst::Log::Init();
    AM_LOG_INFO("Amethyst Test App");

    VkContext ctx;
    if (!contextInit(ctx, 1280, 720, "Amethyst Test")) {
        AM_LOG_ERROR("Failed to initialize Vulkan context");
        return 1;
    }

    Amethyst::GeometryRegistry registry;

    Amethyst::VulkanInitInfo initInfo{};
    initInfo.device = ctx.device;
    initInfo.instance = ctx.instance;
    initInfo.physicalDevice = ctx.physicalDevice;
    initInfo.queue = ctx.graphicsQueue;
    initInfo.queueFamiliy = ctx.graphicsQueueFamily;
    initInfo.pool = ctx.descriptorPool;
    initInfo.minImageCount = static_cast<uint32_t>(ctx.swapchainImages.size());
    initInfo.imageCount = static_cast<uint32_t>(ctx.swapchainImages.size());
    initInfo.colorFormat = ctx.swapchainFormat;
    initInfo.extent = ctx.swapchainExtent;

    Amethyst::VkBackend backend;
    backend.init(initInfo);

    glm::vec2 screenSize = {static_cast<float>(ctx.swapchainExtent.width), static_cast<float>(ctx.swapchainExtent.height)};

    Amethyst::Window window;
    window.absoluteSize = screenSize;

    Amethyst::Frame frame1;
    frame1.size = Amethyst::UDim2::fromOffset(300, 200);
    frame1.position = Amethyst::UDim2::fromOffset(200, 200);
    frame1.backgroundColor = {0.9f, 0.2f, 0.2f};
    frame1.cornerRadius = 10.0f;
    frame1.setParent(&window);
    frame1.markDirty();
    frame1.computeAbsolutes(screenSize, glm::vec2(0.0f), 0.0f);

    Amethyst::Frame frame2;
    frame2.size = Amethyst::UDim2::fromScale(0.5f, 0.5f);
    frame2.position = Amethyst::UDim2::fromOffset(0.0f, 0.0f);
    frame2.backgroundColor = {0.2f, 0.9f, 0.2f};
    frame2.cornerRadius = 0.0f;
    frame2.setParent(&frame1);
    frame2.markDirty();
    frame2.computeAbsolutes(frame1.absoluteSize, frame1.absolutePosition, 0.0f);

    Amethyst::Frame frame3;
    frame3.size = Amethyst::UDim2(0.0f, 100.0f, 0.9f, 0.0f);
    frame3.position = Amethyst::UDim2::fromOffset(550, 0);
    frame3.backgroundColor = {0.2f, 0.2f, 0.9f};
    frame3.cornerRadius = 20.0f;
    frame3.setParent(&window);
    frame3.markDirty();
    frame3.computeAbsolutes(screenSize, glm::vec2(0.0f), 0.0f);

    window.draw(registry);

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

        uint32_t imageIndex;
        if (!contextBeginFrame(ctx, imageIndex)) {
            continue;
        }

        VkCommandBuffer cmd = ctx.commandBuffers[ctx.currentFrame];
        backend.record(cmd, registry);

        contextEndFrame(ctx, imageIndex);
    }

    backend.shutdown();
    contextShutdown(ctx);

    Amethyst::Log::Shutdown();
    return 0;
}
