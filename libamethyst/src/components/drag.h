/*
 * Drag widgets: scrub a numeric value by dragging, click to type an exact value
 */

#ifndef AMETHYST__DRAG_H
#define AMETHYST__DRAG_H

#include "components/properties.h"
#include "components/ui_object.h"
#include "utils/number_format.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>

namespace Amethyst {

struct DrawContext;
class NumberInput;

class Drag : public UIObject {
  public:
    virtual ~Drag();

    void draw(DrawContext &ctx) override;
    void resolveStyle() override;

    bool setDragProperties(const DragStylePropertiesArgs &props);
    const DragStyleProperties &getDragProperties() const { return m_dProps; }

    /**
     * @brief Set the printf-style format of the shown value. It must hold exactly one conversion
     * matching the value type (%d/%i/%u/%x... for DragInt, %f/%e/%g/%a for DragFloat); any length
     * modifier is fixed up. An invalid format logs a warning and the default is used.
     * @param format The format, e.g. "%.2f m" or "%d px"
     */
    void setFormat(std::string format);
    const std::string &getFormat() const { return m_format; }

    /**
     * @brief Format the bound value into the text shown when not editing.
     */
    virtual std::string formatValue() const = 0;

    /**
     * @brief Map a value through the scale/speed for a scrub of pixelDelta pixels.
     *
     * Linear adds pixelDelta * speed. Logarithmic works on the magnitude and keeps the sign: each
     * pixel scales |value| by about e^speed, and inside logLinearZone of zero it turns linear, so
     * the value can leave 0, cross it, and always moves right for a rightward drag.
     * @param current The current value
     * @param pixelDelta Mouse movement in pixels since the last scrub step
     * @param speed Value change per pixel (linear) or log-magnitude change per pixel (logarithmic)
     * @return The scrubbed value, before any clamping the subclass applies
     */
    double scrubValue(double current, float pixelDelta, double speed) const;

  public:
    ValueScale scale = ValueScale::LINEAR;

    // half-width of the linear band around zero in logarithmic scale; must be positive
    double logLinearZone = 1.0;

  protected:
    Drag(NumberKind kind, std::string defaultFormat);

    /**
     * @brief Apply a horizontal scrub of pixelDelta pixels to the bound value.
     * @param pixelDelta Mouse movement in pixels since the last scrub step
     */
    virtual void applyScrub(float pixelDelta) = 0;

    /**
     * @brief Read the edit field back into the bound value when an edit commits.
     */
    virtual void commitFromField() = 0;

    /**
     * @brief Configure the edit field's numeric grammar for this value type.
     */
    virtual void configureField() = 0;

    /**
     * @brief Called when a scrub gesture starts, before its first applyScrub().
     */
    virtual void beginScrub() {}

    EventResult onInputBegan(const InputObject &input) override;
    EventResult onInputEnded(const InputObject &input) override;
    EventResult onMouseMoved(int32_t x, int32_t y) override;

    NumberInput *m_field = nullptr;
    NumberFormat m_numberFormat;

  private:
    void syncFieldStyle();
    void updateComponents();
    void enterEdit();
    void exitEdit();

    enum class State {
        IDLE,
        PENDING,
        SCRUBBING,
        EDITING,
    };

    DragStyleProperties m_dProps;
    std::string m_format;
    std::string m_defaultFormat;
    State m_state = State::IDLE;
    float m_pressX = 0.0f;
    float m_lastX = 0.0f;
    bool m_reseedScrub = false;
};

class DragFloat : public Drag {
  public:
    DragFloat();
    virtual ~DragFloat() = default;

  public:
    double *value = nullptr; // 8-byte binding
    float *valueF = nullptr; // 4-byte binding, used when value is null
    std::function<void(double)> onValueChanged;

    double speed = 1.0;
    double min = std::numeric_limits<double>::lowest();
    double max = std::numeric_limits<double>::max();

    std::string formatValue() const override;

    /**
     * @brief Scrub the bound value as a drag of pixelDelta pixels would.
     */
    void applyScrub(float pixelDelta) override;

  protected:
    void commitFromField() override;
    void configureField() override;

  private:
    double clampValue(double v) const;

    bool hasBinding() const { return value != nullptr || valueF != nullptr; }
    double readBinding() const { return value != nullptr ? *value : (valueF != nullptr ? static_cast<double>(*valueF) : 0.0); }
    void writeBinding(double v)
    {
        if (value != nullptr) {
            *value = v;
        } else if (valueF != nullptr) {
            *valueF = static_cast<float>(v);
        }
    }
};

class DragInt : public Drag {
  public:
    DragInt();
    virtual ~DragInt() = default;

  public:
    int64_t *value = nullptr; // 8-byte binding
    int32_t *valueI = nullptr; // 4-byte binding, used when value is null
    std::function<void(int64_t)> onValueChanged;

    // fractional speeds accumulate across pixels instead of rounding every step away
    double speed = 1.0;
    int64_t min = std::numeric_limits<int64_t>::min();
    int64_t max = std::numeric_limits<int64_t>::max();

    std::string formatValue() const override;

    /**
     * @brief Scrub the bound value as a drag of pixelDelta pixels would.
     */
    void applyScrub(float pixelDelta) override;

  protected:
    void beginScrub() override;
    void commitFromField() override;
    void configureField() override;

  private:
    int64_t clampValue(int64_t v) const;

    // unrounded value of the running scrub; reseeded when the binding changes underneath it
    double m_scrubExact = 0.0;
    int64_t m_scrubWritten = 0;
    bool m_scrubSeeded = false;

    bool hasBinding() const { return value != nullptr || valueI != nullptr; }
    int64_t readBinding() const { return value != nullptr ? *value : (valueI != nullptr ? static_cast<int64_t>(*valueI) : 0); }
    void writeBinding(int64_t v)
    {
        if (value != nullptr) {
            *value = v;
        } else if (valueI != nullptr) {
            *valueI = static_cast<int32_t>(std::clamp<int64_t>(v, INT32_MIN, INT32_MAX));
        }
    }
};

} // namespace Amethyst

#endif // AMETHYST__DRAG_H
