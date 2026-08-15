#ifndef AMETHYST__UI_ASPECT_RATIO_CONSTRAINT_H
#define AMETHYST__UI_ASPECT_RATIO_CONSTRAINT_H

#include "components/extensions/ui_extension.h"

#include "math/math.h"

namespace Amethyst {

class Instance;

enum class DominantAxis {
    WIDTH,
    HEIGHT
};

class UIAspectRatioConstraint : public UIExtension {
  public:
    explicit UIAspectRatioConstraint(UIObject *owner) : UIExtension(owner) {}
    virtual ~UIAspectRatioConstraint() = default;

    /**
     * @brief A size with this constraint honoured
     * @param size The size to constrain
     * @return The constrained size
     */
    vec2 constrain(vec2 size) const;

  public:
    float aspectRatio = 1.0f;
    DominantAxis dominantAxis = DominantAxis::WIDTH;
};

} // namespace Amethyst

#endif // AMETHYST__UI_ASPECT_RATIO_CONSTRAINT_H
