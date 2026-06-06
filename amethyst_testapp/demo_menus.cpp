#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/ui_scope.h"
#include "modules/style.h"
#include "vk_context.h"

#include <string>

using namespace Amethyst;

static std::string g_lastAction = "(none)";

int main()
{
    Log::Init();

    Style::load(AMETHYST_ASSETS_DIR "/theme.toml");

    VkContext ctx;
    if (!contextInit(ctx, 1200, 800, "Amethyst - Menus Demo")) {
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

    bool running = true;

    Window window;
    window.absoluteSize = screenSize;
    window.absoluteRotation = 0.0f;
    window.setDisplayOrder(10);

    TextLabel *statusLabel = nullptr;
    Dropdown *toolDropdown = nullptr;

    UIScope(window)
        .menuBar({.base = {.position = {0.0f, 0.0f, 0.0f, 0.0f}, .size = {1.0f, 0.0f, 0.0f, 28.0f}},
                  .style = {.backgroundColor = {0.15f, 0.15f, 0.15f}, .backgroundTransparency = 0.0f, .borderPixelSize = 0.0f}},
                 [&running](MenuBarScope &mb) {
                     mb.menuItem("File", [&running](DropdownScope &d) {
                         d.action("New", [] { g_lastAction = "New"; });
                         d.action("Open", [] { g_lastAction = "Open"; });
                         d.submenu("Recent", [](DropdownScope &d) {
                             for (int i = 1; i <= 12; i++) {
                                 std::string name = "project_0" + std::to_string(i) + ".aml";
                                 d.action(name, [name] { g_lastAction = "Open recent: " + name; });
                             }
                         });
                         d.separator();
                         d.action("Quit", [&running] { running = false; });
                     });
                     mb.menuItem("Edit", [](DropdownScope &d) {
                         d.action("Undo", [] { g_lastAction = "Undo"; });
                         d.action("Redo", [] { g_lastAction = "Redo"; });
                         d.separator();
                         d.action("Cut", [] { g_lastAction = "Cut"; });
                         d.action("Copy", [] { g_lastAction = "Copy"; });
                         d.action("Paste", [] { g_lastAction = "Paste"; });
                     });
                     mb.menuItem("View", [](DropdownScope &d) {
                         d.toggle("Show Grid", [](bool v) { g_lastAction = v ? "Grid on" : "Grid off"; });
                         d.toggle("Show Ruler", [](bool v) { g_lastAction = v ? "Ruler on" : "Ruler off"; });
                     });
                 })
        .textLabel({.base = {.position = UDim2::fromOffset(40.0f, 60.0f), .size = UDim2::fromOffset(120.0f, 28.0f)},
                    .text = {.fontSize = 14.0f,
                             .textColor = {0.85f, 0.85f, 0.85f, 1.0f},
                             .textXAlignment = TextXAlignment::LEFT,
                             .textYAlignment = TextYAlignment::CENTER},
                    .label = "Active tool:"})
        .dropdown({.base = {.position = UDim2::fromOffset(160.0f, 60.0f), .size = UDim2::fromOffset(140.0f, 28.0f)},
                   .label = "Pencil \xe2\x96\xbe",
                   .dropdown = {.popupWidth = 140.0f}},
                  [&toolDropdown](DropdownScope &d) {
                      toolDropdown = &d.component;
                      d.action("Pencil", [&toolDropdown] {
                          toolDropdown->setText("Pencil \xe2\x96\xbe");
                          g_lastAction = "Tool: Pencil";
                      });
                      d.action("Brush", [&toolDropdown] {
                          toolDropdown->setText("Brush \xe2\x96\xbe");
                          g_lastAction = "Tool: Brush";
                      });
                      d.action("Eraser", [&toolDropdown] {
                          toolDropdown->setText("Eraser \xe2\x96\xbe");
                          g_lastAction = "Tool: Eraser";
                      });
                      d.action("Select", [&toolDropdown] {
                          toolDropdown->setText("Select \xe2\x96\xbe");
                          g_lastAction = "Tool: Select";
                      });
                  })
        .textLabel({.base = {.position = UDim2::fromOffset(40.0f, 110.0f), .size = UDim2::fromOffset(400.0f, 28.0f)},
                    .text = {.fontSize = 13.0f,
                             .textColor = {0.6f, 0.6f, 0.6f, 1.0f},
                             .textXAlignment = TextXAlignment::LEFT,
                             .textYAlignment = TextYAlignment::CENTER}},
                   [&statusLabel](TextLabelScope &t) { statusLabel = &t.component; })
        .tabBar({.base = {.position = UDim2::fromOffset(40.0f, 160.0f), .size = UDim2::fromOffset(700.0f, 220.0f)},
                 .style = {.backgroundColor = {0.14f, 0.14f, 0.17f}, .backgroundTransparency = 0.0f},
                 .tabBar = {.tabWidth = 110.0f, .tabOffset = 10.0f}},
                [](TabBarScope &tabs) {
                    tabs.tab("Scene", [](FrameScope &f) {
                        f.textLabel({.base = {.position = UDim2::fromOffset(10.0f, 10.0f), .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                     .style = {.backgroundTransparency = 1.0f},
                                     .text = {.fontSize = 13.0f,
                                              .textColor = {0.8f, 0.8f, 0.8f, 1.0f},
                                              .textYAlignment = TextYAlignment::CENTER},
                                     .label = "Scene graph \xe2\x80\x94 312 nodes, 18 lights"});
                    });
                    tabs.tab("Assets", [](FrameScope &f) {
                        f.textLabel({.base = {.position = UDim2::fromOffset(10.0f, 10.0f), .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                     .style = {.backgroundTransparency = 1.0f},
                                     .text = {.fontSize = 13.0f,
                                              .textColor = {0.8f, 0.8f, 0.8f, 1.0f},
                                              .textYAlignment = TextYAlignment::CENTER},
                                     .label = "Assets \xe2\x80\x94 84 meshes, 210 textures, 12 materials"});
                    });
                    tabs.tab("Console", [](FrameScope &f) {
                        f.textLabel({.base = {.position = UDim2::fromOffset(10.0f, 10.0f), .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                     .style = {.backgroundTransparency = 1.0f},
                                     .text = {.fontSize = 13.0f,
                                              .textColor = {0.5f, 0.9f, 0.5f, 1.0f},
                                              .textYAlignment = TextYAlignment::CENTER},
                                     .label = "> Engine initialised in 142ms"});
                    });
                });

    amCtx.draw(window);

    int frameCount = 0;
    double lastTime = glfwGetTime();
    std::string lastStatus;

    while (!glfwWindowShouldClose(ctx.window) && running) {
        glfwPollEvents();

        if (g_lastAction != lastStatus) {
            lastStatus = g_lastAction;
            statusLabel->setText("Last action: " + g_lastAction);
        }

        uint32_t imageIndex;
        if (!contextBeginFrame(ctx, imageIndex)) continue;

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
