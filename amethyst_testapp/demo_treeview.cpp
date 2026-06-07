#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/tree_view.h"
#include "components/ui_scope.h"
#include "modules/style.h"
#include "vk_context.h"

#include <string>

using namespace Amethyst;

int main()
{
    Log::Init();

    Style::load(AMETHYST_ASSETS_DIR "/theme.ams");

    VkContext ctx;
    if (!contextInit(ctx, 800, 900, "Amethyst - TreeView Demo")) {
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

    vec2 screenSize = {
        static_cast<float>(ctx.swapchainExtent.width),
        static_cast<float>(ctx.swapchainExtent.height),
    };

    bool running = true;

    Window window;
    window.absoluteSize = screenSize;
    window.absoluteRotation = 0.0f;
    window.setDisplayOrder(10);

    TextLabel *statusLabel = nullptr;

    UIScope(window)
        .textLabel({.base = {.position = {0.0f, 0.0f, 0.0f, 0.0f}, .size = {1.0f, 0.0f, 0.0f, 24.0f}},
                    .style = {.backgroundColor = {0.12f, 0.12f, 0.14f}, .backgroundTransparency = 0.0f},
                    .text = {.fontSize = 13.0f,
                             .textColor = {0.7f, 0.7f, 0.7f, 1.0f},
                             .textXAlignment = TextXAlignment::LEFT,
                             .textYAlignment = TextYAlignment::CENTER},
                    .label = "Selected row: (none)"},
                   [&statusLabel](TextLabelScope &t) { statusLabel = &t.component; })
        .scrollingFrame(
            {.base = {.clipsDescendants = true, .position = {0.0f, 0.0f, 0.0f, 24.0f}, .size = {1.0f, 0.0f, 1.0f, -24.0f}},
             .style = {.backgroundColor = Color3::fromHex(0x1E1E1E)},
             .scroll = {.canvasSize = UDim2::fromOffset(800, 3200),
                        .scrollBarColor = {0.2f, 0.2f, 0.25f},
                        .scrollBarThumbColor = {0.45f, 0.45f, 0.55f}}},
            [statusLabel](ScrollingFrameScope &sf) {
                sf.treeView(
                    {.base = {.clipsDescendants = true, .size = UDim2::fromScale(1.0f, 1.0f)},
                     .treeView = {.rowHeight = 22.0f,
                                  .showColumnSeparators = true,
                                  .indentPerLevel = 18.0f,
                                  .rowBackgroundColor = Color4::fromHex(0x1E1E1E),
                                  .rowAlternateColor = Color4::fromHex(0x252527),
                                  .rowHoverColor = {0.2f, 0.3f, 0.45f, 1.0f},
                                  .rowSelectedColor = {0.18f, 0.38f, 0.62f, 1.0f},
                                  .fillRows = true}},
                    [statusLabel](TreeViewScope &tv) {
                        const Color4 COL_DIM = {0.55f, 0.55f, 0.58f, 1.0f};
                        const Color4 COL_ROOT = {1.0f, 1.0f, 1.0f, 1.0f};
                        const Color4 COL_GROUP = {0.85f, 0.75f, 0.45f, 1.0f};
                        const Color4 COL_SUBGRP = {0.65f, 0.85f, 0.65f, 1.0f};
                        const Color4 COL_LEAF = {0.78f, 0.78f, 0.82f, 1.0f};
                        const Color4 COL_SPEC = {0.55f, 0.75f, 1.0f, 1.0f};

                        tv.component.onRowClicked = [statusLabel](uint32_t row) {
                            statusLabel->setText("Selected row: " + std::to_string(row));
                        };

                        tv.column("Name", 0.5f).column("Type", 0.3f).column("", 0.2f);

                        auto cellLabel = [](Color4 color, std::string text) {
                            return [color, text](UIScope &c) {
                                c.textLabel(
                                    {.base = {.size = UDim2::fromScale(1.0f, 1.0f)},
                                     .style = {.backgroundTransparency = 1.0f},
                                     .text = {.fontSize = 13.0f, .textColor = color, .textYAlignment = TextYAlignment::CENTER},
                                     .label = text});
                            };
                        };

                        auto labeled = [&](TreeRowScope &r, std::string name, Color4 color, std::string type) {
                            r.cell(cellLabel(color, name)).cell(cellLabel(COL_DIM, type)).cell(nullptr);
                        };

                        tv.row([&](TreeRowScope &scene) {
                            labeled(scene, "Scene", COL_ROOT, "Root");

                            scene.row([&](TreeRowScope &entities) {
                                labeled(entities, "Entities", COL_GROUP, "Group");
                                for (int i = 0; i < 50; i++) {
                                    entities.row(
                                        [&, i](TreeRowScope &e) { labeled(e, "entity_" + std::to_string(i), COL_LEAF, "Entity"); });
                                }
                            });

                            scene.row([&](TreeRowScope &assets) {
                                labeled(assets, "Assets", COL_GROUP, "Group");

                                assets.row([&](TreeRowScope &meshes) {
                                    labeled(meshes, "Meshes", COL_SUBGRP, "Folder");
                                    for (int i = 0; i < 20; i++) {
                                        meshes.row([&, i](TreeRowScope &m) {
                                            labeled(m, "mesh_" + std::to_string(i) + ".obj", COL_LEAF, "Mesh");
                                        });
                                    }
                                });

                                assets.row([&](TreeRowScope &textures) {
                                    labeled(textures, "Textures", COL_SUBGRP, "Folder");
                                    for (int i = 0; i < 20; i++) {
                                        textures.row([&, i](TreeRowScope &t) {
                                            labeled(t, "texture_" + std::to_string(i) + ".png", COL_LEAF, "Texture");
                                        });
                                    }
                                });

                                assets.row([&](TreeRowScope &shaders) {
                                    labeled(shaders, "Shaders", COL_SUBGRP, "Folder");
                                    for (int i = 0; i < 8; i++) {
                                        shaders.row([&, i](TreeRowScope &s) {
                                            labeled(s, "shader_" + std::to_string(i) + ".glsl", COL_SPEC, "Shader");
                                        });
                                    }
                                });
                            });

                            scene.row([&](TreeRowScope &systems) {
                                labeled(systems, "Systems", COL_GROUP, "Group");

                                systems.row([&](TreeRowScope &render) {
                                    labeled(render, "RenderSystem", COL_SUBGRP, "System");
                                    render.row([&](TreeRowScope &r) { labeled(r, "OpaquePass", COL_LEAF, "Pass"); });
                                    render.row([&](TreeRowScope &r) { labeled(r, "ShadowPass", COL_LEAF, "Pass"); });
                                    render.row([&](TreeRowScope &r) { labeled(r, "TransparentPass", COL_LEAF, "Pass"); });
                                    render.row([&](TreeRowScope &r) { labeled(r, "PostProcess", COL_LEAF, "Pass"); });
                                });

                                systems.row([&](TreeRowScope &physics) {
                                    labeled(physics, "PhysicsSystem", COL_SUBGRP, "System");
                                    physics.row([&](TreeRowScope &r) { labeled(r, "BroadPhase", COL_LEAF, "Phase"); });
                                    physics.row([&](TreeRowScope &r) { labeled(r, "NarrowPhase", COL_LEAF, "Phase"); });
                                    physics.row([&](TreeRowScope &r) { labeled(r, "Solver", COL_LEAF, "Solver"); });
                                });

                                systems.row([&](TreeRowScope &audio) {
                                    labeled(audio, "AudioSystem", COL_SUBGRP, "System");
                                    audio.row([&](TreeRowScope &r) { labeled(r, "SFX", COL_LEAF, "Channel"); });
                                    audio.row([&](TreeRowScope &r) { labeled(r, "Music", COL_LEAF, "Channel"); });
                                });
                            });
                        });
                    });
            });

    amCtx.draw(window);

    int frameCount = 0;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(ctx.window) && running) {
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
