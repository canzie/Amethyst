#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/ui_scope.h"
#include "modules/style.h"
#include "vk_context.h"

using namespace Amethyst;

static const char *SVG_GEAR = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="white">
  <path d="M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58a.49.49 0 0 0 .12-.61l-1.92-3.32a.49.49 0 0 0-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54a.484.484 0 0 0-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96a.49.49 0 0 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58a.49.49 0 0 0-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6A3.61 3.61 0 0 1 8.4 12 3.61 3.61 0 0 1 12 8.4a3.61 3.61 0 0 1 3.6 3.6 3.61 3.61 0 0 1-3.6 3.6z"/>
</svg>
)";

static const char *SVG_STAR = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="white">
  <path d="M12 2l3.09 6.26L22 9.27l-5 4.87 1.18 6.88L12 17.77l-6.18 3.25L7 14.14 2 9.27l6.91-1.01L12 2z"/>
</svg>
)";

static const char *SVG_ARROW_RIGHT = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="white">
  <path d="M10 6l6 6-6 6V6z"/>
</svg>
)";

static const char *SVG_HOME = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="white">
  <path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/>
</svg>
)";

static const char *SVG_HEART = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="white">
  <path d="M12 21.35l-1.45-1.32C5.4 15.36 2 12.28 2 8.5 2 5.42 4.42 3 7.5 3c1.74 0 3.41.81 4.5 2.09C13.09 3.81 14.76 3 16.5 3 19.58 3 22 5.42 22 8.5c0 3.78-3.4 6.86-8.55 11.54L12 21.35z"/>
</svg>
)";

static const char *SVG_CHECK = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="white">
  <path d="M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41L9 16.17z"/>
</svg>
)";

int main()
{
    Log::Init();

    Style::load(AMETHYST_ASSETS_DIR "/theme.ams");

    VkContext ctx;
    if (!contextInit(ctx, 900, 600, "Amethyst - SVG Demo")) {
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

    struct IconDef {
        const char *svg;
        const char *label;
        Color3 tint;
    };

    IconDef icons[] = {
        {SVG_GEAR, "Settings", {0.7f, 0.7f, 0.8f}},    {SVG_STAR, "Favorite", {1.0f, 0.85f, 0.2f}},
        {SVG_ARROW_RIGHT, "Next", {0.4f, 0.9f, 0.4f}}, {SVG_HOME, "Home", {0.5f, 0.7f, 1.0f}},
        {SVG_HEART, "Like", {1.0f, 0.4f, 0.4f}},       {SVG_CHECK, "Done", {0.3f, 1.0f, 0.5f}},
    };

    TextLabel *statusLabel = nullptr;

    UIScope ui(window);

    ui.textLabel({.base = {.position = {0.0f, 0.0f, 0.0f, 10.0f}, .size = {1.0f, 0.0f, 0.0f, 40.0f}},
                  .style = {.backgroundTransparency = 1.0f},
                  .text = {.fontSize = 20.0f,
                           .textColor = {1.0f, 1.0f, 1.0f, 1.0f},
                           .textXAlignment = TextXAlignment::CENTER,
                           .textYAlignment = TextYAlignment::CENTER},
                  .label = "SVG Icons"});

    float xOffset = 40.0f;
    for (auto &icon : icons) {
        ui.frame({.base = {.position = UDim2::fromOffset(xOffset, 70.0f), .size = UDim2::fromOffset(120.0f, 160.0f)},
                  .style = {.backgroundColor = {0.18f, 0.18f, 0.22f}, .cornerRadius = 8.0f}},
                 [&icon](FrameScope &f) {
                     f.imageLabel({.base = {.position = UDim2::fromOffset(28.0f, 20.0f), .size = UDim2::fromOffset(64.0f, 64.0f)},
                                   .style = {.backgroundTransparency = 1.0f},
                                   .image = {.imageColor = {icon.tint.r, icon.tint.g, icon.tint.b, 1.0f}},
                                   .svg = icon.svg});
                     f.textLabel({.base = {.position = {0.0f, 0.0f, 0.0f, 100.0f}, .size = {1.0f, 0.0f, 0.0f, 24.0f}},
                                  .style = {.backgroundTransparency = 1.0f},
                                  .text = {.fontSize = 14.0f,
                                           .textColor = {0.85f, 0.85f, 0.85f, 1.0f},
                                           .textXAlignment = TextXAlignment::CENTER,
                                           .textYAlignment = TextYAlignment::CENTER},
                                  .label = icon.label});
                 });
        xOffset += 135.0f;
    }

    ui.textLabel({.base = {.position = {0.0f, 0.0f, 0.0f, 260.0f}, .size = {1.0f, 0.0f, 0.0f, 30.0f}},
                  .style = {.backgroundTransparency = 1.0f},
                  .text = {.fontSize = 16.0f,
                           .textColor = {0.8f, 0.8f, 0.8f, 1.0f},
                           .textXAlignment = TextXAlignment::CENTER,
                           .textYAlignment = TextYAlignment::CENTER},
                  .label = "SVG Buttons (hover to see effect)"});

    ui.textLabel({.base = {.position = {0.0f, 0.0f, 0.0f, 540.0f}, .size = {1.0f, 0.0f, 0.0f, 24.0f}},
                  .style = {.backgroundTransparency = 1.0f},
                  .text = {.fontSize = 13.0f,
                           .textColor = {0.6f, 0.6f, 0.6f, 1.0f},
                           .textXAlignment = TextXAlignment::CENTER,
                           .textYAlignment = TextYAlignment::CENTER},
                  .label = "Click a button..."},
                 [&statusLabel](TextLabelScope &t) { statusLabel = &t.component; });

    float btnX = 100.0f;
    for (int i = 0; i < 6; i++) {
        ui.imageButton({.base = {.position = UDim2::fromOffset(btnX, 310.0f), .size = UDim2::fromOffset(80.0f, 80.0f)},
                        .style = {.backgroundColor = {0.22f, 0.22f, 0.28f}, .cornerRadius = 12.0f},
                        .image = {.imageColor = {icons[i].tint.r, icons[i].tint.g, icons[i].tint.b, 1.0f}},
                        .svg = icons[i].svg},
                       [statusLabel, name = icons[i].label](ImageButtonScope &btn) {
                           btn.component.onMouseButton1ClickCb = [statusLabel, name]() {
                               statusLabel->setText("Clicked: " + std::string(name));
                               return EventResult::CONSUMED;
                           };
                       });
        btnX += 115.0f;
    }

    amCtx.draw(window);

    int frameCount = 0;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

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
