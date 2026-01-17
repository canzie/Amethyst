/*
 * Text display label
 */

#ifndef AMETHYST__TEXT_LABEL_H
#define AMETHYST__TEXT_LABEL_H

#include "components/instance.h"
#include "components/ui_label.h"
#include <string>

namespace Amethyst {

class TextLabel : public UILabel {
  public:
    TextLabel() = default;
    TextLabel(Instance *parent) { setParent(parent); }
    virtual ~TextLabel() = default;

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

  private:
    uint32_t m_textAllocationIndex = UINT32_MAX;
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_LABEL_H
