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
class TextRegistry;

class UILayer : public UIBase2D {
  public:
    UILayer();
    virtual ~UILayer();

    virtual void draw(DrawContext &ctx) = 0;

    /**
     * @brief Get the geometry registry for this layer
     */
    GeometryRegistry *geometryRegistry() { return m_geometryRegistry.get(); }

    /**
     * @brief Get the text registry for this layer
     */
    TextRegistry *textRegistry() { return m_textRegistry.get(); }

    /**
     * @brief Set the display order for this layer
     */
    void setDisplayOrder(int32_t order);

    /**
     * @brief Get the display order for this layer
     */
    int32_t getDisplayOrder() const { return m_displayOrder; }

  public:
    bool visible = true;
    bool resetOnSpawn = false;
    ZIndexBehavior zindexBehavior;

  private:
    std::unique_ptr<GeometryRegistry> m_geometryRegistry;
    std::unique_ptr<TextRegistry> m_textRegistry;
    int32_t m_displayOrder = 0;
};

} // namespace Amethyst

#endif // AMETHYST__UI_LAYER_H
