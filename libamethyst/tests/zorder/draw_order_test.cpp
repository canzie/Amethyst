#include "text/text_test_env.h"

#include "components/frame.h"
#include "components/panel_layer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace Amethyst;
using namespace AmethystTest;

namespace {

struct DrawOrderFixture : ::testing::Test {
    TextEnv env;
    TestWindow win;
    std::vector<std::string> clicks;

    /** @brief A named frame at a pixel rect that logs the presses it receives. */
    Frame *frame(Instance *parent, const char *frameName, float x, float y, float w, float h, int32_t z = 1)
    {
        auto *f = parent->add<Frame>();
        f->name = frameName;
        place(f, x, y, w, h);
        f->setBaseProperties({.zIndex = z});
        f->track(f->onInputBeganCb.connect([this, f](const InputObject &) { clicks.push_back(f->name); }));
        return f;
    }

    Instance *root() { return win.window.get(); }

    void render()
    {
        win.layout();
        win->draw(env.ctx);
        win->geometryRegistry()->flush();
    }

    std::vector<std::string> clickAt(int32_t x, int32_t y)
    {
        clicks.clear();
        win.click(x, y);
        return clicks;
    }

    /** @brief Position of the allocation carrying z in the window's sorted buffer, -1 if none. */
    int32_t sortedIndexOf(int32_t z) const
    {
        const auto &sorted = win.window->geometryRegistry()->getAllocations();
        for (size_t i = 0; i < sorted.size(); i++) {
            if (sorted[i].zIndex == z) {
                return static_cast<int32_t>(i);
            }
        }
        return -1;
    }
};

using Names = std::vector<std::string>;

} // namespace

TEST_F(DrawOrderFixture, SiblingsDrawByZIndexThenChildOrder)
{
    Frame *parentFrame = frame(root(), "p", 0, 0, 300, 300);
    Frame *a = frame(parentFrame, "a", 0, 0, 100, 100, 2);
    Frame *b = frame(parentFrame, "b", 0, 0, 100, 100, 1);
    Frame *c = frame(parentFrame, "c", 0, 0, 100, 100, 1);
    render();

    EXPECT_LT(parentFrame->drawZ(), b->drawZ());
    EXPECT_LT(b->drawZ(), c->drawZ());
    EXPECT_LT(c->drawZ(), a->drawZ());

    EXPECT_LT(sortedIndexOf(parentFrame->drawZ()), sortedIndexOf(b->drawZ()));
    EXPECT_LT(sortedIndexOf(b->drawZ()), sortedIndexOf(c->drawZ()));
    EXPECT_LT(sortedIndexOf(c->drawZ()), sortedIndexOf(a->drawZ()));

    EXPECT_EQ(clickAt(50, 50), Names{"a"});
}

// regression: zIndex summed down the tree, so a deep child of a lower sibling outranked a higher
// sibling when drawn (1 + 1 + 100 > 2) while the click went to the sibling
TEST_F(DrawOrderFixture, SubtreeStaysBelowAHigherSibling)
{
    Frame *a = frame(root(), "a", 0, 0, 200, 200, 1);
    Frame *a1 = frame(a, "a1", 0, 0, 200, 200);
    Frame *a2 = frame(a1, "a2", 100, 100, 80, 80, 100);
    Frame *b = frame(root(), "b", 90, 90, 100, 100, 2);
    render();

    EXPECT_LT(a2->drawZ(), b->drawZ());
    EXPECT_LT(sortedIndexOf(a2->drawZ()), sortedIndexOf(b->drawZ()));
    EXPECT_EQ(clickAt(120, 120), Names{"b"});

    // the other way round the whole of a, deep child included, covers b, and takes the click
    b->setBaseProperties({.zIndex = 0});
    render();

    EXPECT_GT(a2->drawZ(), b->drawZ());
    EXPECT_GT(a->drawZ(), b->drawZ());
    EXPECT_GT(sortedIndexOf(a2->drawZ()), sortedIndexOf(b->drawZ()));
    EXPECT_EQ(clickAt(120, 120), Names{"a2"});
}

TEST_F(DrawOrderFixture, SubtreeIsContiguous)
{
    Frame *a = frame(root(), "a", 0, 0, 100, 100, 1);
    Frame *a1 = frame(a, "a1", 0, 0, 10, 10, 50);
    Frame *a11 = frame(a1, "a11", 0, 0, 10, 10, -3);
    Frame *b = frame(root(), "b", 0, 0, 100, 100, 2);
    Frame *b1 = frame(b, "b1", 0, 0, 10, 10, -50);
    render();

    // pre-order: a, a1, a11, b, b1, with nothing of b in between
    EXPECT_LT(a->drawZ(), a1->drawZ());
    EXPECT_LT(a1->drawZ(), a11->drawZ());
    EXPECT_LT(a11->drawZ(), b->drawZ());
    EXPECT_LT(b->drawZ(), b1->drawZ());
}

TEST_F(DrawOrderFixture, AbsoluteNodesLeaveTheTree)
{
    Frame *low = frame(root(), "low", 0, 0, 200, 200, 1);
    Frame *lifted = frame(low, "lifted", 50, 50, 100, 100, 5);
    lifted->setBaseProperties({.zindexBehavior = ZIndexBehavior::ABSOLUTE});
    Frame *liftedChild = frame(lifted, "liftedChild", 0, 0, 50, 50);
    Frame *high = frame(root(), "high", 0, 0, 200, 200, 2);
    Frame *highChild = frame(high, "highChild", 180, 180, 20, 20, 100);
    Frame *sunk = frame(high, "sunk", 0, 0, 200, 200, -1);
    sunk->setBaseProperties({.zindexBehavior = ZIndexBehavior::ABSOLUTE});
    render();

    // sunk goes below all tree content, even its own parent; lifted (and its subtree) above all of it
    EXPECT_LT(sunk->drawZ(), low->drawZ());
    EXPECT_LT(low->drawZ(), high->drawZ());
    EXPECT_LT(high->drawZ(), highChild->drawZ());
    EXPECT_LT(highChild->drawZ(), lifted->drawZ());
    EXPECT_LT(lifted->drawZ(), liftedChild->drawZ());
    EXPECT_LT(sortedIndexOf(highChild->drawZ()), sortedIndexOf(lifted->drawZ()));
    EXPECT_LT(sortedIndexOf(sunk->drawZ()), sortedIndexOf(low->drawZ()));

    EXPECT_EQ(clickAt(60, 60), Names{"liftedChild"});
    EXPECT_EQ(clickAt(120, 120), Names{"lifted"});
    EXPECT_EQ(clickAt(170, 170), Names{"high"});

    // with the tree letting the click through, it ends at the sunk node, below everything
    high->propagate(INTERACTION_CATEGORY_CLICK);
    low->propagate(INTERACTION_CATEGORY_CLICK);
    EXPECT_EQ(clickAt(170, 170), (Names{"high", "low", "sunk"}));
}

TEST_F(DrawOrderFixture, AbsoluteNodeKeepsItsAncestorsClip)
{
    Frame *clipper = frame(root(), "clipper", 0, 0, 100, 100, 1);
    Frame *lifted = frame(clipper, "lifted", 50, 50, 100, 100, 5);
    lifted->setBaseProperties({.zindexBehavior = ZIndexBehavior::ABSOLUTE});
    render();

    EXPECT_EQ(clickAt(75, 75), Names{"lifted"});
    // drawn clipped to its parent, so it cannot be hit outside of it either
    EXPECT_TRUE(clickAt(125, 125).empty());

    clipper->setBaseProperties({.clipsDescendants = false});
    render();
    EXPECT_EQ(clickAt(125, 125), Names{"lifted"});
}

TEST_F(DrawOrderFixture, HitOrderIsTheReverseOfDrawOrder)
{
    // everything covers (5, 5) and lets the click through, so every node reports in hit order
    std::vector<Frame *> all;
    Frame *a = frame(root(), "a", 0, 0, 100, 100, 3);
    Frame *b = frame(root(), "b", 0, 0, 100, 100, 1);
    all.insert(all.end(), {a, b});
    all.push_back(frame(a, "a1", 0, 0, 50, 50, 2));
    all.push_back(frame(a, "a2", 0, 0, 50, 50, 1));
    all.push_back(frame(b, "b1", 0, 0, 50, 50, 7));
    Frame *ignored = frame(b, "ignored", 0, 0, 50, 50, 8);
    ignored->ignore(INTERACTION_CATEGORY_CLICK);
    all.push_back(frame(ignored, "ignoredChild", 0, 0, 20, 20));
    Frame *abs = frame(b, "abs", 0, 0, 50, 50, 4);
    abs->setBaseProperties({.zindexBehavior = ZIndexBehavior::ABSOLUTE});
    all.push_back(abs);
    all.push_back(frame(abs, "absChild", 0, 0, 20, 20));
    Frame *absLow = frame(a, "absLow", 0, 0, 50, 50, -2);
    absLow->setBaseProperties({.zindexBehavior = ZIndexBehavior::ABSOLUTE});
    all.push_back(absLow);
    for (Frame *f : all) {
        f->propagate(INTERACTION_CATEGORY_CLICK);
    }
    render();

    std::sort(all.begin(), all.end(), [](Frame *x, Frame *y) { return x->drawZ() > y->drawZ(); });
    Names expected;
    for (Frame *f : all) {
        expected.push_back(f->name);
    }
    EXPECT_EQ(clickAt(5, 5), expected);
    EXPECT_EQ(expected.front(), "absChild");
    EXPECT_EQ(expected.back(), "absLow");
}

TEST_F(DrawOrderFixture, HoverStopsAtTheTopmostConsumer)
{
    Frame *a = frame(root(), "a", 0, 0, 200, 200, 1);
    Frame *a2 = frame(a, "a2", 100, 100, 80, 80, 100);
    Frame *b = frame(root(), "b", 90, 90, 100, 100, 2);
    render();

    bool a2Hovered = false;
    bool bHovered = false;
    a2->track(a2->onHoverChanged.connect([&a2Hovered](bool h) { a2Hovered = h; }));
    b->track(b->onHoverChanged.connect([&bHovered](bool h) { bHovered = h; }));

    win.move(120, 120);
    EXPECT_TRUE(bHovered);
    EXPECT_FALSE(a2Hovered);
}

TEST_F(DrawOrderFixture, StructureChangesReorderWithoutStaleEntries)
{
    Frame *a = frame(root(), "a", 0, 0, 50, 50);
    Frame *b = frame(root(), "b", 0, 0, 50, 50);
    Frame *c = frame(root(), "c", 0, 0, 50, 50);
    render();
    size_t before = win->geometryRegistry()->size();
    int32_t cBefore = c->drawZ();

    auto checkNoStale = [this](const std::vector<UIObject *> &live) {
        std::set<int32_t> zs;
        for (UIObject *obj : live) {
            zs.insert(obj->drawZ());
        }
        const auto &sorted = win->geometryRegistry()->getAllocations();
        for (size_t i = 0; i < sorted.size(); i++) {
            EXPECT_TRUE(zs.count(sorted[i].zIndex) == 1) << "stale z " << sorted[i].zIndex;
            if (i > 0) {
                EXPECT_LT(sorted[i - 1].zIndex, sorted[i].zIndex);
            }
        }
        EXPECT_EQ(sorted.size(), live.size());
    };

    std::unique_ptr<Instance> removed = root()->removeChild(b);
    removed.reset();
    render();
    EXPECT_EQ(win->geometryRegistry()->size(), before - 1);
    EXPECT_LT(c->drawZ(), cBefore);
    checkNoStale({a, c});

    // a new first child below the rest moves everything up
    Frame *d = frame(root(), "d", 0, 0, 50, 50, 0);
    render();
    EXPECT_LT(d->drawZ(), a->drawZ());
    checkNoStale({a, c, d});
    EXPECT_EQ(clickAt(10, 10), Names{"c"});

    a->setBaseProperties({.zIndex = 10});
    render();
    EXPECT_GT(a->drawZ(), c->drawZ());
    EXPECT_GT(sortedIndexOf(a->drawZ()), sortedIndexOf(c->drawZ()));
    checkNoStale({a, c, d});
    EXPECT_EQ(clickAt(10, 10), Names{"a"});

    // reparenting under d puts c in d's subtree, still below a
    c->reparent(d);
    render();
    EXPECT_GT(c->drawZ(), d->drawZ());
    EXPECT_LT(c->drawZ(), a->drawZ());
    checkNoStale({a, c, d});

    // becoming absolute below zero sinks it under everything, then back
    a->setBaseProperties({.zIndex = -1, .zindexBehavior = ZIndexBehavior::ABSOLUTE});
    render();
    EXPECT_LT(a->drawZ(), d->drawZ());
    EXPECT_EQ(clickAt(10, 10), Names{"c"});
    a->setBaseProperties({.zIndex = 10, .zindexBehavior = ZIndexBehavior::RELATIVE});
    render();
    EXPECT_GT(a->drawZ(), c->drawZ());
    checkNoStale({a, c, d});
}

TEST_F(DrawOrderFixture, DestroyedAbsoluteNodeLeavesTheHitOrder)
{
    Frame *holder = frame(root(), "holder", 0, 0, 100, 100);
    Frame *abs = frame(holder, "abs", 0, 0, 100, 100, 3);
    abs->setBaseProperties({.zindexBehavior = ZIndexBehavior::ABSOLUTE});
    render();
    EXPECT_EQ(clickAt(10, 10), Names{"abs"});

    // a press handler that removes the absolute node it sits under must not leave it behind in the layer's list
    holder->propagate(INTERACTION_CATEGORY_CLICK);
    abs->propagate(INTERACTION_CATEGORY_CLICK);
    abs->track(abs->onInputBeganCb.connect([holder](const InputObject &) { holder->removeAllChildren(); }));
    EXPECT_EQ(clickAt(10, 10), Names{"abs"});
    EXPECT_EQ(clickAt(10, 10), Names{"holder"});
}

TEST_F(DrawOrderFixture, NestedLayersHitByDisplayOrder)
{
    frame(root(), "windowFrame", 0, 0, 100, 100, 50);
    auto *panel = root()->add<PanelLayer>();
    panel->absoluteSize = vec2(800.0f, 600.0f);
    frame(panel, "panelFrame", 0, 0, 100, 100);
    panel->setDisplayOrder(5);
    render();

    // the panel's registry draws after the window's, so it is hit first despite the window frame's zIndex
    EXPECT_EQ(clickAt(10, 10), Names{"panelFrame"});

    panel->setDisplayOrder(-5);
    EXPECT_EQ(clickAt(10, 10), Names{"windowFrame"});
}

TEST_F(DrawOrderFixture, OverlayContentDrawsAboveForeignParts)
{
    Frame *own = frame(root(), "own", 0, 0, 50, 50);
    Frame *popup = frame(win->getOverlayLayer(), "popup", 0, 0, 50, 50);
    render();

    // parts other layers draw into the overlay registry use those layers' orders, which stay below this
    EXPECT_GT(popup->drawZ(), own->drawZ());
    EXPECT_GE(popup->getDrawOrder(), 1u << 27);
    EXPECT_EQ(clickAt(10, 10), Names{"popup"});
}
