/*
 * Slider UI elements for adjusting numeric values
 */

#ifndef AMETHYST__SLIDER_H
#define AMETHYST__SLIDER_H

#include "components/common.h"
#include "components/frame.h"
#include "components/properties.h"
#include "components/text_label.h"
#include "components/ui_object.h"
#include "math/math.h"
#include <functional>

namespace Amethyst {

class UIDragDetector;

class Slider : public UIObject {
  public:
    Slider();
    virtual ~Slider() = default;

    void resolveStyle() override;

    bool setSliderProperties(const SliderStyleProperties &props);
    const SliderStyleProperties &getSliderProperties() const { return m_sProps; }

    void setLabel(std::string label);
    const std::string &getLabel() const { return m_label; }

    void setValueSuffix(std::string valueSuffix);
    const std::string &getValueSuffix() const { return m_valueSuffix; }

  protected:
    SliderStyleProperties m_sProps;
    std::string m_label;
    std::string m_valueSuffix;
    TextLabel m_sideLabel;
};

class SliderFloat : public Slider {
  public:
    SliderFloat();
    virtual ~SliderFloat() = default;

    void draw(DrawContext &ctx) override;
    std::vector<Instance *> getHittableInstances() override;

  public:
    float *valueRef = nullptr;
    std::function<void(float)> onValueChanged;

    float min = 0.0f;
    float max = 100.0f;
    float speed = 1.0f;

  private:
    void updateComponents();
    std::string formatValue() const;

    Frame m_track;
    Frame m_thumb;
    TextLabel m_valueLabel;
};

class SliderInt : public Slider {
  public:
    SliderInt();
    virtual ~SliderInt() = default;

    void draw(DrawContext &ctx) override;
    std::vector<Instance *> getHittableInstances() override;

  public:
    int *valueRef = nullptr;
    std::function<void(int)> onValueChanged;

    int min = 0;
    int max = 100;
    float speed = 1.0f;

  private:
    void updateComponents();
    std::string formatValue() const;

    Frame m_track;
    Frame m_thumb;
    TextLabel m_valueLabel;
};

class SliderVec2 : public Slider {
  public:
    SliderVec2();
    virtual ~SliderVec2() = default;

    void draw(DrawContext &ctx) override;
    std::vector<Instance *> getHittableInstances() override;

  public:
    vec2 *valueRef = nullptr;
    std::function<void(vec2)> onValueChanged;

    vec2 min = vec2(0.0f);
    vec2 max = vec2(100.0f);
    float speed = 1.0f;

  private:
    void updateComponents();
    std::string formatValue(int component) const;

    Frame m_track[2];
    Frame m_thumb[2];
    TextLabel m_valueLabel[2];
};

class SliderVec3 : public Slider {
  public:
    SliderVec3();
    virtual ~SliderVec3() = default;

    void draw(DrawContext &ctx) override;
    std::vector<Instance *> getHittableInstances() override;

  public:
    vec3 *valueRef = nullptr;
    std::function<void(vec3)> onValueChanged;

    vec3 min = vec3(0.0f);
    vec3 max = vec3(100.0f);
    float speed = 1.0f;

  private:
    void updateComponents();
    std::string formatValue(int component) const;

    Frame m_track[3];
    Frame m_thumb[3];
    TextLabel m_valueLabel[3];
};

} // namespace Amethyst

#endif // AMETHYST__SLIDER_H
