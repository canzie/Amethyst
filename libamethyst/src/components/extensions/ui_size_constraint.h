#ifndef AMETHYST__UI_SIZE_CONSTRAINT_H
#define AMETHYST__UI_SIZE_CONSTRAINT_H

#include "components/extensions/ui_extension.h"

#include "math/math.h"

namespace Amethyst {

class Instance;

class UISizeConstraint : public UIExtension {
  public:
    explicit UISizeConstraint(UIObject *owner) : UIExtension(owner) {}
    virtual ~UISizeConstraint() = default;

    /**
     * @brief A size with this constraint honoured
     * @param size The size to constrain
     * @return The constrained size
     */
    vec2 constrain(vec2 size) const { return clamp(size, minSize, maxSize); }

  public:
    vec2 maxSize;
    vec2 minSize;
};

} // namespace Amethyst

#endif // AMETHYST__UI_SIZE_CONSTRAINT_H
