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
    // labels are display content: they still get hover (styles, tooltips) but every event carries on
    // to what is behind them or above them, so text over a row or a button never swallows its input
    UILabel() { propagate(INTERACTION_CATEGORY_ALL); }
    virtual ~UILabel() = default;
};

} // namespace Amethyst

#endif // AMETHYST__UI_LABEL_H
