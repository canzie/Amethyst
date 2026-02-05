#ifndef AMETHYST__UI_ASPECT_RATIO_CONSTRAINT_H
#define AMETHYST__UI_ASPECT_RATIO_CONSTRAINT_H

#include "components/extensions/ui_extension.h"

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

    void apply();

  public:
    float aspectRatio = 1.0f;
    DominantAxis dominantAxis = DominantAxis::WIDTH;
};

} // namespace Amethyst

#endif // AMETHYST__UI_ASPECT_RATIO_CONSTRAINT_H
