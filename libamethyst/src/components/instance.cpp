/*
 * Instance implementation
 */

#include "components/instance.h"
#include <algorithm>

namespace Amethyst {

void Instance::setParent(Instance* newParent) {
    if (parent) {
        parent->removeChild(this);
    }
    parent = newParent;
    if (parent) {
        parent->addChild(this);
    }
}

void Instance::addChild(Instance* child) {
    children.push_back(child);
}

void Instance::removeChild(Instance* child) {
    auto it = std::find(children.begin(), children.end(), child);
    if (it != children.end()) {
        children.erase(it);
    }
}

void Instance::markDirty() {
    flags |= FLAG_DIRTY;
    for (Instance* p = parent; p && !(p->flags & FLAG_CHILD_DIRTY); p = p->parent) {
        p->flags |= FLAG_CHILD_DIRTY;
    }
}

} // namespace Amethyst
