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
#include "utils/number_format.h"
#include <functional>
#include <string>

namespace Amethyst {

class Slider : public UIObject {
  public:
    virtual ~Slider() = default;

    void draw(DrawContext &ctx) override;
    void arrange() override;
    void resolveStyle() override;

    bool setSliderProperties(const SliderStylePropertiesArgs &props);
    const SliderStyleProperties &getSliderProperties() const { return m_sProps; }

    /**
     * @brief Set the printf-style format of the value text. It must hold exactly one conversion
     * matching the value type (%d/%i/%u/%x... for SliderInt, %f/%e/%g/%a for SliderFloat); any
     * length modifier is fixed up. An invalid format logs a warning and the default is used.
     * @param format The format, e.g. "%.1f%%" or "%d px"
     */
    void setFormat(std::string format);
    const std::string &getFormat() const { return m_format; }

    /**
     * @brief The text shown over the track for the current value.
     */
    virtual std::string formatValue() const = 0;

  public:
    ValueScale scale = ValueScale::LINEAR;
    ShapeKind thumbShape = ShapeKind::RECT;

  protected:
    Slider(NumberKind kind, std::string defaultFormat);

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
    NumberFormat m_numberFormat;
    std::string m_defaultFormat;

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

    // swapped when reversed; a NaN bound falls back to 0 (min) or min (max)
    float min = 0.0f;
    float max = 100.0f;

    std::string formatValue() const override;

  protected:
    void updateComponents() override;
};

class SliderInt : public Slider {
  public:
    SliderInt();
    virtual ~SliderInt() = default;

  public:
    int *value = nullptr;
    std::function<void(int)> onValueChanged;

    // swapped when reversed
    int min = 0;
    int max = 100;

    std::string formatValue() const override;

  protected:
    void updateComponents() override;
};

} // namespace Amethyst

#endif // AMETHYST__SLIDER_H
