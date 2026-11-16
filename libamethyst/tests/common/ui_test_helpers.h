#ifndef AMETHYST__TESTS_UI_TEST_HELPERS_H
#define AMETHYST__TESTS_UI_TEST_HELPERS_H

#include "components/input_events.h"
#include "components/ui_button.h"
#include "components/ui_object.h"
#include "components/window.h"

#include <cstdint>
#include <memory>

namespace AmethystTest {

using namespace Amethyst;

/**
 * @brief A Window laid out at a fixed size, with helpers that feed it raw mouse input the
 * way a backend would. No GPU involved: arrange() and dispatch are pure CPU.
 */
class TestWindow {
  public:
    explicit TestWindow(float width = 800.0f, float height = 600.0f) : window(std::make_unique<Window>())
    {
        window->absoluteSize = vec2(width, height);
    }

    void layout() { window->arrange(); }

    void press(int32_t x, int32_t y, int button = MOUSE_BUTTON_1) { window->onMouseButton(button, MOUSE_ACTION_PRESS, 0, x, y); }
    void release(int32_t x, int32_t y, int button = MOUSE_BUTTON_1)
    {
        window->onMouseButton(button, MOUSE_ACTION_RELEASE, 0, x, y);
    }
    void click(int32_t x, int32_t y, int button = MOUSE_BUTTON_1)
    {
        press(x, y, button);
        release(x, y, button);
    }
    void move(int32_t x, int32_t y) { window->onMouseMove(x, y); }
    void scroll(float dy, int32_t x, int32_t y) { window->onMouseScroll(0.0f, dy, x, y); }

    Window &operator*() { return *window; }
    Window *operator->() { return window.get(); }

    std::unique_ptr<Window> window;
};

/** @brief Places @p obj at a pixel rect inside its parent. */
inline void place(UIObject *obj, float x, float y, float w, float h)
{
    obj->setBaseProperties({.position = UDim2::fromOffset(x, y), .size = UDim2::fromOffset(w, h)});
}

/** @brief A UIButton with no visuals; behaviour is all UIButton's. */
class TestButton : public UIButton {
  public:
    void draw(DrawContext &) override {}

    using UIButton::onPressCancelled;
};

/** @brief A plain hit-testable node with no visuals. */
class TestNode : public UIObject {
  public:
    void draw(DrawContext &) override {}
};

} // namespace AmethystTest

#endif // AMETHYST__TESTS_UI_TEST_HELPERS_H
