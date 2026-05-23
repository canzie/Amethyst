#ifndef AMETHYST__COLLAPSIBLE_HEADER_H
#define AMETHYST__COLLAPSIBLE_HEADER_H

#include "components/frame.h"
#include "components/invisible_button.h"
#include "components/text_label.h"
#include "components/ui_object.h"

#include <functional>
#include <memory>
#include <string>

namespace Amethyst {

class CollapsibleHeader : public UIObject {
  public:
    CollapsibleHeader();
    ~CollapsibleHeader() override;

    void draw(DrawContext &ctx) override;
    std::vector<Instance *> getHittableInstances() override;

    void toggle();
    void expand();
    void collapse();

  public:
    bool expanded = true;

    std::string title;
    std::string fontFamily;
    float fontSize = 14.0f;
    Color4 titleColor = {1.0f, 1.0f, 1.0f, 1.0f};
    TextXAlignment titleXAlignment = TextXAlignment::LEFT;
    TextYAlignment titleYAlignment = TextYAlignment::CENTER;

    float headerHeight = 30.0f;
    Color3 headerColor = {0.25f, 0.25f, 0.28f};
    float headerTransparency = 0.0f;
    float headerCornerRadius = 0.0f;

    bool showIndicator = true;
    float indicatorSize = 10.0f;
    float indicatorPadding = 6.0f;
    Color4 indicatorColor = {0.7f, 0.7f, 0.7f, 1.0f};

    std::function<void(bool)> onToggled;

  private:
    std::unique_ptr<Frame> m_headerBackground;
    std::unique_ptr<InvisibleButton> m_headerButton;
    std::unique_ptr<Frame> m_indicator;
    std::unique_ptr<TextLabel> m_titleLabel;
};

} // namespace Amethyst

#endif // AMETHYST__COLLAPSIBLE_HEADER_H
