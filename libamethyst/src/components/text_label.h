/*
 * Text display label
 */

#ifndef AMETHYST__TEXT_LABEL_H
#define AMETHYST__TEXT_LABEL_H

#include "components/instance.h"
#include "components/properties.h"
#include "components/ui_label.h"

#include <string>
#include <vector>

namespace Amethyst {

struct GeometryAllocation;

class TextLabel : public UILabel {
  public:
    TextLabel();
    virtual ~TextLabel();

    void draw(DrawContext &ctx) override;

    bool setTextProperties(const TextProperties &props);
    const TextProperties &getTextProperties() const { return m_textProps; }

    glm::vec2 getTextSize() const { return m_textSize; }

  protected:
    TextProperties m_textProps;

  private:
    std::vector<GeometryAllocation *> m_textAllocations;
    glm::vec2 m_textSize = {0.0f, 0.0f};
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_LABEL_H
