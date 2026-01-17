/*
 * DockingLayer - layer with BSP-based spatial partitioning for dockable frames
 *
 * The BSP tree determines layout, but DockableFrames remain children
 * in the scene graph. Normal propagation is unchanged.
 *
 */

#ifndef AMETHYST__DOCKING_LAYER_H
#define AMETHYST__DOCKING_LAYER_H

#include "components/common.h"
#include "components/tab_bar.h"
#include "components/ui_layer.h"
#include "components/ui_object.h"
#include <vector>

namespace Amethyst {

class TabBar;

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

    TabBar *content = nullptr;

    bool isLeaf() const { return axis == SplitAxis::NONE; }
};

class DockingLayer : public UILayer {
  public:
    DockingLayer();
    DockingLayer(Instance *parent);
    virtual ~DockingLayer() = default;

    void draw(DrawContext &ctx) override;

    void dock(UIObject *obj, DockZone zone);
    void dock(UIObject *obj, int32_t targetNode, DockZone zone);
    void undock(UIObject *obj);

    DockZone hitTestZone(int32_t nodeIndex, glm::vec2 position);

  public:
    float zoneEdgeRatio = 0.2f;
    float minSplitSize = 50.0f;

    Color3 zoneHighlightColor = {0.3f, 0.5f, 0.8f};
    float zoneHighlightTransparency = 0.5f;

  private:
    void computeLayout();
    void computeNodeLayout(int32_t nodeIndex, glm::vec2 pos, glm::vec2 size);

    std::vector<DockNode> m_nodes;
    int32_t m_rootNode = 0;
};

} // namespace Amethyst

#endif // AMETHYST__DOCKING_LAYER_H
