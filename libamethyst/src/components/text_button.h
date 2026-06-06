/*
 * Text button
 */

#ifndef AMETHYST__TEXT_BUTTON_H
#define AMETHYST__TEXT_BUTTON_H

#include "components/common.h"
#include "components/properties.h"
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

    bool setTextStyleProperties(const TextStyleProperties &props);
    const TextStyleProperties &getTextStyleProperties() const { return m_textStyle; }

    void setText(std::string text);
    const std::string &getText() const { return m_text; }

    glm::vec2 getTextSize() const { return m_textSize; }

  protected:
    TextStyleProperties m_textStyle;
    std::string m_text;

  private:
    glm::vec2 m_textSize = {0.0f, 0.0f};
    std::vector<GeometryAllocation *> m_textAllocations;
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_BUTTON_H
