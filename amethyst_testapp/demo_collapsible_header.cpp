#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/ui_scope.h"
#include "modules/style.h"
#include "vk_context.h"

#include <string>

using namespace Amethyst;

static const char *SVG_ARROW = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="white">
  <path d="M10 6l6 6-6 6V6z"/>
</svg>
)";

static const char *SVG_GEAR = R"(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="white">
  <path d="M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58a.49.49 0 0 0 .12-.61l-1.92-3.32a.49.49 0 0 0-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54a.484.484 0 0 0-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96a.49.49 0 0 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58a.49.49 0 0 0-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6A3.61 3.61 0 0 1 8.4 12 3.61 3.61 0 0 1 12 8.4a3.61 3.61 0 0 1 3.6 3.6 3.61 3.61 0 0 1-3.6 3.6z"/>
</svg>
)";

int main()
{
    Log::Init();

    Style::load(AMETHYST_ASSETS_DIR "/theme.toml");

    VkContext ctx;
    if (!contextInit(ctx, 900, 700, "Amethyst - CollapsibleHeader Demo")) {
        AM_LOG_ERROR("Failed to initialize Vulkan context");
        return 1;
    }

    AmethystContext amCtx;
    if (!amCtx.loadFont(AMETHYST_ASSETS_DIR "/fonts/OpenSans-Regular.ttf")) {
        AM_LOG_ERROR("Failed to load font");
        return 1;
    }

    VulkanInitInfo initInfo{};
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

    GLFWInitInfo glfwInfo{};
    glfwInfo.window = ctx.window;

    VkBackend backend;
    backend.init(initInfo, glfwInfo);

    amCtx.init(backend);

    glm::vec2 screenSize = {
        static_cast<float>(ctx.swapchainExtent.width),
        static_cast<float>(ctx.swapchainExtent.height),
    };

    Window window;
    window.absoluteSize = screenSize;
    window.absoluteRotation = 0.0f;
    window.setDisplayOrder(10);

    TextLabel *statusLabel = nullptr;

    UIScope(window)
        .textLabel({.backgroundColor = {0.12f, 0.12f, 0.14f},
                    .backgroundTransparency = 0.0f,
                    .position = {0.0f, 0.0f, 0.0f, 0.0f},
                    .size = {1.0f, 0.0f, 0.0f, 24.0f}},
                   {.fontSize = 13.0f,
                    .textColor = {0.7f, 0.7f, 0.7f, 1.0f},
                    .textXAlignment = TextXAlignment::LEFT,
                    .textYAlignment = TextYAlignment::CENTER,
                    .text = "Click any header to toggle"},
                   [&statusLabel](TextLabelScope &t) { statusLabel = &t.component; })
        .scrollingFrame(
            {.backgroundColor = Color3::fromHex(0x1A1A1E),
             .clipsDescendants = true,
             .position = UDim2::fromOffset(10.0f, 29.0f),
             .size = {1.0f, -20.0f, 1.0f, -34.0f}},
            {.canvasSize = UDim2::fromOffset(880, 1200),
             .scrollBarColor = {0.2f, 0.2f, 0.25f},
             .scrollBarThumbColor = {0.45f, 0.45f, 0.55f}},
            [statusLabel](ScrollingFrameScope &sf) {
                // --- Section 1: General Settings ---
                sf.collapsibleHeader(
                      {.backgroundColor = {0.16f, 0.16f, 0.18f},
                       .backgroundTransparency = 0.0f,
                       .cornerRadius = 4.0f,
                       .position = {0.0f, 0.0f, 0.0f, 0.0f},
                       .size = {1.0f, 0.0f, 0.0f, 180.0f}},
                      {.expanded = true,
                       .title = {.fontSize = 15.0f, .textColor = {1.0f, 1.0f, 1.0f, 1.0f}, .text = "General Settings"},
                       .headerHeight = 32.0f,
                       .headerColor = {0.22f, 0.28f, 0.38f},
                       .headerCornerRadius = 4.0f,
                       .indicatorColor = {0.8f, 0.85f, 1.0f, 1.0f}},
                      [statusLabel](CollapsibleHeaderScope &ch) {
                          ch.component.onToggled = [statusLabel](bool exp) {
                              statusLabel->setTextProperties(
                                  {.text = std::string("General Settings: ") + (exp ? "expanded" : "collapsed")});
                          };
                          ch.textLabel({.backgroundTransparency = 1.0f,
                                        .position = UDim2::fromOffset(10.0f, 10.0f),
                                        .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                       {.fontSize = 13.0f,
                                        .textColor = {0.8f, 0.8f, 0.8f, 1.0f},
                                        .textYAlignment = TextYAlignment::CENTER,
                                        .text = "Resolution: 1920x1080"});
                          ch.textLabel({.backgroundTransparency = 1.0f,
                                        .position = UDim2::fromOffset(10.0f, 38.0f),
                                        .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                       {.fontSize = 13.0f,
                                        .textColor = {0.8f, 0.8f, 0.8f, 1.0f},
                                        .textYAlignment = TextYAlignment::CENTER,
                                        .text = "Fullscreen: Off"});
                          ch.textLabel({.backgroundTransparency = 1.0f,
                                        .position = UDim2::fromOffset(10.0f, 66.0f),
                                        .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                       {.fontSize = 13.0f,
                                        .textColor = {0.8f, 0.8f, 0.8f, 1.0f},
                                        .textYAlignment = TextYAlignment::CENTER,
                                        .text = "VSync: Enabled"});
                      })
                    // --- Section 2: Audio (starts collapsed) ---
                    .collapsibleHeader({.backgroundColor = {0.16f, 0.16f, 0.18f},
                                        .backgroundTransparency = 0.0f,
                                        .cornerRadius = 4.0f,
                                        .position = {0.0f, 0.0f, 0.0f, 190.0f},
                                        .size = {1.0f, 0.0f, 0.0f, 150.0f}},
                                       {.expanded = false,
                                        .title = {.fontSize = 15.0f, .textColor = {1.0f, 1.0f, 1.0f, 1.0f}, .text = "Audio"},
                                        .headerHeight = 32.0f,
                                        .headerColor = {0.28f, 0.22f, 0.32f},
                                        .headerCornerRadius = 4.0f,
                                        .indicatorColor = {0.9f, 0.75f, 1.0f, 1.0f}},
                                       [statusLabel](CollapsibleHeaderScope &ch) {
                                           ch.component.onToggled = [statusLabel](bool exp) {
                                               statusLabel->setTextProperties(
                                                   {.text = std::string("Audio: ") + (exp ? "expanded" : "collapsed")});
                                           };
                                           ch.textLabel({.backgroundTransparency = 1.0f,
                                                         .position = UDim2::fromOffset(10.0f, 10.0f),
                                                         .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                                        {.fontSize = 13.0f,
                                                         .textColor = {0.8f, 0.8f, 0.8f, 1.0f},
                                                         .textYAlignment = TextYAlignment::CENTER,
                                                         .text = "Master Volume: 80%"});
                                           ch.textLabel({.backgroundTransparency = 1.0f,
                                                         .position = UDim2::fromOffset(10.0f, 38.0f),
                                                         .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                                        {.fontSize = 13.0f,
                                                         .textColor = {0.8f, 0.8f, 0.8f, 1.0f},
                                                         .textYAlignment = TextYAlignment::CENTER,
                                                         .text = "Music: 60%"});
                                           ch.textLabel({.backgroundTransparency = 1.0f,
                                                         .position = UDim2::fromOffset(10.0f, 66.0f),
                                                         .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                                        {.fontSize = 13.0f,
                                                         .textColor = {0.8f, 0.8f, 0.8f, 1.0f},
                                                         .textYAlignment = TextYAlignment::CENTER,
                                                         .text = "SFX: 100%"});
                                       })
                    // --- Section 3: Graphics (table inside) ---
                    .collapsibleHeader({.backgroundColor = {0.16f, 0.16f, 0.18f},
                                        .backgroundTransparency = 0.0f,
                                        .cornerRadius = 4.0f,
                                        .position = {0.0f, 0.0f, 0.0f, 350.0f},
                                        .size = {1.0f, 0.0f, 0.0f, 210.0f}},
                                       {.title = {.fontSize = 15.0f, .textColor = {1.0f, 1.0f, 1.0f, 1.0f}, .text = "Graphics"},
                                        .headerHeight = 32.0f,
                                        .headerColor = {0.2f, 0.32f, 0.25f},
                                        .headerCornerRadius = 4.0f,
                                        .indicatorSize = 12.0f,
                                        .indicatorColor = {0.6f, 1.0f, 0.7f, 1.0f}},
                                       [statusLabel](CollapsibleHeaderScope &ch) {
                                           ch.component.onToggled = [statusLabel](bool exp) {
                                               statusLabel->setTextProperties(
                                                   {.text = std::string("Graphics: ") + (exp ? "expanded" : "collapsed")});
                                           };
                                           ch.table({.backgroundColor = {0.14f, 0.14f, 0.16f},
                                                     .backgroundTransparency = 0.0f,
                                                     .position = UDim2::fromOffset(5.0f, 5.0f),
                                                     .size = {1.0f, -10.0f, 0.0f, 168.0f}},
                                                    {.rowHeight = 28.0f,
                                                     .cellPadding = {{2.0f, 0.0f}, {8.0f, 0.0f}, {2.0f, 0.0f}, {8.0f, 0.0f}},
                                                     .showColumnSeparators = true,
                                                     .columnSeparatorColor = {0.3f, 0.3f, 0.35f, 0.5f},
                                                     .showHeader = true},
                                                    [](TableScope &t) {
                                                        t.column("Setting", 0.55f).column("Value", 0.45f);

                                                        struct SettingRow {
                                                            const char *setting;
                                                            const char *value;
                                                        };
                                                        SettingRow graphicsRows[] = {
                                                            {"Shadow Quality", "Ultra"},    {"Anti-Aliasing", "TAA"},
                                                            {"Texture Quality", "High"},    {"Draw Distance", "Far"},
                                                            {"Ambient Occlusion", "HBAO+"}, {"Anisotropic", "16x"},
                                                        };

                                                        for (auto &row : graphicsRows) {
                                                            t.row([&row](TableRowScope &r) {
                                                                r.cell([&row](UIScope &cell) {
                                                                    cell.textLabel({.backgroundTransparency = 1.0f,
                                                                                    .size = UDim2::fromScale(1.0f, 1.0f)},
                                                                                   {.fontSize = 13.0f,
                                                                                    .textColor = {0.7f, 0.7f, 0.7f, 1.0f},
                                                                                    .textYAlignment = TextYAlignment::CENTER,
                                                                                    .text = row.setting});
                                                                });
                                                                r.cell([&row](UIScope &cell) {
                                                                    cell.textLabel({.backgroundTransparency = 1.0f,
                                                                                    .size = UDim2::fromScale(1.0f, 1.0f)},
                                                                                   {.fontSize = 13.0f,
                                                                                    .textColor = {0.5f, 0.9f, 0.6f, 1.0f},
                                                                                    .textYAlignment = TextYAlignment::CENTER,
                                                                                    .text = row.value});
                                                                });
                                                            });
                                                        }
                                                    });
                                       })
                    // --- Section 4: No indicator ---
                    .collapsibleHeader(
                        {.backgroundColor = {0.16f, 0.16f, 0.18f},
                         .backgroundTransparency = 0.0f,
                         .cornerRadius = 4.0f,
                         .position = {0.0f, 0.0f, 0.0f, 560.0f},
                         .size = {1.0f, 0.0f, 0.0f, 120.0f}},
                        {.title = {.fontSize = 15.0f, .textColor = {1.0f, 1.0f, 1.0f, 1.0f}, .text = "Controls (no indicator)"},
                         .headerHeight = 32.0f,
                         .headerColor = {0.32f, 0.22f, 0.2f},
                         .headerCornerRadius = 4.0f,
                         .showIndicator = false},
                        [statusLabel](CollapsibleHeaderScope &ch) {
                            ch.component.onToggled = [statusLabel](bool exp) {
                                statusLabel->setTextProperties(
                                    {.text = std::string("Controls: ") + (exp ? "expanded" : "collapsed")});
                            };
                            ch.textLabel({.backgroundTransparency = 1.0f,
                                          .position = UDim2::fromOffset(10.0f, 10.0f),
                                          .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                         {.fontSize = 13.0f,
                                          .textColor = {0.8f, 0.8f, 0.8f, 1.0f},
                                          .textYAlignment = TextYAlignment::CENTER,
                                          .text = "Mouse Sensitivity: 2.5"});
                            ch.textLabel({.backgroundTransparency = 1.0f,
                                          .position = UDim2::fromOffset(10.0f, 38.0f),
                                          .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                         {.fontSize = 13.0f,
                                          .textColor = {0.8f, 0.8f, 0.8f, 1.0f},
                                          .textYAlignment = TextYAlignment::CENTER,
                                          .text = "Invert Y: No"});
                        })
                    // --- Section 5: custom SVG indicator + custom header ---
                    .collapsibleHeader({.backgroundColor = {0.16f, 0.16f, 0.18f},
                                        .backgroundTransparency = 0.0f,
                                        .cornerRadius = 4.0f,
                                        .position = {0.0f, 0.0f, 0.0f, 690.0f},
                                        .size = {1.0f, 0.0f, 0.0f, 160.0f}},
                                       {.headerHeight = 36.0f, .headerCornerRadius = 4.0f, .indicatorSize = 18.0f},
                                       [statusLabel](CollapsibleHeaderScope &ch) {
                                           ch.component.onToggled = [statusLabel](bool exp) {
                                               statusLabel->setTextProperties(
                                                   {.text = std::string("Shader Pipeline: ") + (exp ? "expanded" : "collapsed")});
                                           };

                                           ch.indicator([](UIScope &indicator) {
                                               indicator.imageLabel({.backgroundTransparency = 1.0f, .borderPixelSize = 0.0f},
                                                                    {.imageColor = {0.75f, 0.85f, 1.0f, 1.0f}, .svg = SVG_ARROW});
                                           });

                                           ch.header([statusLabel](FrameScope &header) {
                                               header.textLabel({.backgroundTransparency = 1.0f,
                                                                 .position = {0.0f, 0.0f, 0.0f, 0.0f},
                                                                 .size = {1.0f, -36.0f, 1.0f, 0.0f}},
                                                                {.fontSize = 15.0f,
                                                                 .textColor = {1.0f, 1.0f, 1.0f, 1.0f},
                                                                 .textXAlignment = TextXAlignment::LEFT,
                                                                 .textYAlignment = TextYAlignment::CENTER,
                                                                 .text = "Shader Pipeline"});
                                               header.imageButton({.anchorPoint = {1.0f, 0.5f},
                                                                   .backgroundColor = {0.28f, 0.28f, 0.38f},
                                                                   .cornerRadius = 4.0f,
                                                                   .position = {1.0f, -4.0f, 0.5f, 0.0f},
                                                                   .size = UDim2::fromOffset(22.0f, 22.0f),
                                                                   .zIndex = 101},
                                                                  {.imageColor = {0.7f, 0.7f, 0.85f, 1.0f}, .svg = SVG_GEAR}, {},
                                                                  [statusLabel](ImageButtonScope &btn) {
                                                                      btn.component.onMouseButton1ClickCb = [statusLabel]() {
                                                                          statusLabel->setTextProperties(
                                                                              {.text = "Shader Pipeline: options clicked"});
                                                                          return EventResult::CONSUMED;
                                                                      };
                                                                  });
                                           });

                                           ch.textLabel({.backgroundTransparency = 1.0f,
                                                         .position = UDim2::fromOffset(10.0f, 10.0f),
                                                         .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                                        {.fontSize = 13.0f,
                                                         .textColor = {0.75f, 0.85f, 1.0f, 1.0f},
                                                         .textYAlignment = TextYAlignment::CENTER,
                                                         .text = "Vertex:   mesh.vert.spv"});
                                           ch.textLabel({.backgroundTransparency = 1.0f,
                                                         .position = UDim2::fromOffset(10.0f, 38.0f),
                                                         .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                                        {.fontSize = 13.0f,
                                                         .textColor = {0.75f, 0.85f, 1.0f, 1.0f},
                                                         .textYAlignment = TextYAlignment::CENTER,
                                                         .text = "Fragment: mesh.frag.spv"});
                                           ch.textLabel({.backgroundTransparency = 1.0f,
                                                         .position = UDim2::fromOffset(10.0f, 66.0f),
                                                         .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                                        {.fontSize = 13.0f,
                                                         .textColor = {0.6f, 0.6f, 0.65f, 1.0f},
                                                         .textYAlignment = TextYAlignment::CENTER,
                                                         .text = "Specialisation: SKINNING=1, SHADOWS=1"});
                                       });
            });

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

        amCtx.sync(static_cast<void *>(cmd));
        amCtx.draw(window);
        backend.record(cmd);
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
