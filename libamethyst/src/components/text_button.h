/*
 * Text button
 */

#ifndef AMETHYST__TEXT_BUTTON_H
#define AMETHYST__TEXT_BUTTON_H

#include "components/common.h"
#include "components/ui_button.h"

#include <string>
#include <vector>

namespace Amethyst {

struct Font;
struct GeometryAllocation;

class TextButton : public UIButton {
  public:
    TextButton();
    virtual ~TextButton();

    void draw(DrawContext &ctx) override;

  public:
    std::string text;
    std::string fontFamily;
    float fontSize = 14.0f;
    Color4 textColor = {0.0f, 0.0f, 0.0f, 1.0f};
    TextXAlignment textXAlignment = TextXAlignment::LEFT;
    TextYAlignment textYAlignment = TextYAlignment::TOP;
    TextTruncate textTruncate = TextTruncate::NONE;
    bool richText = false;
    bool textWrapped = false;
    bool textScaled = false;
    float lineHeight = 1.0f;
    float strokeThickness = 0.0f;
    Color4 strokeColor = {0.0f, 0.0f, 0.0f, 1.0f};

    glm::vec2 getTextSize() const { return m_textSize; }

  private:
    glm::vec2 m_textSize = {0.0f, 0.0f};
    std::vector<GeometryAllocation *> m_textAllocations;
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_BUTTON_H
