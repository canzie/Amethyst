#include "common/ui_test_helpers.h"

#include "components/instance.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using namespace AmethystTest;

TEST(Instance, AddChildSetsParent)
{
    Instance root;
    auto *child = root.add<Instance>();
    EXPECT_EQ(child->parent, &root);
    EXPECT_EQ(root.getChildren().size(), 1u);
}

TEST(Instance, DestructionFiresOnDestroyAndClearsToken)
{
    auto root = std::make_unique<Instance>();
    auto *child = root->add<Instance>();
    std::weak_ptr<bool> token = child->aliveToken();
    int destroyed = 0;
    child->onDestroy.detachedOnce([&](Instance *) { ++destroyed; });
    EXPECT_TRUE(Instance::isAlive(token));
    root.reset();
    EXPECT_EQ(destroyed, 1);
    EXPECT_FALSE(Instance::isAlive(token));
}

TEST(Instance, RemoveChildHandsOwnershipBack)
{
    Instance root;
    auto *child = root.add<Instance>();
    auto owned = root.removeChild(child);
    ASSERT_EQ(owned.get(), child);
    EXPECT_EQ(child->parent, nullptr);
    EXPECT_TRUE(root.getChildren().empty());
}

TEST(Instance, ReparentMovesNode)
{
    Instance root;
    auto *a = root.add<Instance>();
    auto *b = root.add<Instance>();
    a->reparent(b);
    EXPECT_EQ(a->parent, b);
    EXPECT_EQ(root.getChildren().size(), 1u);
    EXPECT_EQ(b->getChildren().size(), 1u);
}

#ifdef NDEBUG
TEST(Instance, ReparentUnderOwnDescendantIsRefused)
{
    Instance root;
    auto *a = root.add<Instance>();
    auto *grandchild = a->add<Instance>()->add<Instance>();
    a->reparent(grandchild);
    EXPECT_EQ(a->parent, &root);
    EXPECT_EQ(root.getChildren().size(), 1u);
}
#endif

TEST(Instance, ReparentToNullDestroys)
{
    Instance root;
    auto *a = root.add<Instance>();
    std::weak_ptr<bool> token = a->aliveToken();
    a->reparent(nullptr);
    EXPECT_FALSE(Instance::isAlive(token));
    EXPECT_TRUE(root.getChildren().empty());
}

TEST(Instance, IsSelfOrDescendantOf)
{
    Instance root;
    auto *a = root.add<Instance>();
    auto *b = a->add<Instance>();
    EXPECT_TRUE(b->isSelfOrDescendantOf(&root));
    EXPECT_TRUE(a->isSelfOrDescendantOf(a));
    EXPECT_FALSE(a->isSelfOrDescendantOf(b));
}
