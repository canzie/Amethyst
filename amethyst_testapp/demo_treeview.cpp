#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/scrolling_frame.h"
#include "components/tree_view.h"
#include "modules/style.h"
#include "vk_context.h"

#include <string>

static void addLabel(Amethyst::TreeView *tv, const char *text, Amethyst::Color4 color)
{
    auto *lbl = tv->add<Amethyst::TextLabel>();
    lbl->text = text;
    lbl->textColor = color;
    lbl->backgroundTransparency = 1.0f;
    lbl->fontSize = 13.0f;
    lbl->textYAlignment = Amethyst::TextYAlignment::CENTER;
    lbl->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    lbl->markDirty();
}

int main()
{
    Amethyst::Log::Init();

    Amethyst::Style::load(AMETHYST_ASSETS_DIR "/theme.toml");

    VkContext ctx;
    if (!contextInit(ctx, 800, 900, "Amethyst - TreeView Demo")) {
        AM_LOG_ERROR("Failed to initialize Vulkan context");
        return 1;
    }

    Amethyst::AmethystContext amCtx;
    if (!amCtx.loadFont(AMETHYST_ASSETS_DIR "/fonts/OpenSans-Regular.ttf")) {
        AM_LOG_ERROR("Failed to load font");
        return 1;
    }

    Amethyst::VulkanInitInfo initInfo{};
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

    Amethyst::GLFWInitInfo glfwInfo{};
    glfwInfo.window = ctx.window;

    Amethyst::VkBackend backend;
    backend.init(initInfo, glfwInfo);

    amCtx.init(backend);

    glm::vec2 screenSize = {
        static_cast<float>(ctx.swapchainExtent.width),
        static_cast<float>(ctx.swapchainExtent.height),
    };

    bool running = true;

    Amethyst::Window window;
    window.absoluteSize = screenSize;
    window.absoluteRotation = 0.0f;
    window.setDisplayOrder(10);

    auto *statusLabel = window.add<Amethyst::TextLabel>();
    statusLabel->size = Amethyst::UDim2(1.0f, 0.0f, 0.0f, 24.0f);
    statusLabel->position = Amethyst::UDim2::fromOffset(0.0f, 0.0f);
    statusLabel->backgroundColor = {0.12f, 0.12f, 0.14f};
    statusLabel->backgroundTransparency = 0.0f;
    statusLabel->textColor = {0.7f, 0.7f, 0.7f, 1.0f};
    statusLabel->fontSize = 13.0f;
    statusLabel->textXAlignment = Amethyst::TextXAlignment::LEFT;
    statusLabel->textYAlignment = Amethyst::TextYAlignment::CENTER;
    statusLabel->text = "Selected row: (none)";
    statusLabel->markDirty();

    auto *scrollFrame = window.add<Amethyst::ScrollingFrame>();
    scrollFrame->name = "Tree Scroll";
    scrollFrame->size = Amethyst::UDim2(1.0f, 0.0f, 1.0f, -24.0f);
    scrollFrame->position = Amethyst::UDim2::fromOffset(0.0f, 24.0f);
    scrollFrame->backgroundColor = Amethyst::Color3::fromHex(0x1E1E1E);
    scrollFrame->canvasSize = Amethyst::UDim2::fromOffset(800, 3200);
    scrollFrame->scrollBarColor = {0.2f, 0.2f, 0.25f};
    scrollFrame->scrollBarThumbColor = {0.45f, 0.45f, 0.55f};
    scrollFrame->clipsDescendants = true;
    scrollFrame->markDirty();

    auto *treeView = scrollFrame->add<Amethyst::TreeView>();
    treeView->name = "TreeView Stress";
    treeView->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    treeView->backgroundTransparency = 0.0f;
    treeView->backgroundColor = Amethyst::Color3::fromHex(0x1E1E1E);
    treeView->rowBackgroundColor = Amethyst::Color4::fromHex(0x1E1E1E);
    treeView->rowAlternateColor = Amethyst::Color4::fromHex(0x252527);
    treeView->rowHoverColor = {0.2f, 0.3f, 0.45f, 1.0f};
    treeView->rowSelectedColor = {0.18f, 0.38f, 0.62f, 1.0f};
    treeView->numCols = 1;
    treeView->rowHeight = 22.0f;
    treeView->indentPerLevel = 18.0f;
    treeView->clipsDescendants = true;
    treeView->fillRows = true;
    treeView->markDirty();

    treeView->onRowClicked = [statusLabel](uint32_t row) {
        statusLabel->text = "Selected row: " + std::to_string(row);
        statusLabel->markDirty();
    };

    auto addRow = [&](uint32_t parent, const std::string &label, Amethyst::Color4 color) -> uint32_t {
        uint32_t row = treeView->beginRow(parent);
        addLabel(treeView, label.c_str(), color);
        treeView->endRow();
        return row;
    };

    const Amethyst::Color4 COL_ROOT   = {1.0f,  1.0f,  1.0f,  1.0f};
    const Amethyst::Color4 COL_GROUP  = {0.85f, 0.75f, 0.45f, 1.0f};
    const Amethyst::Color4 COL_SUBGRP = {0.65f, 0.85f, 0.65f, 1.0f};
    const Amethyst::Color4 COL_LEAF   = {0.78f, 0.78f, 0.82f, 1.0f};
    const Amethyst::Color4 COL_SPEC   = {0.55f, 0.75f, 1.0f,  1.0f};

    uint32_t scene = treeView->beginRow();
    addLabel(treeView, "Scene", COL_ROOT);

    // ---- Entities: 50 flat children ----
    uint32_t entities = treeView->beginRow(scene);
    addLabel(treeView, "Entities", COL_GROUP);
    for (int i = 0; i < 50; i++) {
        addRow(entities, "entity_" + std::to_string(i), COL_LEAF);
    }
    treeView->endRow();

    // ---- Assets: two levels of nesting ----
    uint32_t assets = treeView->beginRow(scene);
    addLabel(treeView, "Assets", COL_GROUP);

    uint32_t meshes = treeView->beginRow(assets);
    addLabel(treeView, "Meshes", COL_SUBGRP);
    for (int i = 0; i < 20; i++) {
        addRow(meshes, "mesh_" + std::to_string(i) + ".obj", COL_LEAF);
    }
    treeView->endRow();

    uint32_t textures = treeView->beginRow(assets);
    addLabel(treeView, "Textures", COL_SUBGRP);
    for (int i = 0; i < 20; i++) {
        addRow(textures, "texture_" + std::to_string(i) + ".png", COL_LEAF);
    }
    treeView->endRow();

    uint32_t shaders = treeView->beginRow(assets);
    addLabel(treeView, "Shaders", COL_SUBGRP);
    for (int i = 0; i < 8; i++) {
        addRow(shaders, "shader_" + std::to_string(i) + ".glsl", COL_SPEC);
    }
    treeView->endRow();

    treeView->endRow();

    // ---- Systems: three levels ----
    uint32_t systems = treeView->beginRow(scene);
    addLabel(treeView, "Systems", COL_GROUP);

    uint32_t render = treeView->beginRow(systems);
    addLabel(treeView, "RenderSystem", COL_SUBGRP);
    addRow(render, "OpaquePass", COL_LEAF);
    addRow(render, "ShadowPass", COL_LEAF);
    addRow(render, "TransparentPass", COL_LEAF);
    addRow(render, "PostProcess", COL_LEAF);
    treeView->endRow();

    uint32_t physics = treeView->beginRow(systems);
    addLabel(treeView, "PhysicsSystem", COL_SUBGRP);
    addRow(physics, "BroadPhase", COL_LEAF);
    addRow(physics, "NarrowPhase", COL_LEAF);
    addRow(physics, "Solver", COL_LEAF);
    treeView->endRow();

    uint32_t audio = treeView->beginRow(systems);
    addLabel(treeView, "AudioSystem", COL_SUBGRP);
    addRow(audio, "SFX", COL_LEAF);
    addRow(audio, "Music", COL_LEAF);
    treeView->endRow();

    treeView->endRow();

    treeView->endRow();

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
    Amethyst::Log::Shutdown();
    return 0;
}
