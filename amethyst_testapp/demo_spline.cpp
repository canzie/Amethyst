#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/canvas.h"
#include "components/spline.h"
#include "components/ui_scope.h"
#include "modules/style.h"
#include "vk_context.h"

#include <cstdint>

using namespace Amethyst;

static void buildGrid(Canvas &canvas, vec2 size, float spacing)
{
    Color4 gridColor(1.0f, 1.0f, 1.0f, 0.06f);
    Color4 axisColor(1.0f, 1.0f, 1.0f, 0.16f);

    for (float x = 0.0f; x <= size.x; x += spacing) {
        canvas.drawLine({x, 0.0f}, {x, size.y}, gridColor, 1.0f);
    }
    for (float y = 0.0f; y <= size.y; y += spacing) {
        canvas.drawLine({0.0f, y}, {size.x, y}, gridColor, 1.0f);
    }
    canvas.drawLine({0.0f, size.y * 0.5f}, {size.x, size.y * 0.5f}, axisColor, 1.5f);
}

int main()
{
    Log::Init();

    Style::load(AMETHYST_ASSETS_DIR "/theme.ams");

    VkContext ctx;
    if (!contextInit(ctx, 900, 600, "Amethyst - Spline Demo")) {
        AM_LOG_ERROR("Failed to initialize Vulkan context");
        return 1;
    }

    AmethystContext amCtx;
    if (!amCtx.loadFont(AMETHYST_ASSETS_DIR "/fonts/OpenSans-Regular.ttf").isValid()) {
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

    UIScope root(window);
    root.frame({.base = {.size = UDim2::fromScale(1.0f, 1.0f)}, .style = {.backgroundColor = Color3::fromHex(0x14141A)}});

    root.canvas({.base = {.size = UDim2::fromScale(1.0f, 1.0f)}}, [&](CanvasScope &c) { buildGrid(c.component, screenSize, 40.0f); });

    auto bandLabel = [&](const char *text, float y) {
        root.textLabel({.base = {.position = UDim2::fromOffset(16, y), .size = UDim2::fromOffset(120, 20)},
                        .style = {.backgroundTransparency = 1.0f},
                        .text = {.fontSize = 13.0f, .textColor = {0.75f, 0.75f, 0.78f, 1.0f}, .textYAlignment = TextYAlignment::CENTER},
                        .label = text});
    };

    bandLabel("Linear", 70);
    root.spline({.base = {.size = UDim2::fromScale(1.0f, 1.0f)},
                 .spline = {.type = CurveType::LINEAR, .thickness = 2.0f, .color = {1.0f, 0.55f, 0.20f, 1.0f}},
                 .knots = {{140, 120}, {300, 70}, {460, 140}, {620, 80}, {780, 120}}});

    bandLabel("Quadratic", 210);
    root.spline({.base = {.size = UDim2::fromScale(1.0f, 1.0f)},
                 .spline = {.type = CurveType::QUADRATIC, .thickness = 2.5f, .color = {0.72f, 0.52f, 1.0f, 1.0f}},
                 .knots = {{140, 250}, {230, 160}, {320, 250}, {460, 160}, {600, 250}, {690, 160}, {780, 250}}});

    bandLabel("Cubic", 350);
    root.spline({.base = {.size = UDim2::fromScale(1.0f, 1.0f)},
                 .spline = {.type = CurveType::CUBIC, .thickness = 2.5f, .color = {0.40f, 0.85f, 0.50f, 1.0f}},
                 .knots = {{160, 380}, {240, 300}, {360, 300}, {440, 380}, {520, 460}, {640, 460}, {720, 380}}});

    bandLabel("Catmull-Rom", 500);
    root.spline({.base = {.size = UDim2::fromScale(1.0f, 1.0f)},
                 .spline = {.type = CurveType::CATMULL_ROM, .thickness = 3.0f, .color = {0.30f, 0.70f, 1.0f, 1.0f}},
                 .knots = {{140, 520}, {280, 470}, {420, 540}, {560, 470}, {700, 530}, {780, 500}}});

    root.textLabel({.base = {.position = UDim2::fromOffset(24, 20), .size = UDim2::fromOffset(700, 24)},
                    .style = {.backgroundTransparency = 1.0f},
                    .text = {.fontSize = 16.0f, .textColor = {1.0f, 1.0f, 1.0f, 1.0f}, .textYAlignment = TextYAlignment::CENTER},
                    .label = "Four curve types. Drag anchors (x-limited) or bezier control handles (free)."});

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
