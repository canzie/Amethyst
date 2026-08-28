#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/text_area.h"
#include "components/ui_scope.h"
#include "modules/style.h"
#include "vk_context.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace Amethyst;

/**
 * @brief Text held as one string per line, the smallest thing a TextArea can read and edit.
 */
class LineVectorText : public TextSourceBase {
  public:
    explicit LineVectorText(std::vector<std::string> lines) : m_lines(std::move(lines))
    {
        if (m_lines.empty()) {
            m_lines.emplace_back();
        }
    }

    uint64_t lineCount() const override { return m_lines.size(); }

    std::string_view line(uint64_t index) const override
    {
        if (index >= m_lines.size()) {
            return {};
        }
        return m_lines[index];
    }

    void replace(TextRange range, std::string_view with) override
    {
        if (range.start.line >= m_lines.size()) {
            return;
        }
        uint64_t endLine = std::min<uint64_t>(range.end.line, m_lines.size() - 1);

        const std::string &startText = m_lines[range.start.line];
        const std::string &endText = m_lines[endLine];
        std::string head = startText.substr(0, std::min<size_t>(range.start.column, startText.size()));
        std::string tail = endText.substr(std::min<size_t>(range.end.column, endText.size()));

        std::vector<std::string> replacement;
        replacement.push_back(std::move(head));
        for (char c : with) {
            if (c == '\n') {
                replacement.emplace_back();
            } else {
                replacement.back().push_back(c);
            }
        }
        replacement.back() += tail;

        auto first = m_lines.begin() + static_cast<ptrdiff_t>(range.start.line);
        auto last = m_lines.begin() + static_cast<ptrdiff_t>(endLine) + 1;
        m_lines.erase(first, last);
        m_lines.insert(m_lines.begin() + static_cast<ptrdiff_t>(range.start.line), replacement.begin(), replacement.end());

        m_revision++;
    }

    uint64_t revision() const override { return m_revision; }

  private:
    std::vector<std::string> m_lines;
    uint64_t m_revision = 0;
};

static std::vector<std::string> buildSampleText()
{
    std::vector<std::string> lines;
    lines.reserve(5000);

    lines.push_back("// TextArea demo: 5000 lines, only the visible ones are ever shaped.");
    lines.push_back("");
    lines.push_back("Arrows, Home/End (Ctrl for document), PageUp/PageDown, Shift to select.");
    lines.push_back("Ctrl+A/C/X/V, Backspace and Delete join lines, Tab inserts a real tab.");
    lines.push_back("");

    std::string longLine = "This line is deliberately long so horizontal scrolling has something to chew on: ";
    for (int i = 0; i < 60; i++) {
        longLine += "column_" + std::to_string(i) + " ";
    }
    lines.push_back(longLine);
    lines.push_back("");

    for (int i = 0; i < 4990; i++) {
        if (i % 17 == 0) {
            lines.push_back("");
        } else if (i % 5 == 0) {
            lines.push_back("\tindented line " + std::to_string(i) + " starts after a tab");
        } else {
            lines.push_back("line " + std::to_string(i) + ": the quick brown fox jumps over the lazy dog");
        }
    }

    return lines;
}

int main()
{
    Log::Init();

    Style::load(AMETHYST_ASSETS_DIR "/theme.ams");

    VkContext ctx;
    if (!contextInit(ctx, 1100, 800, "Amethyst - TextArea Demo")) {
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

    LineVectorText source(buildSampleText());

    TextLabel *statusLabel = nullptr;
    UIScope(window).textLabel({.base = {.position = {0.0f, 0.0f, 0.0f, 0.0f}, .size = {1.0f, 0.0f, 0.0f, 24.0f}},
                               .style = {.backgroundColor = {0.12f, 0.12f, 0.14f}, .backgroundTransparency = 0.0f},
                               .text = {.fontSize = 13.0f,
                                        .textColor = {0.7f, 0.7f, 0.7f, 1.0f},
                                        .textXAlignment = TextXAlignment::LEFT,
                                        .textYAlignment = TextYAlignment::CENTER},
                               .label = ""},
                              [&statusLabel](TextLabelScope &t) { statusLabel = &t.component; });

    TextArea *editor = window.add<TextArea>();
    editor->setBaseProperties({.clipsDescendants = true,
                               .padding = UDim4{.top = UDim::fromOffset(4.0f), .left = UDim::fromOffset(6.0f)},
                               .position = {0.0f, 0.0f, 0.0f, 24.0f},
                               .size = {1.0f, 0.0f, 1.0f, -24.0f}});
    editor->setBaseStyleProperties({.backgroundColor = Color3::fromHex(0x1E1E1E), .backgroundTransparency = 0.0f});
    editor->setTextAreaProperties({.text = {.fontSize = 14.0f, .textColor = {0.85f, 0.85f, 0.88f, 1.0f}},
                                   .selectionColor = {0.20f, 0.40f, 0.75f, 0.45f},
                                   .cursorColor = {0.95f, 0.95f, 0.95f, 1.0f}});
    editor->setSource(&source);
    editor->focus();

    amCtx.draw(window);

    TextPosition lastCursor{UINT64_MAX, UINT64_MAX};
    uint64_t lastRevision = UINT64_MAX;
    int frameCount = 0;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

        TextPosition cursor = editor->getCursorPosition();
        if (cursor != lastCursor || source.revision() != lastRevision) {
            lastCursor = cursor;
            lastRevision = source.revision();
            statusLabel->setText("Ln " + std::to_string(cursor.line + 1) + ", Col " + std::to_string(cursor.column + 1) +
                                 "   |   " + std::to_string(source.lineCount()) + " lines   |   top " +
                                 std::to_string(editor->getFirstVisibleLine() + 1));
        }

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
