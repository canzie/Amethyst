/*
 * Amethyst test application
 */

#include "components/frame.h"
#include "components/instance.h"
#include "components/window.h"
#include "logging/log.h"
#include "utils/am_assert.h"

int main()
{
    Amethyst::Log::Init();

    AM_LOG_INFO("Amethyst Test App");

    Amethyst::Window window;
    window.name = "MainWindow";

    Amethyst::Frame frame;
    frame.name = "TestFrame";
    frame.setParent(&window);

    AM_LOG_INFO("Window: {}", window.name);
    AM_LOG_INFO("Frame parent: {}", frame.parent->name);
    AM_LOG_INFO("Window children count: {}", window.children.size());

    frame.markDirty();
    AM_LOG_INFO("Frame dirty: {}", (frame.flags & Amethyst::FLAG_DIRTY) != 0);
    AM_LOG_INFO("Window child dirty: {}", (window.flags & Amethyst::FLAG_CHILD_DIRTY) != 0);

    // Test dynamic_cast
    if (auto *f = window.children[0]->as<Amethyst::Frame>()) {
        AM_LOG_INFO("Successfully cast child to Frame: {}", f->name);
    }

    AM_ASSERT(frame.parent != nullptr, "Frame should have a parent");

    AM_LOG_INFO("All tests passed!");

    Amethyst::Log::Shutdown();
    return 0;
}
