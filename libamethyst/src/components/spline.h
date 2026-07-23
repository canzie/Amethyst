/**
 * @file spline.h
 * @brief Retained Catmull-Rom spline rendered as analytic quadratic Bezier segments
 *
 * Knots are specified in local pixel coordinates relative to the object's top-left.
 */

#ifndef AMETHYST__SPLINE_H
#define AMETHYST__SPLINE_H

#include "components/common.h"
#include "components/properties.h"
#include "components/ui_object.h"

#include <vector>

namespace Amethyst {

class GeometryRegistry;
struct GeometryAllocation;
class Frame;

class Spline : public UIObject {
  public:
    Spline();
    ~Spline();

    void draw(DrawContext &ctx) override;
    void arrange() override;
    void resolveStyle() override;

    /**
     * @brief Replace the full set of knots the curve passes through.
     * @param knots Knot positions in local pixel coordinates
     */
    void setKnots(std::vector<vec2> knots);
    void addKnot(vec2 knot);
    void clearKnots();
    const std::vector<vec2> &getKnots() const { return m_knots; }

    /**
     * @brief Merge in the set fields of a spline style block, keeping current values for unset ones.
     * @param props Style block; only fields that are set override
     * @return True if any resolved value changed
     */
    bool setSplineProperties(const SplineStylePropertiesArgs &props);
    const SplineStyleProperties &getSplineProperties() const { return m_splineProps; }

  private:
    struct QuadBezier {
        vec2 a;
        vec2 control;
        vec2 c;
    };

    bool isAnchor(size_t index) const;
    size_t anchorStride() const;
    void appendCubic(std::vector<QuadBezier> &out, vec2 b0, vec2 b1, vec2 b2, vec2 b3) const;
    void buildCurve(std::vector<QuadBezier> &out) const;
    void buildInstances(std::vector<InstanceData> &out) const;
    InstanceData makeInstance(const QuadBezier &seg, float thickness, Color4 color) const;
    void syncHandles();
    void smoothPartner(size_t control);
    vec2 clampKnot(size_t index, vec2 knot) const;

  private:
    std::vector<vec2> m_knots;
    SplineStyleProperties m_splineProps;

    GeometryRegistry *m_registry = nullptr;
    std::vector<GeometryAllocation *> m_allocations;

    std::vector<Frame *> m_handles;
};

} // namespace Amethyst

#endif // AMETHYST__SPLINE_H
