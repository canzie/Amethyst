/*
 * Slider UI elements for adjusting numeric values
 */

#ifndef AMETHYST__SLIDER_H
#define AMETHYST__SLIDER_H

#include "components/common.h"
#include "components/ui_object.h"
#include <functional>
#include <optional>

namespace Amethyst {

struct Font;

class Slider : public UIObject {
  public:
    Slider() = default;
    Slider(Instance *parent) { setParent(parent); };
    virtual ~Slider() = default;

  public:
    Color3 sliderColor = {0.5f, 0.5f, 0.5f};
    float sliderTransparency = 0.0f;
    Color3 thumbColor = {0.8f, 0.8f, 0.8f};
    float thumbTransparency = 0.0f;
    float thumbSize = 10.0f;

    std::string label;
    Font *font = nullptr;
    Color4 labelColor = {0.0f, 0.0f, 0.0f, 1.0f};
    LabelSide labelSide = LabelSide::LEFT;
    UDim labelPadding = UDim::fromOffset(5.0f);
};

class SliderFloat : public Slider {
  public:
    SliderFloat() = default;
    SliderFloat(Instance *parent) { setParent(parent); };
    virtual ~SliderFloat() = default;

    void draw(DrawContext &ctx) override;

  public:
    float *valueRef = nullptr;
    std::function<void(float)> onValueChanged;

    std::optional<float> min;
    std::optional<float> max;
    float speed = 1.0f;
};

class SliderInt : public Slider {
  public:
    SliderInt() = default;
    SliderInt(Instance *parent) { setParent(parent); };
    virtual ~SliderInt() = default;

    void draw(DrawContext &ctx) override;

  public:
    int *valueRef = nullptr;
    std::function<void(int)> onValueChanged;

    std::optional<int> min;
    std::optional<int> max;
    float speed = 1.0f;
};

class SliderVec2 : public Slider {
  public:
    SliderVec2() = default;
    SliderVec2(Instance *parent) { setParent(parent); };
    virtual ~SliderVec2() = default;

    void draw(DrawContext &ctx) override;

  public:
    glm::vec2 *valueRef = nullptr;
    std::function<void(glm::vec2)> onValueChanged;

    std::optional<glm::vec2> min;
    std::optional<glm::vec2> max;
    float speed = 1.0f;
};

class SliderVec3 : public Slider {
  public:
    SliderVec3() = default;
    SliderVec3(Instance *parent) { setParent(parent); };
    virtual ~SliderVec3() = default;

    void draw(DrawContext &ctx) override;

  public:
    glm::vec3 *valueRef = nullptr;
    std::function<void(glm::vec3)> onValueChanged;

    std::optional<glm::vec3> min;
    std::optional<glm::vec3> max;
    float speed = 1.0f;
};

} // namespace Amethyst

#endif // AMETHYST__SLIDER_H
