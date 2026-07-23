#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/ui_scope.h"
#include "modules/style.h"
#include "vk_context.h"

using namespace Amethyst;

static constexpr float SECTION_GAP = 10.0f;
static constexpr float ROW_SIMPLE = 64.0f;
static constexpr float ROW_PSEUDO = 64.0f;
static constexpr float ROW_TABBAR = 190.0f;
static constexpr float ROW_LEAK = 64.0f;
static constexpr float ROW_MENUBAR = 60.0f;
static constexpr float ROW_HEADER = 110.0f;
static constexpr float CAPTION_WIDTH = 380.0f;

static constexpr float CANVAS_HEIGHT = ROW_SIMPLE * 4.0f + ROW_PSEUDO + ROW_TABBAR * 3.0f + ROW_LEAK + ROW_MENUBAR + ROW_HEADER +
                                       10.0f * SECTION_GAP + 20.0f;

static void addCaption(UIScope &row, const char *text)
{
    row.textLabel({
        .base = {.position = UDim2::fromOffset(0.0f, 4.0f), .size = UDim2(0.0f, CAPTION_WIDTH, 1.0f, -8.0f)},
        .style = {.backgroundTransparency = 1.0f},
        .text = {.fontSize = 13.0f,
                 .textColor = {0.75f, 0.75f, 0.75f, 1.0f},
                 .textYAlignment = TextYAlignment::TOP,
                 .textWrapped = true},
        .label = text,
    });
}

int main()
{
    Log::Init();
    AM_LOG_INFO("Amethyst Selector Showcase");

    Style::load(AMETHYST_ASSETS_DIR "/theme_selector_demo.ams");

    VkContext ctx;
    if (!contextInit(ctx, 1100, 800, "Amethyst - Selector Showcase")) {
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

    vec2 screenSize = {static_cast<float>(ctx.swapchainExtent.width), static_cast<float>(ctx.swapchainExtent.height)};

    window.absoluteSize = screenSize;
    window.absoluteRotation = 0.0f;
    window.setDisplayOrder(10);

    UIScope(window)
        .textLabel({.base = {.position = {0.0f, 0.0f, 0.0f, 0.0f}, .size = {1.0f, 0.0f, 0.0f, 24.0f}},
                    .style = {.backgroundColor = {0.12f, 0.12f, 0.14f}, .backgroundTransparency = 0.0f},
                    .text = {.fontSize = 13.0f,
                             .textColor = {0.7f, 0.7f, 0.7f, 1.0f},
                             .textXAlignment = TextXAlignment::LEFT,
                             .textYAlignment = TextYAlignment::CENTER},
                    .label = "Selector grammar showcase -- see theme_selector_demo.ams for every rule"})
        .scrollingFrame(
            {.base = {.clipsDescendants = true, .position = UDim2::fromOffset(10.0f, 29.0f), .size = {1.0f, -20.0f, 1.0f, -34.0f}},
             .style = {.backgroundColor = Color3::fromHex(0x101010)},
             .scroll = {.canvasSize = UDim2(1.0f, 0.0f, 0.0f, CANVAS_HEIGHT),
                        .scrollBarColor = {0.2f, 0.2f, 0.25f},
                        .scrollBarThumbColor = {0.45f, 0.45f, 0.55f}}},
            [](ScrollingFrameScope &sf) {
                auto *layout = sf.component.addExtension<UIListLayout>();
                layout->innerPadding = UDim::fromOffset(SECTION_GAP);
                uint32_t order = 0;

                // --- 1. Bare type selector ---
                sf.frame({.base = {.layoutOrder = order++, .size = {1.0f, 0.0f, 0.0f, ROW_SIMPLE}}}, [](FrameScope &row) {
                    addCaption(row, "1. Bare type -- `frame { background-color: #262626 }`");
                    row.frame({.base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 10.0f, 8.0f),
                                        .size = UDim2::fromOffset(120.0f, ROW_SIMPLE - 16.0f)}});
                });

                // --- 2. Class only ---
                sf.frame({.base = {.layoutOrder = order++, .size = {1.0f, 0.0f, 0.0f, ROW_SIMPLE}}}, [](FrameScope &row) {
                    addCaption(row, "2. Class only -- `.class-only { background-color: red }`");
                    row.frame({.classes = {"class-only"},
                               .base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 10.0f, 8.0f),
                                        .size = UDim2::fromOffset(120.0f, ROW_SIMPLE - 16.0f)}});
                });

                // --- 3. Type + class, scoped to frame ---
                sf.frame({.base = {.layoutOrder = order++, .size = {1.0f, 0.0f, 0.0f, ROW_SIMPLE}}}, [](FrameScope &row) {
                    addCaption(row, "3. Type + class -- `frame.type-class-only` (blue). Same class on a "
                                    "text-button below stays unaffected: type-scoped.");
                    row.frame({.classes = {"type-class-only"},
                               .base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 10.0f, 8.0f),
                                        .size = UDim2::fromOffset(120.0f, ROW_SIMPLE - 16.0f)}});
                    row.textButton({.classes = {"type-class-only"},
                                    .base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 140.0f, 8.0f),
                                             .size = UDim2::fromOffset(120.0f, ROW_SIMPLE - 16.0f)},
                                    .text = {.textXAlignment = TextXAlignment::CENTER, .textYAlignment = TextYAlignment::CENTER},
                                    .label = "unaffected"});
                });

                // --- 4. Comma-separated selector list ---
                sf.frame({.base = {.layoutOrder = order++, .size = {1.0f, 0.0f, 0.0f, ROW_SIMPLE}}}, [](FrameScope &row) {
                    addCaption(row, "4. Comma list -- `.list-a, .list-b { background-color: cyan }`");
                    row.frame({.classes = {"list-a"},
                               .base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 10.0f, 8.0f),
                                        .size = UDim2::fromOffset(120.0f, ROW_SIMPLE - 16.0f)}});
                    row.frame({.classes = {"list-b"},
                               .base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 140.0f, 8.0f),
                                        .size = UDim2::fromOffset(120.0f, ROW_SIMPLE - 16.0f)}});
                });

                // --- 5. Pseudo-states on a class ---
                sf.frame({.base = {.layoutOrder = order++, .size = {1.0f, 0.0f, 0.0f, ROW_PSEUDO}}}, [](FrameScope &row) {
                    addCaption(row, "5. `.pseudo-demo:hover/:pressed/:disabled` -- hover/press me");
                    row.textButton({.classes = {"pseudo-demo"},
                                    .base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 10.0f, 8.0f),
                                             .size = UDim2::fromOffset(140.0f, ROW_PSEUDO - 16.0f)},
                                    .text = {.textXAlignment = TextXAlignment::CENTER, .textYAlignment = TextYAlignment::CENTER},
                                    .label = "hover/press"});
                    row.textButton({.classes = {"pseudo-demo"},
                                    .base = {.interactable = false,
                                             .position = UDim2::fromOffset(CAPTION_WIDTH + 160.0f, 8.0f),
                                             .size = UDim2::fromOffset(140.0f, ROW_PSEUDO - 16.0f)},
                                    .text = {.textXAlignment = TextXAlignment::CENTER, .textYAlignment = TextYAlignment::CENTER},
                                    .label = "disabled"});
                });

                // --- 6. type#part baseline, shared globally by every TabBar ---
                sf.frame({.base = {.layoutOrder = order++, .size = {1.0f, 0.0f, 0.0f, ROW_TABBAR}}}, [](FrameScope &row) {
                    addCaption(row, "6. `tab-bar#tab` baseline (blue/hover-green/press-red). No class "
                                    "needed; applies to every TabBar's tabs.");
                    row.tabBar(
                        {.base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 10.0f, 0.0f),
                                  .size = UDim2(1.0f, -(CAPTION_WIDTH + 20.0f), 0.0f, ROW_TABBAR)}},
                        [](TabBarScope &tabs) {
                            tabs.tab("Plain A", [](FrameScope &) {});
                            tabs.tab("Plain B", [](FrameScope &) {});
                        });
                });

                // --- 7. Full specificity ladder ---
                sf.frame({.base = {.layoutOrder = order++, .size = {1.0f, 0.0f, 0.0f, ROW_TABBAR}}}, [](FrameScope &row) {
                    addCaption(row, "7. Specificity ladder on class \"ladder\", applied to each tab: "
                                    "frame(red) < .ladder(orange) < frame.ladder(yellow) < tab-bar#tab(blue) "
                                    "< .ladder#tab(cyan) -- cyan wins, white on hover. The bar itself (behind "
                                    "the tabs) is also class \"ladder\", so its own background is plain "
                                    "orange from `.ladder` -- classes aren't type-scoped unless you write "
                                    "`frame.ladder`.");
                    row.tabBar(
                        {.classes = {"ladder"},
                         .base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 10.0f, 0.0f),
                                  .size = UDim2(1.0f, -(CAPTION_WIDTH + 20.0f), 0.0f, ROW_TABBAR)}},
                        [](TabBarScope &tabs) {
                            tabs.tab("Rung A", [](FrameScope &) {});
                            tabs.tab("Rung B", [](FrameScope &) {});
                        });
                });

                // --- 8. Leak check: same class, no part context ---
                sf.frame({.base = {.layoutOrder = order++, .size = {1.0f, 0.0f, 0.0f, ROW_LEAK}}}, [](FrameScope &row) {
                    addCaption(row, "8. Leak check -- a text-button (not a frame, not a tab) also "
                                    "carries class \"ladder\". Only the plain `.ladder` rule can match, "
                                    "so it stays orange; .ladder#tab never applies here.");
                    row.textButton({.classes = {"ladder"},
                                    .base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 10.0f, 8.0f),
                                             .size = UDim2::fromOffset(120.0f, ROW_LEAK - 16.0f)},
                                    .text = {.textXAlignment = TextXAlignment::CENTER, .textYAlignment = TextYAlignment::CENTER},
                                    .label = "orange"});
                });

                // --- 9. class#part propagating into a nested child ---
                sf.frame({.base = {.layoutOrder = order++, .size = {1.0f, 0.0f, 0.0f, ROW_TABBAR}}}, [](FrameScope &row) {
                    addCaption(row, "9. Class \"showcase\" on the TabBar itself: every tab's frame turns "
                                    "magenta (.showcase#tab) and its label text stays green (.showcase) "
                                    "at all times, purely from propagated classes. Hover turns the frame "
                                    "orange (.showcase#tab:hover); the text stays green throughout.");
                    row.tabBar(
                        {.classes = {"showcase"},
                         .base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 10.0f, 0.0f),
                                  .size = UDim2(1.0f, -(CAPTION_WIDTH + 20.0f), 0.0f, ROW_TABBAR)}},
                        [](TabBarScope &tabs) {
                            auto makeTab = [](TabScope &t, const char *text) {
                                t.label([text](FrameScope &lf) {
                                    lf.textLabel({.classes = {"showcase"},
                                                  .base = {.size = UDim2::fromScale(1.0f, 1.0f)},
                                                  .style = {.backgroundTransparency = 1.0f},
                                                  .text = {.textXAlignment = TextXAlignment::CENTER,
                                                           .textYAlignment = TextYAlignment::CENTER},
                                                  .label = text});
                                });
                            };
                            tabs.tab([&makeTab](TabScope &t) { makeTab(t, "Magenta A"); });
                            tabs.tab([&makeTab](TabScope &t) { makeTab(t, "Magenta B"); });
                        });
                });

                // --- 10. class#part on MenuBar's entry part ---
                sf.frame({.base = {.layoutOrder = order++, .size = {1.0f, 0.0f, 0.0f, ROW_MENUBAR}}}, [](FrameScope &row) {
                    addCaption(row, "10. `.mb-demo#entry` -- MenuBar entries turn magenta, cyan on "
                                    "hover, purely from the MenuBar's own class.");
                    row.menuBar(
                        {.classes = {"mb-demo"},
                         .base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 10.0f, 8.0f),
                                  .size = UDim2::fromOffset(260.0f, ROW_MENUBAR - 16.0f)}},
                        [](MenuBarScope &mb) {
                            mb.menuItem("File", [](DropdownScope &d) { d.action("New"); });
                            mb.menuItem("Edit", [](DropdownScope &d) { d.action("Undo"); });
                        });
                });

                // --- 11. class#part on CollapsibleHeader's header part ---
                sf.frame({.base = {.layoutOrder = order++, .size = {1.0f, 0.0f, 0.0f, ROW_HEADER}}}, [](FrameScope &row) {
                    addCaption(row, "11. `.ch-demo#header` -- header bar turns yellow, purely from the "
                                    "CollapsibleHeader's own class.");
                    row.collapsibleHeader(
                        {.classes = {"ch-demo"},
                         .base = {.position = UDim2::fromOffset(CAPTION_WIDTH + 10.0f, 0.0f),
                                  .size = UDim2(1.0f, -(CAPTION_WIDTH + 20.0f), 0.0f, ROW_HEADER)},
                         .header = {.titleStyle = {.textColor = {1.0f, 1.0f, 1.0f, 1.0f}}},
                         .title = "Yellow header"},
                        [](CollapsibleHeaderScope &ch) {
                            ch.textLabel({.base = {.position = UDim2::fromOffset(10.0f, 40.0f), .size = {1.0f, -20.0f, 0.0f, 24.0f}},
                                          .style = {.backgroundTransparency = 1.0f},
                                          .text = {.fontSize = 13.0f, .textColor = {0.8f, 0.8f, 0.8f, 1.0f}},
                                          .label = "Body content"});
                        });
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
