#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/canvas.h"
#include "components/common.h"
#include "components/docking_layer.h"
#include "components/extensions/ui_grid_layout.h"
#include "components/panel_layer.h"
#include "components/scrolling_frame.h"
#include "components/slider.h"
#include "components/table.h"
#include "components/text_button.h"
#include "components/text_input.h"
#include "components/tree_view.h"
#include "modules/style.h"
#include "parsers/config/layout_config.h"
#include "utils/profiling.h"
#include "vk_context.h"

#include <cstdint>

int main()
{
    Amethyst::Log::Init();
    AM_LOG_INFO("Amethyst Test App");

    Amethyst::Style::load(AMETHYST_ASSETS_DIR "/theme.toml");

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

    glm::vec2 screenSize = {static_cast<float>(ctx.swapchainExtent.width), static_cast<float>(ctx.swapchainExtent.height)};

    Amethyst::Window window;
    window.absoluteSize = screenSize;
    window.absoluteRotation = 0.0f;
    window.setDisplayOrder(10);

    auto *frame1 = window.add<Amethyst::Frame>();
    frame1->name = "Frame 1";
    frame1->size = Amethyst::UDim2::fromOffset(300, 200);
    frame1->position = Amethyst::UDim2::fromOffset(200, 200);
    frame1->backgroundColor = {0.9f, 0.2f, 0.2f};
    frame1->borderPixelSize = 10.0f;
    frame1->borderMode = Amethyst::BorderMode::INSET;
    frame1->borderColor = glm::vec3(1.0f);
    frame1->cornerRadius = 10.0f;
    frame1->markDirty();
    frame1->addExtension<Amethyst::UIDragDetector>();

    auto *frame2 = frame1->add<Amethyst::Frame>();
    frame2->name = "Child of frame 1";
    frame2->size = Amethyst::UDim2::fromScale(0.5f, 0.5f);
    frame2->position = Amethyst::UDim2::fromOffset(0.0f, 0.0f);
    frame2->anchorPoint = glm::vec2(0.5f);
    frame2->backgroundColor = {0.2f, 0.9f, 0.2f};
    frame2->cornerRadius = 0.0f;
    frame2->markDirty();
    frame2->addExtension<Amethyst::UIDragDetector>();

    auto *button1 = frame2->add<Amethyst::TextButton>();
    button1->name = "button";
    button1->text = "Exit";
    button1->size = Amethyst::UDim2::fromScale(0.9f, 0.9f);
    button1->position = Amethyst::UDim2::fromScale(0.5f, 0.5f);
    button1->anchorPoint = glm::vec2(0.5f);
    button1->backgroundColor = {0.5f, 0.0f, 0.5f};
    button1->cornerRadius = 5.0f;
    button1->textScaled = true;
    button1->textXAlignment = Amethyst::TextXAlignment::CENTER;
    button1->textYAlignment = Amethyst::TextYAlignment::CENTER;
    button1->onMouseEnterCb = [button1]() {
        button1->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
        button1->markDirty();
        return Amethyst::EventResult::CONSUMED;
    };

    button1->onMouseLeaveCb = [button1]() {
        button1->size = Amethyst::UDim2::fromScale(0.9f, 0.9f);
        button1->markDirty();
        return Amethyst::EventResult::CONSUMED;
    };

    bool running = true;
    button1->onMouseButton1ClickCb = [running = &running]() {
        *running = false;
        return Amethyst::EventResult::CONSUMED;
    };
    button1->markDirty();

    TextureInfo checkerboardTex = createCheckerboardTexture(ctx, 64, 8);
    Amethyst::AmTextureId checkerboardId = backend.registerTexture(checkerboardTex.view, checkerboardTex.sampler);

    auto *textLabel = window.add<Amethyst::TextLabel>();
    textLabel->name = "pangram fixed size";
    textLabel->text = "The quick brown fox jumps over the lazy dog";
    textLabel->fontSize = 24.0f;
    textLabel->textColor = {0.9f, 0.9f, 1.0f, 1.0f};
    textLabel->strokeThickness = 0.0f;
    textLabel->strokeColor = {0.0f, 0.0f, 0.0f, 1.0f};
    textLabel->position = Amethyst::UDim2::fromOffset(10, 20);
    textLabel->backgroundTransparency = 1.0f;
    textLabel->size = Amethyst::UDim2(0.98f, 0.0f, 0.0f, 60.0f);
    textLabel->backgroundColor = glm::vec3(0.0f);
    textLabel->textXAlignment = Amethyst::TextXAlignment::CENTER;
    textLabel->textYAlignment = Amethyst::TextYAlignment::CENTER;
    textLabel->addExtension<Amethyst::UIDragDetector>();
    textLabel->markDirty();

    auto *textInput = window.add<Amethyst::TextInput>();
    textInput->name = "text input example";
    textInput->size = Amethyst::UDim2::fromOffset(400, 40);
    textInput->position = Amethyst::UDim2::fromOffset(10, 100);
    textInput->backgroundColor = {0.15f, 0.15f, 0.15f};
    textInput->borderPixelSize = 2.0f;
    textInput->borderColor = {0.3f, 0.5f, 0.8f};
    textInput->cornerRadius = 5.0f;
    textInput->fontSize = 18.0f;
    textInput->textColor = {1.0f, 1.0f, 1.0f, 1.0f};
    textInput->placeholderText = "Type something here...";
    textInput->placeholderColor = {0.5f, 0.5f, 0.5f, 1.0f};
    textInput->selectionColor = {0.3f, 0.5f, 0.9f, 0.5f};
    textInput->cursorColor = {1.0f, 1.0f, 1.0f, 1.0f};
    textInput->textXAlignment = Amethyst::TextXAlignment::LEFT;
    textInput->onTextChanged = [](const std::string &text) { AM_LOG_INFO("Text changed: '{}'", text); };
    textInput->onEnterPressed = []() { AM_LOG_INFO("Enter pressed!"); };
    textInput->markDirty();

    float sliderFloatValue = 50.0f;
    auto *sliderFloat = window.add<Amethyst::SliderFloat>();
    sliderFloat->name = "float slider";
    sliderFloat->size = Amethyst::UDim2::fromOffset(300, 40);
    sliderFloat->position = Amethyst::UDim2::fromOffset(10, 150);
    sliderFloat->backgroundColor = {0.9f, 0.9f, 0.9f};
    sliderFloat->label = "Volume";
    sliderFloat->labelSide = Amethyst::LabelSide::LEFT;
    sliderFloat->valueSuffix = "%";
    sliderFloat->min = 0.0f;
    sliderFloat->max = 100.0f;
    sliderFloat->speed = 1.0f;
    sliderFloat->valueRef = &sliderFloatValue;
    sliderFloat->markDirty();

    int sliderIntValue = 25;
    auto *sliderInt = window.add<Amethyst::SliderInt>();
    sliderInt->name = "int slider";
    sliderInt->size = Amethyst::UDim2::fromOffset(300, 40);
    sliderInt->position = Amethyst::UDim2::fromOffset(10, 200);
    sliderInt->backgroundColor = {0.1f, 0.1f, 0.1f};
    sliderInt->label = "Count";
    sliderInt->labelSide = Amethyst::LabelSide::LEFT;
    sliderInt->min = 0;
    sliderInt->max = 50;
    sliderInt->speed = 1.0f;
    sliderInt->valueRef = &sliderIntValue;
    sliderInt->markDirty();

    glm::vec2 sliderVec2Value = glm::vec2(50.0f, 75.0f);
    auto *sliderVec2 = window.add<Amethyst::SliderVec2>();
    sliderVec2->name = "vec2 slider";
    sliderVec2->size = Amethyst::UDim2::fromOffset(300, 80);
    sliderVec2->position = Amethyst::UDim2::fromOffset(10, 250);
    sliderVec2->backgroundColor = {0.1f, 0.1f, 0.1f};
    sliderVec2->label = "Position";
    sliderVec2->labelSide = Amethyst::LabelSide::LEFT;
    sliderVec2->layout = Amethyst::ValueControlLayout::STACKED;
    sliderVec2->min = glm::vec2(0.0f);
    sliderVec2->max = glm::vec2(100.0f);
    sliderVec2->speed = 1.0f;
    sliderVec2->valueRef = &sliderVec2Value;
    sliderVec2->markDirty();

    glm::vec3 sliderVec3Value = glm::vec3(0.5f, 0.3f, 0.8f);
    auto *sliderVec3 = window.add<Amethyst::SliderVec3>();
    sliderVec3->name = "vec3 slider";
    sliderVec3->size = Amethyst::UDim2::fromOffset(400, 40);
    sliderVec3->position = Amethyst::UDim2::fromOffset(10, 340);
    sliderVec3->backgroundColor = {0.1f, 0.1f, 0.1f};
    sliderVec3->label = "Color";
    sliderVec3->labelSide = Amethyst::LabelSide::LEFT;
    sliderVec3->layout = Amethyst::ValueControlLayout::SIDE_BY_SIDE;
    sliderVec3->min = glm::vec3(0.0f);
    sliderVec3->max = glm::vec3(1.0f);
    sliderVec3->speed = 0.01f;
    sliderVec3->valueRef = &sliderVec3Value;
    sliderVec3->markDirty();

    auto *scrollFrame = window.add<Amethyst::ScrollingFrame>();
    scrollFrame->name = "Scroll Test";
    scrollFrame->size = Amethyst::UDim2::fromOffset(250, 200);
    scrollFrame->position = Amethyst::UDim2::fromOffset(530, 100);
    scrollFrame->backgroundColor = {0.12f, 0.12f, 0.15f};
    scrollFrame->canvasSize = Amethyst::UDim2::fromOffset(250, 350);
    scrollFrame->scrollBarColor = {0.25f, 0.25f, 0.3f};
    scrollFrame->scrollBarThumbColor = {0.5f, 0.5f, 0.6f};
    scrollFrame->cornerRadius = 5.0f;
    scrollFrame->clipsDescendants = true;
    auto *scrollLayout = scrollFrame->addExtension<Amethyst::UIGridLayout>();
    scrollLayout->cellSize = Amethyst::UDim2::fromOffset(70, 70);
    scrollLayout->cellPadding = Amethyst::UDim2::fromOffset(10, 10);
    scrollLayout->fillDirection = Amethyst::FillDirection::FILL_HORIZONTAL;
    scrollLayout->fillDirectionMaxCells = 3;
    scrollFrame->markDirty();

    Amethyst::Color3 scrollColors[] = {
        {0.8f, 0.3f, 0.3f}, {0.3f, 0.8f, 0.3f}, {0.3f, 0.3f, 0.8f}, {0.8f, 0.8f, 0.3f}, {0.8f, 0.3f, 0.8f}, {0.3f, 0.8f, 0.8f},
        {0.6f, 0.4f, 0.2f}, {0.4f, 0.2f, 0.6f}, {0.2f, 0.6f, 0.4f}, {0.5f, 0.5f, 0.5f}, {0.9f, 0.6f, 0.3f}, {0.3f, 0.6f, 0.9f},
    };
    for (auto &color : scrollColors) {
        auto *item = scrollFrame->add<Amethyst::Frame>();
        item->backgroundColor = color;
        item->cornerRadius = 8.0f;
        item->markDirty();
    }

    auto *table = window.add<Amethyst::Table>();
    table->name = "Test Table";
    table->size = Amethyst::UDim2::fromOffset(500, 160);
    table->position = Amethyst::UDim2::fromOffset(10, 400);
    table->backgroundColor = {0.15f, 0.15f, 0.15f};
    table->addColumn({"Label", Amethyst::TableColumnSizing::STRETCH, 0.3f});
    table->addColumn({"Color", Amethyst::TableColumnSizing::STRETCH, 0.2f});
    table->addColumn({"Slider", Amethyst::TableColumnSizing::STRETCH, 0.5f});
    table->rowHeight = 40.0f;
    table->showColumnSeparators = true;
    table->columnSeparatorColor = {0.4f, 0.4f, 0.4f, 1.0f};
    table->markDirty();

    struct TableRow {
        const char *label;
        Amethyst::Color3 frameColor;
        float sliderVal;
    };
    TableRow rows[] = {
        {"Row 1", {0.8f, 0.2f, 0.2f}, 25.0f},
        {"Row 2", {0.2f, 0.8f, 0.2f}, 50.0f},
        {"Row 3", {0.2f, 0.2f, 0.8f}, 75.0f},
        {"Row 4", {0.8f, 0.8f, 0.2f}, 100.0f},
    };
    float tableSliderVals[4];
    for (int i = 0; i < 4; i++) {
        tableSliderVals[i] = rows[i].sliderVal;
        table->addRow();

        auto lbl = std::make_unique<Amethyst::TextLabel>();
        lbl->text = rows[i].label;
        lbl->textColor = {1.0f, 1.0f, 1.0f, 1.0f};
        lbl->backgroundColor = {0.2f, 0.2f, 0.2f};
        lbl->fontSize = 16.0f;
        lbl->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
        lbl->markDirty();
        table->nextCell(std::move(lbl));

        auto frm = std::make_unique<Amethyst::Frame>();
        frm->backgroundColor = rows[i].frameColor;
        frm->cornerRadius = 4.0f;
        frm->size = Amethyst::UDim2::fromOffset(30.0f, 30.0f);
        frm->position = Amethyst::UDim2::fromOffset(5.0f, 5.0f);
        frm->markDirty();
        table->nextCell(std::move(frm));

        auto sld = std::make_unique<Amethyst::SliderFloat>();
        sld->min = 0.0f;
        sld->max = 100.0f;
        sld->valueRef = &tableSliderVals[i];
        sld->size = Amethyst::UDim2::fromScale(0.95f, 0.8f);
        sld->position = Amethyst::UDim2::fromScale(0.025f, 0.1f);
        sld->markDirty();
        table->nextCell(std::move(sld));
    }

    auto treePanel = std::make_unique<Amethyst::PanelLayer>();
    treePanel->name = "Tree Panel";
    treePanel->markDirty();

    auto *treeScrollFrame = treePanel->add<Amethyst::ScrollingFrame>();
    treeScrollFrame->name = "Tree Scroll Container";
    treeScrollFrame->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    treeScrollFrame->backgroundColor = Amethyst::Color3::fromHex(0x282828);
    treeScrollFrame->canvasSize = Amethyst::UDim2::fromOffset(250, 500);
    treeScrollFrame->scrollBarColor = {0.2f, 0.2f, 0.25f};
    treeScrollFrame->scrollBarThumbColor = {0.4f, 0.4f, 0.5f};
    treeScrollFrame->clipsDescendants = true;
    treeScrollFrame->markDirty();

    auto *treeView = treeScrollFrame->add<Amethyst::TreeView>();
    treeView->name = "Test TreeView";
    treeView->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    treeView->backgroundColor = Amethyst::Color3(1.0f, 0.0f, 0.0f);
    treeView->backgroundTransparency = 0.0f;
    treeView->rowBackgroundColor = Amethyst::Color4::fromHex(0x282828);
    treeView->rowAlternateColor = Amethyst::Color4::fromHex(0x2B2B2B);
    treeView->numCols = 1;
    treeView->rowHeight = 24.0f;
    treeView->indentPerLevel = 20.0f;
    treeView->clipsDescendants = true;
    treeView->markDirty();

    auto addTreeLabel = [&](Amethyst::TreeView *tv, const char *text, Amethyst::Color4 color) {
        auto *lbl = tv->add<Amethyst::TextLabel>();
        lbl->text = text;
        lbl->textColor = color;
        lbl->backgroundTransparency = 1.0f;
        lbl->fontSize = 14.0f;
        lbl->textYAlignment = Amethyst::TextYAlignment::CENTER;
        lbl->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
        lbl->markDirty();
    };

    uint32_t scene = treeView->beginRow();
    addTreeLabel(treeView, "Scene", {1.0f, 1.0f, 1.0f, 1.0f});

    treeView->beginRow(scene);
    addTreeLabel(treeView, "Camera", {0.8f, 0.9f, 1.0f, 1.0f});
    treeView->endRow();

    uint32_t player = treeView->beginRow(scene);
    addTreeLabel(treeView, "Player", {0.5f, 1.0f, 0.5f, 1.0f});

    treeView->beginRow(player);
    addTreeLabel(treeView, "Mesh", {0.9f, 0.9f, 0.9f, 1.0f});
    treeView->endRow();

    treeView->beginRow(player);
    addTreeLabel(treeView, "Collider", {0.9f, 0.9f, 0.9f, 1.0f});
    treeView->endRow();
    treeView->endRow();

    uint32_t lights = treeView->beginRow(scene);
    addTreeLabel(treeView, "Lights", {1.0f, 1.0f, 0.5f, 1.0f});

    treeView->beginRow(lights);
    addTreeLabel(treeView, "Sun", {1.0f, 0.9f, 0.6f, 1.0f});
    treeView->endRow();

    treeView->beginRow(lights);
    addTreeLabel(treeView, "Point Light", {0.6f, 0.8f, 1.0f, 1.0f});
    treeView->endRow();
    treeView->endRow();
    treeView->endRow();

    /*
    Amethyst::TabBar tabBar(&window);
    tabBar.name = "TabBar Example";
    tabBar.size = Amethyst::UDim2::fromOffset(400, 300);
    tabBar.position = Amethyst::UDim2::fromOffset(50, 500);
    tabBar.backgroundColor = {0.2f, 0.2f, 0.2f};
    tabBar.tabPosition = Amethyst::TabBarPosition::TOP;
    tabBar.mode = Amethyst::TabBarMode::INSIDE;
    tabBar.markDirty();

    Amethyst::Frame tabContent1(&tabBar);
    tabContent1.name = "Tab 1";
    tabContent1.backgroundColor = {0.8f, 0.3f, 0.3f};
    tabContent1.markDirty();

    Amethyst::Frame tabContent2(&tabBar);
    tabContent2.name = "Tab 2";
    tabContent2.backgroundColor = {0.3f, 0.8f, 0.3f};
    tabContent2.markDirty();

    Amethyst::Frame tabContent3(&tabBar);
    tabContent3.name = "Tab 3";
    tabContent3.backgroundColor = {0.3f, 0.3f, 0.8f};
    tabContent3.markDirty();
    */

    auto *canvas = window.add<Amethyst::Canvas>();
    canvas->name = "Canvas Demo";
    canvas->size = Amethyst::UDim2::fromOffset(460, 120);
    canvas->position = Amethyst::UDim2::fromOffset(530, 310);
    canvas->backgroundColor = {0.1f, 0.1f, 0.12f};
    canvas->backgroundTransparency = 0.0f;
    canvas->cornerRadius = 5.0f;

    canvas->drawLine({10, 20}, {50, 90}, {1.0f, 0.4f, 0.4f, 1.0f}, 3.0f);
    canvas->drawTriangleFilled({90, 90}, {120, 20}, {150, 90}, {0.4f, 1.0f, 0.4f, 1.0f});
    canvas->drawTriangleStroke({170, 90}, {200, 20}, {230, 90}, {0.4f, 0.4f, 1.0f, 1.0f}, 2.0f);
    canvas->drawQuadFilled({250, 25}, {310, 25}, {310, 90}, {250, 90}, {1.0f, 1.0f, 0.4f, 1.0f});
    canvas->drawQuadStroke({320, 25}, {380, 30}, {375, 90}, {325, 85}, {1.0f, 0.4f, 1.0f, 1.0f}, 2.0f);
    canvas->drawCircleFilled({415, 55}, 30.0f, {0.4f, 1.0f, 1.0f, 1.0f});
    canvas->drawCircleStroke({415, 55}, 30.0f, {1.0f, 1.0f, 1.0f, 1.0f}, 2.0f);
    canvas->drawText("Canvas", {10, 95}, 16.0f, {1.0f, 1.0f, 1.0f, 1.0f}, 16);
    canvas->markDirty();

    auto *dockingLayer = window.add<Amethyst::DockingLayer>();
    dockingLayer->name = "Docking Example";
    dockingLayer->absoluteSize = {500.0f, 400.0f};
    dockingLayer->absolutePosition = {480.0f, 500.0f};
    dockingLayer->innerSpacing = 2.0f;
    dockingLayer->markDirty();

    auto dockPanel1 = std::make_unique<Amethyst::PanelLayer>();
    dockPanel1->name = "Panel A";
    dockPanel1->markDirty();
    auto *dockPanel1Bg = dockPanel1->add<Amethyst::Frame>();
    dockPanel1Bg->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    dockPanel1Bg->backgroundColor = {0.6f, 0.2f, 0.2f};
    dockPanel1Bg->markDirty();
    dockingLayer->dock(std::move(dockPanel1), glm::vec2(600.0f, 600.0f));

    auto dockPanel2 = std::make_unique<Amethyst::PanelLayer>();
    dockPanel2->name = "Panel B";
    dockPanel2->markDirty();
    auto *dockPanel2Bg = dockPanel2->add<Amethyst::Frame>();
    dockPanel2Bg->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    dockPanel2Bg->backgroundColor = {0.2f, 0.6f, 0.2f};
    dockPanel2Bg->markDirty();
    dockingLayer->dock(std::move(dockPanel2), glm::vec2(900.0f, 600.0f));

    auto imageLabel = std::make_unique<Amethyst::ImageLabel>();
    imageLabel->name = "checkerboard";
    imageLabel->image = backend.getAtlasTextureId();
    imageLabel->markDirty();

    dockingLayer->dock(std::move(imageLabel), glm::vec2(600.0f, 850.0f));
    dockingLayer->dock(std::move(treePanel), glm::vec2(750.0f, 600.0f));
    dockingLayer->persistLayout = true;

    if (Amethyst::LayoutConfig::instance().loadFromFile("layout.conf")) {
        if (auto *entry = Amethyst::LayoutConfig::instance().get("Docking Example")) {
            if (entry->type == Amethyst::ConfigType::DOCK_LAYOUT) dockingLayer->applyConfig(entry->dockLayout);
        }
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

        textInput->update(deltaTime);

        uint32_t imageIndex;
        if (!contextBeginFrame(ctx, imageIndex)) {
            continue;
        }

        VkCommandBuffer cmd = ctx.commandBuffers[ctx.currentFrame];

        amCtx.sync(static_cast<void *>(cmd));
        amCtx.draw(window);
        backend.record(cmd);

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
