/*
 * Extension that shows a tooltip once the cursor rests on a UIObject
 */

#ifndef AMETHYST__UI_TOOLTIP_H
#define AMETHYST__UI_TOOLTIP_H

#include "components/extensions/ui_extension.h"
#include "math/math.h"

#include <cstdint>
#include <functional>

namespace Amethyst {

class Tooltip;
class UIObject;
class Window;

class UITooltip : public UIExtension {
  public:
    explicit UITooltip(UIObject *owner) : UIExtension(owner) {}
    ~UITooltip() override;

    void handleMouseEnter();
    /**
     * @brief Moves a pending tooltip along with the pointer.
     * @param screenPoint The pointer in window coordinates
     */
    void handleMouseMove(vec2 screenPoint);
    void handleMouseLeave();

  public:
    float delaySeconds = 1.0f;

    /**
     * @brief Fills the surface the moment before it appears
     */
    std::function<void(Tooltip &surface)> build;

  private:
    Window *m_scheduledOn = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__UI_TOOLTIP_H
