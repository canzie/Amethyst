#include "amethyst/Amethyst.h"
#include "amethyst__vk13_glfw.h"
#include "components/extensions/ui_drag_detector.h"
#include "components/extensions/ui_list_layout.h"
#include "components/ui_scope.h"
#include "modules/style.h"
#include "vk_context.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

using namespace Amethyst;

static constexpr float MIN_ZOOM = 0.1f;
static constexpr float MAX_ZOOM = 12.0f;
static constexpr float WHEEL_STEP = 1.15f;

/**
 * @brief A clipped area whose one child is the canvas: the wheel zooms it around the cursor and dragging its empty
 * space pans it. The canvas is moved with its position and drawn larger with its scale, so its layout never changes.
 */
class ZoomViewport : public Frame {
  public:
    void setCanvas(UIObject *canvas) { m_canvas = canvas; }

    float getZoom() const { return m_canvas != nullptr ? m_canvas->getBaseProperties().scale : 1.0f; }

    /**
     * @brief Zooms so the canvas point under a viewport point stays under it.
     * @param viewportPoint The point to zoom around, relative to the viewport's content box
     * @param zoom The new zoom, clamped to MIN_ZOOM..MAX_ZOOM
     */
    void zoomAt(vec2 viewportPoint, float zoom)
    {
        if (m_canvas == nullptr) {
            return;
        }
        const float current = getZoom();
        const float next = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
        const vec2 pan = m_canvas->getBaseProperties().position.offset;
        const vec2 moved = viewportPoint - (viewportPoint - pan) * (next / current);
        m_canvas->setBaseProperties({.position = UDim2::fromOffset(moved.x, moved.y), .scale = next});
        if (onZoomChanged != nullptr) {
            onZoomChanged(next);
        }
    }

    void zoomAtCenter(float zoom) { zoomAt(absoluteContentSize * 0.5f, zoom); }

    void reset()
    {
        if (m_canvas == nullptr) {
            return;
        }
        m_canvas->setBaseProperties({.position = UDim2::fromOffset(0, 0), .scale = 1.0f});
        if (onZoomChanged != nullptr) {
            onZoomChanged(1.0f);
        }
    }

    std::function<void(float)> onZoomChanged;

  protected:
    EventResult onMouseScrollUp() override
    {
        zoomAtCursor(getZoom() * WHEEL_STEP);
        return EventResult::CONSUMED;
    }

    EventResult onMouseScrollDown() override
    {
        zoomAtCursor(getZoom() / WHEEL_STEP);
        return EventResult::CONSUMED;
    }

  private:
    void zoomAtCursor(float zoom)
    {
        const Window *window = getWindow();
        const std::optional<vec2> mouse = window != nullptr ? window->getMousePosition() : std::nullopt;
        if (!mouse.has_value()) {
            zoomAtCenter(zoom);
            return;
        }
        zoomAt(toLayout(*mouse) - absoluteContentPosition, zoom);
    }

    UIObject *m_canvas = nullptr;
};

int main()
{
    Log::Init();

    Style::load(AMETHYST_ASSETS_DIR "/theme.ams");

    VkContext ctx;
    if (!contextInit(ctx, 1280, 820, "Amethyst - Scale Demo")) {
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

    window.setFramebufferSize(vec2(static_cast<float>(ctx.swapchainExtent.width), static_cast<float>(ctx.swapchainExtent.height)));
    window.absoluteRotation = 0.0f;
    window.setDisplayOrder(10);

    // a 16x16 checkerboard of single pixels: smooth sampling blurs it when enlarged, pixelated keeps every square
    TextureInfo checkerTex = createCheckerboardTexture(ctx, 16, 1);
    AmTextureId checkerId = backend.registerTexture(checkerTex.view, checkerTex.sampler);

    const Color3 panelColor = Color3::fromHex(0x232329);
    const Color3 cardColor = Color3::fromHex(0x2C2C34);
    const Color3 accent = Color3::fromHex(0x4A7BD0);
    const TextStylePropertiesArgs bodyText = {.fontSize = 14.0f, .textColor = {0.85f, 0.85f, 0.88f, 1.0f}};
    const TextStylePropertiesArgs headingText = {.fontSize = 18.0f, .textColor = {1.0f, 1.0f, 1.0f, 1.0f}};

    auto heading = [&](UIScope &scope, const char *text, vec2 pos) {
        scope.textLabel({.base = {.position = UDim2::fromOffset(pos.x, pos.y), .size = UDim2::fromOffset(360, 26)},
                         .style = {.backgroundTransparency = 1.0f},
                         .text = headingText,
                         .label = text});
    };
    auto body = [&](UIScope &scope, std::string text, vec2 pos, vec2 size, float fontSize = 14.0f) {
        TextStylePropertiesArgs style = bodyText;
        style.fontSize = fontSize;
        style.textWrapped = true;
        scope.textLabel({.base = {.position = UDim2::fromOffset(pos.x, pos.y), .size = UDim2::fromOffset(size.x, size.y)},
                         .style = {.backgroundTransparency = 1.0f},
                         .text = style,
                         .label = std::move(text)});
    };
    auto card = [&](UIScope &scope, vec2 pos, vec2 size, std::function<void(FrameScope &)> fill) {
        scope.frame({.base = {.position = UDim2::fromOffset(pos.x, pos.y), .size = UDim2::fromOffset(size.x, size.y)},
                     .style = {.backgroundColor = cardColor, .borderPixelSize = 1.0f, .borderColor = Color3::fromHex(0x3A3A44),
                               .cornerRadius = 8.0f}},
                    std::move(fill));
    };

    UIScope root(window);
    ZoomViewport *viewport = nullptr;
    TextLabel *zoomReadout = nullptr;
    TextLabel *clickReadout = nullptr;
    SliderFloat *zoomSlider = nullptr;
    float zoomValue = 1.0f;
    int clicks = 0;
    bool checked = true;
    float sliderValue = 40.0f;

    // hover-grow buttons ease towards their target scale every tick
    struct Grower {
        TextButton *button = nullptr;
        float target = 1.0f;
    };
    std::array<Grower, 4> growers{};

    root.frame({.base = {.size = UDim2::fromScale(1.0f, 1.0f)}, .style = {.backgroundColor = Color3::fromHex(0x18181C)}});

    // --- side panel, never scaled ---
    root.frame({.base = {.position = UDim2::fromOffset(0, 0), .size = UDim2(0.0f, 260.0f, 1.0f, 0.0f)},
                .style = {.backgroundColor = panelColor}},
               [&](FrameScope &panel) {
                   heading(panel, "Scale demo", vec2(20, 20));
                   body(panel, "Wheel over the canvas zooms at the cursor. Drag empty canvas to pan. Everything on the canvas keeps its layout.",
                        vec2(20, 54), vec2(220, 80), 13.0f);

                   panel.textLabel({.base = {.position = UDim2::fromOffset(20, 150), .size = UDim2::fromOffset(220, 24)},
                                    .style = {.backgroundTransparency = 1.0f},
                                    .text = bodyText,
                                    .label = "Zoom 100%"},
                                   [&](TextLabelScope &s) { zoomReadout = &s.component; });

                   panel.sliderFloat({.base = {.position = UDim2::fromOffset(20, 180), .size = UDim2::fromOffset(220, 28)},
                                      .style = {.backgroundColor = Color3::fromHex(0x33333A), .cornerRadius = 4.0f},
                                      .slider = {.fillColor = Color4(accent, 1.0f)},
                                      .format = "%.2fx",
                                      .scale = ValueScale::LOGARITHMIC,
                                      .min = MIN_ZOOM,
                                      .max = MAX_ZOOM,
                                      .value = &zoomValue},
                                     [&](SliderFloatScope &s) {
                                         zoomSlider = &s.component;
                                         s.component.onValueChanged = [&](float v) {
                                             if (viewport != nullptr && v != viewport->getZoom()) {
                                                 viewport->zoomAtCenter(v);
                                             }
                                         };
                                     });

                   panel.textButton({.base = {.position = UDim2::fromOffset(20, 220), .size = UDim2::fromOffset(220, 30)},
                                     .style = {.backgroundColor = Color3::fromHex(0x3A3A44), .cornerRadius = 4.0f},
                                     .text = bodyText,
                                     .label = "Reset view"},
                                    [&](TextButtonScope &b) {
                                        b.component.track(b.component.onMouseButton1ClickCb.connect([&] {
                                            if (viewport != nullptr) {
                                                viewport->reset();
                                            }
                                        }));
                                    });

                   heading(panel, "UI scale", vec2(20, 280));
                   body(panel, "Window-wide, seen by layout: the whole UI relays out to fit.", vec2(20, 308), vec2(220, 40), 13.0f);
                   const std::array<float, 4> uiScales = {1.0f, 1.25f, 1.5f, 2.0f};
                   for (size_t i = 0; i < uiScales.size(); i++) {
                       const float uiScale = uiScales[i];
                       const float x = 20.0f + static_cast<float>(i % 2) * 112.0f;
                       const float y = 356.0f + static_cast<float>(i / 2u) * 38.0f;
                       panel.textButton({.base = {.position = UDim2::fromOffset(x, y), .size = UDim2::fromOffset(106, 30)},
                                         .style = {.backgroundColor = Color3::fromHex(0x3A3A44), .cornerRadius = 4.0f},
                                         .text = bodyText,
                                         .label = std::to_string(static_cast<int>(std::lround(uiScale * 100.0f))) + "%"},
                                        [&window, uiScale](TextButtonScope &b) {
                                            b.component.track(
                                                b.component.onMouseButton1ClickCb.connect([&window, uiScale] { window.setUiScale(uiScale); }));
                                        });
                   }
               });

    // --- the zoomable canvas ---
    viewport = window.add<ZoomViewport>();
    viewport->setBaseProperties({.position = UDim2::fromOffset(260, 0), .size = UDim2(1.0f, -260.0f, 1.0f, 0.0f)});
    viewport->setBaseStyleProperties({.backgroundColor = Color3::fromHex(0x1B1B20)});

    Frame *canvas = viewport->add<Frame>();
    canvas->setBaseProperties({.position = UDim2::fromOffset(0, 0), .size = UDim2::fromOffset(1500, 1000)});
    canvas->setBaseStyleProperties({.backgroundTransparency = 1.0f});
    // dragging the canvas itself pans it: the detector moves it by the distance in the viewport's layout
    canvas->addExtension<UIDragDetector>();
    viewport->setCanvas(canvas);
    viewport->onZoomChanged = [&](float zoom) {
        zoomValue = zoom;
        if (zoomReadout != nullptr) {
            zoomReadout->setText("Zoom " + std::to_string(static_cast<int>(std::lround(zoom * 100.0f))) + "%");
        }
        if (zoomSlider != nullptr) {
            zoomSlider->markDirty();
        }
    };

    UIScope canvasScope(*canvas);

    // a dot grid: repeated marks grow with the zoom like any other length
    canvasScope.frame({.base = {.size = UDim2::fromScale(1.0f, 1.0f)},
                       .style = {.backgroundColor = Color3::fromHex(0x34343F),
                                 .backgroundRepeat = BackgroundRepeat::REPEAT,
                                 .backgroundSize = vec2(3.0f),
                                 .backgroundStep = vec2(40.0f),
                                 .backgroundPosition = vec2(18.0f)}},
                      [](FrameScope &grid) {
                          // presses fall through to the canvas so its empty space pans
                          grid.component.ignore(INTERACTION_CATEGORY_CLICK);
                          grid.component.ignore(INTERACTION_CATEGORY_HOVER);
                      });

    // text: line breaks never change with the zoom, glyphs are rasterised at the size they are drawn at
    card(canvasScope, vec2(40, 40), vec2(420, 280), [&](FrameScope &c) {
        heading(c, "Text", vec2(16, 12));
        body(c,
             "This paragraph wraps at the width it was laid out at. Zoom in and out: the words on each line stay the "
             "same, only the glyphs get sharper or smaller.",
             vec2(16, 48), vec2(388, 90));
        body(c, "11px caption text, still readable at 4x", vec2(16, 150), vec2(388, 20), 11.0f);
        body(c, "8px fine print", vec2(16, 176), vec2(388, 16), 8.0f);
        body(c, "28px", vec2(16, 206), vec2(388, 40), 28.0f);
    });

    // shapes: radii, borders and hairlines are geometry under a scale; the last outline keeps its window width
    card(canvasScope, vec2(500, 40), vec2(420, 280), [&](FrameScope &c) {
        heading(c, "Shapes and borders", vec2(16, 12));
        const std::array<uvec4, 3> radii = {uvec4(4u), uvec4(14u), uvec4(30u)};
        for (size_t i = 0; i < radii.size(); i++) {
            c.frame({.base = {.position = UDim2::fromOffset(16.0f + static_cast<float>(i) * 130.0f, 52), .size = UDim2::fromOffset(116, 60)},
                     .style = {.backgroundColor = accent, .borderPixelSize = 1.0f, .borderColor = Color3(1.0f, 1.0f, 1.0f), .cornerRadius = radii[i]}});
        }
        body(c, "1px border", vec2(16, 128), vec2(180, 20), 12.0f);
        c.frame({.base = {.position = UDim2::fromOffset(16, 152), .size = UDim2::fromOffset(180, 60)},
                 .style = {.backgroundTransparency = 1.0f, .borderMode = BorderMode::INSET, .borderPixelSize = 1.0f,
                           .borderColor = Color3::fromHex(0xE0A040)}});
        body(c, "1px, fixed thickness", vec2(216, 128), vec2(188, 20), 12.0f);
        c.frame({.base = {.position = UDim2::fromOffset(216, 152), .size = UDim2::fromOffset(188, 60)},
                 .style = {.backgroundTransparency = 1.0f, .borderMode = BorderMode::INSET, .borderPixelSize = 1.0f,
                           .borderColor = Color3::fromHex(0xE0A040), .thicknessScaling = ThicknessScaling::FIXED}});
        body(c, "Zoom out: the left border fades away, the right one stays a crisp pixel.", vec2(16, 222), vec2(388, 40), 12.0f);
    });

    // hover grow: a list keeps its rows where they are, each button eases its own scale around its centre
    card(canvasScope, vec2(960, 40), vec2(300, 280), [&](FrameScope &c) {
        heading(c, "Hover grow", vec2(16, 12));
        c.frame({.base = {.position = UDim2::fromOffset(16, 52), .size = UDim2::fromOffset(268, 210)}, .style = {.backgroundTransparency = 1.0f}},
                [&](FrameScope &list) {
                    auto *layout = list.component.addExtension<UIListLayout>();
                    layout->innerPadding = UDim::fromOffset(10.0f);
                    for (size_t i = 0; i < growers.size(); i++) {
                        // the wrapper holds the row's place in the list; the button inside scales around its centre
                        list.frame({.base = {.layoutOrder = static_cast<uint32_t>(i), .size = UDim2::fromOffset(268, 40)},
                                    .style = {.backgroundTransparency = 1.0f}},
                                   [&, i](FrameScope &row) {
                                       row.component.setBaseProperties({.clipsDescendants = false});
                                       row.textButton({.base = {.anchorPoint = vec2(0.5f), .position = UDim2::fromScale(0.5f, 0.5f),
                                                                .size = UDim2::fromScale(1.0f, 1.0f)},
                                                       .style = {.backgroundColor = Color3::fromHex(0x3A3A44), .cornerRadius = 6.0f},
                                                       .text = bodyText,
                                                       .label = "Hover me " + std::to_string(i + 1)},
                                                      [&, i](TextButtonScope &b) {
                                                          growers[i].button = &b.component;
                                                          b.component.track(b.component.onHoverChanged.connect(
                                                              [&, i](bool hovered) { growers[i].target = hovered ? 1.12f : 1.0f; }));
                                                      });
                                   });
                    }
                });
    });

    // images: a 16x16 checkerboard of single pixels, sampled smooth and pixelated
    card(canvasScope, vec2(40, 360), vec2(420, 260), [&](FrameScope &c) {
        heading(c, "Image sampling", vec2(16, 12));
        body(c, "smooth", vec2(16, 48), vec2(180, 20), 12.0f);
        c.imageLabel({.base = {.position = UDim2::fromOffset(16, 72), .size = UDim2::fromOffset(160, 160)},
                      .style = {.backgroundTransparency = 1.0f},
                      .texture = checkerId});
        body(c, "pixelated", vec2(216, 48), vec2(180, 20), 12.0f);
        c.imageLabel({.base = {.position = UDim2::fromOffset(216, 72), .size = UDim2::fromOffset(160, 160)},
                      .style = {.backgroundTransparency = 1.0f},
                      .image = {.samplingMode = ImageSamplingMode::PIXELATED},
                      .texture = checkerId});
    });

    // widgets: input lands where it is drawn, popups open against the drawn rect
    card(canvasScope, vec2(500, 360), vec2(420, 260), [&](FrameScope &c) {
        heading(c, "Widgets", vec2(16, 12));
        c.textInput({.base = {.position = UDim2::fromOffset(16, 52), .size = UDim2::fromOffset(240, 30)},
                     .style = {.backgroundColor = Color3::fromHex(0x1E1E24), .cornerRadius = 4.0f},
                     .textInput = {.text = bodyText},
                     .placeholder = "Type, select, drag the caret"});
        c.dropdown({.base = {.position = UDim2::fromOffset(270, 52), .size = UDim2::fromOffset(134, 30)},
                    .text = bodyText,
                    .label = "Dropdown",
                    .dropdown = {.popupWidth = 134.0f}},
                   [](DropdownScope &d) {
                       d.action("First");
                       d.action("Second");
                       d.action("Third");
                   });
        c.sliderFloat({.base = {.position = UDim2::fromOffset(16, 100), .size = UDim2::fromOffset(240, 28)},
                       .style = {.backgroundColor = Color3::fromHex(0x33333A), .cornerRadius = 4.0f},
                       .slider = {.fillColor = Color4(accent, 1.0f)},
                       .format = "%.0f",
                       .value = &sliderValue});
        c.checkbox({.base = {.position = UDim2::fromOffset(270, 100), .size = UDim2::fromOffset(28, 28)}, .value = &checked});
        c.textButton({.base = {.position = UDim2::fromOffset(16, 148), .size = UDim2::fromOffset(140, 32)},
                      .style = {.backgroundColor = accent, .cornerRadius = 4.0f},
                      .text = bodyText,
                      .label = "Click me"},
                     [&](TextButtonScope &b) {
                         b.component.track(b.component.onMouseButton1ClickCb.connect([&] {
                             clicks++;
                             if (clickReadout != nullptr) {
                                 clickReadout->setText("Clicked " + std::to_string(clicks) + " times");
                             }
                         }));
                     });
        c.textLabel({.base = {.position = UDim2::fromOffset(170, 148), .size = UDim2::fromOffset(234, 32)},
                     .style = {.backgroundTransparency = 1.0f},
                     .text = {.fontSize = 14.0f, .textColor = {0.85f, 0.85f, 0.88f, 1.0f}, .textYAlignment = TextYAlignment::CENTER},
                     .label = "Clicked 0 times"},
                    [&](TextLabelScope &s) { clickReadout = &s.component; });
    });

    // nesting: a scaled node inside the zoomed canvas multiplies with it
    card(canvasScope, vec2(960, 360), vec2(300, 260), [&](FrameScope &c) {
        heading(c, "Nested scale", vec2(16, 12));
        c.frame({.base = {.position = UDim2::fromOffset(16, 52), .size = UDim2::fromOffset(268, 90), .scale = 0.5f},
                 .style = {.backgroundColor = Color3::fromHex(0x38384A), .cornerRadius = 8.0f}},
                [&](FrameScope &half) {
                    half.textButton({.base = {.position = UDim2::fromOffset(12, 12), .size = UDim2::fromOffset(244, 30)},
                                     .style = {.backgroundColor = Color3::fromHex(0x4A4A5C), .cornerRadius = 4.0f},
                                     .text = bodyText,
                                     .label = "scale 0.5"});
                    body(half, "laid out at full size, drawn at half", vec2(12, 50), vec2(244, 30), 13.0f);
                });
        c.frame({.base = {.position = UDim2::fromOffset(16, 110), .size = UDim2::fromOffset(130, 50), .scale = 1.5f},
                 .style = {.backgroundColor = Color3::fromHex(0x38384A), .cornerRadius = 8.0f}},
                [&](FrameScope &big) { body(big, "scale 1.5", vec2(10, 14), vec2(110, 24), 13.0f); });
    });

    // scrolling: a scroll frame inside the zoom still clips and scrolls where it is drawn
    card(canvasScope, vec2(40, 660), vec2(420, 300), [&](FrameScope &c) {
        heading(c, "Scrolling inside the zoom", vec2(16, 12));
        c.scrollingFrame({.base = {.position = UDim2::fromOffset(16, 52), .size = UDim2::fromOffset(388, 232)},
                          .style = {.backgroundColor = Color3::fromHex(0x1E1E24), .cornerRadius = 4.0f},
                          .scroll = {.canvasSize = UDim2::fromScale(1.0f, 0.0f), .automaticCanvasSize = AutomaticSize::Y}},
                         [&](ScrollingFrameScope &sf) {
                             auto *layout = sf.component.addExtension<UIListLayout>();
                             layout->innerPadding = UDim::fromOffset(4.0f);
                             for (uint32_t i = 0; i < 30; i++) {
                                 sf.textLabel({.base = {.layoutOrder = i, .size = UDim2(1.0f, -16.0f, 0.0f, 24.0f)},
                                               .style = {.backgroundColor = Color3::fromHex(i % 2 == 0 ? 0x2A2A32 : 0x26262D)},
                                               .text = {.fontSize = 13.0f, .textColor = {0.8f, 0.8f, 0.84f, 1.0f},
                                                        .textYAlignment = TextYAlignment::CENTER},
                                               .label = "  Row " + std::to_string(i + 1)});
                             }
                         });
    });

    TickHandle growTick = window.registerTick([&growers](float dt) {
        const float blend = std::min(1.0f, dt * 14.0f);
        for (Grower &g : growers) {
            if (g.button == nullptr) {
                continue;
            }
            const float current = g.button->getBaseProperties().scale;
            if (current == g.target) {
                continue;
            }
            float next = current + (g.target - current) * blend;
            if (std::abs(next - g.target) < 0.001f) {
                next = g.target;
            }
            g.button->setBaseProperties({.scale = next});
        }
    });

    amCtx.draw(window);

    double lastUpdateTime = glfwGetTime();
    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

        double now = glfwGetTime();
        float deltaTime = static_cast<float>(now - lastUpdateTime);
        lastUpdateTime = now;

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
    }

    growTick.unregister();
    backend.unregisterTexture(checkerId);
    destroyTexture(ctx, checkerTex);
    backend.shutdown();
    contextShutdown(ctx);
    Log::Shutdown();
    return 0;
}
