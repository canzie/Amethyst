/*
 * Container for grouping UI elements
 */

#ifndef AMETHYST__UI_LAYER_H
#define AMETHYST__UI_LAYER_H

#include "components/common.h"
#include "components/ui_base_2d.h"
#include <memory>

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

  public:
    bool visible = true;
    bool resetOnSpawn = false;
    ZIndexBehavior zindexBehavior = ZIndexBehavior::GLOBAL;

  private:
    std::unique_ptr<GeometryRegistry> m_geometryRegistry;
    int32_t m_displayOrder = 0;
};

} // namespace Amethyst

#endif // AMETHYST__UI_LAYER_H
