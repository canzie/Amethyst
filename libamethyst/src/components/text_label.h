/*
 * Text display label
 */

#ifndef AMETHYST__TEXT_LABEL_H
#define AMETHYST__TEXT_LABEL_H

#include "components/instance.h"
#include "components/ui_label.h"
#include <string>

namespace Amethyst {

enum class TextXAlignment : uint8_t {
    LEFT,
    CENTER,
    RIGHT,
};

enum class TextYAlignment : uint8_t {
    TOP,
    CENTER,
    BOTTOM,
};

enum class TextTruncate : uint8_t {
    NONE,
    AT_END,
    SPLIT_WORD,
};

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
    float lineHeight = 1.0f;

  private:
    uint32_t m_textAllocationIndex = UINT32_MAX;
};

} // namespace Amethyst

#endif // AMETHYST__TEXT_LABEL_H
