/*
 * DockingLayer - layer with BSP-based spatial partitioning for dockable frames
 */

#ifndef AMETHYST__DOCKING_LAYER_H
#define AMETHYST__DOCKING_LAYER_H

#include "components/frame.h"
#include "components/invisible_button.h"
#include "components/tab_bar.h"
#include "components/ui_layer.h"
#include "parsers/config/config_types.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Amethyst {

enum class DockZone {
    LEFT,
    RIGHT,
    TOP,
    BOTTOM,
    CENTER
};

enum class SplitAxis {
    NONE,
    HORIZONTAL,
    VERTICAL
};

struct DockNode {
    SplitAxis axis = SplitAxis::NONE;
    float ratio = 0.5f;

    int32_t firstChild = -1;
    int32_t secondChild = -1;
    int32_t parentNode = -1;

    TabBar *content = nullptr;

    std::unique_ptr<InvisibleButton> resizeHandle;

    vec2 nodePosition = vec2(0.0f);
    vec2 nodeSize = vec2(0.0f);

    bool isLeaf() const { return axis == SplitAxis::NONE; }
};

class DockingLayer : public UILayer {
  public:
    DockingLayer();
    virtual ~DockingLayer();

    DockingLayer(const DockingLayer &) = delete;
    DockingLayer &operator=(const DockingLayer &) = delete;
    DockingLayer(DockingLayer &&) = delete;
    DockingLayer &operator=(DockingLayer &&) = delete;

    void draw(DrawContext &ctx) override;
    void arrange() override;
    std::vector<Instance *> getHittableInstances() override;

    DockLayoutConfig saveConfig() const;
    void applyConfig(const DockLayoutConfig &config);

    bool isEmpty() const { return m_rootNode < 0; }

    /**
     * @brief Create an empty leaf node. If no root exists, this node becomes the root.
     * @return Index of the new leaf node.
     */
    int32_t createLeaf();

    /**
     * @brief Split an empty leaf node into two child leaves along the given axis.
     * @param nodeIndex Index of the leaf to split. Must be empty (no TabBar attached yet).
     * @param axis HORIZONTAL splits top/bottom, VERTICAL splits left/right.
     * @param ratio Fraction of the available space given to the first child, in [0, 1].
     * @return Indices of the first and second child leaf nodes.
     */
    std::pair<int32_t, int32_t> splitLeaf(int32_t nodeIndex, SplitAxis axis, float ratio);

    /**
     * @brief Obtain the TabBar for a leaf node, creating one if it does not exist yet.
     * @param nodeIndex Index of the leaf node.
     * @return Pointer to the leaf's TabBar. Never null for a valid leaf index.
     */
    TabBar *obtainLeafTabBar(int32_t nodeIndex);

    /**
     * @brief Dock a fresh empty region into the layer and return its TabBar.
     *
     * If the layer is empty the region fills the whole layer. Otherwise the current
     * root is wrapped under a new split and the region is placed on the given zone.
     * @param zone Side of the layer the new region is placed on.
     * @return TabBar of the new region. Never null.
     */
    TabBar *dockNewRegion(DockZone zone);

  private:
    int32_t createNode();
    void swapAndRemoveNode(int32_t nodeIndex);
    void collapseNode(int32_t nodeIndex);
    void computeLayout(int32_t nodeIndex, vec2 nodeSize, vec2 nodePosition);
    void setupTabBarCallbacks(TabBar *tabBar);
    void setupResizeHandle(int32_t nodeIndex, vec2 nodeSize, vec2 nodePosition);
    void processPendingDeletions();
    void initDockHints();
    void updateDockHints(vec2 mousePos);
    void hideDockHints();
    DockZone hitTestZone(int32_t nodeIndex, vec2 position);
    int32_t findNodeByPosition(vec2 pos, int32_t nodeIndex, vec2 parentSize, vec2 parentPosition);
    int32_t findNodeByResizeHandlePosition(vec2 pos, int32_t nodeIndex);
    int32_t splitNode(int32_t nodeIndex, DockZone targetZone, std::unique_ptr<TabBar::Tab> tab);
    void recalculateChildren(int32_t parentIndex, vec2 parentSize, vec2 parentPosition);

  public:
    float outerSpacing = 0.0f;
    float innerSpacing = 0.0f;
    bool persistLayout = false;

    /**
     * @brief Style classes applied to every TabBar the layer creates
     */
    std::vector<std::string> tabBarClasses;

  private:
    std::vector<DockNode> m_nodes;
    std::vector<std::unique_ptr<TabBar>> m_tabBars;
    int32_t m_rootNode = -1;
    std::array<std::unique_ptr<Frame>, 5> m_dockHintComponents;

    std::vector<TabBar *> m_pendingDeletions;

    float m_resizeHandleThickness = 8.0f;
};

} // namespace Amethyst

#endif // AMETHYST__DOCKING_LAYER_H
