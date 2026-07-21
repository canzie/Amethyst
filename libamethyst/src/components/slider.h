/*
 * Slider UI elements for adjusting numeric values
 */

#ifndef AMETHYST__SLIDER_H
#define AMETHYST__SLIDER_H

#include "components/common.h"
#include "components/frame.h"
#include "components/properties.h"
#include "components/shape.h"
#include "components/text_label.h"
#include "components/ui_object.h"
#include <functional>
#include <string>

namespace Amethyst {

class Slider : public UIObject {
  public:
    Slider();
    virtual ~Slider() = default;

    void draw(DrawContext &ctx) override;
    void arrange() override;
    std::vector<Instance *> getHittableInstances() override;
    void resolveStyle() override;

    bool setSliderProperties(const SliderStyleProperties &props);
    const SliderStyleProperties &getSliderProperties() const { return m_sProps; }

    void setFormat(std::string format);
    const std::string &getFormat() const { return m_format; }

  public:
    ValueScale scale = ValueScale::LINEAR;
    ShapeKind thumbShape = ShapeKind::RECT;

  protected:
    virtual void updateComponents() = 0;

    /**
     * @brief Lay out the track, thumb and value text, then (re)wire the drag detector
     * @param normalizedPos Thumb position along the track in [0, 1]
     * @param thumbWidth Resolved thumb width in pixels
     * @param valueText Formatted value to display over the track
     * @param applyNormalized Maps a [0, 1] cursor position back onto the bound value
     */
    void layoutTrack(float normalizedPos, float thumbWidth, const std::string &valueText,
                     const std::function<void(float)> &applyNormalized);

    SliderStyleProperties m_sProps;
    std::string m_format;

    Frame *m_track = nullptr;
    Frame *m_fill = nullptr;
    Shape *m_thumb = nullptr;
    TextLabel *m_valueLabel = nullptr;
};

class SliderFloat : public Slider {
  public:
    SliderFloat();
    virtual ~SliderFloat() = default;

  public:
    float *value = nullptr;
    std::function<void(float)> onValueChanged;

    float min = 0.0f;
    float max = 100.0f;

  protected:
    void updateComponents() override;

  private:
    std::string formatValue() const;
};

class SliderInt : public Slider {
  public:
    SliderInt();
    virtual ~SliderInt() = default;

  public:
    int *value = nullptr;
    std::function<void(int)> onValueChanged;

    int min = 0;
    int max = 100;

  protected:
    void updateComponents() override;

  private:
    std::string formatValue() const;
};

} // namespace Amethyst

#endif // AMETHYST__SLIDER_H
