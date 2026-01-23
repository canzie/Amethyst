/*
 * DockingLayer - layer with BSP-based spatial partitioning for dockable frames
 */

#ifndef AMETHYST__DOCKING_LAYER_H
#define AMETHYST__DOCKING_LAYER_H

#include "components/common.h"
#include "components/frame.h"
#include "components/invisible_button.h"
#include "components/tab_bar.h"
#include "components/ui_layer.h"
#include "components/ui_object.h"

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

    glm::vec2 nodePosition = glm::vec2(0.0f);
    glm::vec2 nodeSize = glm::vec2(0.0f);

    bool isLeaf() const { return axis == SplitAxis::NONE; }
};

class DockingLayer : public UILayer {
  public:
    DockingLayer();
    DockingLayer(Instance *parent);
    virtual ~DockingLayer() = default;

    DockingLayer(const DockingLayer &) = delete;
    DockingLayer &operator=(const DockingLayer &) = delete;

    DockingLayer(DockingLayer &&) = default;
    DockingLayer &operator=(DockingLayer &&) = default;

    void draw(DrawContext &ctx) override;
    std::vector<Instance *> getHittableInstances() override;

    // the node to split or create will be determined by the pos
    void dock(UIObject *obj, glm::vec2 pos);
    void undock(UIObject *obj);

    DockZone hitTestZone(int32_t nodeIndex, glm::vec2 position);
    int32_t findNodeByPosition(glm::vec2 pos, int32_t nodeIndex, glm::vec2 parentSize, glm::vec2 parentPosition);
    void splitNode(int32_t nodeIndex, DockZone targetZone, UIObject *newContent);
    void recalculateChildren(int32_t parentIndex, glm::vec2 parentSize, glm::vec2 parentPosition);

  private:
    int32_t createNode();
    void swapAndRemoveNode(int32_t nodeIndex);
    void collapseNode(int32_t nodeIndex);
    void computeLayout(int32_t nodeIndex, glm::vec2 nodeSize, glm::vec2 nodePosition);
    void setupTabBarCallbacks(TabBar *tabBar);
    void setupResizeHandle(int32_t nodeIndex, glm::vec2 nodeSize, glm::vec2 nodePosition);
    int32_t findNodeByChildTabBar(TabBar *childTabBar);
    void processPendingDeletions();

  private:
    void initDockHints();
    void updateDockHints(glm::vec2 mousePos);
    void hideDockHints();

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
