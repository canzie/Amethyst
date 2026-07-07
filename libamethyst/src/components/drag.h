/*
 * Drag widgets: scrub a numeric value by dragging, click to type an exact value
 */

#ifndef AMETHYST__DRAG_H
#define AMETHYST__DRAG_H

#include "components/properties.h"
#include "components/ui_object.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <string>

namespace Amethyst {

struct DrawContext;
class NumberInput;

class Drag : public UIObject {
  public:
    Drag();
    virtual ~Drag();

    void draw(DrawContext &ctx) override;
    void resolveStyle() override;

    bool setDragProperties(const DragStyleProperties &props);
    const DragStyleProperties &getDragProperties() const { return m_dProps; }

    void setFormat(std::string format);
    const std::string &getFormat() const { return m_format; }

  public:
    ValueScale scale = ValueScale::LINEAR;

  protected:
    /**
     * @brief Format the bound value into the text shown when not editing.
     */
    virtual std::string formatValue() const = 0;

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
     * @brief Map a value through the scale/speed for a scrub of pixelDelta pixels.
     * @param current The current value
     * @param pixelDelta Mouse movement in pixels since the last scrub step
     * @param speed Value change per pixel scrubbed
     * @return The scrubbed value, before any clamping the subclass applies
     */
    double scrubValue(double current, float pixelDelta, double speed) const;

    EventResult onInputBegan(const InputObject &input) override;
    EventResult onInputEnded(const InputObject &input) override;
    EventResult onMouseMoved(int32_t x, int32_t y) override;

    NumberInput *m_field = nullptr;

  private:
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

  protected:
    std::string formatValue() const override;
    void applyScrub(float pixelDelta) override;
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

    int64_t speed = 1;
    int64_t min = std::numeric_limits<int64_t>::min();
    int64_t max = std::numeric_limits<int64_t>::max();

  protected:
    std::string formatValue() const override;
    void applyScrub(float pixelDelta) override;
    void commitFromField() override;
    void configureField() override;

  private:
    int64_t clampValue(int64_t v) const;

    bool hasBinding() const { return value != nullptr || valueI != nullptr; }
    int64_t readBinding() const { return value != nullptr ? *value : (valueI != nullptr ? static_cast<int64_t>(*valueI) : 0); }
    void writeBinding(int64_t v)
    {
        if (value != nullptr) {
            *value = v;
        } else if (valueI != nullptr) {
            *valueI = static_cast<int32_t>(v);
        }
    }
};

} // namespace Amethyst

#endif // AMETHYST__DRAG_H
