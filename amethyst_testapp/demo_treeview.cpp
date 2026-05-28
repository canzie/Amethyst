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

    Style::load(AMETHYST_ASSETS_DIR "/theme.toml");

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

    UIScope(window)
        .textLabel({.backgroundColor = {0.12f, 0.12f, 0.14f},
                    .backgroundTransparency = 0.0f,
                    .position = {0.0f, 0.0f, 0.0f, 0.0f},
                    .size = {1.0f, 0.0f, 0.0f, 24.0f}},
                   {.fontSize = 13.0f,
                    .textColor = {0.7f, 0.7f, 0.7f, 1.0f},
                    .textXAlignment = TextXAlignment::LEFT,
                    .textYAlignment = TextYAlignment::CENTER,
                    .text = "Selected row: (none)"},
                   [&statusLabel](TextLabelScope &t) { statusLabel = &t.component; })
        .scrollingFrame(
            {.backgroundColor = Color3::fromHex(0x1E1E1E),
             .clipsDescendants = true,
             .position = {0.0f, 0.0f, 0.0f, 24.0f},
             .size = {1.0f, 0.0f, 1.0f, -24.0f}},
            {.canvasSize = UDim2::fromOffset(800, 3200),
             .scrollBarColor = {0.2f, 0.2f, 0.25f},
             .scrollBarThumbColor = {0.45f, 0.45f, 0.55f}},
            [statusLabel](ScrollingFrameScope &sf) {
                sf.treeView(
                    {.backgroundColor = Color3::fromHex(0x1E1E1E),
                     .backgroundTransparency = 0.0f,
                     .clipsDescendants = true,
                     .size = UDim2::fromScale(1.0f, 1.0f)},
                    {.rowHeight = 22.0f,
                     .showColumnSeparators = true,
                     .indentPerLevel = 18.0f,
                     .rowBackgroundColor = Color4::fromHex(0x1E1E1E),
                     .rowAlternateColor = Color4::fromHex(0x252527),
                     .rowHoverColor = {0.2f, 0.3f, 0.45f, 1.0f},
                     .rowSelectedColor = {0.18f, 0.38f, 0.62f, 1.0f},
                     .fillRows = true},
                    [statusLabel](TreeViewScope &tv) {
                        tv.component.numCols = 3;
                        tv.component.columnWeights = {0.5f, 0.3f, 0.2f};
                        tv.component.onRowClicked = [statusLabel](uint32_t row) {
                            statusLabel->setTextProperties({.text = "Selected row: " + std::to_string(row)});
                        };

                        const Color4 COL_DIM = {0.55f, 0.55f, 0.58f, 1.0f};
                        const Color4 COL_ROOT = {1.0f, 1.0f, 1.0f, 1.0f};
                        const Color4 COL_GROUP = {0.85f, 0.75f, 0.45f, 1.0f};
                        const Color4 COL_SUBGRP = {0.65f, 0.85f, 0.65f, 1.0f};
                        const Color4 COL_LEAF = {0.78f, 0.78f, 0.82f, 1.0f};
                        const Color4 COL_SPEC = {0.55f, 0.75f, 1.0f, 1.0f};

                        auto addRow = [&tv, &COL_DIM](uint32_t parent, const std::string &label, Color4 color,
                                                      const char *type) -> uint32_t {
                            uint32_t row = tv.component.beginRow(parent);
                            tv.textLabel(
                                {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                {.fontSize = 13.0f, .textColor = color, .textYAlignment = TextYAlignment::CENTER, .text = label});
                            tv.textLabel(
                                {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER, .text = type});
                            tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                         {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER});
                            tv.component.endRow();
                            return row;
                        };

                        // Scene root
                        uint32_t scene = tv.component.beginRow();
                        tv.textLabel(
                            {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                            {.fontSize = 13.0f, .textColor = COL_ROOT, .textYAlignment = TextYAlignment::CENTER, .text = "Scene"});
                        tv.textLabel(
                            {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                            {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER, .text = "Root"});
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER});
                        tv.component.endRow();

                        // Entities: 50 flat children
                        uint32_t entities = tv.component.beginRow(scene);
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f,
                                      .textColor = COL_GROUP,
                                      .textYAlignment = TextYAlignment::CENTER,
                                      .text = "Entities"});
                        tv.textLabel(
                            {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                            {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER, .text = "Group"});
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER});
                        tv.component.endRow();
                        for (int i = 0; i < 50; i++) {
                            addRow(entities, "entity_" + std::to_string(i), COL_LEAF, "Entity");
                        }

                        // Assets: two levels of nesting
                        uint32_t assets = tv.component.beginRow(scene);
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f,
                                      .textColor = COL_GROUP,
                                      .textYAlignment = TextYAlignment::CENTER,
                                      .text = "Assets"});
                        tv.textLabel(
                            {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                            {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER, .text = "Group"});
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER});
                        tv.component.endRow();

                        uint32_t meshes = tv.component.beginRow(assets);
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f,
                                      .textColor = COL_SUBGRP,
                                      .textYAlignment = TextYAlignment::CENTER,
                                      .text = "Meshes"});
                        tv.textLabel(
                            {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                            {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER, .text = "Folder"});
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER});
                        tv.component.endRow();
                        for (int i = 0; i < 20; i++) {
                            addRow(meshes, "mesh_" + std::to_string(i) + ".obj", COL_LEAF, "Mesh");
                        }

                        uint32_t textures = tv.component.beginRow(assets);
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f,
                                      .textColor = COL_SUBGRP,
                                      .textYAlignment = TextYAlignment::CENTER,
                                      .text = "Textures"});
                        tv.textLabel(
                            {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                            {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER, .text = "Folder"});
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER});
                        tv.component.endRow();
                        for (int i = 0; i < 20; i++) {
                            addRow(textures, "texture_" + std::to_string(i) + ".png", COL_LEAF, "Texture");
                        }

                        uint32_t shaders = tv.component.beginRow(assets);
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f,
                                      .textColor = COL_SUBGRP,
                                      .textYAlignment = TextYAlignment::CENTER,
                                      .text = "Shaders"});
                        tv.textLabel(
                            {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                            {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER, .text = "Folder"});
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER});
                        tv.component.endRow();
                        for (int i = 0; i < 8; i++) {
                            addRow(shaders, "shader_" + std::to_string(i) + ".glsl", COL_SPEC, "Shader");
                        }

                        // Systems: three levels
                        uint32_t systems = tv.component.beginRow(scene);
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f,
                                      .textColor = COL_GROUP,
                                      .textYAlignment = TextYAlignment::CENTER,
                                      .text = "Systems"});
                        tv.textLabel(
                            {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                            {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER, .text = "Group"});
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER});
                        tv.component.endRow();

                        uint32_t render = tv.component.beginRow(systems);
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f,
                                      .textColor = COL_SUBGRP,
                                      .textYAlignment = TextYAlignment::CENTER,
                                      .text = "RenderSystem"});
                        tv.textLabel(
                            {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                            {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER, .text = "System"});
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER});
                        tv.component.endRow();
                        addRow(render, "OpaquePass", COL_LEAF, "Pass");
                        addRow(render, "ShadowPass", COL_LEAF, "Pass");
                        addRow(render, "TransparentPass", COL_LEAF, "Pass");
                        addRow(render, "PostProcess", COL_LEAF, "Pass");

                        uint32_t physics = tv.component.beginRow(systems);
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f,
                                      .textColor = COL_SUBGRP,
                                      .textYAlignment = TextYAlignment::CENTER,
                                      .text = "PhysicsSystem"});
                        tv.textLabel(
                            {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                            {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER, .text = "System"});
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER});
                        tv.component.endRow();
                        addRow(physics, "BroadPhase", COL_LEAF, "Phase");
                        addRow(physics, "NarrowPhase", COL_LEAF, "Phase");
                        addRow(physics, "Solver", COL_LEAF, "Solver");

                        uint32_t audio = tv.component.beginRow(systems);
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f,
                                      .textColor = COL_SUBGRP,
                                      .textYAlignment = TextYAlignment::CENTER,
                                      .text = "AudioSystem"});
                        tv.textLabel(
                            {.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                            {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER, .text = "System"});
                        tv.textLabel({.backgroundTransparency = 1.0f, .size = UDim2::fromScale(1.0f, 1.0f)},
                                     {.fontSize = 13.0f, .textColor = COL_DIM, .textYAlignment = TextYAlignment::CENTER});
                        tv.component.endRow();
                        addRow(audio, "SFX", COL_LEAF, "Channel");
                        addRow(audio, "Music", COL_LEAF, "Channel");
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
