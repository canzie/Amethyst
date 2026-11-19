#include "components/drag.h"

#include "components/input_interface.h"
#include "components/number_input.h"
#include "components/window.h"
#include "logging/log.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "utils/am_assert.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace Amethyst {

static constexpr float SCRUB_THRESHOLD = 4.0f;

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

Drag::Drag(NumberKind kind, std::string defaultFormat) : m_format(defaultFormat), m_defaultFormat(std::move(defaultFormat))
{
    if (auto parsed = NumberFormat::parse(m_defaultFormat, kind); parsed.has_value()) {
        m_numberFormat = *parsed;
    }

    m_field = add<NumberInput>();
    m_field->setClasses(getClasses());
    m_field->setBaseProperties({
        .interactable = false,
        .position = UDim2::fromOffset(0, 0),
        .size = UDim2::fromScale(1.0f, 1.0f),
    });
    m_field->setBaseStyleProperties({.backgroundTransparency = 1.0f, .borderPixelSize = 0.0f});

    TextInputStylePropertiesArgs fieldStyle;
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
    resolveBaseStyle(ComponentType::DRAG);
    m_field->setClasses(getClasses());
    DragStyleProperties resolved = Style::instance().getDragStyle(ComponentType::DRAG, getClasses(), effectiveGuiState());
    if (m_dProps.apply(resolved)) {
        syncFieldStyle();
        markDirty();
    }
}

void Drag::syncFieldStyle()
{
    // the field draws the text, so the drag's resolved text style has to reach it
    TextInputStylePropertiesArgs fieldStyle;
    fieldStyle.text = m_dProps.text;
    fieldStyle.text.textXAlignment = TextXAlignment::CENTER;
    fieldStyle.text.textYAlignment = TextYAlignment::CENTER;
    fieldStyle.cursorColor = m_dProps.text.textColor;
    m_field->setTextInputProperties(fieldStyle);
}

bool Drag::setDragProperties(const DragStylePropertiesArgs &props)
{
    bool changed = m_dProps.apply(props);
    if (changed) {
        syncFieldStyle();
        markDirty();
    }
    return changed;
}

void Drag::setFormat(std::string format)
{
    if (m_format == format) {
        return;
    }
    std::optional<NumberFormat> parsed = NumberFormat::parse(format, m_numberFormat.kind());
    if (!parsed.has_value()) {
        AM_LOG_WARN("Drag format '{}' needs exactly one {} conversion; using '{}'", format,
                    m_numberFormat.kind() == NumberKind::INTEGER ? "integer" : "floating-point", m_defaultFormat);
        parsed = NumberFormat::parse(m_defaultFormat, m_numberFormat.kind());
    }
    if (parsed.has_value()) {
        m_numberFormat = *parsed;
    }
    m_format = std::move(format);
    markDirty();
}

double Drag::scrubValue(double current, float pixelDelta, double speed) const
{
    double step = static_cast<double>(pixelDelta) * speed;
    if (scale != ValueScale::LOGARITHMIC) {
        return current + step;
    }
    // symmetric log: u = sign(v) * log1p(|v| / zone) is linear near zero and logarithmic far from
    // it, so a pixel multiplies large magnitudes by about e^speed yet 0 and negatives still move
    double zone = logLinearZone > 0.0 ? logLinearZone : 1.0;
    double u = std::copysign(std::log1p(std::abs(current) / zone), current) + step;
    return std::copysign(zone * std::expm1(std::abs(u)), u);
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
    setGuiState(static_cast<uint16_t>(getGuiState() | GUI_STATE_PRESSED));
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
        beginScrub();
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
    setGuiState(static_cast<uint16_t>(getGuiState() & ~GUI_STATE_PRESSED));
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

    TextInputStylePropertiesArgs editable;
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

    TextInputStylePropertiesArgs readOnly;
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
        pushData(ctx.geometry, bgData);
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

DragFloat::DragFloat() : Drag(NumberKind::FLOATING, "%.3f")
{
    configureField();
}

std::string DragFloat::formatValue() const
{
    return m_numberFormat.format(readBinding());
}

void DragFloat::applyScrub(float pixelDelta)
{
    if (!hasBinding()) {
        return;
    }
    writeBinding(clampValue(scrubValue(readBinding(), pixelDelta, speed)));
    if (onValueChanged) {
        onValueChanged(readBinding());
    }
    markDirty();
}

void DragFloat::commitFromField()
{
    if (!hasBinding()) {
        return;
    }
    writeBinding(clampValue(m_field->asDouble()));
    if (onValueChanged) {
        onValueChanged(readBinding());
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
        return readBinding();
    }
    if (v < min) {
        v = min;
    }
    if (v > max) {
        v = max;
    }
    return v;
}

DragInt::DragInt() : Drag(NumberKind::INTEGER, "%lld")
{
    configureField();
}

std::string DragInt::formatValue() const
{
    return m_numberFormat.format(static_cast<long long>(readBinding()));
}

void DragInt::beginScrub()
{
    m_scrubSeeded = false;
}

void DragInt::applyScrub(float pixelDelta)
{
    if (!hasBinding()) {
        return;
    }
    // scrub an unrounded shadow value so a speed below one step per pixel still adds up
    int64_t current = readBinding();
    if (!m_scrubSeeded || current != m_scrubWritten) {
        m_scrubExact = static_cast<double>(current);
        m_scrubSeeded = true;
    }
    m_scrubExact = scrubValue(m_scrubExact, pixelDelta, speed);
    if (std::isnan(m_scrubExact)) {
        m_scrubExact = static_cast<double>(current);
    }
    m_scrubExact = std::clamp(m_scrubExact, static_cast<double>(min), static_cast<double>(max));
    writeBinding(clampValue(s_saturateToInt64(m_scrubExact)));
    m_scrubWritten = readBinding();
    if (onValueChanged) {
        onValueChanged(readBinding());
    }
    markDirty();
}

void DragInt::commitFromField()
{
    if (!hasBinding()) {
        return;
    }
    writeBinding(clampValue(m_field->asInt64()));
    if (onValueChanged) {
        onValueChanged(readBinding());
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
