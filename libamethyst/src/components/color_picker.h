/*
 * Color picker core: a headless saturation/value field plus hue (and alpha) bars
 */

#ifndef AMETHYST__COLOR_PICKER_H
#define AMETHYST__COLOR_PICKER_H

#include "components/ui_object.h"
#include "modules/color.h"

#include <functional>

namespace Amethyst {

class Frame;
class SliderFloat;

class ColorPicker : public UIObject {
  public:
    ColorPicker();
    virtual ~ColorPicker() = default;

    void draw(DrawContext &ctx) override;
    void arrange() override;

    /**
     * @brief Pull the bound color into the internal channels and redraw.
     * Call after assigning the value pointer or when the bound color changed externally.
     */
    void syncFromValue();

  public:
    ColorModel model = ColorModel::HSV;
    ColorPickerShape shape = ColorPickerShape::SQUARE;
    float fieldThumbRadius = 6.0f;

  protected:
    virtual void pullValue() = 0;
    virtual void pushValue() = 0;
    virtual void updateComponents() = 0;

    void onChannelsChanged();

    float m_hue = 0.0f;
    float m_saturation = 0.0f;
    union {
        float m_value = 1.0f;
        float m_lightness;
    };

    Frame *m_field = nullptr;
    Frame *m_fieldShade = nullptr;
    Frame *m_fieldThumb = nullptr;
    SliderFloat *m_hueSlider = nullptr;
};

class Color3Picker : public ColorPicker {
  public:
    Color3Picker() = default;
    virtual ~Color3Picker() = default;

  public:
    Color3 *value = nullptr;
    std::function<void(const Color3 &)> onValueChanged;

  protected:
    void pullValue() override;
    void pushValue() override;
    void updateComponents() override;
};

class Color4Picker : public ColorPicker {
  public:
    Color4Picker();
    virtual ~Color4Picker() = default;

  public:
    Color4 *value = nullptr;
    std::function<void(const Color4 &)> onValueChanged;

  protected:
    void pullValue() override;
    void pushValue() override;
    void updateComponents() override;

  private:
    float m_alpha = 1.0f;
    SliderFloat *m_alphaSlider = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__COLOR_PICKER_H
