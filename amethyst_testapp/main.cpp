#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/canvas.h"
#include "components/checkbox.h"
#include "components/common.h"
#include "components/tree_view.h"
#include "components/ui_scope.h"
#include "modules/style.h"
#include "parsers/config/layout_config.h"
#include "utils/profiling.h"
#include "vk_context.h"

#include <string>
#include <utility>
#include <vector>

using namespace Amethyst;

int main()
{
    Amethyst::Log::Init();
    AM_LOG_INFO("Amethyst Test App");

    Amethyst::Style::load(AMETHYST_ASSETS_DIR "/theme.ams");

    VkContext ctx;
    if (!contextInit(ctx, 1000, 1000, "Amethyst Test")) {
        AM_LOG_ERROR("Failed to initialize Vulkan context");
        return 1;
    }

    Amethyst::AmethystContext amCtx;
    if (!amCtx.loadFont(AMETHYST_ASSETS_DIR "/fonts/OpenSans-Regular.ttf")) {
        AM_LOG_ERROR("Failed to load font");
        return 1;
    }

    Amethyst::AmVulkanInitInfo initInfo{};
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

    Amethyst::Window window;

    Amethyst::AmGlfwInitInfo glfwInfo{};
    glfwInfo.window = ctx.window;
    glfwInfo.uiWindow = &window;

    Amethyst::AmVulkanBackend backend;
    backend.init(initInfo, glfwInfo);

    amCtx.init(backend);

    vec2 screenSize = {static_cast<float>(ctx.swapchainExtent.width), static_cast<float>(ctx.swapchainExtent.height)};

    window.absoluteSize = screenSize;
    window.absoluteRotation = 0.0f;
    window.setDisplayOrder(10);

    bool running = true;
    Amethyst::DockingLayer *dockingLayer = nullptr;

    TextureInfo checkerboardTex = createCheckerboardTexture(ctx, 64, 8);
    Amethyst::AmTextureId checkerboardId = backend.registerTexture(checkerboardTex.view, checkerboardTex.sampler);

    float sliderFloatValue = 50.0f;
    int sliderIntValue = 3;
    bool checkboxValue = true;

    Amethyst::UIScope(window)
        // --- Draggable frames ---
        .frame(
            {
                .base = {
                    .position = Amethyst::UDim2::fromOffset(200, 200),
                    .size = Amethyst::UDim2::fromOffset(300, 200),
                },
                .style = {
                    .backgroundColor = Color3::fromGradient(
                        Gradient::linear(45.0f, {{0.0f, Color3::fromHex(0xFF5555)}, {1.0f, Color3::fromHex(0x5555FF)}})),
                    .borderMode = Amethyst::BorderMode::INSET,
                    .borderPixelSize = 10.0f,
                    .borderColor = {1.0f, 1.0f, 1.0f},
                    .cornerRadius = 10.0f,
                },
            },
            [&](Amethyst::FrameScope &f) {
                f.component.name = "Frame 1";
                f.component.addExtension<Amethyst::UIDragDetector>();
                f.frame(
                    {
                        .base = {
                            .anchorPoint = {0.5f, 0.5f},
                            .position = Amethyst::UDim2::fromOffset(0, 0),
                            .size = Amethyst::UDim2::fromScale(0.5f, 0.5f),
                        },
                        .style = {
                            .backgroundColor = {0.2f, 0.9f, 0.2f},
                            .cornerRadius = 0.0f,
                        },
                    },
                    [&](Amethyst::FrameScope &child) {
                        child.component.name = "Child of frame 1";
                        child.component.addExtension<Amethyst::UIDragDetector>();
                        child.textButton(
                            {
                                .base = {
                                    .anchorPoint = {0.5f, 0.5f},
                                    .position = Amethyst::UDim2::fromScale(0.5f, 0.5f),
                                    .size = Amethyst::UDim2::fromScale(0.9f, 0.9f),
                                },
                                .style = {
                                    .backgroundColor = {0.5f, 0.0f, 0.5f},
                                    .cornerRadius = 5.0f,
                                },
                                .text = {
                                    .textXAlignment = Amethyst::TextXAlignment::CENTER,
                                    .textYAlignment = Amethyst::TextYAlignment::CENTER,
                                    .textScaled = true,
                                },
                                .label = "Exit",
                            },
                            [&](Amethyst::TextButtonScope &btn) {
                                btn.component.name = "button";
                                auto *c = &btn.component;
                                btn.component.onMouseEnterCb = [c]() {
                                    c->setBaseProperties({.size = Amethyst::UDim2::fromScale(1.0f, 1.0f)});
                                    return Amethyst::EventResult::CONSUMED;
                                };
                                btn.component.onMouseLeaveCb = [c]() {
                                    c->setBaseProperties({.size = Amethyst::UDim2::fromScale(0.9f, 0.9f)});
                                    return Amethyst::EventResult::CONSUMED;
                                };
                                btn.component.onMouseButton1ClickCb = [&running]() {
                                    running = false;
                                    return Amethyst::EventResult::CONSUMED;
                                };
                            });
                    });
            })
        // --- Text label ---
        .textLabel(
            {
                .base = {
                    .position = Amethyst::UDim2::fromOffset(10, 20),
                    .size = Amethyst::UDim2(0.98f, 0.0f, 0.0f, 60.0f),
                },
                .style = {
                    .backgroundColor = {0.0f, 0.0f, 0.0f},
                    .backgroundTransparency = 1.0f,
                },
                .text = {
                    .fontSize = 24.0f,
                    .textColor = {0.9f, 0.9f, 1.0f, 1.0f},
                    .textXAlignment = Amethyst::TextXAlignment::CENTER,
                    .textYAlignment = Amethyst::TextYAlignment::CENTER,
                    .strokeThickness = 0.0f,
                    .strokeColor = {0.0f, 0.0f, 0.0f, 1.0f},
                },
                .label = "The quick brown fox jumps over the lazy dog",
            },
            [](Amethyst::TextLabelScope &lbl) {
                lbl.component.name = "pangram fixed size";
                lbl.component.addExtension<Amethyst::UIDragDetector>();
            })
        // --- Text input ---
        .textInput(
            {
                .base = {
                    .position = Amethyst::UDim2::fromOffset(10, 100),
                    .size = Amethyst::UDim2::fromOffset(400, 40),
                },
                .style = {
                    .backgroundColor = {0.15f, 0.15f, 0.15f},
                    .borderPixelSize = 2.0f,
                    .borderColor = {0.3f, 0.5f, 0.8f},
                    .cornerRadius = 5.0f,
                },
                .textInput = {
                    .text =
                        {
                            .fontSize = 18.0f,
                            .textColor = {1.0f, 1.0f, 1.0f, 1.0f},
                            .textXAlignment = Amethyst::TextXAlignment::LEFT,
                        },
                    .placeholderColor = {0.5f, 0.5f, 0.5f, 1.0f},
                    .selectionColor = {0.3f, 0.5f, 0.9f, 0.5f},
                    .cursorColor = {1.0f, 1.0f, 1.0f, 1.0f},
                },
                .placeholder = "Type something here...",
            },
            [&](Amethyst::TextInputScope &ti) {
                ti.component.name = "text input example";
                ti.component.onTextChanged = [](const std::string &text) { AM_LOG_INFO("Text changed: '{}'", text); };
                ti.component.onEnterPressed = []() { AM_LOG_INFO("Enter pressed!"); };
            })
        // --- Sliders ---
        .sliderFloat({
            .base = {
                .position = Amethyst::UDim2::fromOffset(10, 150),
                .size = Amethyst::UDim2::fromOffset(300, 40),
            },
            .style = {.backgroundColor = {0.3f, 0.3f, 0.3f}},
            .format = "%.1f%%",
            .min = 0.0f,
            .max = 100.0f,
            .value = &sliderFloatValue,
        })
        .sliderInt({
            .base = {
                .position = Amethyst::UDim2::fromOffset(10, 200),
                .size = Amethyst::UDim2::fromOffset(300, 40),
            },
            .style = {.backgroundColor = {0.3f, 0.3f, 0.3f}},
            .format = "%d",
            .min = 0,
            .max = 5,
            .value = &sliderIntValue,
        })
        // --- Checkbox ---
        .checkbox(
            {
                .base = {
                    .position = Amethyst::UDim2::fromOffset(10, 250),
                    .size = Amethyst::UDim2::fromOffset(24, 24),
                },
                .value = &checkboxValue,
            },
            [&](Amethyst::CheckboxScope &cb) {
                cb.component.name = "Enable Feature";
                cb.component.onValueChanged = [](bool v) { AM_LOG_INFO("Enabled: {}", v); };
            })
        .textLabel({
            .base = {
                .position = Amethyst::UDim2::fromOffset(44, 250),
                .size = Amethyst::UDim2::fromOffset(200, 24),
            },
            .style = {.backgroundTransparency = 1.0f},
            .text = {.fontSize = 14.0f, .textColor = {0.9f, 0.9f, 0.9f, 1.0f}, .textYAlignment = Amethyst::TextYAlignment::CENTER},
            .label = "Enable Feature",
        })
        // --- Scrolling frame with grid ---
        .scrollingFrame(
            {
                .base = {
                    .clipsDescendants = true,
                    .position = Amethyst::UDim2::fromOffset(530, 100),
                    .size = Amethyst::UDim2::fromOffset(250, 200),
                },
                .style = {
                    .backgroundColor = {0.12f, 0.12f, 0.15f},
                    .cornerRadius = 5.0f,
                },
                .scroll = {
                    .canvasSize = Amethyst::UDim2::fromOffset(250, 350),
                    .scrollBarColor = {0.25f, 0.25f, 0.3f},
                    .scrollBarThumbColor = {0.5f, 0.5f, 0.6f},
                },
            },
            [](Amethyst::ScrollingFrameScope &sf) {
                sf.component.name = "Scroll Test";
                auto *layout = sf.component.addExtension<Amethyst::UIGridLayout>();
                layout->cellSize = Amethyst::UDim2::fromOffset(70, 70);
                layout->cellPadding = Amethyst::UDim2::fromOffset(10, 10);
                layout->fillDirection = Amethyst::FillDirection::FILL_HORIZONTAL;
                layout->fillDirectionMaxCells = 3;

                Amethyst::Color3 colors[] = {
                    {0.8f, 0.3f, 0.3f}, {0.3f, 0.8f, 0.3f}, {0.3f, 0.3f, 0.8f}, {0.8f, 0.8f, 0.3f},
                    {0.8f, 0.3f, 0.8f}, {0.3f, 0.8f, 0.8f}, {0.6f, 0.4f, 0.2f}, {0.4f, 0.2f, 0.6f},
                    {0.2f, 0.6f, 0.4f}, {0.5f, 0.5f, 0.5f}, {0.9f, 0.6f, 0.3f}, {0.3f, 0.6f, 0.9f},
                };
                for (auto &color : colors) {
                    sf.frame({.style = {.backgroundColor = color, .cornerRadius = 8.0f}});
                }
            })
        // --- Table ---
        .table(
            {
                .base = {
                    .position = Amethyst::UDim2::fromOffset(10, 400),
                    .size = Amethyst::UDim2::fromOffset(500, 160),
                },
            },
            [](Amethyst::TableScope &t) {
                t.component.name = "Test Table";
                t.column("Label", 0.3f).column("Color", 0.2f).column("Slider", 0.5f);

                struct Row {
                    const char *label;
                    Amethyst::Color3 color;
                };
                static Row rows[] = {
                    {"Row 1", {0.8f, 0.2f, 0.2f}},
                    {"Row 2", {0.2f, 0.8f, 0.2f}},
                    {"Row 3", {0.2f, 0.2f, 0.8f}},
                    {"Row 4", {0.8f, 0.8f, 0.2f}},
                };
                static float sliderVals[4] = {25.0f, 50.0f, 75.0f, 100.0f};

                for (int i = 0; i < 4; ++i) {
                    t.row([i](Amethyst::TableRowScope &r) {
                        r.cell([i](Amethyst::UIScope &cell) {
                            cell.textLabel({
                                .base = {.size = Amethyst::UDim2::fromScale(1.0f, 1.0f)},
                                .style = {.backgroundColor = {0.2f, 0.2f, 0.2f}},
                                .text = {.fontSize = 16.0f, .textColor = {1.0f, 1.0f, 1.0f, 1.0f}},
                                .label = rows[i].label,
                            });
                        });
                        r.cell([i](Amethyst::UIScope &cell) {
                            cell.frame({
                                .base = {
                                    .position = Amethyst::UDim2::fromOffset(5, 5),
                                    .size = Amethyst::UDim2::fromOffset(30, 30),
                                },
                                .style = {
                                    .backgroundColor = rows[i].color,
                                    .cornerRadius = 4.0f,
                                },
                            });
                        });
                        r.cell([i](Amethyst::UIScope &cell) {
                            cell.sliderFloat({
                                .base = {
                                    .position = Amethyst::UDim2::fromScale(0.025f, 0.1f),
                                    .size = Amethyst::UDim2::fromScale(0.95f, 0.8f),
                                },
                                .min = 0.0f,
                                .max = 100.0f,
                                .value = &sliderVals[i],
                            });
                        });
                    });
                }
            })
        // --- Canvas ---
        .canvas(
            {
                .base = {
                    .position = Amethyst::UDim2::fromOffset(530, 310),
                    .size = Amethyst::UDim2::fromOffset(460, 120),
                },
            },
            [](Amethyst::CanvasScope &c) {
                c.component.name = "Canvas Demo";
                c.component.drawLine({10, 20}, {50, 90}, {1.0f, 0.4f, 0.4f, 1.0f}, 3.0f);
                c.component.drawTriangleFilled({90, 90}, {120, 20}, {150, 90}, {0.4f, 1.0f, 0.4f, 1.0f});
                c.component.drawTriangleStroke({170, 90}, {200, 20}, {230, 90}, {0.4f, 0.4f, 1.0f, 1.0f}, 2.0f);
                c.component.drawQuadFilled({250, 25}, {310, 25}, {310, 90}, {250, 90}, {1.0f, 1.0f, 0.4f, 1.0f});
                c.component.drawQuadStroke({320, 25}, {380, 30}, {375, 90}, {325, 85}, {1.0f, 0.4f, 1.0f, 1.0f}, 2.0f);
                c.component.drawCircleFilled({415, 55}, 30.0f, {0.4f, 1.0f, 1.0f, 1.0f});
                c.component.drawCircleStroke({415, 55}, 30.0f, {1.0f, 1.0f, 1.0f, 1.0f}, 2.0f);
                c.component.drawText("Canvas", {10, 95}, 16.0f, {1.0f, 1.0f, 1.0f, 1.0f}, 16);
            });

    // --- Class styling showcase (.ams classes + precedence) ---
    Amethyst::UIScope(window).frame(
        {
            .classes = {"card"},
            .base =
                {
                    .position = Amethyst::UDim2::fromOffset(800, 100),
                    .size = Amethyst::UDim2::fromOffset(180, 244),
                },
        },
        [](Amethyst::FrameScope &card) {
            card.component.name = "Class Card";

            auto styledButton = [&card](float y, std::vector<std::string> classes, const char *label) {
                card.textButton({
                    .classes = std::move(classes),
                    .base =
                        {
                            .position = Amethyst::UDim2(0.0f, 12.0f, 0.0f, y),
                            .size = Amethyst::UDim2(1.0f, -24.0f, 0.0f, 44.0f),
                        },
                    .text =
                        {
                            .textXAlignment = Amethyst::TextXAlignment::CENTER,
                            .textYAlignment = Amethyst::TextYAlignment::CENTER,
                        },
                    .label = label,
                });
            };

            styledButton(12.0f, {}, "Default");
            styledButton(68.0f, {"primary"}, "Primary");
            styledButton(124.0f, {"danger"}, "Danger");
            styledButton(180.0f, {"danger", "primary"}, "Both");
        });

    // --- Standalone tab bar test ---
    Amethyst::UIScope(window).tabBar(
        {
            .base =
                {
                    .position = Amethyst::UDim2::fromOffset(10, 570),
                    .size = Amethyst::UDim2::fromOffset(460, 200),
                },
            .style = {.backgroundColor = {0.15f, 0.15f, 0.2f}},
        },
        [](Amethyst::TabBarScope &tabs) {
            tabs.tab("Tab A", [](Amethyst::FrameScope &f) {
                f.frame(
                    {.base = {.size = Amethyst::UDim2::fromScale(1.0f, 1.0f)}, .style = {.backgroundColor = {0.8f, 0.2f, 0.2f}}});
            });
            tabs.tab("Tab B", [](Amethyst::FrameScope &f) {
                f.frame(
                    {.base = {.size = Amethyst::UDim2::fromScale(1.0f, 1.0f)}, .style = {.backgroundColor = {0.2f, 0.8f, 0.2f}}});
            });
        });

    // --- Docking layout ---
    dockingLayer = window.add<Amethyst::DockingLayer>();
    dockingLayer->name = "Docking Example";
    dockingLayer->absoluteSize = {500.0f, 400.0f};
    dockingLayer->absolutePosition = {480.0f, 500.0f};
    dockingLayer->innerSpacing = 2.0f;
    dockingLayer->persistLayout = true;

    auto loadedLayout = Amethyst::LayoutConfig::instance().loadFromFile("layout.conf");
    auto *layoutEntry = loadedLayout ? Amethyst::LayoutConfig::instance().get("Docking Example") : nullptr;
    if (layoutEntry != nullptr && layoutEntry->type == Amethyst::ConfigType::DOCK_LAYOUT) {
        dockingLayer->applyConfig(layoutEntry->dockLayout);
    }
    if (dockingLayer->isEmpty()) {
        Amethyst::DockScope(*dockingLayer)
            .split(
                Amethyst::SplitAxis::VERTICAL, 0.35f,
                [&](Amethyst::DockScope &left) {
                    left.panel([&](Amethyst::TabBarScope &tabs) {
                        tabs.tab("Tree", [&](Amethyst::FrameScope &content) {
                            content.scrollingFrame(
                                {
                                    .base =
                                        {
                                            .clipsDescendants = true,
                                            .size = Amethyst::UDim2::fromScale(1.0f, 1.0f),
                                        },
                                    .style = {.backgroundColor = Amethyst::Color3::fromHex(0x282828)},
                                    .scroll =
                                        {
                                            .canvasSize = Amethyst::UDim2::fromOffset(250, 500),
                                            .scrollBarColor = {0.2f, 0.2f, 0.25f},
                                            .scrollBarThumbColor = {0.4f, 0.4f, 0.5f},
                                        },
                                },
                                [](Amethyst::ScrollingFrameScope &sf) {
                                    auto *tv = sf.get().add<Amethyst::TreeView>();
                                    tv->name = "Test TreeView";
                                    tv->setBaseProperties({
                                        .clipsDescendants = true,
                                        .size = Amethyst::UDim2::fromScale(1.0f, 1.0f),
                                    });
                                    tv->setTreeViewProperties({
                                        .rowHeight = 24.0f,
                                        .indentPerLevel = 20.0f,
                                        .rowBackgroundColor = Amethyst::Color4::fromHex(0x282828),
                                        .rowAlternateColor = Amethyst::Color4::fromHex(0x2B2B2B),
                                    });
                                    tv->addColumn({});

                                    auto addRow = [tv](uint16_t depth, const char *text, Amethyst::Color4 color) {
                                        tv->addRow(depth);
                                        auto lbl = std::make_unique<Amethyst::TextLabel>();
                                        lbl->setBaseProperties({.size = Amethyst::UDim2::fromScale(1.0f, 1.0f)});
                                        lbl->setBaseStyleProperties({.backgroundTransparency = 1.0f});
                                        lbl->setTextStyleProperties({
                                            .fontSize = 14.0f,
                                            .textColor = color,
                                            .textYAlignment = Amethyst::TextYAlignment::CENTER,
                                        });
                                        lbl->setText(text);
                                        tv->nextCell(std::move(lbl));
                                    };

                                    addRow(0, "Scene", {1.0f, 1.0f, 1.0f, 1.0f});
                                    addRow(1, "Camera", {0.8f, 0.9f, 1.0f, 1.0f});
                                    addRow(1, "Player", {0.5f, 1.0f, 0.5f, 1.0f});
                                    addRow(2, "Mesh", {0.9f, 0.9f, 0.9f, 1.0f});
                                    addRow(2, "Collider", {0.9f, 0.9f, 0.9f, 1.0f});
                                    addRow(1, "Lights", {1.0f, 1.0f, 0.5f, 1.0f});
                                    addRow(2, "Sun", {1.0f, 0.9f, 0.6f, 1.0f});
                                    addRow(2, "Point Light", {0.6f, 0.8f, 1.0f, 1.0f});
                                });
                        });
                    });
                },
                [&](Amethyst::DockScope &right) {
                    right.split(
                        Amethyst::SplitAxis::HORIZONTAL, 0.6f,
                        [&](Amethyst::DockScope &top) {
                            top.panel([&](Amethyst::TabBarScope &tabs) {
                                tabs.tab("Panel A", [](Amethyst::FrameScope &f) {
                                    f.frame({.base = {.size = Amethyst::UDim2::fromScale(1.0f, 1.0f)},
                                             .style = {.backgroundColor = {0.6f, 0.2f, 0.2f}}});
                                });
                                tabs.tab("Checkerboard", [&](Amethyst::FrameScope &f) {
                                    f.imageLabel({.base = {.size = Amethyst::UDim2::fromScale(1.0f, 1.0f)},
                                                  .texture = amCtx.getGlyphAtlasTexture()});
                                });
                            });
                        },
                        [](Amethyst::DockScope &bottom) {
                            bottom.panel([](Amethyst::TabBarScope &tabs) {
                                tabs.tab("Panel B", [](Amethyst::FrameScope &f) {
                                    f.frame({.base = {.size = Amethyst::UDim2::fromScale(1.0f, 1.0f)},
                                             .style = {.backgroundColor = {0.2f, 0.6f, 0.2f}}});
                                });
                            });
                        });
                });
    }

    amCtx.draw(window);

    double lastTime = glfwGetTime();
    int frameCount = 0;
    double lastUpdateTime = glfwGetTime();

    while (!glfwWindowShouldClose(ctx.window) && running) {
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

        AM_PROFILE_FRAME();

        frameCount++;
        double currentTime = glfwGetTime();
        if (currentTime - lastTime >= 1.0) {
            AM_LOG_INFO("FPS: {}", frameCount);
            frameCount = 0;
            lastTime = currentTime;
        }
    }

    backend.unregisterTexture(checkerboardId);
    destroyTexture(ctx, checkerboardTex);
    backend.shutdown();
    contextShutdown(ctx);

    Amethyst::Log::Shutdown();
    return 0;
}
