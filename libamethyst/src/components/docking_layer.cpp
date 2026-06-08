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
#include "utils/am_assert.h"

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

    DrawContext layerCtx;
    layerCtx.geometry = geometryRegistry();
    layerCtx.overlay = ctx.overlay;
    layerCtx.textProcessor = ctx.textProcessor;
    layerCtx.glyphAtlas = ctx.glyphAtlas;
    layerCtx.svgAtlas = ctx.svgAtlas;

    if (!visible) {
        if (flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY)) {
            for (auto &tabBar : m_tabBars) {
                tabBar->markDirty();
                tabBar->draw(layerCtx);
            }
        }
        flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
        return;
    }

    if (flags & FLAG_DIRTY) {
        computeLayout(m_rootNode, absoluteSize, absolutePosition);
    }

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

DockZone DockingLayer::hitTestZone(int32_t nodeIndex, vec2 position)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_nodes.size())) {
        return DockZone::CENTER;
    }

    DockNode &node = m_nodes[nodeIndex];

    vec2 nodePos = node.content->absolutePosition;
    vec2 nodeSize = node.content->absoluteSize;

    vec2 local = position - nodePos;
    vec2 normalized = local / nodeSize;

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

void DockingLayer::computeLayout(int32_t nodeIndex, vec2 nodeSize, vec2 nodePosition)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_nodes.size())) {
        return;
    }

    DockNode &node = m_nodes[nodeIndex];
    node.nodePosition = nodePosition;
    node.nodeSize = nodeSize;

    if (node.isLeaf()) {
        vec2 contentSize = nodeSize;
        vec2 contentPos = nodePosition;

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

        node.content->setBaseProperties({.position = UDim2::fromScale(0.0f, 0.0f), .size = UDim2::fromScale(1.0f, 1.0f)});
        node.content->computeAbsolutes(contentSize, contentPos, 0.0f);
        node.content->markDirty();
        return;
    }

    vec2 firstSize = nodeSize, firstPos = nodePosition;
    vec2 secondSize = nodeSize, secondPos = nodePosition;

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
            node.resizeHandle->setBaseProperties({
                .position = UDim2(node.ratio, -m_resizeHandleThickness / 2.0f, 0.0f, 0.0f),
                .size = UDim2(0.0f, m_resizeHandleThickness, 1.0f, 0.0f),
            });
        } else {
            node.resizeHandle->setBaseProperties({
                .position = UDim2(0.0f, 0.0f, node.ratio, -m_resizeHandleThickness / 2.0f),
                .size = UDim2(1.0f, 0.0f, 0.0f, m_resizeHandleThickness),
            });
        }
        node.resizeHandle->computeAbsolutes(nodeSize, nodePosition, 0.0f);
    }
}

void DockingLayer::setupResizeHandle(int32_t nodeIndex, vec2 nodeSize, vec2 nodePosition)
{
    DockNode &node = m_nodes[nodeIndex];
    if (node.isLeaf()) {
        return;
    }

    node.resizeHandle = std::make_unique<InvisibleButton>();
    node.resizeHandle->parent = this;

    if (node.axis == SplitAxis::VERTICAL) {
        node.resizeHandle->setBaseProperties({
            .position = UDim2(node.ratio, -m_resizeHandleThickness / 2.0f, 0.0f, 0.0f),
            .size = UDim2(0.0f, m_resizeHandleThickness, 1.0f, 0.0f),
            .zIndex = 10,
        });
    } else {
        node.resizeHandle->setBaseProperties({
            .position = UDim2(0.0f, 0.0f, node.ratio, -m_resizeHandleThickness / 2.0f),
            .size = UDim2(1.0f, 0.0f, 0.0f, m_resizeHandleThickness),
            .zIndex = 10,
        });
    }

    CursorShape cursorShape = (node.axis == SplitAxis::VERTICAL) ? CURSOR_HORI_RESIZE : CURSOR_VERT_RESIZE;

    node.resizeHandle->onMouseEnterCb = [cursorShape]() {
        InputInterface::setCursorShape(cursorShape);
        return EventResult::CONSUMED;
    };

    node.resizeHandle->onMouseLeaveCb = []() {
        InputInterface::setCursorShape(CURSOR_ARROW);
        return EventResult::CONSUMED;
    };

    auto *drag = node.resizeHandle->addExtension<UIDragDetector>();
    drag->mode = (node.axis == SplitAxis::VERTICAL) ? DragMode::HORIZONTAL : DragMode::VERTICAL;

    InvisibleButton *handlePtr = node.resizeHandle.get();

    drag->onDragStart = [cursorShape](vec2) { InputInterface::setCursorShape(cursorShape); };

    drag->onDragUpdate = [this, handlePtr](vec2, vec2 mousePos) {
        vec2 handlePos = handlePtr->absolutePosition + handlePtr->absoluteSize * 0.5f;
        int32_t ownerIndex = findNodeByResizeHandlePosition(handlePos, m_rootNode);

        if (ownerIndex < 0 || ownerIndex >= static_cast<int32_t>(m_nodes.size())) {
            return;
        }

        DockNode &owner = m_nodes[ownerIndex];

        if (owner.axis == SplitAxis::VERTICAL) {
            float relativeX = mousePos.x - owner.nodePosition.x;
            owner.ratio = clamp(relativeX / owner.nodeSize.x, 0.1f, 0.9f);
        } else {
            float relativeY = mousePos.y - owner.nodePosition.y;
            owner.ratio = clamp(relativeY / owner.nodeSize.y, 0.1f, 0.9f);
        }

        markDirty();
    };

    drag->onDragEnd = [](vec2) { InputInterface::setCursorShape(CURSOR_ARROW); };

    node.resizeHandle->computeAbsolutes(nodeSize, nodePosition, 0.0f);
}

int32_t DockingLayer::splitNode(int32_t nodeIndex, DockZone zone, std::unique_ptr<TabBar::Tab> tab)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_nodes.size())) {
        return -1;
    }

    if (!m_nodes[nodeIndex].isLeaf()) {
        AM_LOG_WARN("node index not a leaf node, cannot split");
        return -1;
    }

    TabBar *existingContent = m_nodes[nodeIndex].content;
    vec2 nodeSize = existingContent->absoluteSize;
    vec2 nodePosition = existingContent->absolutePosition;

    int32_t existingChild = createNode();
    int32_t newChild = createNode();

    m_nodes[existingChild].content = existingContent;
    m_nodes[existingChild].parentNode = nodeIndex;

    auto newTabBar = std::make_unique<TabBar>();
    newTabBar->parent = this;
    setupTabBarCallbacks(newTabBar.get());
    newTabBar->setTabBarProperties({.tabTearOffEnabled = true});
    newTabBar->addTab(std::move(tab));
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

void DockingLayer::recalculateChildren(int32_t parentIndex, vec2 parentSize, vec2 parentPosition)
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
        firstChild.content->setBaseProperties(
            {.position = UDim2::fromScale(0.0f), .size = UDim2::fromScale(1.0f, parentNode.ratio)});
        secondChild.content->setBaseProperties(
            {.position = UDim2::fromScale(0.0f, parentNode.ratio), .size = UDim2::fromScale(1.0f, 1.0f - parentNode.ratio)});
    } else {
        firstChild.content->setBaseProperties(
            {.position = UDim2::fromScale(0.0f), .size = UDim2::fromScale(parentNode.ratio, 1.0f)});
        secondChild.content->setBaseProperties(
            {.position = UDim2::fromScale(parentNode.ratio, 0.0f), .size = UDim2::fromScale(1.0f - parentNode.ratio, 1.0f)});
    }

    firstChild.content->computeAbsolutes(parentSize, parentPosition, 0.0f);
    firstChild.content->markDirty();
    secondChild.content->computeAbsolutes(parentSize, parentPosition, 0.0f);
    secondChild.content->markDirty();
}

int32_t DockingLayer::findNodeByPosition(vec2 pos, int32_t nodeIndex, vec2 parentSize, vec2 parentPosition)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_nodes.size())) {
        return -1;
    }

    DockNode &node = m_nodes[nodeIndex];

    if (node.isLeaf()) {
        return nodeIndex;
    }

    vec2 firstPos = parentPosition;
    vec2 firstSize = parentSize;
    vec2 secondPos = parentPosition;
    vec2 secondSize = parentSize;

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

    vec2 firstEnd = firstPos + firstSize;
    if (pos.x >= firstPos.x && pos.x < firstEnd.x && pos.y >= firstPos.y && pos.y < firstEnd.y) {
        return findNodeByPosition(pos, node.firstChild, firstSize, firstPos);
    }

    return findNodeByPosition(pos, node.secondChild, secondSize, secondPos);
}

int32_t DockingLayer::findNodeByResizeHandlePosition(vec2 pos, int32_t nodeIndex)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(m_nodes.size())) {
        return -1;
    }

    DockNode &node = m_nodes[nodeIndex];

    if (node.resizeHandle) {
        vec2 handlePos = node.resizeHandle->absolutePosition;
        vec2 handleSize = node.resizeHandle->absoluteSize;
        if (pos.x >= handlePos.x && pos.x < handlePos.x + handleSize.x && pos.y >= handlePos.y &&
            pos.y < handlePos.y + handleSize.y) {
            return nodeIndex;
        }
    }

    if (node.isLeaf()) {
        return -1;
    }

    vec2 nodePos = node.nodePosition;
    vec2 nodeSize = node.nodeSize;

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
    tabBar->onTornOffTabReleased = [this, tabBar](std::unique_ptr<TabBar::Tab> tab, vec2 dropPos) {
        hideDockHints();
        int32_t sourceNode = findNodeByPosition(tabBar->absolutePosition, m_rootNode, absoluteSize, absolutePosition);

        if (tabBar->getTabCount() == 0) {
            collapseNode(sourceNode);
        }

        int32_t targetNode = findNodeByPosition(dropPos, m_rootNode, absoluteSize, absolutePosition);
        if (targetNode < 0) {
            int32_t root = createLeaf();
            obtainLeafTabBar(root)->addTab(std::move(tab));
            markDirty();
            return;
        }

        DockZone zone = hitTestZone(targetNode, dropPos);
        if (zone == DockZone::CENTER) {
            m_nodes[targetNode].content->addTab(std::move(tab));
        } else {
            splitNode(targetNode, zone, std::move(tab));
        }

        markDirty();
    };

    tabBar->onTornOffTabMoved = [this](Instance *, vec2 pos) { updateDockHints(pos); };
}

void DockingLayer::initDockHints()
{
    const Color3 hintColor = {0.2f, 0.5f, 0.8f};
    const float hintTransparency = 0.5f;

    for (auto &hint : m_dockHintComponents) {
        hint = std::make_unique<Frame>();
        hint->parent = this;
        hint->setBaseStyleProperties({
            .backgroundColor = hintColor,
            .backgroundTransparency = hintTransparency,
            .borderPixelSize = 0.0f,
            .cornerRadius = 0.0f,
        });
        hint->setBaseProperties({
            .interactable = false,
            .visible = false,
        });
    }
}

void DockingLayer::updateDockHints(vec2 mousePos)
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
    vec2 nodePos = node.content->absolutePosition;
    vec2 nodeSize = node.content->absoluteSize;

    vec2 layerPos = absolutePosition;

    const float zoneSize = 0.33f;
    const float centerSize = 0.34f;

    auto &leftHint = m_dockHintComponents[0];
    leftHint->setBaseProperties({
        .position = UDim2::fromOffset(nodePos.x - layerPos.x, nodePos.y - layerPos.y),
        .size = UDim2::fromOffset(nodeSize.x * zoneSize, nodeSize.y),
        .visible = true,
    });

    auto &rightHint = m_dockHintComponents[1];
    rightHint->setBaseProperties({
        .position = UDim2::fromOffset(nodePos.x - layerPos.x + nodeSize.x * (1.0f - zoneSize), nodePos.y - layerPos.y),
        .size = UDim2::fromOffset(nodeSize.x * zoneSize, nodeSize.y),
        .visible = true,
    });

    auto &topHint = m_dockHintComponents[2];
    topHint->setBaseProperties({
        .position = UDim2::fromOffset(nodePos.x - layerPos.x, nodePos.y - layerPos.y),
        .size = UDim2::fromOffset(nodeSize.x, nodeSize.y * zoneSize),
        .visible = true,
    });

    auto &bottomHint = m_dockHintComponents[3];
    bottomHint->setBaseProperties({
        .position = UDim2::fromOffset(nodePos.x - layerPos.x, nodePos.y - layerPos.y + nodeSize.y * (1.0f - zoneSize)),
        .size = UDim2::fromOffset(nodeSize.x, nodeSize.y * zoneSize),
        .visible = true,
    });

    auto &centerHint = m_dockHintComponents[4];
    centerHint->setBaseProperties({
        .position =
            UDim2::fromOffset(nodePos.x - layerPos.x + nodeSize.x * zoneSize, nodePos.y - layerPos.y + nodeSize.y * zoneSize),
        .size = UDim2::fromOffset(nodeSize.x * centerSize, nodeSize.y * centerSize),
        .visible = true,
    });
}

void DockingLayer::hideDockHints()
{

    for (auto &hint : m_dockHintComponents) {
        hint->setBaseProperties({.visible = false});
    }
}

int32_t DockingLayer::createLeaf()
{
    int32_t idx = createNode();
    if (m_rootNode < 0) {
        m_rootNode = idx;
    }
    return idx;
}

std::pair<int32_t, int32_t> DockingLayer::splitLeaf(int32_t nodeIndex, SplitAxis axis, float ratio)
{
    AM_ASSERT(nodeIndex >= 0 && nodeIndex < static_cast<int32_t>(m_nodes.size()), "Invalid node index");
    AM_ASSERT(m_nodes[nodeIndex].isLeaf(), "splitLeaf called on non-leaf node");
    AM_ASSERT(m_nodes[nodeIndex].content == nullptr, "splitLeaf called on non-empty leaf");

    int32_t firstChild = createLeaf();
    int32_t secondChild = createLeaf();

    m_nodes[nodeIndex].axis = axis;
    m_nodes[nodeIndex].ratio = ratio;
    m_nodes[nodeIndex].firstChild = firstChild;
    m_nodes[nodeIndex].secondChild = secondChild;
    m_nodes[firstChild].parentNode = nodeIndex;
    m_nodes[secondChild].parentNode = nodeIndex;

    setupResizeHandle(nodeIndex, absoluteSize, absolutePosition);
    markDirty();
    return {firstChild, secondChild};
}

TabBar *DockingLayer::obtainLeafTabBar(int32_t nodeIndex)
{
    AM_ASSERT(nodeIndex >= 0 && nodeIndex < static_cast<int32_t>(m_nodes.size()), "Invalid node index");
    AM_ASSERT(m_nodes[nodeIndex].isLeaf(), "obtainLeafTabBar called on non-leaf node");

    if (!m_nodes[nodeIndex].content) {
        auto tabBar = std::make_unique<TabBar>();
        tabBar->parent = this;
        setupTabBarCallbacks(tabBar.get());
        tabBar->setTabBarProperties({.tabTearOffEnabled = true});
        tabBar->setBaseProperties({
            .position = UDim2::fromScale(0.0f),
            .size = UDim2::fromScale(1.0f),
        });
        m_nodes[nodeIndex].content = tabBar.get();
        m_tabBars.push_back(std::move(tabBar));
    }
    return m_nodes[nodeIndex].content;
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
            for (int32_t j = 0; j < node.content->getTabCount(); ++j) {
                if (auto *c = node.content->getTabContent(j)) {
                    out.panels.push_back(c->name);
                }
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

    std::unordered_map<std::string, std::unique_ptr<TabBar::Tab>> panelsByName;
    for (auto &node : m_nodes) {
        if (!node.isLeaf() || node.content == nullptr) continue;
        for (auto &tab : node.content->removeAllTabs()) {
            if (tab->content != nullptr && !tab->content->name.empty()) {
                panelsByName[tab->content->name] = std::move(tab);
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
            tabBar->setTabBarProperties({.tabTearOffEnabled = true});

            for (const auto &panelName : src.panels) {
                auto it = panelsByName.find(panelName);
                if (it != panelsByName.end()) {
                    tabBar->addTab(std::move(it->second));
                    panelsByName.erase(it);
                }
            }

            if (!src.selected.empty()) {
                for (int32_t j = 0; j < tabBar->getTabCount(); ++j) {
                    auto *c = tabBar->getTabContent(j);
                    if (c && c->name == src.selected) {
                        tabBar->select(c);
                        break;
                    }
                }
            }

            tabBar->setBaseProperties({
                .position = UDim2::fromScale(0.0f),
                .size = UDim2::fromScale(1.0f),
            });
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
            if (m_nodes[i].isLeaf() && m_nodes[i].content != nullptr && m_nodes[i].content->getTabCount() == 0) {
                collapseNode(i);
                processPendingDeletions();
                collapsed = true;
                break;
            }
        }
    }

    for (auto &[panelName, tab] : panelsByName) {
        for (auto &node : m_nodes) {
            if (node.isLeaf() && node.content != nullptr) {
                node.content->addTab(std::move(tab));
                break;
            }
        }
    }

    markDirty();
}

} // namespace Amethyst
