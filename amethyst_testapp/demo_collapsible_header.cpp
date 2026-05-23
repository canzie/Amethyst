#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/collapsible_header.h"
#include "components/table.h"
#include "modules/style.h"
#include "vk_context.h"

#include <string>

int main()
{
    Amethyst::Log::Init();

    Amethyst::Style::load(AMETHYST_ASSETS_DIR "/theme.toml");

    VkContext ctx;
    if (!contextInit(ctx, 900, 700, "Amethyst - CollapsibleHeader Demo")) {
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

    glm::vec2 screenSize = {
        static_cast<float>(ctx.swapchainExtent.width),
        static_cast<float>(ctx.swapchainExtent.height),
    };

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
    statusLabel->text = "Click any header to toggle";
    statusLabel->markDirty();

    auto *container = window.add<Amethyst::ScrollingFrame>();
    container->size = Amethyst::UDim2(1.0f, -20.0f, 1.0f, -34.0f);
    container->position = Amethyst::UDim2::fromOffset(10.0f, 29.0f);
    container->backgroundColor = Amethyst::Color3::fromHex(0x1A1A1E);
    container->canvasSize = Amethyst::UDim2::fromOffset(880, 1200);
    container->scrollBarColor = {0.2f, 0.2f, 0.25f};
    container->scrollBarThumbColor = {0.45f, 0.45f, 0.55f};
    container->clipsDescendants = true;
    container->markDirty();

    // --- Section 1: General Settings ---
    auto *section1 = container->add<Amethyst::CollapsibleHeader>();
    section1->title = "General Settings";
    section1->size = Amethyst::UDim2(1.0f, 0.0f, 0.0f, 180.0f);
    section1->position = Amethyst::UDim2::fromOffset(0.0f, 0.0f);
    section1->headerColor = {0.22f, 0.28f, 0.38f};
    section1->headerHeight = 32.0f;
    section1->fontSize = 15.0f;
    section1->titleColor = {1.0f, 1.0f, 1.0f, 1.0f};
    section1->indicatorColor = {0.8f, 0.85f, 1.0f, 1.0f};
    section1->backgroundColor = {0.16f, 0.16f, 0.18f};
    section1->backgroundTransparency = 0.0f;
    section1->cornerRadius = 4.0f;
    section1->headerCornerRadius = 4.0f;
    section1->onToggled = [statusLabel](bool exp) {
        statusLabel->text = std::string("General Settings: ") + (exp ? "expanded" : "collapsed");
        statusLabel->markDirty();
    };
    section1->markDirty();

    auto *lbl1 = section1->add<Amethyst::TextLabel>();
    lbl1->text = "Resolution: 1920x1080";
    lbl1->size = Amethyst::UDim2(1.0f, -20.0f, 0.0f, 24.0f);
    lbl1->position = Amethyst::UDim2::fromOffset(10.0f, 10.0f);
    lbl1->textColor = {0.8f, 0.8f, 0.8f, 1.0f};
    lbl1->fontSize = 13.0f;
    lbl1->backgroundTransparency = 1.0f;
    lbl1->textYAlignment = Amethyst::TextYAlignment::CENTER;
    lbl1->markDirty();

    auto *lbl2 = section1->add<Amethyst::TextLabel>();
    lbl2->text = "Fullscreen: Off";
    lbl2->size = Amethyst::UDim2(1.0f, -20.0f, 0.0f, 24.0f);
    lbl2->position = Amethyst::UDim2::fromOffset(10.0f, 38.0f);
    lbl2->textColor = {0.8f, 0.8f, 0.8f, 1.0f};
    lbl2->fontSize = 13.0f;
    lbl2->backgroundTransparency = 1.0f;
    lbl2->textYAlignment = Amethyst::TextYAlignment::CENTER;
    lbl2->markDirty();

    auto *lbl3 = section1->add<Amethyst::TextLabel>();
    lbl3->text = "VSync: Enabled";
    lbl3->size = Amethyst::UDim2(1.0f, -20.0f, 0.0f, 24.0f);
    lbl3->position = Amethyst::UDim2::fromOffset(10.0f, 66.0f);
    lbl3->textColor = {0.8f, 0.8f, 0.8f, 1.0f};
    lbl3->fontSize = 13.0f;
    lbl3->backgroundTransparency = 1.0f;
    lbl3->textYAlignment = Amethyst::TextYAlignment::CENTER;
    lbl3->markDirty();

    // --- Section 2: Audio (starts collapsed) ---
    auto *section2 = container->add<Amethyst::CollapsibleHeader>();
    section2->title = "Audio";
    section2->size = Amethyst::UDim2(1.0f, 0.0f, 0.0f, 150.0f);
    section2->position = Amethyst::UDim2::fromOffset(0.0f, 190.0f);
    section2->headerColor = {0.28f, 0.22f, 0.32f};
    section2->headerHeight = 32.0f;
    section2->fontSize = 15.0f;
    section2->titleColor = {1.0f, 1.0f, 1.0f, 1.0f};
    section2->indicatorColor = {0.9f, 0.75f, 1.0f, 1.0f};
    section2->backgroundColor = {0.16f, 0.16f, 0.18f};
    section2->backgroundTransparency = 0.0f;
    section2->cornerRadius = 4.0f;
    section2->headerCornerRadius = 4.0f;
    section2->expanded = false;
    section2->onToggled = [statusLabel](bool exp) {
        statusLabel->text = std::string("Audio: ") + (exp ? "expanded" : "collapsed");
        statusLabel->markDirty();
    };
    section2->markDirty();

    auto *audioLbl1 = section2->add<Amethyst::TextLabel>();
    audioLbl1->text = "Master Volume: 80%";
    audioLbl1->size = Amethyst::UDim2(1.0f, -20.0f, 0.0f, 24.0f);
    audioLbl1->position = Amethyst::UDim2::fromOffset(10.0f, 10.0f);
    audioLbl1->textColor = {0.8f, 0.8f, 0.8f, 1.0f};
    audioLbl1->fontSize = 13.0f;
    audioLbl1->backgroundTransparency = 1.0f;
    audioLbl1->textYAlignment = Amethyst::TextYAlignment::CENTER;
    audioLbl1->markDirty();

    auto *audioLbl2 = section2->add<Amethyst::TextLabel>();
    audioLbl2->text = "Music: 60%";
    audioLbl2->size = Amethyst::UDim2(1.0f, -20.0f, 0.0f, 24.0f);
    audioLbl2->position = Amethyst::UDim2::fromOffset(10.0f, 38.0f);
    audioLbl2->textColor = {0.8f, 0.8f, 0.8f, 1.0f};
    audioLbl2->fontSize = 13.0f;
    audioLbl2->backgroundTransparency = 1.0f;
    audioLbl2->textYAlignment = Amethyst::TextYAlignment::CENTER;
    audioLbl2->markDirty();

    auto *audioLbl3 = section2->add<Amethyst::TextLabel>();
    audioLbl3->text = "SFX: 100%";
    audioLbl3->size = Amethyst::UDim2(1.0f, -20.0f, 0.0f, 24.0f);
    audioLbl3->position = Amethyst::UDim2::fromOffset(10.0f, 66.0f);
    audioLbl3->textColor = {0.8f, 0.8f, 0.8f, 1.0f};
    audioLbl3->fontSize = 13.0f;
    audioLbl3->backgroundTransparency = 1.0f;
    audioLbl3->textYAlignment = Amethyst::TextYAlignment::CENTER;
    audioLbl3->markDirty();

    // --- Section 3: Graphics (table inside) ---
    auto *section3 = container->add<Amethyst::CollapsibleHeader>();
    section3->title = "Graphics";
    section3->size = Amethyst::UDim2(1.0f, 0.0f, 0.0f, 210.0f);
    section3->position = Amethyst::UDim2::fromOffset(0.0f, 350.0f);
    section3->headerColor = {0.2f, 0.32f, 0.25f};
    section3->headerHeight = 32.0f;
    section3->fontSize = 15.0f;
    section3->titleColor = {1.0f, 1.0f, 1.0f, 1.0f};
    section3->indicatorColor = {0.6f, 1.0f, 0.7f, 1.0f};
    section3->indicatorSize = 12.0f;
    section3->backgroundColor = {0.16f, 0.16f, 0.18f};
    section3->backgroundTransparency = 0.0f;
    section3->cornerRadius = 4.0f;
    section3->headerCornerRadius = 4.0f;
    section3->onToggled = [statusLabel](bool exp) {
        statusLabel->text = std::string("Graphics: ") + (exp ? "expanded" : "collapsed");
        statusLabel->markDirty();
    };
    section3->markDirty();

    auto *table = section3->add<Amethyst::Table>();
    table->size = Amethyst::UDim2(1.0f, -10.0f, 0.0f, 168.0f);
    table->position = Amethyst::UDim2::fromOffset(5.0f, 5.0f);
    table->numCols = 2;
    table->columnWeights = {0.55f, 0.45f};
    table->rowHeight = 28.0f;
    table->showColumnSeparators = true;
    table->columnSeparatorColor = {0.3f, 0.3f, 0.35f, 0.5f};
    table->backgroundColor = {0.14f, 0.14f, 0.16f};
    table->backgroundTransparency = 0.0f;
    table->cellPadding = Amethyst::UDim4{
        Amethyst::UDim::fromOffset(2.0f),
        Amethyst::UDim::fromOffset(8.0f),
        Amethyst::UDim::fromOffset(2.0f),
        Amethyst::UDim::fromOffset(8.0f),
    };
    table->markDirty();

    struct SettingRow {
        const char *setting;
        const char *value;
    };
    SettingRow graphicsRows[] = {
        {"Shadow Quality",    "Ultra"},
        {"Anti-Aliasing",     "TAA"},
        {"Texture Quality",   "High"},
        {"Draw Distance",     "Far"},
        {"Ambient Occlusion", "HBAO+"},
        {"Anisotropic",       "16x"},
    };

    for (auto &row : graphicsRows) {
        auto *settingLbl = table->add<Amethyst::TextLabel>();
        settingLbl->text = row.setting;
        settingLbl->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
        settingLbl->textColor = {0.7f, 0.7f, 0.7f, 1.0f};
        settingLbl->fontSize = 13.0f;
        settingLbl->backgroundTransparency = 1.0f;
        settingLbl->textYAlignment = Amethyst::TextYAlignment::CENTER;
        settingLbl->markDirty();

        auto *valueLbl = table->add<Amethyst::TextLabel>();
        valueLbl->text = row.value;
        valueLbl->size = Amethyst::UDim2::fromScale(1.0f, 1.0f);
        valueLbl->textColor = {0.5f, 0.9f, 0.6f, 1.0f};
        valueLbl->fontSize = 13.0f;
        valueLbl->backgroundTransparency = 1.0f;
        valueLbl->textYAlignment = Amethyst::TextYAlignment::CENTER;
        valueLbl->markDirty();
    }

    // --- Section 4: No indicator ---
    auto *section4 = container->add<Amethyst::CollapsibleHeader>();
    section4->title = "Controls (no indicator)";
    section4->size = Amethyst::UDim2(1.0f, 0.0f, 0.0f, 120.0f);
    section4->position = Amethyst::UDim2::fromOffset(0.0f, 560.0f);
    section4->headerColor = {0.32f, 0.22f, 0.2f};
    section4->headerHeight = 32.0f;
    section4->fontSize = 15.0f;
    section4->titleColor = {1.0f, 1.0f, 1.0f, 1.0f};
    section4->showIndicator = false;
    section4->backgroundColor = {0.16f, 0.16f, 0.18f};
    section4->backgroundTransparency = 0.0f;
    section4->cornerRadius = 4.0f;
    section4->headerCornerRadius = 4.0f;
    section4->onToggled = [statusLabel](bool exp) {
        statusLabel->text = std::string("Controls: ") + (exp ? "expanded" : "collapsed");
        statusLabel->markDirty();
    };
    section4->markDirty();

    auto *ctrlLbl1 = section4->add<Amethyst::TextLabel>();
    ctrlLbl1->text = "Mouse Sensitivity: 2.5";
    ctrlLbl1->size = Amethyst::UDim2(1.0f, -20.0f, 0.0f, 24.0f);
    ctrlLbl1->position = Amethyst::UDim2::fromOffset(10.0f, 10.0f);
    ctrlLbl1->textColor = {0.8f, 0.8f, 0.8f, 1.0f};
    ctrlLbl1->fontSize = 13.0f;
    ctrlLbl1->backgroundTransparency = 1.0f;
    ctrlLbl1->textYAlignment = Amethyst::TextYAlignment::CENTER;
    ctrlLbl1->markDirty();

    auto *ctrlLbl2 = section4->add<Amethyst::TextLabel>();
    ctrlLbl2->text = "Invert Y: No";
    ctrlLbl2->size = Amethyst::UDim2(1.0f, -20.0f, 0.0f, 24.0f);
    ctrlLbl2->position = Amethyst::UDim2::fromOffset(10.0f, 38.0f);
    ctrlLbl2->textColor = {0.8f, 0.8f, 0.8f, 1.0f};
    ctrlLbl2->fontSize = 13.0f;
    ctrlLbl2->backgroundTransparency = 1.0f;
    ctrlLbl2->textYAlignment = Amethyst::TextYAlignment::CENTER;
    ctrlLbl2->markDirty();

    window.draw(drawCtx);

    int frameCount = 0;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

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
