#ifndef AMETHYST__GIZMO_H
#define AMETHYST__GIZMO_H

#include <glm/glm.hpp>
#include <memory>

namespace Amethyst {

class Canvas;
class UIBase2D;

enum class GizmoOperation {
    TRANSLATE,
    ROTATE,
    SCALE,
    COMBINED
};

enum class GizmoSpace {
    LOCAL,
    WORLD
};

enum class GizmoAxis {
    NONE,
    X,
    Y,
    Z,
    XY,
    XZ,
    YZ,
    XYZ
};

struct GizmoSnapSettings {
    bool enabled = false;
    float translate = 1.0f;
    float rotate = 15.0f;
    float scale = 0.1f;
    bool shiftToSnap = true;
};

struct GizmoConfig {
    float sizeFactor = 0.15f;
    float axisLength = 1.0f;
    float thickness = 3.0f;
    float pickRadius = 10.0f;
    float arrowSize = 12.0f;
    float planeSize = 0.25f;
    float ringRadius = 0.9f;
    float handleSize = 8.0f;
    GizmoSnapSettings snap;
};

struct GizmoParams {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 objectTransform{1.0f};
    glm::vec3 pivot{0.0f};
    GizmoOperation operation = GizmoOperation::TRANSLATE;
    GizmoSpace space = GizmoSpace::WORLD;

    bool operator==(const GizmoParams &) const = default;
};

struct GizmoResult {
    bool active = false;
    bool hovered = false;
    GizmoAxis axis = GizmoAxis::NONE;
    GizmoOperation operation = GizmoOperation::TRANSLATE;
    glm::vec3 deltaPosition{0.0f};
    glm::vec3 deltaRotation{0.0f};
    glm::vec3 deltaScale{1.0f};
    float rotationDegrees = 0.0f;
};

class Gizmo {
  public:
    Gizmo(UIBase2D *parent);
    ~Gizmo();

    Gizmo(const Gizmo &) = delete;
    Gizmo &operator=(const Gizmo &) = delete;
    Gizmo(Gizmo &&) noexcept;
    Gizmo &operator=(Gizmo &&) noexcept;

    GizmoResult update(const GizmoParams &params);

    void reset();

    Canvas &canvas();

    GizmoConfig &config();
    const GizmoConfig &config() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Amethyst

#endif // AMETHYST__GIZMO_H
