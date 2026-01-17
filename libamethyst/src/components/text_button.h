/*
 * Text button
 */

#ifndef AMETHYST__TEXT_BUTTON_H
#define AMETHYST__TEXT_BUTTON_H

#include "components/common.h"
#include "components/text_label.h"
#include "components/ui_button.h"
#include "glm/fwd.hpp"

namespace Amethyst {

struct Font;

class TextButton : public UIButton {
  public:
    TextButton(Instance *parent) { setParent(parent); };
    virtual ~TextButton() = default;

    void draw(DrawContext &ctx) override;

  public:
    std::string text;
    Font *font = nullptr; // make this a ref when used. maybe;
    float lineHeight;
    TextDirection textDirection = TextDirection::LEFT_TO_RIGHT;
    bool textScaled = false;
    Color4 textColor = {0.0f, 0.0f, 0.0f, 1.0f};
    TextXAlignment textXAlignment = TextXAlignment::LEFT;
    TextYAlignment textYAlignment = TextYAlignment::TOP;
    TextTruncate textTruncate = TextTruncate::NONE;
    bool richText = false;
    bool textWrapped = false;

  private:
    glm::vec2 textBounds;
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_BUTTON_H
