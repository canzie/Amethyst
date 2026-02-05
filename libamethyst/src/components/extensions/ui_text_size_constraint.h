#ifndef AMETHYST__UI_TEXT_SIZE_CONSTRAINT_H
#define AMETHYST__UI_TEXT_SIZE_CONSTRAINT_H

#include "components/extensions/ui_extension.h"

namespace Amethyst {

class Instance;

class UITextSizeConstraint : public UIExtension {
  public:
    explicit UITextSizeConstraint(UIObject *owner) : UIExtension(owner) {}
    virtual ~UITextSizeConstraint() = default;

    void apply();

  public:
    float maxTextSize;
    float minTextSize;
};

} // namespace Amethyst

#endif // AMETHYST__UI_TEXT_SIZE_CONSTRAINT_H
