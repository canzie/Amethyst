#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/text_button.h"
#include "parsers/ttf/ttf_parser.h"
#include "vk_context.h"

#include <cstdint>

int main()
{
    Amethyst::Log::Init();
    AM_LOG_INFO("Amethyst Test App");

    // Test TTF parsing
    Amethyst::TTF::Parser ttfParser;
    auto fontData = ttfParser.parse("/home/Thomas/dev/Amethyst/libamethyst/assets/fonts/Roboto-Regular.ttf");
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

    Amethyst::VkBackend backend;
    backend.init(initInfo);

    if (fontData) {
        backend.uploadFontData(*fontData);
    }

    Amethyst::GLFWInitInfo glfwInfo{};
    glfwInfo.window = ctx.window;
    setupGLFWCallbacks(glfwInfo);

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
    frame2.markDirty();
    frame2.addExtension<Amethyst::UIDragDetector>();

    Amethyst::TextButton button1(&frame2);
    button1.name = "button";
    button1.size = Amethyst::UDim2::fromScale(0.9f, 0.9f);
    button1.position = Amethyst::UDim2::fromScale(0.5f, 0.5f);
    button1.anchorPoint = glm::vec2(0.5f);
    button1.backgroundColor = {0.2f, 0.0f, 0.2f};
    button1.cornerRadius = 5.0f;
    button1.onMouseEnterCb = [button1 = &button1]() {
        button1->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
        button1->markDirty();
    };

    button1.onMouseLeaveCb = [button1 = &button1]() {
        button1->size = Amethyst::UDim2::fromScale(0.9f, 0.9f);
        button1->markDirty();
    };

    button1.markDirty();

    Amethyst::Frame frame3(&window);
    frame3.name = "long bar";
    frame3.size = Amethyst::UDim2(0.0f, 100.0f, 0.9f, 0.0f);
    frame3.position = Amethyst::UDim2::fromOffset(550, 0);
    frame3.backgroundColor = {0.2f, 0.2f, 0.9f};
    frame3.cornerRadius = 50.0f;
    frame3.markDirty();
    frame3.addExtension<Amethyst::UIDragDetector>();

    Amethyst::TextLabel textLabel(&window);
    textLabel.name = "test label";
    textLabel.text = "Hello Amethyst!";
    textLabel.fontSize = 128.0f;
    textLabel.textColor = {1.0f, 1.0f, 1.0f, 1.0f};
    textLabel.position = Amethyst::UDim2::fromOffset(200, 200);
    textLabel.size = Amethyst::UDim2::fromOffset(400, 50);
    textLabel.markDirty();

    window.draw(drawCtx);

    double lastTime = glfwGetTime();
    int frameCount = 0;

    while (!glfwWindowShouldClose(ctx.window)) {
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

    backend.shutdown();
    contextShutdown(ctx);

    Amethyst::Log::Shutdown();
    return 0;
}
