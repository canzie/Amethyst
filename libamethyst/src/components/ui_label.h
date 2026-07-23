/*
 * Base class for label UI elements
 */

#ifndef AMETHYST__UI_LABEL_H
#define AMETHYST__UI_LABEL_H

#include "components/common.h"
#include "components/ui_object.h"

namespace Amethyst {

class UILabel : public UIObject {
  public:
    UILabel() = default;
    virtual ~UILabel() = default;
};

} // namespace Amethyst

#endif // AMETHYST__UI_LABEL_H
