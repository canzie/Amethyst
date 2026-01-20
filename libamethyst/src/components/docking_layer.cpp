#include "docking_layer.h"

#include "components/common.h"
#include "components/tab_bar.h"
#include "logging/log.h"
#include "rendering/draw_context.h"

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace Amethyst {

DockingLayer::DockingLayer() {}

DockingLayer::DockingLayer(Instance *parent)
{
    setParent(parent);
}

void DockingLayer::draw(DrawContext &ctx)
{
    processPendingDeletions();

    if (m_rootNode < 0) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        computeLayout(m_rootNode, absoluteSize, absolutePosition);
    }

    for (auto &tabBar : m_tabBars) {
        tabBar->draw(ctx);
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void DockingLayer::processPendingDeletions()
{
    if (m_pendingDeletions.empty()) return;

    for (TabBar *ptr : m_pendingDeletions) {
        std::erase_if(m_tabBars, [ptr](const auto &uptr) { return uptr.get() == ptr; });
    }
    m_pendingDeletions.clear();
}

std::vector<Instance *> DockingLayer::getHittableInstances()
{
    std::vector<Instance *> result;
    for (auto &tabBar : m_tabBars) {
        result.push_back(tabBar.get());
    }
    return result;
}

void DockingLayer::dock(UIObject *obj, glm::vec2 pos)
{
    if (m_rootNode < 0) {
        int32_t nodeIdx = createNode();
        m_rootNode = nodeIdx;

        auto tabBar = std::make_unique<TabBar>(this);
        setupTabBarCallbacks(tabBar.get());
        tabBar->addChild(obj);
        m_nodes[nodeIdx].content = tabBar.get();
        tabBar->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        tabBar->position = UDim2::fromScale(0.0f);
        tabBar->size = UDim2::fromScale(1.0f);
        m_tabBars.push_back(std::move(tabBar));
    } else {
        int32_t targetNode = findNodeByPosition(pos, m_rootNode, absoluteSize, absolutePosition);
        DockZone targetZone = hitTestZone(targetNode, pos);
        if (targetZone == DockZone::CENTER) {
            DockNode &node = m_nodes[targetNode];
            node.content->addChild(obj);
        } else {
            AM_LOG_INFO("Splitting {} {}", targetNode, static_cast<int32_t>(targetZone));
            splitNode(targetNode, targetZone, obj);
        }
    }

    markDirty();
}

void DockingLayer::undock(UIObject *obj)
{
    glm::vec2 objPos = obj->absolutePosition;
    int32_t nodeIndex = findNodeByPosition(objPos, m_rootNode, absoluteSize, absolutePosition);
    if (nodeIndex < 0) {
        return;
    }

    DockNode &node = m_nodes[nodeIndex];
    node.content->removeChild(obj);

    if (node.content->children.empty()) {
        collapseNode(nodeIndex);
    }

    markDirty();
}

void DockingLayer::swapAndRemoveNode(int32_t nodeIndex)
{
    int32_t lastIndex = static_cast<int32_t>(m_nodes.size()) - 1;
    if (nodeIndex != lastIndex) {
        std::swap(m_nodes[nodeIndex], m_nodes[lastIndex]);
        DockNode &swapped = m_nodes[nodeIndex];

        if (swapped.parentNode >= 0) {
            DockNode &p = m_nodes[swapped.parentNode];
            if (p.firstChild == lastIndex) {
                p.firstChild = nodeIndex;
            } else if (p.secondChild == lastIndex) {
                p.secondChild = nodeIndex;
            }
        }
        if (swapped.firstChild >= 0) {
            m_nodes[swapped.firstChild].parentNode = nodeIndex;
        }
        if (swapped.secondChild >= 0) {
            m_nodes[swapped.secondChild].parentNode = nodeIndex;
        }
        if (m_rootNode == lastIndex) {
            m_rootNode = nodeIndex;
        }
    }
    m_nodes.pop_back();
}

void DockingLayer::collapseNode(int32_t nodeIndex)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_nodes.size())) {
        return;
    }

    DockNode &node = m_nodes[nodeIndex];
    int32_t parentIndex = node.parentNode;
    TabBar *emptyTabBar = node.content;

    if (parentIndex < 0) {
        swapAndRemoveNode(nodeIndex);
        m_rootNode = -1;
        m_pendingDeletions.push_back(emptyTabBar);
        return;
    }

    DockNode &parent = m_nodes[parentIndex];
    int32_t siblingIndex = (parent.firstChild == nodeIndex) ? parent.secondChild : parent.firstChild;
    DockNode &sibling = m_nodes[siblingIndex];

    parent.axis = sibling.axis;
    parent.ratio = sibling.ratio;
    parent.content = sibling.content;

    int32_t firstChild = sibling.firstChild;
    int32_t secondChild = sibling.secondChild;

    int32_t firstToRemove = std::max(nodeIndex, siblingIndex);
    int32_t secondToRemove = std::min(nodeIndex, siblingIndex);

    auto trackSwap = [&](int32_t oldIndex, int32_t newIndex) {
        if (firstChild == oldIndex) {
            firstChild = newIndex;
        } else if (secondChild == oldIndex) {
            secondChild = newIndex;
        }
    };

    int32_t lastIndex = static_cast<int32_t>(m_nodes.size() - 1);
    if (firstToRemove != lastIndex) {
        trackSwap(lastIndex, firstToRemove);
    }
    swapAndRemoveNode(firstToRemove);

    lastIndex = static_cast<int32_t>(m_nodes.size() - 1);
    if (secondToRemove != lastIndex) {
        trackSwap(lastIndex, secondToRemove);
    }
    swapAndRemoveNode(secondToRemove);

    parent.firstChild = firstChild;
    parent.secondChild = secondChild;

    if (parent.firstChild >= 0) {
        m_nodes[parent.firstChild].parentNode = parentIndex;
    }
    if (parent.secondChild >= 0) {
        m_nodes[parent.secondChild].parentNode = parentIndex;
    }
    m_pendingDeletions.push_back(emptyTabBar);
}

DockZone DockingLayer::hitTestZone(int32_t nodeIndex, glm::vec2 position)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_nodes.size())) {
        return DockZone::CENTER;
    }

    DockNode &node = m_nodes[nodeIndex];

    glm::vec2 nodePos = node.content->absolutePosition;
    glm::vec2 nodeSize = node.content->absoluteSize;

    glm::vec2 local = position - nodePos;
    glm::vec2 normalized = local / nodeSize;

    float zoneEdgeRatio = 0.33f;
    if (normalized.x < zoneEdgeRatio) return DockZone::LEFT;
    if (normalized.x > 1.0f - zoneEdgeRatio) return DockZone::RIGHT;
    if (normalized.y < zoneEdgeRatio) return DockZone::TOP;
    if (normalized.y > 1.0f - zoneEdgeRatio) return DockZone::BOTTOM;

    return DockZone::CENTER;
}

int32_t DockingLayer::createNode()
{
    int32_t idx = static_cast<int32_t>(m_nodes.size());
    m_nodes.emplace_back();
    return idx;
}

void DockingLayer::computeLayout(int32_t nodeIndex, glm::vec2 nodeSize, glm::vec2 nodePosition)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_nodes.size())) {
        return;
    }

    DockNode &node = m_nodes[nodeIndex];

    if (node.isLeaf()) {
        node.content->size = UDim2::fromScale(1.0f, 1.0f);
        node.content->position = UDim2::fromScale(0.0f, 0.0f);
        node.content->computeAbsolutes(nodeSize, nodePosition, 0.0f);
        node.content->markDirty();
        return;
    }

    glm::vec2 firstSize = nodeSize, firstPos = nodePosition;
    glm::vec2 secondSize = nodeSize, secondPos = nodePosition;

    if (node.axis == SplitAxis::HORIZONTAL) {
        firstSize.y = nodeSize.y * node.ratio;
        secondSize.y = nodeSize.y * (1.0f - node.ratio);
        secondPos.y = nodePosition.y + firstSize.y;
    } else {
        firstSize.x = nodeSize.x * node.ratio;
        secondSize.x = nodeSize.x * (1.0f - node.ratio);
        secondPos.x = nodePosition.x + firstSize.x;
    }

    computeLayout(node.firstChild, firstSize, firstPos);
    computeLayout(node.secondChild, secondSize, secondPos);
}

void DockingLayer::splitNode(int32_t nodeIndex, DockZone zone, UIObject *newContent)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_nodes.size())) {
        return;
    }

    if (!m_nodes[nodeIndex].isLeaf()) {
        AM_LOG_WARN("node index not a leaf node, cannot split");
        return;
    }

    TabBar *existingContent = m_nodes[nodeIndex].content;
    glm::vec2 nodeSize = existingContent->absoluteSize;
    glm::vec2 nodePosition = existingContent->absolutePosition;

    int32_t existingChild = createNode();
    int32_t newChild = createNode();

    m_nodes[existingChild].content = existingContent;
    m_nodes[existingChild].parentNode = nodeIndex;

    auto newTabBar = std::make_unique<TabBar>(this);
    setupTabBarCallbacks(newTabBar.get());
    newTabBar->addChild(newContent);
    m_nodes[newChild].content = newTabBar.get();
    m_nodes[newChild].parentNode = nodeIndex;

    bool newFirst = (zone == DockZone::LEFT || zone == DockZone::TOP);
    m_nodes[nodeIndex].content = nullptr;
    m_nodes[nodeIndex].firstChild = newFirst ? newChild : existingChild;
    m_nodes[nodeIndex].secondChild = newFirst ? existingChild : newChild;
    m_nodes[nodeIndex].axis = (zone == DockZone::LEFT || zone == DockZone::RIGHT) ? SplitAxis::HORIZONTAL : SplitAxis::VERTICAL;

    recalculateChildren(nodeIndex, nodeSize, nodePosition);
    m_tabBars.push_back(std::move(newTabBar));
}

void DockingLayer::recalculateChildren(int32_t parentIndex, glm::vec2 parentSize, glm::vec2 parentPosition)
{
    if (parentIndex < 0 || parentIndex >= static_cast<int32_t>(m_nodes.size())) {
        return;
    }

    DockNode &parentNode = m_nodes[parentIndex];
    if (parentNode.isLeaf()) {
        AM_LOG_WARN("Leaf node does not have children?");
        return;
    }

    DockNode &firstChild = m_nodes[parentNode.firstChild];
    DockNode &secondChild = m_nodes[parentNode.secondChild];

    if (parentNode.axis == SplitAxis::HORIZONTAL) {
        firstChild.content->size = UDim2::fromScale(1.0f, parentNode.ratio);
        firstChild.content->position = UDim2::fromScale(0.0f);
        secondChild.content->size = UDim2::fromScale(1.0f, 1.0f - parentNode.ratio);
        secondChild.content->position = UDim2::fromScale(0.0f, parentNode.ratio);
    } else {
        firstChild.content->size = UDim2::fromScale(parentNode.ratio, 1.0f);
        firstChild.content->position = UDim2::fromScale(0.0f);
        secondChild.content->size = UDim2::fromScale(1.0f - parentNode.ratio, 1.0f);
        secondChild.content->position = UDim2::fromScale(parentNode.ratio, 0.0f);
    }

    firstChild.content->computeAbsolutes(parentSize, parentPosition, 0.0f);
    firstChild.content->markDirty();
    secondChild.content->computeAbsolutes(parentSize, parentPosition, 0.0f);
    secondChild.content->markDirty();
}

int32_t DockingLayer::findNodeByPosition(glm::vec2 pos, int32_t nodeIndex, glm::vec2 parentSize, glm::vec2 parentPosition)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_nodes.size())) {
        return -1;
    }

    DockNode &node = m_nodes[nodeIndex];

    if (node.isLeaf()) {
        return nodeIndex;
    }

    glm::vec2 firstPos = parentPosition;
    glm::vec2 firstSize = parentSize;
    glm::vec2 secondPos = parentPosition;
    glm::vec2 secondSize = parentSize;

    if (node.axis == SplitAxis::HORIZONTAL) {
        float splitSizeTop = parentSize.y * node.ratio;
        firstSize.y = splitSizeTop;
        secondPos.y = parentPosition.y + splitSizeTop;
        secondSize.y = parentSize.y - splitSizeTop;
    } else if (node.axis == SplitAxis::VERTICAL) {
        float splitSizeLeft = parentSize.x * node.ratio;
        firstSize.x = splitSizeLeft;
        secondPos.x = parentPosition.x + splitSizeLeft;
        secondSize.x = parentSize.x - splitSizeLeft;
    }

    glm::vec2 firstEnd = firstPos + firstSize;
    if (pos.x >= firstPos.x && pos.x < firstEnd.x && pos.y >= firstPos.y && pos.y < firstEnd.y) {
        return findNodeByPosition(pos, node.firstChild, firstSize, firstPos);
    }

    return findNodeByPosition(pos, node.secondChild, secondSize, secondPos);
}

void DockingLayer::setupTabBarCallbacks(TabBar *tabBar)
{
    tabBar->onTornOffTabReleased = [this, tabBar](Instance *content, glm::vec2 dropPos) {
        int32_t sourceNode = findNodeByPosition(tabBar->absolutePosition, m_rootNode, absoluteSize, absolutePosition);
        tabBar->removeChild(content);

        if (tabBar->children.empty()) {
            collapseNode(sourceNode);
        }

        if (UIObject *obj = content->as<UIObject>()) {
            dock(obj, dropPos);
        }
    };
}

} // namespace Amethyst
