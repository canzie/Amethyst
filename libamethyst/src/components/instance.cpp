/*
 * Instance implementation
 */

#include "components/instance.h"
#include "utils/am_assert.h"
#include "utils/profiling.h"
#include <algorithm>

namespace Amethyst {

Instance::~Instance()
{
    onDestroy.fire(this);
    for (auto &child : m_children) {
        child->parent = nullptr;
    }
}

void Instance::reparent(Instance *newParent)
{
    AM_ASSERT(newParent != this, "An instance cannot be its own parent");
    AM_ASSERT(parent, "reparent requires an existing parent; use addChild or add<T> for initial parenting");
    if (parent == newParent) return;

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
    return raw;
}

std::unique_ptr<Instance> Instance::removeChild(Instance *child)
{
    auto it = std::find_if(m_children.begin(), m_children.end(), [child](const auto &p) { return p.get() == child; });
    if (it != m_children.end()) {
        std::unique_ptr<Instance> result = std::move(*it);
        m_children.erase(it);
        result->parent = nullptr;
        return result;
    }
    return nullptr;
}

std::vector<std::unique_ptr<Instance>> Instance::removeAllChildren()
{
    for (auto &child : m_children) {
        child->parent = nullptr;
    }
    return std::move(m_children);
}

std::vector<Instance *> Instance::getHittableInstances()
{
    std::vector<Instance *> result;
    result.reserve(m_children.size());
    for (auto &child : m_children) {
        result.push_back(child.get());
    }
    return result;
}

void Instance::markChildrenDirty()
{
    AM_PROFILE_FUNCTION();
    for (size_t i = 0; i < m_children.size(); i++) {
        m_children[i]->markChildrenDirty();
        m_children[i]->flags |= FLAG_DIRTY;
    }
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

    markChildrenDirty();
}

} // namespace Amethyst
