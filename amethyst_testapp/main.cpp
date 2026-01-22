#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/common.h"
#include "components/docking_layer.h"
#include "components/tab_bar.h"
#include "components/text_button.h"
#include "parsers/ttf/ttf_parser.h"
#include "rendering/geometry_registry.h"
#include "vk_context.h"

#include <cstdint>

namespace Amethyst {
void testZIndexOrdering();
}

int main()
{
    Amethyst::Log::Init();
    AM_LOG_INFO("Amethyst Test App");

    Amethyst::testZIndexOrdering();

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

    Amethyst::GeometryRegistry geometryRegistry;
    Amethyst::TextRegistry textRegistry;
    Amethyst::TextProcessor textProcessor;
    if (fontData) {
        textProcessor.setFontData(&*fontData);
    }

    Amethyst::DrawContext drawCtx;
    drawCtx.geometry = &geometryRegistry;
    drawCtx.text = &textRegistry;
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
    dockingLayer.markDirty();

    Amethyst::Frame dockPanel1;
    dockPanel1.name = "Panel A";
    dockPanel1.backgroundColor = {0.6f, 0.2f, 0.2f};
    dockPanel1.markDirty();
    dockingLayer.dock(&dockPanel1, glm::vec2(600.0f, 600.0f));

    Amethyst::Frame dockPanel2;
    dockPanel2.name = "Panel B";
    dockPanel2.backgroundColor = {0.2f, 0.6f, 0.2f};
    dockPanel2.markDirty();
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

    while (!glfwWindowShouldClose(ctx.window) && running) {
        glfwPollEvents();

        uint32_t imageIndex;
        if (!contextBeginFrame(ctx, imageIndex)) {
            continue;
        }

        window.draw(drawCtx);
        VkCommandBuffer cmd = ctx.commandBuffers[ctx.currentFrame];
        backend.record(cmd, geometryRegistry, textRegistry);

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
