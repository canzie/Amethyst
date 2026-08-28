#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/ui_scope.h"
#include "modules/style.h"
#include "vk_context.h"

#include <cstdint>

using namespace Amethyst;

int main()
{
    Log::Init();

    Style::load(AMETHYST_ASSETS_DIR "/theme.ams");

    VkContext ctx;
    if (!contextInit(ctx, 520, 360, "Amethyst - Drag Demo")) {
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

    double position = 0.0;
    double opacity = 1.0;
    int64_t count = 10;
    float progressWithThumb = 65.0f;
    float progressNoThumb = 40.0f;
    float progressLeftLabel = 25.0f;

    BaseStylePropertiesArgs dragStyle = {.backgroundColor = Color3::fromHex(0x33333A), .cornerRadius = 4.0f};
    DragStylePropertiesArgs dragText = {.text = {.fontSize = 14.0f, .textColor = {1.0f, 1.0f, 1.0f, 1.0f}}};

    auto caption = [](const char *label, float y) {
        return TextLabelProperties{
            .base = {.position = UDim2::fromOffset(40, y), .size = UDim2::fromOffset(120, 28)},
            .style = {.backgroundTransparency = 1.0f},
            .text = {.fontSize = 14.0f, .textColor = {0.8f, 0.8f, 0.8f, 1.0f}, .textYAlignment = TextYAlignment::CENTER},
            .label = label,
        };
    };

    UIScope root(window);
    root.frame({.base = {.size = UDim2::fromScale(1.0f, 1.0f)}, .style = {.backgroundColor = Color3::fromHex(0x1A1A1E)}});

    root.textLabel({.base = {.position = UDim2::fromOffset(40, 24), .size = UDim2::fromOffset(360, 24)},
                    .style = {.backgroundTransparency = 1.0f},
                    .text = {.fontSize = 16.0f, .textColor = {1.0f, 1.0f, 1.0f, 1.0f}, .textYAlignment = TextYAlignment::CENTER},
                    .label = "Drag to scrub, click without dragging to type"});

    // --- Free DragFloat (unbounded position) ---
    root.textLabel(caption("Position", 70.0f));
    root.dragFloat({.base = {.position = UDim2::fromOffset(170, 70), .size = UDim2::fromOffset(160, 28)},
                    .style = dragStyle,
                    .drag = dragText,
                    .format = "%.2f",
                    .speed = 0.25,
                    .value = &position},
                   [&](DragFloatScope &d) {
                       d.component.onValueChanged = [](double v) { AM_LOG_INFO("Position: {}", v); };
                   });

    // --- Bounded DragFloat (opacity in [0, 1]) ---
    root.textLabel(caption("Opacity", 110.0f));
    root.dragFloat({.base = {.position = UDim2::fromOffset(170, 110), .size = UDim2::fromOffset(160, 28)},
                    .style = dragStyle,
                    .drag = dragText,
                    .format = "%.2f",
                    .speed = 0.01,
                    .min = 0.0,
                    .max = 1.0,
                    .value = &opacity},
                   [&](DragFloatScope &d) {
                       d.component.onValueChanged = [](double v) { AM_LOG_INFO("Opacity: {}", v); };
                   });

    // --- DragInt (count in [0, 100]) ---
    root.textLabel(caption("Count", 150.0f));
    root.dragInt({.base = {.position = UDim2::fromOffset(170, 150), .size = UDim2::fromOffset(160, 28)},
                  .style = dragStyle,
                  .drag = dragText,
                  .speed = 1,
                  .min = 0,
                  .max = 100,
                  .value = &count},
                 [&](DragIntScope &d) {
                     d.component.onValueChanged = [](int64_t v) { AM_LOG_INFO("Count: {}", v); };
                 });

    // --- SliderFloat with a progress fill, normal (thin) track, circular thumb ---
    root.textLabel(caption("Progress", 190.0f));
    root.sliderFloat({
        .base = {.padding = UDim4::fromOffset(1.0f), .position = UDim2::fromOffset(170, 190), .size = UDim2::fromOffset(160, 28)},
        .style = dragStyle,
        .slider = {.thumb = {.backgroundColor = Color3(0.3f, 0.75f, 0.4f)},
                   .trackHeight = UDim::fromOffset(8.0f),
                   .fillColor = Color4(0.3f, 0.75f, 0.4f, 1.0f)},
        .format = "%.0f%%",
        .thumbShape = ShapeKind::CIRCLE,
        .min = 0.0f,
        .max = 100.0f,
        .value = &progressWithThumb,
    });

    // --- SliderFloat with a progress fill, thumb hidden, track at its default full height (proper bar look) ---
    root.textLabel(caption("Progress (no thumb)", 230.0f));
    root.sliderFloat({
        .base = {.padding = UDim4::fromOffset(3.0f), .position = UDim2::fromOffset(170, 230), .size = UDim2::fromOffset(160, 28)},
        .style = dragStyle,
        .slider = {.thumb = {.backgroundTransparency = 1.0f, .borderMode = BorderMode::NONE},
                   .fillColor = Color4(0.35f, 0.55f, 0.85f, 1.0f)},
        .format = "%.0f%%",
        .min = 0.0f,
        .max = 100.0f,
        .value = &progressNoThumb,
    });

    // --- SliderFloat with a progress fill and left-aligned, padded value text ---
    root.textLabel(caption("Progress (left text)", 270.0f));
    root.sliderFloat({
        .base = {.padding = UDim4::fromOffset(1.0f), .position = UDim2::fromOffset(170, 270), .size = UDim2::fromOffset(160, 28)},
        .style = dragStyle,
        .slider = {.thumb = {.backgroundColor = Color3(0.85f, 0.55f, 0.2f)},
                   .text = {.textXAlignment = TextXAlignment::LEFT},
                   .trackHeight = UDim::fromOffset(8.0f),
                   .fillColor = Color4(0.85f, 0.55f, 0.2f, 1.0f),
                   .labelPadding = 12.0f},
        .format = "%.0f%%",
        .thumbShape = ShapeKind::CIRCLE,
        .min = 0.0f,
        .max = 100.0f,
        .value = &progressLeftLabel,
    });

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
