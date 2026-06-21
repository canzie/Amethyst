#include "components/container.h"

#include "rendering/draw_context.h"

namespace Amethyst {

Container::Container()
{
    propagate(INTERACTION_CATEGORY_ALL);
}

std::vector<Instance *> Container::getHittableInstances()
{
    std::vector<Instance *> result;
    result.reserve(m_children.size());
    for (auto &child : m_children) {
        result.push_back(child.get());
    }
    return result;
}

void Container::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    vec4 childClip = computeChildClipRect();

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteContentSize, absoluteContentPosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
