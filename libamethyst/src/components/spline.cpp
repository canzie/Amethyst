#include "components/spline.h"

#include "components/extensions/ui_drag_detector.h"
#include "components/frame.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "rendering/instance_data.h"

#include "math/math.h"

namespace Amethyst {

static constexpr int SPLINE_SUBDIVISIONS = 4;

static vec2 s_cubicPoint(vec2 b0, vec2 b1, vec2 b2, vec2 b3, float t)
{
    float u = 1.0f - t;
    float w0 = u * u * u;
    float w1 = 3.0f * u * u * t;
    float w2 = 3.0f * u * t * t;
    float w3 = t * t * t;
    return b0 * w0 + b1 * w1 + b2 * w2 + b3 * w3;
}

// Control point of the quadratic that shares the two endpoints and passes through the cubic's
// midpoint. Unlike a tangent-line intersection this is a bounded blend of nearby curve points, so
// near-parallel tangents can never fling the control (and the bounding box) off to infinity.
static vec2 s_midpointControl(vec2 endpointA, vec2 endpointB, vec2 mid)
{
    return mid * 2.0f - (endpointA + endpointB) * 0.5f;
}

Spline::Spline()
{
    m_splineProps.type = CurveType::CATMULL_ROM;
    m_splineProps.showKnots = true;

    propagate(INTERACTION_CATEGORY_CLICK);
    propagate(INTERACTION_CATEGORY_MOVE);
    propagate(INTERACTION_CATEGORY_HOVER);

    resolveStyle();
}

void Spline::resolveStyle()
{
    resolveBaseStyle(ComponentType::SPLINE);

    SplineStyleProperties resolved = Style::instance().getSplineStyle(ComponentType::SPLINE, getClasses(), effectiveGuiState());
    if (m_splineProps.apply(resolved)) {
        markDirty();
    }
}

Spline::~Spline()
{
    if (m_registry != nullptr) {
        for (GeometryAllocation *a : m_allocations) {
            if (a->isValid()) {
                a->registry->release(*a);
            }
        }
    }
}

void Spline::setKnots(std::vector<vec2> knots)
{
    m_knots = std::move(knots);
    markDirty();
}

void Spline::addKnot(vec2 knot)
{
    m_knots.push_back(knot);
    markDirty();
}

void Spline::clearKnots()
{
    if (!m_knots.empty()) {
        m_knots.clear();
        markDirty();
    }
}

bool Spline::setSplineProperties(const SplineStylePropertiesArgs &props)
{
    bool changed = m_splineProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

size_t Spline::anchorStride() const
{
    switch (m_splineProps.type) {
    case CurveType::QUADRATIC:
        return 2;
    case CurveType::CUBIC:
        return 3;
    default:
        return 1;
    }
}

bool Spline::isAnchor(size_t index) const
{
    return index % anchorStride() == 0;
}

void Spline::appendCubic(std::vector<QuadBezier> &out, vec2 b0, vec2 b1, vec2 b2, vec2 b3) const
{
    for (int k = 0; k < SPLINE_SUBDIVISIONS; k++) {
        float t0 = static_cast<float>(k) / SPLINE_SUBDIVISIONS;
        float t1 = static_cast<float>(k + 1) / SPLINE_SUBDIVISIONS;
        float tm = (t0 + t1) * 0.5f;

        vec2 a = s_cubicPoint(b0, b1, b2, b3, t0);
        vec2 c = s_cubicPoint(b0, b1, b2, b3, t1);
        vec2 mid = s_cubicPoint(b0, b1, b2, b3, tm);

        out.push_back({a, s_midpointControl(a, c, mid), c});
    }
}

void Spline::buildCurve(std::vector<QuadBezier> &out) const
{
    size_t n = m_knots.size();
    if (n < 2) {
        return;
    }

    switch (m_splineProps.type) {
    case CurveType::LINEAR:
        for (size_t i = 0; i + 1 < n; i++) {
            vec2 a = m_knots[i];
            vec2 c = m_knots[i + 1];
            out.push_back({a, (a + c) * 0.5f, c});
        }
        break;

    case CurveType::QUADRATIC:
        for (size_t i = 0; i + 2 < n; i += 2) {
            out.push_back({m_knots[i], m_knots[i + 1], m_knots[i + 2]});
        }
        break;

    case CurveType::CUBIC:
        for (size_t i = 0; i + 3 < n; i += 3) {
            appendCubic(out, m_knots[i], m_knots[i + 1], m_knots[i + 2], m_knots[i + 3]);
        }
        break;

    case CurveType::CATMULL_ROM:
    default:
        for (size_t i = 0; i + 1 < n; i++) {
            vec2 prev = m_knots[i == 0 ? 0 : i - 1];
            vec2 p1 = m_knots[i];
            vec2 p2 = m_knots[i + 1];
            vec2 next = m_knots[i + 2 < n ? i + 2 : n - 1];

            appendCubic(out, p1, p1 + (p2 - prev) * (1.0f / 6.0f), p2 - (next - p1) * (1.0f / 6.0f), p2);
        }
        break;
    }
}

InstanceData Spline::makeInstance(const QuadBezier &seg, float thickness, Color4 color) const
{
    InstanceData data{};
    data.clipRect = clipRect;
    data.setPrimitiveType(PRIMITIVE_CANVAS_BEZIER);
    data.setFillColor(color);
    data.setBorderMode(BorderMode::MIDDLE);
    data.zIndex = getZIndex();
    data.setVisible(isVisible());

    float halfThick = thickness * 0.5f;
    float pad = halfThick + 1.0f;

    vec2 bboxMin = min(min(seg.a, seg.control), seg.c) - vec2(pad);
    vec2 bboxMax = max(max(seg.a, seg.control), seg.c) + vec2(pad);
    vec2 center = (bboxMin + bboxMax) * 0.5f;

    data.translation = absolutePosition + center;
    data.scale = bboxMax - bboxMin;
    data.setThickness(halfThick);
    data.setShapePoint(0, seg.a - center);
    data.setShapePoint(1, seg.control - center);
    data.setShapePoint(2, seg.c - center);

    return data;
}

void Spline::buildInstances(std::vector<InstanceData> &out) const
{
    std::vector<QuadBezier> curve;
    buildCurve(curve);
    for (const QuadBezier &seg : curve) {
        out.push_back(makeInstance(seg, m_splineProps.thickness, m_splineProps.color));
    }

    if (m_splineProps.type != CurveType::QUADRATIC && m_splineProps.type != CurveType::CUBIC) {
        return;
    }

    Color4 handleColor = m_splineProps.color;
    handleColor.a *= 0.5f;

    auto connect = [&](size_t anchorIndex, vec2 control) {
        if (anchorIndex >= m_knots.size()) {
            return;
        }
        vec2 anchor = m_knots[anchorIndex];
        out.push_back(makeInstance({anchor, (anchor + control) * 0.5f, control}, 1.0f, handleColor));
    };

    size_t n = m_knots.size();
    for (size_t i = 0; i < n; i++) {
        if (isAnchor(i)) {
            continue;
        }

        vec2 control = m_knots[i];
        if (m_splineProps.type == CurveType::QUADRATIC) {
            connect(i - 1, control);
            connect(i + 1, control);
        } else {
            connect(i % 3 == 1 ? i - 1 : i + 1, control);
        }
    }
}

vec2 Spline::clampKnot(size_t index, vec2 knot) const
{
    knot.x = clamp(knot.x, 0.0f, absoluteSize.x);
    knot.y = clamp(knot.y, 0.0f, absoluteSize.y);

    if (isAnchor(index)) {
        size_t stride = anchorStride();
        if (index >= stride) {
            knot.x = max(knot.x, m_knots[index - stride].x);
        }
        if (index + stride < m_knots.size()) {
            knot.x = min(knot.x, m_knots[index + stride].x);
        }
    }

    return knot;
}

void Spline::smoothPartner(size_t control)
{
    if (m_splineProps.type != CurveType::CUBIC || isAnchor(control)) {
        return;
    }

    size_t anchor;
    size_t partner;
    if (control % 3 == 1) {
        if (control < 2) {
            return;
        }
        anchor = control - 1;
        partner = control - 2;
    } else {
        anchor = control + 1;
        partner = control + 2;
    }

    if (partner >= m_knots.size() || isAnchor(partner)) {
        return;
    }

    vec2 handle = m_knots[control] - m_knots[anchor];
    if (dot(handle, handle) < 1e-6f) {
        return;
    }

    float partnerLength = length(m_knots[partner] - m_knots[anchor]);
    m_knots[partner] = clampKnot(partner, m_knots[anchor] - normalize(handle) * partnerLength);
}

void Spline::syncHandles()
{
    bool show = static_cast<bool>(m_splineProps.showKnots) && !m_knots.empty();
    if (!show) {
        for (Frame *handle : m_handles) {
            removeChild(handle);
        }
        m_handles.clear();
        return;
    }

    if (m_handles.size() != m_knots.size()) {
        for (Frame *handle : m_handles) {
            removeChild(handle);
        }
        m_handles.clear();

        for (size_t i = 0; i < m_knots.size(); i++) {
            Frame *handle = add<Frame>();
            auto *drag = handle->addExtension<UIDragDetector>();
            drag->onDragUpdate = [this, i](vec2, vec2) {
                if (i >= m_handles.size()) {
                    return;
                }
                m_knots[i] = clampKnot(i, m_handles[i]->getBaseProperties().position.offset);
                smoothPartner(i);
                markDirty();
            };
            m_handles.push_back(handle);
        }
    }

    Color4 color = m_splineProps.color;
    float size = m_splineProps.knotSize;

    for (size_t i = 0; i < m_handles.size(); i++) {
        m_handles[i]->setBaseProperties({.anchorPoint = vec2(0.5f, 0.5f),
                                         .position = UDim2::fromOffset(m_knots[i].x, m_knots[i].y),
                                         .size = UDim2::fromOffset(size, size),
                                         .zIndex = 1});
        m_handles[i]->setBaseStyleProperties(
            {.backgroundColor = Color3(color.r, color.g, color.b), .backgroundTransparency = 1.0f - color.a});
    }
}

void Spline::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        if (m_registry != nullptr && m_registry != ctx.geometry) {
            for (GeometryAllocation *a : m_allocations) {
                if (a->isValid()) {
                    a->registry->release(*a);
                }
            }
            m_allocations.clear();
        }
        m_registry = ctx.geometry;

        std::vector<InstanceData> instances;
        buildInstances(instances);

        while (m_allocations.size() > instances.size()) {
            ctx.geometry->release(*m_allocations.back());
            m_allocations.pop_back();
        }

        for (size_t i = 0; i < m_allocations.size(); i++) {
            ctx.geometry->update(*m_allocations[i], instances[i]);
        }

        for (size_t i = m_allocations.size(); i < instances.size(); i++) {
            m_allocations.push_back(ctx.geometry->submit(instances[i]));
        }
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void Spline::arrange()
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    syncHandles();

    for (auto &child : m_children) {
        if (auto *obj = child->asUiObject()) {
            obj->clipRect = clipRect;
            obj->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            obj->arrange();
        }
    }
}

} // namespace Amethyst
