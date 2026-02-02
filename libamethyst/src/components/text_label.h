/*
 * Text display label
 */

#ifndef AMETHYST__TEXT_LABEL_H
#define AMETHYST__TEXT_LABEL_H

#include "components/instance.h"
#include "components/ui_label.h"

#include <string>
#include <vector>

namespace Amethyst {

struct GeometryAllocation;

class TextLabel : public UILabel {
  public:
    TextLabel();
    TextLabel(Instance *parent);
    virtual ~TextLabel();

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
    std::vector<GeometryAllocation *> m_textAllocations;
    glm::vec2 m_textSize = {0.0f, 0.0f};
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_LABEL_H
