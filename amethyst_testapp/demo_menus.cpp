#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/dropdown.h"
#include "components/menu_bar.h"
#include "modules/style.h"
#include "vk_context.h"

#include <string>

static bool g_showGrid = false;
static bool g_showRuler = false;
static std::string g_lastAction = "(none)";

int main()
{
    Amethyst::Log::Init();

    Amethyst::Style::load(AMETHYST_ASSETS_DIR "/theme.toml");

    VkContext ctx;
    if (!contextInit(ctx, 1200, 800, "Amethyst - Menus Demo")) {
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

    // ---- menu bar ----

    auto *menuBar = window.add<Amethyst::MenuBar>();
    menuBar->size = Amethyst::UDim2(glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 28.0f));
    menuBar->position = Amethyst::UDim2::fromOffset(0.0f, 0.0f);
    menuBar->backgroundColor = {0.15f, 0.15f, 0.15f};
    menuBar->backgroundTransparency = 0.0f;
    menuBar->borderPixelSize = 0.0f;

    using DI = Amethyst::DropdownItem;

    std::vector<DI> recentItems;
    for (int i = 1; i <= 12; i++) {
        std::string name = "project_0" + std::to_string(i) + ".aml";
        recentItems.push_back(DI::action(name, [name]{ g_lastAction = "Open recent: " + name; }));
    }

    menuBar->addMenu("File", {
        DI::action("New",  []{ g_lastAction = "New"; }),
        DI::action("Open", []{ g_lastAction = "Open"; }),
        DI::submenu("Recent", std::move(recentItems)),
        DI::separator(),
        DI::action("Quit", [&running]{ running = false; }),
    });

    menuBar->addMenu("Edit", {
        DI::action("Undo", []{ g_lastAction = "Undo"; }).withShortcut("Ctrl+Z"),
        DI::action("Redo", []{ g_lastAction = "Redo"; }).withShortcut("Ctrl+Y"),
        DI::separator(),
        DI::action("Cut",   []{ g_lastAction = "Cut";   }).withShortcut("Ctrl+X"),
        DI::action("Copy",  []{ g_lastAction = "Copy";  }).withShortcut("Ctrl+C"),
        DI::action("Paste", []{ g_lastAction = "Paste"; }).withShortcut("Ctrl+V"),
    });

    menuBar->addMenu("View", {
        DI::toggle("Show Grid",  &g_showGrid,  [](bool v){ g_lastAction = v ? "Grid on"  : "Grid off";  }),
        DI::toggle("Show Ruler", &g_showRuler, [](bool v){ g_lastAction = v ? "Ruler on" : "Ruler off"; }),
    });

    menuBar->markDirty();

    // ---- standalone dropdown ----

    auto *toolLabel = window.add<Amethyst::TextLabel>();
    toolLabel->text = "Active tool:";
    toolLabel->size = Amethyst::UDim2::fromOffset(120.0f, 28.0f);
    toolLabel->position = Amethyst::UDim2::fromOffset(40.0f, 60.0f);
    toolLabel->textColor = {0.85f, 0.85f, 0.85f, 1.0f};
    toolLabel->fontSize = 14.0f;
    toolLabel->textXAlignment = Amethyst::TextXAlignment::LEFT;
    toolLabel->textYAlignment = Amethyst::TextYAlignment::CENTER;
    toolLabel->markDirty();

    auto *toolDropdown = window.add<Amethyst::Dropdown>();
    toolDropdown->text = "Pencil \xe2\x96\xbe";
    toolDropdown->size = Amethyst::UDim2::fromOffset(140.0f, 28.0f);
    toolDropdown->position = Amethyst::UDim2::fromOffset(160.0f, 60.0f);
    toolDropdown->popupWidth = 140.0f;
    toolDropdown->setItems({
        DI::action("Pencil", [toolDropdown]{ toolDropdown->text = "Pencil \xe2\x96\xbe"; toolDropdown->markDirty(); g_lastAction = "Tool: Pencil"; }),
        DI::action("Brush", [toolDropdown]{ toolDropdown->text = "Brush \xe2\x96\xbe"; toolDropdown->markDirty(); g_lastAction = "Tool: Brush"; }),
        DI::action("Eraser", [toolDropdown]{ toolDropdown->text = "Eraser \xe2\x96\xbe"; toolDropdown->markDirty(); g_lastAction = "Tool: Eraser"; }),
        DI::action("Select", [toolDropdown]{ toolDropdown->text = "Select \xe2\x96\xbe"; toolDropdown->markDirty(); g_lastAction = "Tool: Select"; }),
    });
    toolDropdown->markDirty();

    // ---- status label ----

    auto *statusLabel = window.add<Amethyst::TextLabel>();
    statusLabel->size = Amethyst::UDim2::fromOffset(400.0f, 28.0f);
    statusLabel->position = Amethyst::UDim2::fromOffset(40.0f, 110.0f);
    statusLabel->textColor = {0.6f, 0.6f, 0.6f, 1.0f};
    statusLabel->fontSize = 13.0f;
    statusLabel->textXAlignment = Amethyst::TextXAlignment::LEFT;
    statusLabel->textYAlignment = Amethyst::TextYAlignment::CENTER;
    statusLabel->markDirty();

    // ---- main loop ----

    int frameCount = 0;
    double lastTime = glfwGetTime();
    std::string lastStatus;

    while (!glfwWindowShouldClose(ctx.window) && running) {
        glfwPollEvents();

        if (g_lastAction != lastStatus) {
            lastStatus = g_lastAction;
            statusLabel->text = "Last action: " + g_lastAction;
            statusLabel->markDirty();
        }

        uint32_t imageIndex;
        if (!contextBeginFrame(ctx, imageIndex)) continue;

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
