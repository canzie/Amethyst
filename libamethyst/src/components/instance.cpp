/*
 * Instance implementation
 */

#include "components/instance.h"
#include "components/ui_layer.h"
#include "utils/am_assert.h"
#include "utils/profiling.h"
#include <algorithm>

namespace Amethyst {

Instance::~Instance()
{
    onDestroy.fire(this);
    // the layer may hold this node in its draw order (absolute roots, nested layers), which must not outlive it
    if (parent != nullptr) {
        parent->markDrawOrderDirty();
    }
    for (auto &child : m_children) {
        child->parent = nullptr;
    }
}

void Instance::reparent(Instance *newParent)
{
    AM_ASSERT(newParent == nullptr || !newParent->isSelfOrDescendantOf(this),
              "An instance cannot be reparented under itself or one of its descendants");
    AM_ASSERT(parent, "reparent requires an existing parent; use addChild or add<T> for initial parenting");
    if (parent == nullptr || parent == newParent) return;
    if (newParent != nullptr && newParent->isSelfOrDescendantOf(this)) return;

    auto self = parent->removeChild(this);
    if (newParent) {
        newParent->addChild(std::move(self));
    }
}

Instance *Instance::addChild(std::unique_ptr<Instance> child)
{
    Instance *raw = child.get();
    raw->parent = this;
    m_children.push_back(std::move(child));
    raw->markSubtreeDirty();
    markDrawOrderDirty();
    return raw;
}

std::unique_ptr<Instance> Instance::removeChild(Instance *child)
{
    auto it = std::find_if(m_children.begin(), m_children.end(), [child](const auto &p) { return p.get() == child; });
    if (it != m_children.end()) {
        std::unique_ptr<Instance> result = std::move(*it);
        m_children.erase(it);
        result->parent = nullptr;
        markDirty();
        markDrawOrderDirty();
        return result;
    }
    return nullptr;
}

std::vector<std::unique_ptr<Instance>> Instance::removeAllChildren()
{
    for (auto &child : m_children) {
        child->parent = nullptr;
    }
    markDrawOrderDirty();
    return std::move(m_children);
}

void Instance::collectDrawChildren(std::vector<Instance *> &out)
{
    for (auto &child : m_children) {
        out.push_back(child.get());
    }
}

void Instance::collectSortedDrawChildren(std::vector<Instance *> &out)
{
    out.clear();
    collectDrawChildren(out);
    std::stable_sort(out.begin(), out.end(), [](Instance *a, Instance *b) { return a->getZIndex() < b->getZIndex(); });
}

void Instance::markDrawOrderDirty()
{
    for (Instance *node = this; node != nullptr; node = node->parent) {
        if (UILayer *layer = node->asLayer(); layer != nullptr) {
            layer->invalidateDrawOrder();
            return;
        }
    }
}

void Instance::adoptPart(Instance *part)
{
    part->parent = this;
    markDrawOrderDirty();
}

void Instance::releasePart(Instance *part)
{
    markDrawOrderDirty();
    part->parent = nullptr;
}

void Instance::markChildrenDirty()
{
    AM_PROFILE_FUNCTION();
    for (size_t i = 0; i < m_children.size(); i++) {
        m_children[i]->markChildrenDirty();
        m_children[i]->flags |= FLAG_DIRTY;
    }
}

bool Instance::isSelfOrDescendantOf(const Instance *other) const
{
    for (const Instance *node = this; node != nullptr; node = node->parent) {
        if (node == other) {
            return true;
        }
    }
    return false;
}

Instance *Instance::findFirstChild(const std::string &childName) const
{
    for (auto &child : m_children) {
        if (child->name == childName) return child.get();
    }
    return nullptr;
}

Instance *Instance::findFirstDescendant(const std::string &descendantName) const
{
    for (auto &child : m_children) {
        if (child->name == descendantName) return child.get();
        if (auto *found = child->findFirstDescendant(descendantName)) return found;
    }
    return nullptr;
}

void Instance::markDirty()
{
    AM_PROFILE_FUNCTION();
    flags |= FLAG_DIRTY;
    for (Instance *p = parent; p && !(p->flags & FLAG_CHILD_DIRTY); p = p->parent) {
        p->flags |= FLAG_CHILD_DIRTY;
    }
}

void Instance::markSubtreeDirty()
{
    markDirty();
    markChildrenDirty();
}

} // namespace Amethyst
