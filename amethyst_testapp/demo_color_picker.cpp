#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/ui_scope.h"
#include "modules/style.h"
#include "vk_context.h"

#include <cstdio>
#include <string>

using namespace Amethyst;

static std::string formatColor3(const Color3 &c)
{
    int r = static_cast<int>(c.r * 255.0f + 0.5f);
    int g = static_cast<int>(c.g * 255.0f + 0.5f);
    int b = static_cast<int>(c.b * 255.0f + 0.5f);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X   rgb(%d, %d, %d)", r, g, b, r, g, b);
    return std::string(buffer);
}

static std::string formatColor4(const Color4 &c)
{
    int r = static_cast<int>(c.r * 255.0f + 0.5f);
    int g = static_cast<int>(c.g * 255.0f + 0.5f);
    int b = static_cast<int>(c.b * 255.0f + 0.5f);
    int a = static_cast<int>(c.a * 255.0f + 0.5f);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X   rgba(%d, %d, %d, %d)", r, g, b, a, r, g, b, a);
    return std::string(buffer);
}

int main()
{
    Log::Init();

    Style::load(AMETHYST_ASSETS_DIR "/theme.ams");

    VkContext ctx;
    if (!contextInit(ctx, 720, 520, "Amethyst - Color Picker Demo")) {
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

    Color3 rgbColor = Color3::fromHex(0x3498DB);
    Color4 rgbaColor = Color4(Color3::fromHex(0xE74C3C), 0.7f);

    Frame *rgbSwatch = nullptr;
    Frame *rgbaSwatch = nullptr;
    TextLabel *rgbLabel = nullptr;
    TextLabel *rgbaLabel = nullptr;

    UIScope root(window);
    root.frame({.base = {.size = UDim2::fromScale(1.0f, 1.0f)}, .style = {.backgroundColor = Color3::fromHex(0x1A1A1E)}});

    // --- RGB picker (Color3) ---
    root.textLabel({.base = {.position = UDim2::fromOffset(40, 20), .size = UDim2::fromOffset(260, 24)},
                    .style = {.backgroundTransparency = 1.0f},
                    .text = {.fontSize = 16.0f, .textColor = {1.0f, 1.0f, 1.0f, 1.0f}, .textYAlignment = TextYAlignment::CENTER},
                    .label = "RGB"});
    root.color3Picker({.base = {.padding = {{0.0f, 10.0f}, {0.0f, 10.0f}, {0.0f, 10.0f}, {0.0f, 10.0f}},
                                .position = UDim2::fromOffset(40, 50),
                                .size = UDim2::fromOffset(260, 300)},
                       .style = {.backgroundColor = Color3::fromHex(0x26262C), .cornerRadius = 8.0f},
                       .value = &rgbColor},
                      [&](Color3PickerScope &p) {
                          p.component.onValueChanged = [&](const Color3 &c) {
                              if (rgbSwatch != nullptr) {
                                  rgbSwatch->setBaseStyleProperties({.backgroundColor = c});
                              }
                              if (rgbLabel != nullptr) {
                                  rgbLabel->setText(formatColor3(c));
                              }
                          };
                      });
    root.frame({.base = {.position = UDim2::fromOffset(40, 360), .size = UDim2::fromOffset(40, 40)},
                .style = {.backgroundColor = rgbColor, .cornerRadius = 4.0f}},
               [&](FrameScope &f) { rgbSwatch = &f.component; });
    root.textLabel({.base = {.position = UDim2::fromOffset(90, 360), .size = UDim2::fromOffset(210, 40)},
                    .style = {.backgroundTransparency = 1.0f},
                    .text = {.fontSize = 13.0f, .textColor = {0.8f, 0.8f, 0.8f, 1.0f}, .textYAlignment = TextYAlignment::CENTER},
                    .label = formatColor3(rgbColor)},
                   [&](TextLabelScope &t) { rgbLabel = &t.component; });

    // --- RGBA picker (Color4) ---
    root.textLabel({.base = {.position = UDim2::fromOffset(360, 20), .size = UDim2::fromOffset(260, 24)},
                    .style = {.backgroundTransparency = 1.0f},
                    .text = {.fontSize = 16.0f, .textColor = {1.0f, 1.0f, 1.0f, 1.0f}, .textYAlignment = TextYAlignment::CENTER},
                    .label = "RGBA"});
    root.color4Picker({.base = {.padding = {{0.0f, 10.0f}, {0.0f, 10.0f}, {0.0f, 10.0f}, {0.0f, 10.0f}},
                                .position = UDim2::fromOffset(360, 50),
                                .size = UDim2::fromOffset(260, 320)},
                       .style = {.backgroundColor = Color3::fromHex(0x26262C), .cornerRadius = 8.0f},
                       .value = &rgbaColor},
                      [&](Color4PickerScope &p) {
                          p.component.onValueChanged = [&](const Color4 &c) {
                              if (rgbaSwatch != nullptr) {
                                  rgbaSwatch->setBaseStyleProperties(
                                      {.backgroundColor = Color3(c), .backgroundTransparency = 1.0f - c.a});
                              }
                              if (rgbaLabel != nullptr) {
                                  rgbaLabel->setText(formatColor4(c));
                              }
                          };
                      });
    root.frame(
        {.base = {.position = UDim2::fromOffset(360, 380), .size = UDim2::fromOffset(40, 40)},
         .style = {.backgroundColor = Color3(rgbaColor), .backgroundTransparency = 1.0f - rgbaColor.a, .cornerRadius = 4.0f}},
        [&](FrameScope &f) { rgbaSwatch = &f.component; });
    root.textLabel({.base = {.position = UDim2::fromOffset(410, 380), .size = UDim2::fromOffset(260, 40)},
                    .style = {.backgroundTransparency = 1.0f},
                    .text = {.fontSize = 13.0f, .textColor = {0.8f, 0.8f, 0.8f, 1.0f}, .textYAlignment = TextYAlignment::CENTER},
                    .label = formatColor4(rgbaColor)},
                   [&](TextLabelScope &t) { rgbaLabel = &t.component; });

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
        amCtx.sync(static_cast<void *>(cmd));
        backend.record(cmd, amCtx.getDrawList());
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
