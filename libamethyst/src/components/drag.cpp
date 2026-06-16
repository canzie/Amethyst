#include "components/drag.h"

#include "components/input_interface.h"
#include "components/number_input.h"
#include "components/window.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "utils/am_assert.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace Amethyst {

static constexpr float SCRUB_THRESHOLD = 4.0f;

template <typename T> static std::string s_formatNumber(const std::string &format, T value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), format.c_str(), value);
    return std::string(buffer);
}

// Saturating double->int64 conversion: a bare cast/llround is UB outside int64's range,
// and 2^63 is the smallest power of two not representable as int64, so it is the threshold.
static int64_t s_saturateToInt64(double v)
{
    if (std::isnan(v)) {
        return 0;
    }
    if (v >= 9223372036854775808.0) {
        return INT64_MAX;
    }
    if (v <= -9223372036854775808.0) {
        return INT64_MIN;
    }
    return static_cast<int64_t>(std::llround(v));
}

Drag::Drag()
{
    m_field = add<NumberInput>();
    m_field->setBaseProperties({
        .interactable = false,
        .position = UDim2::fromOffset(0, 0),
        .size = UDim2::fromScale(1.0f, 1.0f),
    });

    TextInputStyleProperties fieldStyle;
    fieldStyle.readOnly = true;
    fieldStyle.text.textXAlignment = TextXAlignment::CENTER;
    fieldStyle.text.textYAlignment = TextYAlignment::CENTER;
    m_field->setTextInputProperties(fieldStyle);

    m_field->onFocusLost = [this]() { exitEdit(); };
    m_field->onEnterPressed = [this]() { m_field->loseFocus(); };

    resolveStyle();
}

Drag::~Drag()
{
    if (m_state == State::SCRUBBING) {
        InputInterface::setCursorLocked(false);
    }
}

void Drag::resolveStyle()
{
    auto &style = Style::instance();
    setBaseStyleProperties(style.getBaseStyle(ComponentType::DRAG, getClasses()));
    setDragProperties(style.getDragStyle(ComponentType::DRAG, getClasses()));
}

bool Drag::setDragProperties(const DragStyleProperties &props)
{
    bool changed = m_dProps.apply(props);
    if (changed) {
        TextInputStyleProperties fieldStyle;
        fieldStyle.text = m_dProps.text;
        fieldStyle.text.textXAlignment = TextXAlignment::CENTER;
        fieldStyle.text.textYAlignment = TextYAlignment::CENTER;
        fieldStyle.cursorColor = m_dProps.text.textColor;
        m_field->setTextInputProperties(fieldStyle);
        markDirty();
    }
    return changed;
}

void Drag::setFormat(std::string format)
{
    if (m_format != format) {
        m_format = std::move(format);
        markDirty();
    }
}

double Drag::scrubValue(double current, float pixelDelta, double speed) const
{
    if (scale == ValueScale::LOGARITHMIC) {
        return current * std::exp(static_cast<double>(pixelDelta) * speed);
    }
    return current + static_cast<double>(pixelDelta) * speed;
}

EventResult Drag::onInputBegan(const InputObject &input)
{
    if (input.type != InputType::MOUSE_BUTTON_1) {
        return UIObject::onInputBegan(input);
    }
    if (m_state == State::EDITING) {
        return EventResult::PROPAGATE;
    }
    m_pressX = input.position.x;
    m_lastX = m_pressX;
    m_state = State::PENDING;
    if (auto *window = getWindow()) {
        window->captureMouse(this);
    }
    return EventResult::CONSUMED;
}

EventResult Drag::onMouseMoved(int32_t x, int32_t y)
{
    (void)y;
    if (m_state == State::PENDING && std::abs(static_cast<float>(x) - m_pressX) > SCRUB_THRESHOLD) {
        m_state = State::SCRUBBING;
        // Hide and unlock the cursor for unbounded scrubbing. This switches the coordinate
        // space starting with the next event, so skip this one and reseed the reference there
        // rather than scrubbing a bogus jump across the switch.
        InputInterface::setCursorLocked(true);
        m_reseedScrub = true;
        return EventResult::CONSUMED;
    }
    if (m_state == State::SCRUBBING) {
        if (m_reseedScrub) {
            m_lastX = static_cast<float>(x);
            m_reseedScrub = false;
        } else {
            float delta = static_cast<float>(x) - m_lastX;
            m_lastX = static_cast<float>(x);
            if (delta != 0.0f) {
                applyScrub(delta);
            }
        }
    }
    return EventResult::CONSUMED;
}

EventResult Drag::onInputEnded(const InputObject &input)
{
    if (input.type != InputType::MOUSE_BUTTON_1) {
        return UIObject::onInputEnded(input);
    }
    if (auto *window = getWindow()) {
        window->releaseMouse(this);
    }
    if (m_state == State::PENDING) {
        enterEdit();
    } else if (m_state == State::SCRUBBING) {
        InputInterface::setCursorLocked(false);
        m_state = State::IDLE;
    }
    return EventResult::CONSUMED;
}

void Drag::enterEdit()
{
    m_state = State::EDITING;
    configureField();

    TextInputStyleProperties editable;
    editable.readOnly = false;
    m_field->setTextInputProperties(editable);
    m_field->setBaseProperties({.interactable = true});
    m_field->setText(formatValue());
    m_field->focus();
    m_field->selectAll();
    markDirty();
}

void Drag::exitEdit()
{
    if (m_state != State::EDITING) {
        return;
    }
    commitFromField();
    m_state = State::IDLE;

    TextInputStyleProperties readOnly;
    readOnly.readOnly = true;
    m_field->setTextInputProperties(readOnly);
    m_field->setBaseProperties({.interactable = false});
    markDirty();
}

void Drag::updateComponents()
{
    if (m_state != State::EDITING) {
        std::string text = formatValue();
        if (m_field->getText() != text) {
            m_field->setText(text);
        }
    }
}

void Drag::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        updateComponents();

        InstanceData bgData = createInstanceData();
        bgData.setPrimitiveType(PRIMITIVE_RECT);
        if (m_geometryAlloc == nullptr) {
            m_geometryAlloc = ctx.geometry->submit(bgData);
        } else {
            ctx.geometry->update(*m_geometryAlloc, bgData);
        }
    }

    vec4 childClip = computeChildClipRect();

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteContentSize, absoluteContentPosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

DragFloat::DragFloat()
{
    setFormat("%.3f");
    configureField();
}

std::string DragFloat::formatValue() const
{
    return s_formatNumber(getFormat(), value ? *value : 0.0);
}

void DragFloat::applyScrub(float pixelDelta)
{
    if (!value) {
        return;
    }
    *value = clampValue(scrubValue(*value, pixelDelta, speed));
    if (onValueChanged) {
        onValueChanged(*value);
    }
    markDirty();
}

void DragFloat::commitFromField()
{
    if (!value) {
        return;
    }
    *value = clampValue(m_field->asDouble());
    if (onValueChanged) {
        onValueChanged(*value);
    }
    markDirty();
}

void DragFloat::configureField()
{
    m_field->allowDecimal = true;
    m_field->allowNegative = true;
}

double DragFloat::clampValue(double v) const
{
    AM_ASSERT(min <= max, "DragFloat min must be <= max");
    if (std::isnan(v)) {
        return value ? *value : 0.0;
    }
    if (v < min) {
        v = min;
    }
    if (v > max) {
        v = max;
    }
    return v;
}

DragInt::DragInt()
{
    setFormat("%lld");
    configureField();
}

std::string DragInt::formatValue() const
{
    return s_formatNumber(getFormat(), static_cast<long long>(value ? *value : 0));
}

void DragInt::applyScrub(float pixelDelta)
{
    if (!value) {
        return;
    }
    *value = clampValue(s_saturateToInt64(scrubValue(static_cast<double>(*value), pixelDelta, static_cast<double>(speed))));
    if (onValueChanged) {
        onValueChanged(*value);
    }
    markDirty();
}

void DragInt::commitFromField()
{
    if (!value) {
        return;
    }
    *value = clampValue(m_field->asInt64());
    if (onValueChanged) {
        onValueChanged(*value);
    }
    markDirty();
}

void DragInt::configureField()
{
    m_field->allowDecimal = false;
    m_field->allowNegative = true;
}

int64_t DragInt::clampValue(int64_t v) const
{
    AM_ASSERT(min <= max, "DragInt min must be <= max");
    if (v < min) {
        v = min;
    }
    if (v > max) {
        v = max;
    }
    return v;
}

} // namespace Amethyst
