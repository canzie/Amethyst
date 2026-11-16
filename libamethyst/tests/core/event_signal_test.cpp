#include "modules/event_signal.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace Amethyst;

TEST(EventSignal, FiresEveryConnectionInOrder)
{
    EventSignal<void(int)> signal;
    std::vector<int> seen;
    auto a = signal.connect([&](int v) { seen.push_back(v * 1); });
    auto b = signal.connect([&](int v) { seen.push_back(v * 10); });
    EXPECT_TRUE(signal.fire(2));
    EXPECT_EQ(seen, (std::vector<int>{2, 20}));
}

TEST(EventSignal, ConnectionDisconnectsOnDestruction)
{
    EventSignal<void()> signal;
    int calls = 0;
    {
        auto c = signal.connect([&] { ++calls; });
        signal.fire();
    }
    signal.fire();
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(signal.connectionCount(), 0u);
}

TEST(EventSignal, OnceFiresExactlyOnce)
{
    EventSignal<void()> signal;
    int calls = 0;
    auto c = signal.once([&] { ++calls; });
    signal.fire();
    signal.fire();
    EXPECT_EQ(calls, 1);
}

TEST(EventSignal, DisconnectingLaterSlotDuringFireSkipsIt)
{
    EventSignal<void()> signal;
    int secondCalls = 0;
    EventConnection second;
    auto first = signal.connect([&] { second.disconnect(); });
    second = signal.connect([&] { ++secondCalls; });
    signal.fire();
    EXPECT_EQ(secondCalls, 0);
    EXPECT_EQ(signal.connectionCount(), 1u);
}

TEST(EventSignal, SlotConnectedDuringFireWaitsForNextFire)
{
    EventSignal<void()> signal;
    int lateCalls = 0;
    std::vector<EventConnection> keep;
    auto first = signal.connect([&] { keep.push_back(signal.connect([&] { ++lateCalls; })); });
    signal.fire();
    EXPECT_EQ(lateCalls, 0);
    first.disconnect();
    signal.fire();
    EXPECT_EQ(lateCalls, 1);
}

TEST(EventSignal, NestedFireDoesNotRunOnceSlotTwice)
{
    EventSignal<void()> signal;
    int onceCalls = 0;
    int depth = 0;
    auto reenter = signal.connect([&] {
        if (depth++ == 0) {
            signal.fire();
        }
    });
    auto once = signal.once([&] { ++onceCalls; });
    signal.fire();
    signal.fire();
    EXPECT_EQ(onceCalls, 1);
}

TEST(EventSignal, NestedFireKeepsOuterIterationValid)
{
    // the inner fire used to erase the outer loop's current slot when it finished
    EventSignal<void()> signal;
    int tailCalls = 0;
    bool nested = false;
    EventConnection self;
    self = signal.connect([&] {
        self.disconnect();
        if (!nested) {
            nested = true;
            signal.fire();
        }
    });
    auto tail = signal.connect([&] { ++tailCalls; });
    signal.fire();
    EXPECT_EQ(tailCalls, 2);
}

TEST(EventSignal, CallbackDestroyingTheSignalStopsIteration)
{
    auto signal = std::make_unique<EventSignal<void()>>();
    int laterCalls = 0;
    bool fired = true;
    std::vector<int> big(64, 7); // a capture big enough to live on the heap
    signal->detachedOnce([&, big] {
        signal.reset();
        // the closure must still be alive while it runs, even though its signal is gone
        EXPECT_EQ(big[63], 7);
    });
    signal->detachedOnce([&] { ++laterCalls; });
    fired = signal->fire();
    EXPECT_FALSE(fired);
    EXPECT_EQ(laterCalls, 0);
}

TEST(EventSignal, ConnectionOutlivingSignalIsInert)
{
    EventConnection c;
    {
        EventSignal<void()> signal;
        c = signal.connect([] {});
        EXPECT_TRUE(c.connected());
    }
    EXPECT_FALSE(c.connected());
    c.disconnect();
}
