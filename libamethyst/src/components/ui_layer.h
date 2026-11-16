/*
 * Container for grouping UI elements
 */

#ifndef AMETHYST__UI_LAYER_H
#define AMETHYST__UI_LAYER_H

#include "components/common.h"
#include "components/ui_base_2d.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace Amethyst {

class GeometryRegistry;

class UILayer : public UIBase2D {
  public:
    UILayer();
    virtual ~UILayer();

    void draw(DrawContext &ctx) override = 0;
    void arrange() override;

    /**
     * @brief Get the geometry registry for this layer
     */
    GeometryRegistry *geometryRegistry() { return m_geometryRegistry.get(); }

    /**
     * @brief Set the display order for this layer
     */
    void setDisplayOrder(int32_t order);

    /**
     * @brief Get the display order for this layer
     */
    int32_t getDisplayOrder() const { return m_displayOrder; }
    int32_t getZIndex() const override { return m_displayOrder; }
    bool isHitTestVisible() const override { return visible; }
    bool isVisible() const;

    /**
     * @brief Flag the draw order for a rebuild on the next ensureDrawOrder(). Use
     * Instance::markDrawOrderDirty() from a node, which finds its layer.
     */
    void invalidateDrawOrder() { m_drawOrderDirty = true; }

    /**
     * @brief Rebuild the draw order of the nodes drawing into this layer if anything changed it.
     *
     * The walk is depth-first: a node before its children, siblings by (zIndex, child order),
     * each subtree contiguous. ABSOLUTE nodes take their subtree out of the walk and are placed
     * by zIndex across the layer: below the tree when negative, above it otherwise. The walk
     * stops at nested layers, which order themselves. Nodes whose order changed are marked dirty
     * so their next draw pushes the new z.
     */
    void ensureDrawOrder();

    /**
     * @brief ABSOLUTE nodes found by the last draw order pass, stable-sorted by zIndex.
     */
    const std::vector<UIObject *> &getAbsoluteNodes() const { return m_absoluteNodes; }

    /**
     * @brief Layers nested in this one found by the last draw order pass, in walk order.
     */
    const std::vector<UILayer *> &getNestedLayers() const { return m_nestedLayers; }

    /**
     * @brief Whether node takes part in its parent's tree walk, rather than being placed by its layer
     * (ABSOLUTE nodes, nested layers) or not drawn at all.
     */
    static bool isTreeNode(Instance *node);

  public:
    bool visible = true;
    bool resetOnSpawn = false;

  protected:
    /**
     * @brief First draw order this layer hands out.
     */
    virtual uint32_t drawOrderBase() const { return 0; }

  private:
    void walkDrawTree(Instance *node, std::vector<UIObject *> &out);

  private:
    std::unique_ptr<GeometryRegistry> m_geometryRegistry;
    int32_t m_displayOrder = 0;
    bool m_drawOrderDirty = true;
    std::vector<UIObject *> m_absoluteNodes;
    std::vector<UILayer *> m_nestedLayers;
};

} // namespace Amethyst

#endif // AMETHYST__UI_LAYER_H
