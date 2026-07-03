#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/curve_editor.h"
#include "modules/style.h"
#include "vk_context.h"

#include <cstdint>

using namespace Amethyst;

int main()
{
    Log::Init();

    Style::load(AMETHYST_ASSETS_DIR "/theme.ams");

    VkContext ctx;
    if (!contextInit(ctx, 980, 640, "Amethyst - Curve Editor")) {
        AM_LOG_ERROR("Failed to initialize Vulkan context");
        return 1;
    }

    AmethystContext amCtx;
    if (!amCtx.loadFont(AMETHYST_ASSETS_DIR "/fonts/OpenSans-Regular.ttf")) {
        AM_LOG_ERROR("Failed to load font");
        return 1;
    }

    AmVulkanInitInfo initInfo{};
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
    initInfo.vertexShaderPath = AMETHYST_SHADER_DIR "/ui.vs.spv";
    initInfo.fragmentShaderPath = AMETHYST_SHADER_DIR "/ui.fs.spv";

    Window window;

    AmGlfwInitInfo glfwInfo{};
    glfwInfo.window = ctx.window;
    glfwInfo.uiWindow = &window;

    AmVulkanBackend backend;
    backend.init(initInfo, glfwInfo);

    amCtx.init(backend);

    vec2 screenSize = {
        static_cast<float>(ctx.swapchainExtent.width),
        static_cast<float>(ctx.swapchainExtent.height),
    };

    window.absoluteSize = screenSize;
    window.absoluteRotation = 0.0f;
    window.setDisplayOrder(10);

    auto *editor = window.add<CurveEditor>();
    editor->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});

    const Color4 R = {0.86f, 0.42f, 0.42f, 1.0f};
    const Color4 G = {0.45f, 0.80f, 0.50f, 1.0f};
    const Color4 B = {0.45f, 0.62f, 0.92f, 1.0f};

    editor->addCurve("X", R, {{20, 400}, {180, 300}, {360, 320}, {520, 180}, {660, 120}}, "Translate");
    editor->addCurve("Y", G, {{20, 150}, {180, 220}, {360, 180}, {520, 300}, {660, 260}}, "Translate");
    editor->addCurve("Z", B, {{20, 300}, {180, 120}, {360, 400}, {520, 260}, {660, 440}}, "Translate");
    editor->addCurve("X", R, {{20, 480}, {180, 420}, {360, 300}, {520, 360}, {660, 240}}, "Rotate");
    editor->addCurve("Y", G, {{20, 90}, {180, 180}, {360, 150}, {520, 80}, {660, 200}}, "Rotate");

    editor->selectCurve(0);

    amCtx.draw(window);

    int frameCount = 0;
    double lastTime = glfwGetTime();
    double lastUpdateTime = glfwGetTime();

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

        double currentUpdateTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentUpdateTime - lastUpdateTime);
        lastUpdateTime = currentUpdateTime;

        window.tick(deltaTime);

        uint32_t imageIndex;
        if (!contextBeginFrame(ctx, imageIndex)) {
            continue;
        }

        VkCommandBuffer cmd = ctx.commandBuffers[ctx.currentFrame];

        amCtx.draw(window);
        amCtx.syncShared(static_cast<void *>(cmd));
        amCtx.syncWindow(static_cast<void *>(cmd), window);
        backend.record(cmd, amCtx.getDrawList(window), ctx.swapchainExtent);
        contextEndFrame(ctx, imageIndex);

        frameCount++;
        double now = glfwGetTime();
        if (now - lastTime >= 1.0) {
            AM_LOG_INFO("FPS: {}", frameCount);
            frameCount = 0;
            lastTime = now;
        }
    }

    backend.shutdown();
    contextShutdown(ctx);
    Log::Shutdown();
    return 0;
}
