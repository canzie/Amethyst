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

    bool setTextProperties(const TextProperties &props);
    const TextProperties &getTextProperties() const { return m_textProps; }

    glm::vec2 getTextSize() const { return m_textSize; }

  protected:
    TextProperties m_textProps;

  private:
    glm::vec2 m_textSize = {0.0f, 0.0f};
    std::vector<GeometryAllocation *> m_textAllocations;
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_BUTTON_H
