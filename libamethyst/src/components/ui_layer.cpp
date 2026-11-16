/*
 * UILayer implementation
 */

#include "components/ui_layer.h"

#include "components/ui_object.h"
#include "rendering/geometry_registry.h"

#include <algorithm>

namespace Amethyst {

UILayer::UILayer()
{
    kind |= KIND_UI_LAYER;
    m_geometryRegistry = GeometryRegistry::create(this);
}

UILayer::~UILayer()
{
    m_children.clear();
}

void UILayer::arrange()
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    for (auto &child : m_children) {
        if (auto *obj = child->asUiObject()) {
            obj->clipRect = clipRect;
            obj->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            obj->arrange();
        } else if (auto *layer = child->asLayer()) {
            layer->arrange();
        }
    }
}

bool UILayer::isVisible() const
{
    if (!visible) return false;
    if (parent == nullptr) return true;
    if (auto *obj = parent->asUiObject()) {
        return obj->isVisible();
    } else if (auto *layer = parent->asLayer()) {
        return layer->isVisible();
    }
    return true;
}

bool UILayer::isTreeNode(Instance *node)
{
    UIObject *obj = node->asUiObject();
    return obj != nullptr && obj->getBaseProperties().zindexBehavior != ZIndexBehavior::ABSOLUTE;
}

void UILayer::walkDrawTree(Instance *node, std::vector<UIObject *> &out)
{
    std::vector<Instance *> children;
    node->collectSortedDrawChildren(children);
    for (Instance *child : children) {
        if (UILayer *layer = child->asLayer(); layer != nullptr) {
            m_nestedLayers.push_back(layer);
        } else if (UIObject *obj = child->asUiObject(); obj != nullptr) {
            if (isTreeNode(obj)) {
                out.push_back(obj);
                walkDrawTree(obj, out);
            } else {
                m_absoluteNodes.push_back(obj);
            }
        }
        // anything else is never drawn, and neither is what hangs below it
    }
}

void UILayer::ensureDrawOrder()
{
    if (!m_drawOrderDirty) {
        return;
    }
    m_drawOrderDirty = false;
    m_absoluteNodes.clear();
    m_nestedLayers.clear();

    std::vector<UIObject *> tree;
    walkDrawTree(this, tree);

    // absolute subtrees are walked like the tree; absolute nodes found inside them join the list and
    // get walked in turn
    std::vector<std::vector<UIObject *>> absoluteSubtrees;
    for (size_t i = 0; i < m_absoluteNodes.size(); i++) {
        UIObject *root = m_absoluteNodes[i];
        std::vector<UIObject *> subtree{root};
        walkDrawTree(root, subtree);
        absoluteSubtrees.push_back(std::move(subtree));
    }

    std::vector<size_t> byZ(m_absoluteNodes.size());
    for (size_t i = 0; i < byZ.size(); i++) {
        byZ[i] = i;
    }
    std::stable_sort(byZ.begin(), byZ.end(),
                     [this](size_t a, size_t b) { return m_absoluteNodes[a]->getZIndex() < m_absoluteNodes[b]->getZIndex(); });

    uint32_t next = drawOrderBase();
    auto assign = [&next](const std::vector<UIObject *> &nodes) {
        for (UIObject *obj : nodes) {
            obj->setDrawOrder(next++);
        }
    };

    size_t firstAbove = 0;
    while (firstAbove < byZ.size() && m_absoluteNodes[byZ[firstAbove]]->getZIndex() < 0) {
        firstAbove++;
    }
    for (size_t i = 0; i < firstAbove; i++) {
        assign(absoluteSubtrees[byZ[i]]);
    }
    assign(tree);
    for (size_t i = firstAbove; i < byZ.size(); i++) {
        assign(absoluteSubtrees[byZ[i]]);
    }

    std::vector<UIObject *> sorted;
    sorted.reserve(byZ.size());
    for (size_t index : byZ) {
        sorted.push_back(m_absoluteNodes[index]);
    }
    m_absoluteNodes = std::move(sorted);
}

void UILayer::setDisplayOrder(int32_t order)
{
    if (m_displayOrder == order) {
        return;
    }
    m_displayOrder = order;
    GeometryRegistry::resortRegistries();
}

} // namespace Amethyst
