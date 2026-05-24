#include "docking_layer.h"

#include "components/common.h"
#include "components/extensions/ui_drag_detector.h"
#include "components/input_interface.h"
#include "components/invisible_button.h"
#include "components/tab_bar.h"
#include "components/ui_base_2d.h"
#include "logging/log.h"
#include "parsers/config/layout_config.h"
#include "rendering/draw_context.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace Amethyst {

DockingLayer::DockingLayer()
{
    initDockHints();
}

DockingLayer::~DockingLayer()
{
    if (persistLayout && !name.empty()) {
        LayoutConfig::instance().set(name, ConfigEntry(saveConfig()));
    }
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

    DrawContext layerCtx;
    layerCtx.geometry = geometryRegistry();
    layerCtx.overlay = ctx.overlay;
    layerCtx.textProcessor = ctx.textProcessor;
    layerCtx.glyphAtlas = ctx.glyphAtlas;
    layerCtx.svgAtlas = ctx.svgAtlas;

    for (auto &tabBar : m_tabBars) {
        tabBar->draw(layerCtx);
    }

    DrawContext hintCtx = ctx;
    if (ctx.overlay) {
        hintCtx.geometry = ctx.overlay;
    }
    for (auto &hint : m_dockHintComponents) {
        hint->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        hint->draw(hintCtx);
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
    for (auto &node : m_nodes) {
        if (node.resizeHandle) {
            result.push_back(node.resizeHandle.get());
        }
    }

    for (auto &tabBar : m_tabBars) {
        result.push_back(tabBar.get());
    }
    return result;
}

void DockingLayer::dock(std::unique_ptr<Instance> obj, glm::vec2 pos)
{
    if (m_rootNode < 0) {
        int32_t nodeIdx = createNode();
        m_rootNode = nodeIdx;

        auto tabBar = std::make_unique<TabBar>();
        tabBar->parent = this;
        setupTabBarCallbacks(tabBar.get());
        tabBar->addChild(std::move(obj));
        m_nodes[nodeIdx].content = tabBar.get();
        tabBar->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        tabBar->position = UDim2::fromScale(0.0f);
        tabBar->size = UDim2::fromScale(1.0f);
        m_tabBars.push_back(std::move(tabBar));
    } else {
        int32_t targetNode = findNodeByPosition(pos, m_rootNode, absoluteSize, absolutePosition);
        if (targetNode < 0) return;
        DockZone targetZone = hitTestZone(targetNode, pos);
        if (targetZone == DockZone::CENTER) {
            DockNode &node = m_nodes[targetNode];
            node.content->addChild(std::move(obj));
        } else {
            splitNode(targetNode, targetZone, std::move(obj));
        }
    }

    markDirty();
}

int32_t DockingLayer::dock(std::unique_ptr<Instance> content, int32_t targetLeaf, DockZone zone, float ratio)
{
    if (targetLeaf < 0) {
        int32_t nodeIdx = createNode();
        if (m_rootNode < 0) m_rootNode = nodeIdx;

        auto tabBar = std::make_unique<TabBar>();
        tabBar->parent = this;
        setupTabBarCallbacks(tabBar.get());
        tabBar->addChild(std::move(content));
        m_nodes[nodeIdx].content = tabBar.get();
        tabBar->position = UDim2::fromScale(0.0f);
        tabBar->size = UDim2::fromScale(1.0f);
        m_tabBars.push_back(std::move(tabBar));
        markDirty();
        return nodeIdx;
    }

    if (targetLeaf >= static_cast<int32_t>(m_nodes.size()) || !m_nodes[targetLeaf].isLeaf()) {
        AM_LOG_WARN("dock: target {} is not a valid leaf node", targetLeaf);
        return -1;
    }

    if (zone == DockZone::CENTER) {
        m_nodes[targetLeaf].content->addChild(std::move(content));
        markDirty();
        return targetLeaf;
    }

    int32_t newLeaf = splitNode(targetLeaf, zone, std::move(content));
    m_nodes[targetLeaf].ratio = ratio;
    markDirty();
    return newLeaf;
}

std::unique_ptr<Instance> DockingLayer::undock(UIBase2D *obj)
{
    glm::vec2 objPos = obj->absolutePosition;
    int32_t nodeIndex = findNodeByPosition(objPos, m_rootNode, absoluteSize, absolutePosition);
    if (nodeIndex < 0) {
        return nullptr;
    }

    DockNode &node = m_nodes[nodeIndex];
    auto result = node.content->removeChild(obj);

    if (node.content->getChildren().empty()) {
        collapseNode(nodeIndex);
    }

    markDirty();
    return result;
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
    node.nodePosition = nodePosition;
    node.nodeSize = nodeSize;

    if (node.isLeaf()) {
        glm::vec2 contentSize = nodeSize;
        glm::vec2 contentPos = nodePosition;

        bool isTopEdge = (nodePosition.y <= absolutePosition.y + 0.01f);
        bool isBottomEdge = (nodePosition.y + nodeSize.y >= absolutePosition.y + absoluteSize.y - 0.01f);
        bool isLeftEdge = (nodePosition.x <= absolutePosition.x + 0.01f);
        bool isRightEdge = (nodePosition.x + nodeSize.x >= absolutePosition.x + absoluteSize.x - 0.01f);

        float topSpacing = isTopEdge ? outerSpacing : innerSpacing;
        float bottomSpacing = isBottomEdge ? outerSpacing : innerSpacing;
        float leftSpacing = isLeftEdge ? outerSpacing : innerSpacing;
        float rightSpacing = isRightEdge ? outerSpacing : innerSpacing;

        contentPos.x += leftSpacing;
        contentPos.y += topSpacing;
        contentSize.x -= (leftSpacing + rightSpacing);
        contentSize.y -= (topSpacing + bottomSpacing);

        node.content->size = UDim2::fromScale(1.0f, 1.0f);
        node.content->position = UDim2::fromScale(0.0f, 0.0f);
        node.content->computeAbsolutes(contentSize, contentPos, 0.0f);
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

    if (node.resizeHandle) {
        if (node.axis == SplitAxis::VERTICAL) {
            node.resizeHandle->position = UDim2(node.ratio, -m_resizeHandleThickness / 2.0f, 0.0f, 0.0f);
            node.resizeHandle->size = UDim2(0.0f, m_resizeHandleThickness, 1.0f, 0.0f);
        } else {
            node.resizeHandle->position = UDim2(0.0f, 0.0f, node.ratio, -m_resizeHandleThickness / 2.0f);
            node.resizeHandle->size = UDim2(1.0f, 0.0f, 0.0f, m_resizeHandleThickness);
        }
        node.resizeHandle->computeAbsolutes(nodeSize, nodePosition, 0.0f);
    }
}

void DockingLayer::setupResizeHandle(int32_t nodeIndex, glm::vec2 nodeSize, glm::vec2 nodePosition)
{
    DockNode &node = m_nodes[nodeIndex];
    if (node.isLeaf()) {
        return;
    }

    node.resizeHandle = std::make_unique<InvisibleButton>();
    node.resizeHandle->parent = this;
    node.resizeHandle->zIndex = 10;

    if (node.axis == SplitAxis::VERTICAL) {
        node.resizeHandle->position = UDim2(node.ratio, -m_resizeHandleThickness / 2.0f, 0.0f, 0.0f);
        node.resizeHandle->size = UDim2(0.0f, m_resizeHandleThickness, 1.0f, 0.0f);
    } else {
        node.resizeHandle->position = UDim2(0.0f, 0.0f, node.ratio, -m_resizeHandleThickness / 2.0f);
        node.resizeHandle->size = UDim2(1.0f, 0.0f, 0.0f, m_resizeHandleThickness);
    }

    CursorShape cursorShape = (node.axis == SplitAxis::VERTICAL) ? CURSOR_HORI_RESIZE : CURSOR_VERT_RESIZE;

    node.resizeHandle->onMouseEnterCb = [cursorShape]() { InputInterface::setCursorShape(cursorShape); return EventResult::CONSUMED; };

    node.resizeHandle->onMouseLeaveCb = []() { InputInterface::setCursorShape(CURSOR_ARROW); return EventResult::CONSUMED; };

    auto *drag = node.resizeHandle->addExtension<UIDragDetector>();
    drag->mode = (node.axis == SplitAxis::VERTICAL) ? DragMode::HORIZONTAL : DragMode::VERTICAL;

    InvisibleButton *handlePtr = node.resizeHandle.get();

    drag->onDragStart = [cursorShape](glm::vec2) { InputInterface::setCursorShape(cursorShape); };

    drag->onDragUpdate = [this, handlePtr](glm::vec2, glm::vec2 mousePos) {
        glm::vec2 handlePos = handlePtr->absolutePosition + handlePtr->absoluteSize * 0.5f;
        int32_t ownerIndex = findNodeByResizeHandlePosition(handlePos, m_rootNode);

        if (ownerIndex < 0 || ownerIndex >= static_cast<int32_t>(m_nodes.size())) {
            return;
        }

        DockNode &owner = m_nodes[ownerIndex];

        if (owner.axis == SplitAxis::VERTICAL) {
            float relativeX = mousePos.x - owner.nodePosition.x;
            owner.ratio = glm::clamp(relativeX / owner.nodeSize.x, 0.1f, 0.9f);
        } else {
            float relativeY = mousePos.y - owner.nodePosition.y;
            owner.ratio = glm::clamp(relativeY / owner.nodeSize.y, 0.1f, 0.9f);
        }

        markDirty();
    };

    drag->onDragEnd = [](glm::vec2) { InputInterface::setCursorShape(CURSOR_ARROW); };

    node.resizeHandle->computeAbsolutes(nodeSize, nodePosition, 0.0f);
}

int32_t DockingLayer::splitNode(int32_t nodeIndex, DockZone zone, std::unique_ptr<Instance> newContent)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_nodes.size())) {
        return -1;
    }

    if (!m_nodes[nodeIndex].isLeaf()) {
        AM_LOG_WARN("node index not a leaf node, cannot split");
        return -1;
    }

    TabBar *existingContent = m_nodes[nodeIndex].content;
    glm::vec2 nodeSize = existingContent->absoluteSize;
    glm::vec2 nodePosition = existingContent->absolutePosition;

    int32_t existingChild = createNode();
    int32_t newChild = createNode();

    m_nodes[existingChild].content = existingContent;
    m_nodes[existingChild].parentNode = nodeIndex;

    auto newTabBar = std::make_unique<TabBar>();
    newTabBar->parent = this;
    setupTabBarCallbacks(newTabBar.get());
    newTabBar->addChild(std::move(newContent));
    m_nodes[newChild].content = newTabBar.get();
    m_nodes[newChild].parentNode = nodeIndex;

    bool newFirst = (zone == DockZone::LEFT || zone == DockZone::TOP);
    auto &parentNode = m_nodes[nodeIndex];
    parentNode.content = nullptr;
    parentNode.firstChild = newFirst ? newChild : existingChild;
    parentNode.secondChild = newFirst ? existingChild : newChild;
    parentNode.axis = (zone == DockZone::LEFT || zone == DockZone::RIGHT) ? SplitAxis::VERTICAL : SplitAxis::HORIZONTAL;

    recalculateChildren(nodeIndex, nodeSize, nodePosition);
    setupResizeHandle(nodeIndex, nodeSize, nodePosition);
    m_tabBars.push_back(std::move(newTabBar));
    return newChild;
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

int32_t DockingLayer::findNodeByResizeHandlePosition(glm::vec2 pos, int32_t nodeIndex)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_nodes.size())) {
        return -1;
    }

    DockNode &node = m_nodes[nodeIndex];

    if (node.resizeHandle) {
        glm::vec2 handlePos = node.resizeHandle->absolutePosition;
        glm::vec2 handleSize = node.resizeHandle->absoluteSize;
        if (pos.x >= handlePos.x && pos.x < handlePos.x + handleSize.x && pos.y >= handlePos.y &&
            pos.y < handlePos.y + handleSize.y) {
            return nodeIndex;
        }
    }

    if (node.isLeaf()) {
        return -1;
    }

    glm::vec2 nodePos = node.nodePosition;
    glm::vec2 nodeSize = node.nodeSize;

    if (node.axis == SplitAxis::HORIZONTAL) {
        float splitY = nodePos.y + nodeSize.y * node.ratio;
        if (pos.y < splitY) {
            return findNodeByResizeHandlePosition(pos, node.firstChild);
        } else {
            return findNodeByResizeHandlePosition(pos, node.secondChild);
        }
    } else {
        float splitX = nodePos.x + nodeSize.x * node.ratio;
        if (pos.x < splitX) {
            return findNodeByResizeHandlePosition(pos, node.firstChild);
        } else {
            return findNodeByResizeHandlePosition(pos, node.secondChild);
        }
    }
}

void DockingLayer::setupTabBarCallbacks(TabBar *tabBar)
{
    tabBar->onTornOffTabReleased = [this, tabBar](Instance *content, glm::vec2 dropPos) {
        hideDockHints();
        int32_t sourceNode = findNodeByPosition(tabBar->absolutePosition, m_rootNode, absoluteSize, absolutePosition);
        auto panel = tabBar->removeChild(content);

        if (tabBar->getChildren().empty()) {
            collapseNode(sourceNode);
        }

        if (panel) {
            dock(std::move(panel), dropPos);
        }
    };

    tabBar->onTornOffTabMoved = [this](Instance *, glm::vec2 pos) { updateDockHints(pos); };
}

void DockingLayer::initDockHints()
{
    const Color3 hintColor = {0.2f, 0.5f, 0.8f};
    const float hintTransparency = 0.5f;

    for (auto &hint : m_dockHintComponents) {
        hint = std::make_unique<Frame>();
        hint->parent = this;
        hint->visible = false;
        hint->interactable = false;
        hint->backgroundColor = hintColor;
        hint->backgroundTransparency = hintTransparency;
        hint->borderPixelSize = 0.0f;
        hint->cornerRadius = 0.0f;
        hint->markDirty();
    }
}

void DockingLayer::updateDockHints(glm::vec2 mousePos)
{
    if (m_rootNode < 0) {
        hideDockHints();
        return;
    }

    int32_t hoveredNode = findNodeByPosition(mousePos, m_rootNode, absoluteSize, absolutePosition);
    if (hoveredNode < 0) {
        hideDockHints();
        return;
    }

    DockNode &node = m_nodes[hoveredNode];
    glm::vec2 nodePos = node.content->absolutePosition;
    glm::vec2 nodeSize = node.content->absoluteSize;

    glm::vec2 layerPos = absolutePosition;

    const float zoneSize = 0.33f;
    const float centerSize = 0.34f;

    auto &leftHint = m_dockHintComponents[0];
    leftHint->visible = true;
    leftHint->position = UDim2::fromOffset(nodePos.x - layerPos.x, nodePos.y - layerPos.y);
    leftHint->size = UDim2::fromOffset(nodeSize.x * zoneSize, nodeSize.y);
    leftHint->markDirty();

    auto &rightHint = m_dockHintComponents[1];
    rightHint->visible = true;
    rightHint->position = UDim2::fromOffset(nodePos.x - layerPos.x + nodeSize.x * (1.0f - zoneSize), nodePos.y - layerPos.y);
    rightHint->size = UDim2::fromOffset(nodeSize.x * zoneSize, nodeSize.y);
    rightHint->markDirty();

    auto &topHint = m_dockHintComponents[2];
    topHint->visible = true;
    topHint->position = UDim2::fromOffset(nodePos.x - layerPos.x, nodePos.y - layerPos.y);
    topHint->size = UDim2::fromOffset(nodeSize.x, nodeSize.y * zoneSize);
    topHint->markDirty();

    auto &bottomHint = m_dockHintComponents[3];
    bottomHint->visible = true;
    bottomHint->position = UDim2::fromOffset(nodePos.x - layerPos.x, nodePos.y - layerPos.y + nodeSize.y * (1.0f - zoneSize));
    bottomHint->size = UDim2::fromOffset(nodeSize.x, nodeSize.y * zoneSize);
    bottomHint->markDirty();

    auto &centerHint = m_dockHintComponents[4];
    centerHint->visible = true;
    centerHint->position =
        UDim2::fromOffset(nodePos.x - layerPos.x + nodeSize.x * zoneSize, nodePos.y - layerPos.y + nodeSize.y * zoneSize);
    centerHint->size = UDim2::fromOffset(nodeSize.x * centerSize, nodeSize.y * centerSize);
    centerHint->markDirty();
}

void DockingLayer::hideDockHints()
{

    for (auto &hint : m_dockHintComponents) {
        hint->visible = false;
        hint->markDirty();
    }
}

static int32_t s_saveNode(const std::vector<DockNode> &nodes, int32_t nodeIndex, DockLayoutConfig &cfg)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(nodes.size())) return -1;

    const DockNode &node = nodes[nodeIndex];
    int32_t cfgIndex = static_cast<int32_t>(cfg.nodes.size());
    cfg.nodes.emplace_back();

    if (node.isLeaf()) {
        DockNodeConfig &out = cfg.nodes[cfgIndex];
        if (node.content) {
            for (auto &child : node.content->getChildren()) {
                out.panels.push_back(child->name);
            }
            if (auto *sel = node.content->getSelectedContent()) {
                out.selected = sel->name;
            }
        }
        return cfgIndex;
    }

    int32_t firstCfg = s_saveNode(nodes, node.firstChild, cfg);
    int32_t secondCfg = s_saveNode(nodes, node.secondChild, cfg);

    DockNodeConfig &out = cfg.nodes[cfgIndex];
    out.axis = (node.axis == SplitAxis::VERTICAL) ? "vertical" : "horizontal";
    out.ratio = node.ratio;
    out.first = firstCfg;
    out.second = secondCfg;

    return cfgIndex;
}

DockLayoutConfig DockingLayer::saveConfig() const
{
    DockLayoutConfig cfg;
    if (m_rootNode >= 0) {
        s_saveNode(m_nodes, m_rootNode, cfg);
    }
    return cfg;
}

void DockingLayer::applyConfig(const DockLayoutConfig &config)
{
    if (config.nodes.empty()) return;

    std::unordered_map<std::string, std::unique_ptr<Instance>> panelsByName;
    for (auto &node : m_nodes) {
        if (!node.isLeaf() || !node.content) continue;
        for (auto &child : node.content->removeAllChildren()) {
            if (!child->name.empty()) {
                panelsByName[child->name] = std::move(child);
            }
        }
    }

    m_nodes.clear();
    m_tabBars.clear();
    m_rootNode = -1;

    std::vector<int32_t> cfgToNode(config.nodes.size(), -1);

    for (size_t i = 0; i < config.nodes.size(); ++i) {
        cfgToNode[i] = createNode();
    }

    for (size_t i = 0; i < config.nodes.size(); ++i) {
        const DockNodeConfig &src = config.nodes[i];
        DockNode &dst = m_nodes[cfgToNode[i]];

        if (src.isLeaf()) {
            auto tabBar = std::make_unique<TabBar>();
            tabBar->parent = this;
            setupTabBarCallbacks(tabBar.get());

            for (const auto &panelName : src.panels) {
                auto it = panelsByName.find(panelName);
                if (it != panelsByName.end()) {
                    tabBar->addChild(std::move(it->second));
                    panelsByName.erase(it);
                }
            }

            if (!src.selected.empty()) {
                for (auto &child : tabBar->getChildren()) {
                    if (child->name == src.selected) {
                        tabBar->select(child.get());
                        break;
                    }
                }
            }

            tabBar->position = UDim2::fromScale(0.0f);
            tabBar->size = UDim2::fromScale(1.0f);
            dst.content = tabBar.get();
            m_tabBars.push_back(std::move(tabBar));
        } else {
            dst.axis = (src.axis == "vertical") ? SplitAxis::VERTICAL : SplitAxis::HORIZONTAL;
            dst.ratio = src.ratio;

            if (src.first >= 0 && src.first < static_cast<int32_t>(cfgToNode.size())) {
                dst.firstChild = cfgToNode[src.first];
                m_nodes[dst.firstChild].parentNode = cfgToNode[i];
            }
            if (src.second >= 0 && src.second < static_cast<int32_t>(cfgToNode.size())) {
                dst.secondChild = cfgToNode[src.second];
                m_nodes[dst.secondChild].parentNode = cfgToNode[i];
            }

            setupResizeHandle(cfgToNode[i], absoluteSize, absolutePosition);
        }
    }

    m_rootNode = cfgToNode[0];

    bool collapsed = true;
    while (collapsed) {
        collapsed = false;
        for (int32_t i = 0; i < static_cast<int32_t>(m_nodes.size()); ++i) {
            if (m_nodes[i].isLeaf() && m_nodes[i].content && m_nodes[i].content->getChildren().empty()) {
                collapseNode(i);
                processPendingDeletions();
                collapsed = true;
                break;
            }
        }
    }

    for (auto &[panelName, panel] : panelsByName) {
        for (auto &node : m_nodes) {
            if (node.isLeaf() && node.content) {
                node.content->addChild(std::move(panel));
                break;
            }
        }
    }

    markDirty();
}

} // namespace Amethyst
