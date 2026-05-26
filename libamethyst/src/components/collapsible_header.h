#ifndef AMETHYST__COLLAPSIBLE_HEADER_H
#define AMETHYST__COLLAPSIBLE_HEADER_H

#include "components/frame.h"
#include "components/invisible_button.h"
#include "components/properties.h"
#include "components/ui_object.h"

#include <functional>
#include <memory>

namespace Amethyst {

class CollapsibleHeader : public UIObject {
  public:
    CollapsibleHeader();
    CollapsibleHeader(std::unique_ptr<UIObject> customIndicator, std::unique_ptr<UIObject> customHeader);
    ~CollapsibleHeader() override;

    void draw(DrawContext &ctx) override;
    std::vector<Instance *> getHittableInstances() override;

    void toggle();
    void expand();
    void collapse();

    bool setCollapsibleHeaderProperties(const CollapsibleHeaderProperties &props);
    const CollapsibleHeaderProperties &getCollapsibleHeaderProperties() const { return m_chProps; }

    CollapsibleHeader &header(std::function<void(Frame &)> fn);
    CollapsibleHeader &indicator(std::function<void(UIObject &)> fn);

  public:
    std::function<void(bool)> onToggled;

  protected:
    CollapsibleHeaderProperties m_chProps;

  private:
    std::unique_ptr<Frame> m_headerBackground;
    std::unique_ptr<InvisibleButton> m_headerButton;
    UIObject *m_indicator = nullptr;
    // in case of not using a custom header the title will be used and this will be a text_label
    UIObject *m_headerContent = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__COLLAPSIBLE_HEADER_H
