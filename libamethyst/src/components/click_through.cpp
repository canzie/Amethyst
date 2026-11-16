#include "components/click_through.h"

#include "components/container.h"
#include "components/frame.h"
#include "components/ui_label.h"
#include "components/ui_object.h"

#include <typeinfo>

namespace Amethyst {

static bool s_isPassive(UIObject &obj)
{
    // exact types only: Frame subclasses (popups, editors) carry their own input handling
    const std::type_info &type = typeid(obj);
    return type == typeid(Frame) || type == typeid(Container) || dynamic_cast<UILabel *>(&obj) != nullptr;
}

void passClicksThroughPassiveContent(Instance *root)
{
    if (root == nullptr) {
        return;
    }
    UIObject *obj = root->asUiObject();
    if (obj == nullptr || !s_isPassive(*obj)) {
        return;
    }

    obj->propagate(INTERACTION_CATEGORY_CLICK);
    for (auto &child : obj->getChildren()) {
        passClicksThroughPassiveContent(child.get());
    }
}

} // namespace Amethyst
