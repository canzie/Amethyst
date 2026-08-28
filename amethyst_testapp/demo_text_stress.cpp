// Text stress demo: a scrolling wall of TextLabels next to a live view of the glyph
// atlas, so the packer can be watched filling up under a real workload.
//
// LINE_COUNT and font size variety are the knobs. Both fixed capacities in
// GlyphBuffer are per GeometryRegistry, so they bound the whole document rather than
// the visible part: one slice per label against SLICE_CAPACITY, and every label's
// glyphs against the glyph arena, whether or not the label is scrolled into view. The
// demo logs where it stands against both on startup.

#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/ui_scope.h"
#include "modules/glyph_buffer.h"
#include "modules/style.h"
#include "utils/profiling.h"
#include "vk_context.h"

#include <algorithm>

#include <cstdint>
#include <string>
#include <vector>

using namespace Amethyst;

namespace {

constexpr uint32_t LINE_COUNT = 400;
constexpr float LINE_HEIGHT = 18.0f;
constexpr float ATLAS_PANEL_WIDTH = 320.0f;

// Cycled per line so several pixel sizes share the atlas, which is what makes the
// packing policy visible rather than trivial.
constexpr float FONT_SIZES[] = {11.0f, 12.0f, 13.0f, 14.0f, 16.0f};

const char *const WORDS[] = {
    "amethyst", "glyph",  "atlas",   "shelf",    "skyline", "packer",  "raster",  "cache",
    "viewport", "scroll", "slice",   "instance", "quad",    "batch",   "kerning", "baseline",
    "ascender", "hinting", "subpixel", "codepoint", "cluster", "advance", "bearing", "outline",
};

std::string buildLine(uint32_t index)
{
    std::string line = std::to_string(index + 1);
    line.append(4 - std::min<size_t>(4, line.size()), ' ');
    line += " | ";

    // Deterministic word pick so every run stresses the same glyph set.
    uint32_t seed = index * 2654435761u;
    for (int word = 0; word < 6; ++word) {
        seed = seed * 1664525u + 1013904223u;
        line += WORDS[(seed >> 16) % (sizeof(WORDS) / sizeof(WORDS[0]))];
        line += ' ';
    }
    return line;
}

} // namespace

int main()
{
    Log::Init();

    Style::load(AMETHYST_ASSETS_DIR "/theme.ams");

    VkContext ctx;
    if (!contextInit(ctx, 1280, 800, "Amethyst - Text Stress")) {
        AM_LOG_ERROR("Failed to initialize Vulkan context");
        return 1;
    }

    AmethystContext amCtx;
    if (!amCtx.loadFont(AMETHYST_ASSETS_DIR "/fonts/OpenSans-Regular.ttf").isValid()) {
        AM_LOG_ERROR("Failed to load font");
        return 1;
    }

    AmVulkanInitInfo initInfo{};
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

    Window window;

    AmGlfwInitInfo glfwInfo{};
    glfwInfo.window = ctx.window;
    glfwInfo.uiWindow = &window;

    AmVulkanBackend backend;
    backend.init(initInfo, glfwInfo);

    amCtx.init(backend);

    vec2 screenSize = {
        static_cast<float>(ctx.swapchainExtent.width),
        static_cast<float>(ctx.swapchainExtent.height),
    };
    window.absoluteSize = screenSize;
    window.absoluteRotation = 0.0f;
    window.setDisplayOrder(10);

    std::vector<std::string> lines;
    lines.reserve(LINE_COUNT);
    size_t totalChars = 0;
    for (uint32_t i = 0; i < LINE_COUNT; ++i) {
        lines.push_back(buildLine(i));
        totalChars += lines.back().size();
    }

    // Only labels inside the viewport hold glyph quads: TextLabel gives its slice back
    // when render-culled. So the arena budget is about lines on screen, not lines in the
    // document; the slice table is what scales with the whole document.
    size_t avgChars = totalChars / LINE_COUNT;
    size_t visibleLines = static_cast<size_t>((screenSize.y - 104.0f) / LINE_HEIGHT) + 2;
    size_t residentQuads = visibleLines * avgChars;

    AM_LOG_INFO("text stress: {} labels, {} slices of {}; ~{} resident quads of {} ({} visible lines x ~{} chars)", LINE_COUNT,
                LINE_COUNT, GlyphBuffer::SLICE_CAPACITY, residentQuads, GlyphBuffer::GLYPH_MAX, visibleLines, avgChars);
    if (LINE_COUNT > GlyphBuffer::SLICE_CAPACITY) {
        AM_LOG_WARN("more labels than SLICE_CAPACITY: slices are per label, so this one does scale with the document");
    }
    if (residentQuads > GlyphBuffer::GLYPH_MAX) {
        AM_LOG_WARN("one screenful exceeds GLYPH_MAX: expect dropped text, not a crash");
    }

    UIScope root(window);
    root.frame({.base = {.size = UDim2::fromScale(1.0f, 1.0f)}, .style = {.backgroundColor = Color3::fromHex(0x14141A)}});

    root.textLabel({.base = {.position = UDim2::fromOffset(16, 10), .size = UDim2::fromOffset(700, 22)},
                    .style = {.backgroundTransparency = 1.0f},
                    .text = {.fontSize = 15.0f,
                             .textColor = {0.95f, 0.95f, 1.0f, 1.0f},
                             .textYAlignment = TextYAlignment::CENTER},
                    .label = "Scroll the text. The panel on the right is the live glyph atlas."});

    // The wall of text. Lines are positioned explicitly and the canvas is sized to
    // match, so scrolling is pure translation of already-shaped glyphs.
    root.scrollingFrame(
        {
            .base = {.position = UDim2::fromOffset(16, 44),
                     .size = UDim2{{1.0f, 1.0f}, {-(ATLAS_PANEL_WIDTH + 48.0f), -60.0f}}},
            .style = {.backgroundColor = Color3::fromHex(0x0E0E12), .cornerRadius = 4.0f},
            .scroll = {.scrollAxis = ScrollAxis::Y,
                       .canvasSize = UDim2{{1.0f, 0.0f}, {0.0f, static_cast<float>(LINE_COUNT) * LINE_HEIGHT}},
                       .scrollSpeed = LINE_HEIGHT * 3.0f},
        },
        [&](ScrollingFrameScope &sf) {
            for (uint32_t i = 0; i < LINE_COUNT; ++i) {
                float size = FONT_SIZES[i % (sizeof(FONT_SIZES) / sizeof(FONT_SIZES[0]))];
                float shade = 0.55f + 0.45f * static_cast<float>((i * 37) % 100) / 100.0f;

                sf.textLabel({
                    .base = {.position = UDim2::fromOffset(10, static_cast<float>(i) * LINE_HEIGHT),
                             .size = UDim2{{1.0f, 0.0f}, {-20.0f, LINE_HEIGHT}}},
                    .style = {.backgroundTransparency = 1.0f},
                    .text = {.fontSize = size,
                             .textColor = {shade, shade, 1.0f, 1.0f},
                             .textYAlignment = TextYAlignment::CENTER},
                    .label = lines[i],
                });
            }
        });

    // Live glyph atlas, so the packer's layout is visible as it fills.
    root.textLabel({.base = {.position = UDim2{{1.0f, 0.0f}, {-(ATLAS_PANEL_WIDTH + 16.0f), 44.0f}},
                             .size = UDim2::fromOffset(ATLAS_PANEL_WIDTH, 20)},
                    .style = {.backgroundTransparency = 1.0f},
                    .text = {.fontSize = 13.0f, .textColor = {0.7f, 0.7f, 0.8f, 1.0f}},
                    .label = "glyph atlas"});

    root.imageLabel({.base = {.position = UDim2{{1.0f, 0.0f}, {-(ATLAS_PANEL_WIDTH + 16.0f), 68.0f}},
                              .size = UDim2::fromOffset(ATLAS_PANEL_WIDTH, ATLAS_PANEL_WIDTH)},
                     .style = {.backgroundColor = Color3::fromHex(0x000000), .cornerRadius = 4.0f},
                     .texture = amCtx.getGlyphAtlasTexture()});

    amCtx.draw(window);

    int frameCount = 0;
    double lastTime = glfwGetTime();
    double lastUpdateTime = glfwGetTime();

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

        double currentUpdateTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentUpdateTime - lastUpdateTime);
        lastUpdateTime = currentUpdateTime;

        window.tick(deltaTime);

        uint32_t imageIndex;
        if (!contextBeginFrame(ctx, imageIndex)) {
            continue;
        }

        VkCommandBuffer cmd = ctx.commandBuffers[ctx.currentFrame];

        amCtx.draw(window);
        amCtx.syncShared(static_cast<void *>(cmd));
        amCtx.syncWindow(static_cast<void *>(cmd), window);
        backend.record(cmd, amCtx.getDrawList(window), ctx.swapchainExtent);
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

    backend.shutdown();
    contextShutdown(ctx);
    Log::Shutdown();
    return 0;
}
