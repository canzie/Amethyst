// optional gizmo component, a macro at the top to enable it for compilation, as it is quite a specific thing, probably base it 1-1
// to the old imgui custom gizmo
//
// https://github.com/canzie/RaptureVK/blob/jobification/Editor/src/imguiPanels/modules/Gizmo.h
// https://github.com/canzie/RaptureVK/blob/jobification/Editor/src/imguiPanels/modules/Gizmo.cpp

#ifndef AMETHYST__GIZMO_H
#define AMETHYST__GIZMO_H

#include <glm/glm.hpp>
#include <memory>

enum class Operation {
    TRANSLATE,
    ROTATE,
    SCALE,
    COMBINED
};

enum class Space {
    LOCAL,
    WORLD
};

enum class Axis {
    NONE,
    X,
    Y,
    Z,
    XY,
    XZ,
    YZ,
    XYZ
};

struct SnapSettings {
    bool enabled = false;
    float translate = 1.0f;
    float rotate = 15.0f;
    float scale = 0.1f;
    bool shiftToSnap = true;
};

struct Config {
    float sizeFactor = 0.15f;
    float axisLength = 1.0f;
    float thickness = 3.0f;
    float pickRadius = 10.0f;
    float arrowSize = 12.0f;
    float planeSize = 0.25f;
    float ringRadius = 0.9f;
    float handleSize = 8.0f;
    SnapSettings snap;
};

struct Result {
    bool active = false;
    bool hovered = false;
    Axis axis = Axis::NONE;
    Operation operation = Operation::TRANSLATE;
    glm::vec3 deltaPosition{0.0f};
    glm::vec3 deltaRotation{0.0f};
    glm::vec3 deltaScale{1.0f};
    float rotationDegrees = 0.0f;
};

class Gizmo {
  public:
    Gizmo();
    ~Gizmo();

    Gizmo(const Gizmo &) = delete;
    Gizmo &operator=(const Gizmo &) = delete;
    Gizmo(Gizmo &&) noexcept;
    Gizmo &operator=(Gizmo &&) noexcept;

    Result update(const glm::mat4 &view, const glm::mat4 &projection, const glm::mat4 &objectTransform, const glm::vec3 &pivot,
                  Operation op, Space space, glm::vec2 viewportPos, glm::vec2 viewportSize);

    void reset();

    Config &config();
    const Config &config() const;

    void renderSettings();

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // AMETHYST__GIZMO_H
