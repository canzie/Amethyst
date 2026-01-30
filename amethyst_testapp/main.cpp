#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
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
#include "utils/profiling.h"
#include "vk_context.h"

#include <cstdint>

int main()
{
    Amethyst::Log::Init();
    AM_LOG_INFO("Amethyst Test App");

    VkContext ctx;
    if (!contextInit(ctx, 1000, 1000, "Amethyst Test")) {
        AM_LOG_ERROR("Failed to initialize Vulkan context");
        return 1;
    }

    Amethyst::FontLoader fontLoader;
    if (!fontLoader.loadFont(AMETHYST_ASSETS_DIR "/fonts/OpenSans-Regular.ttf")) {
        AM_LOG_ERROR("Failed to load font");
        return 1;
    }

    Amethyst::GlyphAtlas glyphAtlas(&fontLoader);

    Amethyst::TextProcessor textProcessor;
    textProcessor.setGlyphAtlas(&glyphAtlas);

    Amethyst::DrawContext drawCtx;
    drawCtx.textProcessor = &textProcessor;
    drawCtx.glyphAtlas = &glyphAtlas;

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

    backend.createAtlasTexture(glyphAtlas.getWidth(), glyphAtlas.getHeight());
    glyphAtlas.setTextureId(backend.getAtlasTextureId());

    glm::vec2 screenSize = {static_cast<float>(ctx.swapchainExtent.width), static_cast<float>(ctx.swapchainExtent.height)};

    Amethyst::Window window;
    window.absoluteSize = screenSize;
    window.absoluteRotation = 0.0f;
    window.setDisplayOrder(10);

    Amethyst::Frame frame1(&window);
    frame1.name = "Frame 1";
    frame1.size = Amethyst::UDim2::fromOffset(300, 200);
    frame1.position = Amethyst::UDim2::fromOffset(200, 200);
    frame1.backgroundColor = {0.9f, 0.2f, 0.2f};
    frame1.borderPixelSize = 10.0f;
    frame1.borderMode = Amethyst::BorderMode::INSET;
    frame1.borderColor = glm::vec3(1.0f);
    frame1.cornerRadius = 10.0f;
    frame1.zIndex = 5.0f;
    frame1.markDirty();
    frame1.addExtension<Amethyst::UIDragDetector>();

    Amethyst::Frame frame2(&frame1);
    frame2.name = "Child of frame 1";
    frame2.size = Amethyst::UDim2::fromScale(0.5f, 0.5f);
    frame2.position = Amethyst::UDim2::fromOffset(0.0f, 0.0f);
    frame2.anchorPoint = glm::vec2(0.5f);
    frame2.backgroundColor = {0.2f, 0.9f, 0.2f};
    frame2.cornerRadius = 0.0f;
    frame2.zIndex = 1;
    frame2.markDirty();
    frame2.addExtension<Amethyst::UIDragDetector>();

    Amethyst::TextButton button1(&frame2);
    button1.name = "button";
    button1.text = "Exit";
    button1.size = Amethyst::UDim2::fromScale(0.9f, 0.9f);
    button1.position = Amethyst::UDim2::fromScale(0.5f, 0.5f);
    button1.anchorPoint = glm::vec2(0.5f);
    button1.backgroundColor = {0.2f, 0.0f, 0.2f};
    button1.cornerRadius = 5.0f;
    button1.zIndex = 2;
    button1.textScaled = true;
    button1.textXAlignment = Amethyst::TextXAlignment::CENTER;
    button1.textYAlignment = Amethyst::TextYAlignment::CENTER;
    button1.onMouseEnterCb = [button1 = &button1]() {
        button1->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
        button1->markDirty();
    };

    button1.onMouseLeaveCb = [button1 = &button1]() {
        button1->size = Amethyst::UDim2::fromScale(0.9f, 0.9f);
        button1->markDirty();
    };

    bool running = true;
    button1.onMouseButton1ClickCb = [running = &running]() { *running = false; };

    button1.markDirty();

    TextureInfo checkerboardTex = createCheckerboardTexture(ctx, 64, 8);
    Amethyst::AmTextureId checkerboardId = backend.registerTexture(checkerboardTex.view, checkerboardTex.sampler);

    Amethyst::TextLabel textLabel(&window);
    textLabel.name = "pangram fixed size";
    textLabel.text = "The quick brown fox jumps over the lazy dog";
    textLabel.fontSize = 24.0f;
    textLabel.textColor = {0.9f, 0.9f, 1.0f, 1.0f};
    textLabel.strokeThickness = 0.0f;
    textLabel.strokeColor = {0.0f, 0.0f, 0.0f, 1.0f};
    textLabel.position = Amethyst::UDim2::fromOffset(10, 20);
    textLabel.backgroundTransparency = 1.0f;
    textLabel.size = Amethyst::UDim2(0.98f, 0.0f, 0.0f, 60.0f);
    textLabel.backgroundColor = glm::vec3(0.0f);
    textLabel.textXAlignment = Amethyst::TextXAlignment::CENTER;
    textLabel.textYAlignment = Amethyst::TextYAlignment::CENTER;
    textLabel.addExtension<Amethyst::UIDragDetector>();
    textLabel.markDirty();

    Amethyst::TextInput textInput(&window);
    textInput.name = "text input example";
    textInput.size = Amethyst::UDim2::fromOffset(400, 40);
    textInput.position = Amethyst::UDim2::fromOffset(10, 100);
    textInput.backgroundColor = {0.15f, 0.15f, 0.15f};
    textInput.borderPixelSize = 2.0f;
    textInput.borderColor = {0.3f, 0.5f, 0.8f};
    textInput.cornerRadius = 5.0f;
    textInput.fontSize = 18.0f;
    textInput.textColor = {1.0f, 1.0f, 1.0f, 1.0f};
    textInput.placeholderText = "Type something here...";
    textInput.placeholderColor = {0.5f, 0.5f, 0.5f, 1.0f};
    textInput.selectionColor = {0.3f, 0.5f, 0.9f, 0.5f};
    textInput.cursorColor = {1.0f, 1.0f, 1.0f, 1.0f};
    textInput.textXAlignment = Amethyst::TextXAlignment::LEFT;
    textInput.onTextChanged = [](const std::string &text) { AM_LOG_INFO("Text changed: '{}'", text); };
    textInput.onEnterPressed = []() { AM_LOG_INFO("Enter pressed!"); };
    textInput.markDirty();

    float sliderFloatValue = 50.0f;
    Amethyst::SliderFloat sliderFloat(&window);
    sliderFloat.name = "float slider";
    sliderFloat.size = Amethyst::UDim2::fromOffset(300, 40);
    sliderFloat.position = Amethyst::UDim2::fromOffset(10, 150);
    sliderFloat.backgroundColor = {0.1f, 0.1f, 0.1f};
    sliderFloat.label = "Volume";
    sliderFloat.labelSide = Amethyst::LabelSide::LEFT;
    sliderFloat.valueSuffix = "%";
    sliderFloat.min = 0.0f;
    sliderFloat.max = 100.0f;
    sliderFloat.speed = 1.0f;
    sliderFloat.valueRef = &sliderFloatValue;
    sliderFloat.markDirty();

    int sliderIntValue = 25;
    Amethyst::SliderInt sliderInt(&window);
    sliderInt.name = "int slider";
    sliderInt.size = Amethyst::UDim2::fromOffset(300, 40);
    sliderInt.position = Amethyst::UDim2::fromOffset(10, 200);
    sliderInt.backgroundColor = {0.1f, 0.1f, 0.1f};
    sliderInt.label = "Count";
    sliderInt.labelSide = Amethyst::LabelSide::LEFT;
    sliderInt.min = 0;
    sliderInt.max = 50;
    sliderInt.speed = 1.0f;
    sliderInt.valueRef = &sliderIntValue;
    sliderInt.markDirty();

    glm::vec2 sliderVec2Value = glm::vec2(50.0f, 75.0f);
    Amethyst::SliderVec2 sliderVec2(&window);
    sliderVec2.name = "vec2 slider";
    sliderVec2.size = Amethyst::UDim2::fromOffset(300, 80);
    sliderVec2.position = Amethyst::UDim2::fromOffset(10, 250);
    sliderVec2.backgroundColor = {0.1f, 0.1f, 0.1f};
    sliderVec2.label = "Position";
    sliderVec2.labelSide = Amethyst::LabelSide::LEFT;
    sliderVec2.layout = Amethyst::ValueControlLayout::STACKED;
    sliderVec2.min = glm::vec2(0.0f);
    sliderVec2.max = glm::vec2(100.0f);
    sliderVec2.speed = 1.0f;
    sliderVec2.valueRef = &sliderVec2Value;
    sliderVec2.markDirty();

    glm::vec3 sliderVec3Value = glm::vec3(0.5f, 0.3f, 0.8f);
    Amethyst::SliderVec3 sliderVec3(&window);
    sliderVec3.name = "vec3 slider";
    sliderVec3.size = Amethyst::UDim2::fromOffset(400, 40);
    sliderVec3.position = Amethyst::UDim2::fromOffset(10, 340);
    sliderVec3.backgroundColor = {0.1f, 0.1f, 0.1f};
    sliderVec3.label = "Color";
    sliderVec3.labelSide = Amethyst::LabelSide::LEFT;
    sliderVec3.layout = Amethyst::ValueControlLayout::SIDE_BY_SIDE;
    sliderVec3.min = glm::vec3(0.0f);
    sliderVec3.max = glm::vec3(1.0f);
    sliderVec3.speed = 0.01f;
    sliderVec3.valueRef = &sliderVec3Value;
    sliderVec3.markDirty();

    Amethyst::ScrollingFrame scrollFrame(&window);
    scrollFrame.name = "Scroll Test";
    scrollFrame.size = Amethyst::UDim2::fromOffset(250, 200);
    scrollFrame.position = Amethyst::UDim2::fromOffset(530, 100);
    scrollFrame.backgroundColor = {0.12f, 0.12f, 0.15f};
    scrollFrame.canvasSize = Amethyst::UDim2::fromOffset(250, 350);
    scrollFrame.scrollBarColor = {0.25f, 0.25f, 0.3f};
    scrollFrame.scrollBarThumbColor = {0.5f, 0.5f, 0.6f};
    scrollFrame.cornerRadius = 5.0f;
    scrollFrame.clipsDescendants = true;
    auto *scrollLayout = scrollFrame.addExtension<Amethyst::UIGridLayout>();
    scrollLayout->cellSize = Amethyst::UDim2::fromOffset(70, 70);
    scrollLayout->cellPadding = Amethyst::UDim2::fromOffset(10, 10);
    scrollLayout->fillDirection = Amethyst::FillDirection::FILL_HORIZONTAL;
    scrollLayout->fillDirectionMaxCells = 3;
    scrollFrame.markDirty();

    Amethyst::Frame scrollItem1(&scrollFrame);
    scrollItem1.backgroundColor = {0.8f, 0.3f, 0.3f};
    scrollItem1.cornerRadius = 8.0f;
    scrollItem1.markDirty();

    Amethyst::Frame scrollItem2(&scrollFrame);
    scrollItem2.backgroundColor = {0.3f, 0.8f, 0.3f};
    scrollItem2.cornerRadius = 8.0f;
    scrollItem2.markDirty();

    Amethyst::Frame scrollItem3(&scrollFrame);
    scrollItem3.backgroundColor = {0.3f, 0.3f, 0.8f};
    scrollItem3.cornerRadius = 8.0f;
    scrollItem3.markDirty();

    Amethyst::Frame scrollItem4(&scrollFrame);
    scrollItem4.backgroundColor = {0.8f, 0.8f, 0.3f};
    scrollItem4.cornerRadius = 8.0f;
    scrollItem4.markDirty();

    Amethyst::Frame scrollItem5(&scrollFrame);
    scrollItem5.backgroundColor = {0.8f, 0.3f, 0.8f};
    scrollItem5.cornerRadius = 8.0f;
    scrollItem5.markDirty();

    Amethyst::Frame scrollItem6(&scrollFrame);
    scrollItem6.backgroundColor = {0.3f, 0.8f, 0.8f};
    scrollItem6.cornerRadius = 8.0f;
    scrollItem6.markDirty();

    Amethyst::Frame scrollItem7(&scrollFrame);
    scrollItem7.backgroundColor = {0.6f, 0.4f, 0.2f};
    scrollItem7.cornerRadius = 8.0f;
    scrollItem7.markDirty();

    Amethyst::Frame scrollItem8(&scrollFrame);
    scrollItem8.backgroundColor = {0.4f, 0.2f, 0.6f};
    scrollItem8.cornerRadius = 8.0f;
    scrollItem8.markDirty();

    Amethyst::Frame scrollItem9(&scrollFrame);
    scrollItem9.backgroundColor = {0.2f, 0.6f, 0.4f};
    scrollItem9.cornerRadius = 8.0f;
    scrollItem9.markDirty();

    Amethyst::Frame scrollItem10(&scrollFrame);
    scrollItem10.backgroundColor = {0.5f, 0.5f, 0.5f};
    scrollItem10.cornerRadius = 8.0f;
    scrollItem10.markDirty();

    Amethyst::Frame scrollItem11(&scrollFrame);
    scrollItem11.backgroundColor = {0.9f, 0.6f, 0.3f};
    scrollItem11.cornerRadius = 8.0f;
    scrollItem11.markDirty();

    Amethyst::Frame scrollItem12(&scrollFrame);
    scrollItem12.backgroundColor = {0.3f, 0.6f, 0.9f};
    scrollItem12.cornerRadius = 8.0f;
    scrollItem12.markDirty();

    Amethyst::Table table(&window);
    table.name = "Test Table";
    table.size = Amethyst::UDim2::fromOffset(500, 160);
    table.position = Amethyst::UDim2::fromOffset(10, 400);
    table.backgroundColor = {0.15f, 0.15f, 0.15f};
    table.numCols = 3;
    table.columnWeights = {0.3f, 0.2f, 0.5f};
    table.rowHeight = 40.0f;
    table.showColumnSeparators = true;
    table.columnSeparatorColor = {0.4f, 0.4f, 0.4f, 1.0f};
    table.markDirty();

    Amethyst::TextLabel tableLabel1(&table);
    tableLabel1.text = "Row 1";
    tableLabel1.textColor = {1.0f, 1.0f, 1.0f, 1.0f};
    tableLabel1.backgroundColor = {0.2f, 0.2f, 0.2f};
    tableLabel1.fontSize = 16.0f;
    tableLabel1.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    tableLabel1.markDirty();

    Amethyst::Frame tableFrame1(&table);
    tableFrame1.backgroundColor = {0.8f, 0.2f, 0.2f};
    tableFrame1.cornerRadius = 4.0f;
    tableFrame1.size = Amethyst::UDim2::fromOffset(30.0f, 30.0f);
    tableFrame1.position = Amethyst::UDim2::fromOffset(5.0f, 5.0f);
    tableFrame1.markDirty();

    float tableSliderVal1 = 25.0f;
    Amethyst::SliderFloat tableSlider1(&table);
    tableSlider1.min = 0.0f;
    tableSlider1.max = 100.0f;
    tableSlider1.valueRef = &tableSliderVal1;
    tableSlider1.size = Amethyst::UDim2::fromScale(0.95f, 0.8f);
    tableSlider1.position = Amethyst::UDim2::fromScale(0.025f, 0.1f);
    tableSlider1.markDirty();

    Amethyst::TextLabel tableLabel2(&table);
    tableLabel2.text = "Row 2";
    tableLabel2.textColor = {1.0f, 1.0f, 1.0f, 1.0f};
    tableLabel2.backgroundColor = {0.2f, 0.2f, 0.2f};
    tableLabel2.fontSize = 16.0f;
    tableLabel2.size = Amethyst::UDim2::fromScale(0.98f, 1.0f);
    tableLabel2.markDirty();

    Amethyst::Frame tableFrame2(&table);
    tableFrame2.backgroundColor = {0.2f, 0.8f, 0.2f};
    tableFrame2.cornerRadius = 4.0f;
    tableFrame2.size = Amethyst::UDim2::fromOffset(30.0f, 30.0f);
    tableFrame2.position = Amethyst::UDim2::fromOffset(5.0f, 5.0f);
    tableFrame2.markDirty();

    float tableSliderVal2 = 50.0f;
    Amethyst::SliderFloat tableSlider2(&table);
    tableSlider2.min = 0.0f;
    tableSlider2.max = 100.0f;
    tableSlider2.valueRef = &tableSliderVal2;
    tableSlider2.size = Amethyst::UDim2::fromScale(0.95f, 0.8f);
    tableSlider2.position = Amethyst::UDim2::fromScale(0.025f, 0.1f);
    tableSlider2.markDirty();

    Amethyst::TextLabel tableLabel3(&table);
    tableLabel3.text = "Row 3";
    tableLabel3.textColor = {1.0f, 1.0f, 1.0f, 1.0f};
    tableLabel3.backgroundColor = {0.2f, 0.2f, 0.2f};
    tableLabel3.fontSize = 16.0f;
    tableLabel3.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    tableLabel3.markDirty();

    Amethyst::Frame tableFrame3(&table);
    tableFrame3.backgroundColor = {0.2f, 0.2f, 0.8f};
    tableFrame3.cornerRadius = 4.0f;
    tableFrame3.size = Amethyst::UDim2::fromOffset(30.0f, 30.0f);
    tableFrame3.position = Amethyst::UDim2::fromOffset(5.0f, 5.0f);
    tableFrame3.markDirty();

    float tableSliderVal3 = 75.0f;
    Amethyst::SliderFloat tableSlider3(&table);
    tableSlider3.min = 0.0f;
    tableSlider3.max = 100.0f;
    tableSlider3.valueRef = &tableSliderVal3;
    tableSlider3.size = Amethyst::UDim2::fromScale(0.95f, 0.8f);
    tableSlider3.position = Amethyst::UDim2::fromScale(0.025f, 0.1f);
    tableSlider3.markDirty();

    Amethyst::TextLabel tableLabel4(&table);
    tableLabel4.text = "Row 4";
    tableLabel4.textColor = {1.0f, 1.0f, 1.0f, 1.0f};
    tableLabel4.backgroundColor = {0.2f, 0.2f, 0.2f};
    tableLabel4.fontSize = 16.0f;
    tableLabel4.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    tableLabel4.markDirty();

    Amethyst::Frame tableFrame4(&table);
    tableFrame4.backgroundColor = {0.8f, 0.8f, 0.2f};
    tableFrame4.cornerRadius = 4.0f;
    tableFrame4.size = Amethyst::UDim2::fromOffset(30.0f, 30.0f);
    tableFrame4.position = Amethyst::UDim2::fromOffset(5.0f, 5.0f);
    tableFrame4.markDirty();

    float tableSliderVal4 = 100.0f;
    Amethyst::SliderFloat tableSlider4(&table);
    tableSlider4.min = 0.0f;
    tableSlider4.max = 100.0f;
    tableSlider4.valueRef = &tableSliderVal4;
    tableSlider4.size = Amethyst::UDim2::fromScale(0.95f, 0.8f);
    tableSlider4.position = Amethyst::UDim2::fromScale(0.025f, 0.1f);
    tableSlider4.markDirty();

    Amethyst::TreeView treeView(&window);
    treeView.name = "Test TreeView";
    treeView.size = Amethyst::UDim2::fromOffset(300, 250);
    treeView.position = Amethyst::UDim2::fromOffset(520, 400);
    treeView.backgroundColor = {0.12f, 0.12f, 0.14f};
    treeView.numCols = 1;
    treeView.rowHeight = 24.0f;
    treeView.indentPerLevel = 20.0f;
    treeView.markDirty();

    uint32_t scene = treeView.beginRow();
    Amethyst::TextLabel sceneLabel(&treeView);
    sceneLabel.text = "Scene";
    sceneLabel.textColor = {1.0f, 1.0f, 1.0f, 1.0f};
    sceneLabel.backgroundTransparency = 1.0f;
    sceneLabel.fontSize = 14.0f;
    sceneLabel.textYAlignment = Amethyst::TextYAlignment::CENTER;
    sceneLabel.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    sceneLabel.markDirty();

    treeView.beginRow(scene);
    Amethyst::TextLabel cameraLabel(&treeView);
    cameraLabel.text = "Camera";
    cameraLabel.textColor = {0.8f, 0.9f, 1.0f, 1.0f};
    cameraLabel.backgroundTransparency = 1.0f;
    cameraLabel.fontSize = 14.0f;
    cameraLabel.textYAlignment = Amethyst::TextYAlignment::CENTER;
    cameraLabel.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    cameraLabel.markDirty();
    treeView.endRow();

    uint32_t player = treeView.beginRow(scene);
    Amethyst::TextLabel playerLabel(&treeView);
    playerLabel.text = "Player";
    playerLabel.textColor = {0.5f, 1.0f, 0.5f, 1.0f};
    playerLabel.backgroundTransparency = 1.0f;
    playerLabel.fontSize = 14.0f;
    playerLabel.textYAlignment = Amethyst::TextYAlignment::CENTER;
    playerLabel.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    playerLabel.markDirty();

    treeView.beginRow(player);
    Amethyst::TextLabel meshLabel(&treeView);
    meshLabel.text = "Mesh";
    meshLabel.textColor = {0.9f, 0.9f, 0.9f, 1.0f};
    meshLabel.backgroundTransparency = 1.0f;
    meshLabel.fontSize = 14.0f;
    meshLabel.textYAlignment = Amethyst::TextYAlignment::CENTER;
    meshLabel.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    meshLabel.markDirty();
    treeView.endRow();

    treeView.beginRow(player);
    Amethyst::TextLabel colliderLabel(&treeView);
    colliderLabel.text = "Collider";
    colliderLabel.textColor = {0.9f, 0.9f, 0.9f, 1.0f};
    colliderLabel.backgroundTransparency = 1.0f;
    colliderLabel.fontSize = 14.0f;
    colliderLabel.textYAlignment = Amethyst::TextYAlignment::CENTER;
    colliderLabel.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    colliderLabel.markDirty();
    treeView.endRow();
    treeView.endRow();

    uint32_t lights = treeView.beginRow(scene);
    Amethyst::TextLabel lightsLabel(&treeView);
    lightsLabel.text = "Lights";
    lightsLabel.textColor = {1.0f, 1.0f, 0.5f, 1.0f};
    lightsLabel.backgroundTransparency = 1.0f;
    lightsLabel.fontSize = 14.0f;
    lightsLabel.textYAlignment = Amethyst::TextYAlignment::CENTER;
    lightsLabel.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    lightsLabel.markDirty();

    treeView.beginRow(lights);
    Amethyst::TextLabel sunLabel(&treeView);
    sunLabel.text = "Sun";
    sunLabel.textColor = {1.0f, 0.9f, 0.6f, 1.0f};
    sunLabel.backgroundTransparency = 1.0f;
    sunLabel.fontSize = 14.0f;
    sunLabel.textYAlignment = Amethyst::TextYAlignment::CENTER;
    sunLabel.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    sunLabel.markDirty();
    treeView.endRow();

    treeView.beginRow(lights);
    Amethyst::TextLabel pointLabel(&treeView);
    pointLabel.text = "Point Light";
    pointLabel.textColor = {0.6f, 0.8f, 1.0f, 1.0f};
    pointLabel.backgroundTransparency = 1.0f;
    pointLabel.fontSize = 14.0f;
    pointLabel.textYAlignment = Amethyst::TextYAlignment::CENTER;
    pointLabel.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    pointLabel.zIndex = 100;
    pointLabel.markDirty();
    treeView.endRow();
    treeView.endRow();
    treeView.endRow();

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

    Amethyst::DockingLayer dockingLayer(&window);
    dockingLayer.name = "Docking Example";
    dockingLayer.absoluteSize = {500.0f, 400.0f};
    dockingLayer.absolutePosition = {480.0f, 500.0f};
    dockingLayer.innerSpacing = 2.0f;

    dockingLayer.markDirty();

    Amethyst::PanelLayer dockPanel1;
    dockPanel1.name = "Panel A";
    dockPanel1.markDirty();
    Amethyst::Frame dockPanel1Bg(&dockPanel1);
    dockPanel1Bg.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    dockPanel1Bg.backgroundColor = {0.6f, 0.2f, 0.2f};
    dockPanel1Bg.markDirty();
    dockingLayer.dock(&dockPanel1, glm::vec2(600.0f, 600.0f));

    Amethyst::PanelLayer dockPanel2;
    dockPanel2.name = "Panel B";
    dockPanel2.markDirty();
    Amethyst::Frame dockPanel2Bg(&dockPanel2);
    dockPanel2Bg.size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
    dockPanel2Bg.backgroundColor = {0.2f, 0.6f, 0.2f};
    dockPanel2Bg.markDirty();
    dockingLayer.dock(&dockPanel2, glm::vec2(900.0f, 600.0f));

    Amethyst::ImageLabel imageLabel;
    imageLabel.name = "checkerboard";
    imageLabel.image = backend.getAtlasTextureId();
    imageLabel.markDirty();

    dockingLayer.dock(&imageLabel, glm::vec2(600.0f, 850.0f));

    window.draw(drawCtx);

    double lastTime = glfwGetTime();
    int frameCount = 0;
    double lastUpdateTime = glfwGetTime();

    while (!glfwWindowShouldClose(ctx.window) && running) {
        glfwPollEvents();

        double currentUpdateTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentUpdateTime - lastUpdateTime);
        lastUpdateTime = currentUpdateTime;

        textInput.update(deltaTime);

        uint32_t imageIndex;
        if (!contextBeginFrame(ctx, imageIndex)) {
            continue;
        }

        VkCommandBuffer cmd = ctx.commandBuffers[ctx.currentFrame];

        if (glyphAtlas.isDirty()) {
            backend.uploadAtlasData(cmd, glyphAtlas.getPixels(), glyphAtlas.getWidth(), glyphAtlas.getHeight());
            glyphAtlas.clearDirty();
        }

        window.draw(drawCtx);
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
