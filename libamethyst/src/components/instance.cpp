/*
 * Instance implementation
 */

#include "components/instance.h"
#include "utils/profiling.h"
#include <algorithm>
#include <cstddef>

namespace Amethyst {

Instance::~Instance()
{
    if (onDestroy) {
        onDestroy(this);
    }
    if (parent) {
        parent->removeChild(this);
    }
}

void Instance::setParent(Instance *newParent)
{
    if (parent) {
        parent->removeChild(this);
    }
    parent = newParent;
    if (parent) {
        parent->addChild(this);
    }
}

void Instance::addChild(Instance *child)
{
    children.push_back(child);
    child->parent = this;
}

void Instance::removeChild(Instance *child)
{
    auto it = std::find(children.begin(), children.end(), child);
    if (it != children.end()) {
        children.erase(it);
        child->parent = nullptr;
    }
}

void Instance::markChildrenDirty()
{
    AM_PROFILE_FUNCTION();
    for (size_t i = 0; i < children.size(); i++) {
        children[i]->markChildrenDirty();
        children[i]->flags |= FLAG_DIRTY;
    }
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
