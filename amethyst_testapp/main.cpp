#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/common.h"
#include "components/docking_layer.h"
#include "components/panel_layer.h"
#include "components/slider.h"
#include "components/text_button.h"
#include "components/text_input.h"
#include "parsers/ttf/ttf_parser.h"
#include "vk_context.h"

#include <cstdint>

int main()
{
    Amethyst::Log::Init();
    AM_LOG_INFO("Amethyst Test App");

    // Test TTF parsing
    Amethyst::TTF::Parser ttfParser;
    auto fontData = ttfParser.parse("/home/Thomas/dev/Amethyst/libamethyst/assets/fonts/OpenSans-Regular.ttf");
    if (fontData) {
        AM_LOG_INFO("TTF parsed: {} glyphs, {} points, {} contours", fontData->glyphs.size(), fontData->points.size(),
                    fontData->contours.size());

        // Test looking up 'A' (codepoint 65)
        uint32_t glyphIdx = fontData->getGlyphIndex('A');
        const auto *glyph = fontData->getGlyph(glyphIdx);
        if (glyph) {
            AM_LOG_INFO("Glyph 'A': {} contours, advance={:.3f}", glyph->contourCount, glyph->advanceWidth);
        }
    } else {
        AM_LOG_ERROR("Failed to parse TTF");
    }

    VkContext ctx;
    if (!contextInit(ctx, 1000, 1000, "Amethyst Test")) {
        AM_LOG_ERROR("Failed to initialize Vulkan context");
        return 1;
    }

    Amethyst::TextProcessor textProcessor;
    if (fontData) {
        textProcessor.setFontData(&*fontData);
    }

    Amethyst::DrawContext drawCtx;
    drawCtx.textProcessor = &textProcessor;

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

    Amethyst::GLFWInitInfo glfwInfo{};
    glfwInfo.window = ctx.window;

    Amethyst::VkBackend backend;
    backend.init(initInfo, glfwInfo);

    if (fontData) {
        backend.uploadFontData(*fontData);
    }

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

    Amethyst::Frame frame3(&window);
    frame3.name = "long bar";
    frame3.size = Amethyst::UDim2(0.0f, 100.0f, 0.9f, 0.0f);
    frame3.position = Amethyst::UDim2::fromOffset(550, 0);
    frame3.backgroundColor = {0.2f, 0.2f, 0.9f};
    frame3.cornerRadius = 50.0f;
    frame3.zIndex = 10;
    frame3.markDirty();
    frame3.addExtension<Amethyst::UIDragDetector>();

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
    sliderFloat.onValueChanged = [](float value) { AM_LOG_INFO("Float slider value: {:.2f}", value); };
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
    sliderInt.onValueChanged = [](int value) { AM_LOG_INFO("Int slider value: {}", value); };
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
    sliderVec2.onValueChanged = [](glm::vec2 value) { AM_LOG_INFO("Vec2 slider value: ({:.2f}, {:.2f})", value.x, value.y); };
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
    sliderVec3.onValueChanged = [](glm::vec3 value) {
        AM_LOG_INFO("Vec3 slider value: ({:.2f}, {:.2f}, {:.2f})", value.x, value.y, value.z);
    };
    sliderVec3.markDirty();

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
    imageLabel.image = checkerboardId;
    imageLabel.cornerRadius = 20.0f;
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

        window.draw(drawCtx);
        VkCommandBuffer cmd = ctx.commandBuffers[ctx.currentFrame];
        backend.record(cmd);

        contextEndFrame(ctx, imageIndex);

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
