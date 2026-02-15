#include "gizmo.h"

#include "components/canvas.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>

namespace Amethyst {

static const Color4 COL_X = {0.929f, 0.282f, 0.357f, 1.0f};
static const Color4 COL_Y = {0.525f, 0.788f, 0.247f, 1.0f};
static const Color4 COL_Z = {0.255f, 0.545f, 0.937f, 1.0f};
static const Color4 COL_X_HOVER = {1.0f, 0.420f, 0.478f, 1.0f};
static const Color4 COL_Y_HOVER = {0.659f, 0.878f, 0.373f, 1.0f};
static const Color4 COL_Z_HOVER = {0.420f, 0.659f, 1.0f, 1.0f};
static const Color4 COL_ACTIVE = {1.0f, 0.863f, 0.251f, 1.0f};
static const Color4 COL_PLANE_XY = {0.255f, 0.545f, 0.937f, 0.392f};
static const Color4 COL_PLANE_XZ = {0.525f, 0.788f, 0.247f, 0.392f};
static const Color4 COL_PLANE_YZ = {0.929f, 0.282f, 0.357f, 0.392f};
static const Color4 COL_WHITE = {1.0f, 1.0f, 1.0f, 0.784f};
static const Color4 COL_LABEL_BG = {0.118f, 0.118f, 0.118f, 0.863f};

static constexpr float PI = 3.14159265359f;
static constexpr float TWO_PI = 6.28318530718f;

static glm::vec2 s_worldToScreen(const glm::vec3 &world, const glm::mat4 &viewProj, glm::vec2 vpPos, glm::vec2 vpSize)
{
    glm::vec4 clip = viewProj * glm::vec4(world, 1.0f);
    if (clip.w <= 0.0001f) {
        return glm::vec2(-10000.0f);
    }
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return glm::vec2(vpPos.x + (ndc.x * 0.5f + 0.5f) * vpSize.x, vpPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * vpSize.y);
}

static void s_screenToWorldRay(glm::vec2 screen, const glm::mat4 &invViewProj, glm::vec2 vpPos, glm::vec2 vpSize,
                               glm::vec3 &origin, glm::vec3 &dir)
{
    float ndcX = ((screen.x - vpPos.x) / vpSize.x) * 2.0f - 1.0f;
    float ndcY = 1.0f - ((screen.y - vpPos.y) / vpSize.y) * 2.0f;

    glm::vec4 nearPt = invViewProj * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPt = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);

    nearPt /= nearPt.w;
    farPt /= farPt.w;

    origin = glm::vec3(nearPt);
    dir = glm::normalize(glm::vec3(farPt) - glm::vec3(nearPt));
}

static float s_rayPlaneIntersect(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const glm::vec3 &planePoint,
                                 const glm::vec3 &planeNormal)
{
    float denom = glm::dot(planeNormal, rayDir);
    if (std::abs(denom) < 0.0001f) {
        return -1.0f;
    }
    return glm::dot(planePoint - rayOrigin, planeNormal) / denom;
}

static float s_distanceToSegment2D(glm::vec2 p, glm::vec2 a, glm::vec2 b)
{
    glm::vec2 ab = b - a;
    glm::vec2 ap = p - a;
    float lenSq = glm::dot(ab, ab);
    if (lenSq < 0.0001f) {
        return glm::length(ap);
    }
    float t = glm::clamp(glm::dot(ap, ab) / lenSq, 0.0f, 1.0f);
    return glm::length(p - (a + t * ab));
}

static float s_distanceToPoint2D(glm::vec2 a, glm::vec2 b)
{
    return glm::length(a - b);
}

static bool s_pointInQuad2D(glm::vec2 p, const glm::vec2 quad[4])
{
    auto sign = [](glm::vec2 p1, glm::vec2 p2, glm::vec2 p3) {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    };

    float d1 = sign(p, quad[0], quad[1]);
    float d2 = sign(p, quad[1], quad[2]);
    float d3 = sign(p, quad[2], quad[0]);

    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    if (!(hasNeg && hasPos)) return true;

    d1 = sign(p, quad[0], quad[2]);
    d2 = sign(p, quad[2], quad[3]);
    d3 = sign(p, quad[3], quad[0]);

    hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(hasNeg && hasPos);
}

static glm::vec3 s_getAxisVector(GizmoAxis axis)
{
    switch (axis) {
    case GizmoAxis::X: return glm::vec3(1, 0, 0);
    case GizmoAxis::Y: return glm::vec3(0, 1, 0);
    case GizmoAxis::Z: return glm::vec3(0, 0, 1);
    default: return glm::vec3(0);
    }
}

static Color4 s_getAxisColor(GizmoAxis axis, bool hovered, bool active)
{
    if (active) return COL_ACTIVE;
    switch (axis) {
    case GizmoAxis::X: return hovered ? COL_X_HOVER : COL_X;
    case GizmoAxis::Y: return hovered ? COL_Y_HOVER : COL_Y;
    case GizmoAxis::Z: return hovered ? COL_Z_HOVER : COL_Z;
    case GizmoAxis::XY: return hovered ? COL_Z_HOVER : COL_Z;
    case GizmoAxis::XZ: return hovered ? COL_Y_HOVER : COL_Y;
    case GizmoAxis::YZ: return hovered ? COL_X_HOVER : COL_X;
    default: return COL_WHITE;
    }
}

static Color4 s_getPlaneColor(GizmoAxis axis, bool hovered, bool active)
{
    if (active) return Color4{1.0f, 0.863f, 0.251f, 0.588f};
    switch (axis) {
    case GizmoAxis::XY: return hovered ? Color4{0.255f, 0.545f, 0.937f, 0.706f} : COL_PLANE_XY;
    case GizmoAxis::XZ: return hovered ? Color4{0.525f, 0.788f, 0.247f, 0.706f} : COL_PLANE_XZ;
    case GizmoAxis::YZ: return hovered ? Color4{0.929f, 0.282f, 0.357f, 0.706f} : COL_PLANE_YZ;
    default: return COL_WHITE;
    }
}

class GizmoCanvas : public Canvas {
  public:
    using Canvas::Canvas;

    glm::vec2 mousePos{0.0f};
    bool mouseDown = false;
    bool mouseUp = false;

  protected:
    void onMouseMoved(uint32_t x, uint32_t y) override
    {
        mousePos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
    }

    void onMouseButton1Down(uint32_t x, uint32_t y) override
    {
        mousePos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
        mouseDown = true;
    }

    void onMouseButton1Up(uint32_t x, uint32_t y) override
    {
        mousePos = glm::vec2(static_cast<float>(x), static_cast<float>(y));
        mouseUp = true;
    }
};

struct Gizmo::Impl {
    enum class State {
        IDLE,
        HOVERING,
        DRAGGING
    };

    GizmoCanvas *canvas = nullptr;

    State state = State::IDLE;
    GizmoAxis hoveredAxis = GizmoAxis::NONE;
    GizmoAxis activeAxis = GizmoAxis::NONE;
    GizmoOperation activeOp = GizmoOperation::TRANSLATE;

    glm::vec3 dragStartHitPoint{0.0f};
    float dragStartAngle = 0.0f;
    float dragCurrentAngle = 0.0f;
    float dragStartDistance = 0.0f;
    float accumulatedRotation = 0.0f;
    glm::vec3 accumulatedTranslation{0.0f};
    glm::vec3 accumulatedScale{1.0f};
    glm::vec3 dragPlaneNormal{0.0f, 1.0f, 0.0f};

    glm::vec3 lastGizmoCenter{0.0f};
    bool firstFrame = true;

    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projMatrix{1.0f};
    glm::mat4 viewProjMatrix{1.0f};
    glm::mat4 invViewProjMatrix{1.0f};
    glm::vec3 gizmoCenter{0.0f};
    glm::vec3 cameraPos{0.0f};
    glm::vec3 cameraDir{0.0f, 0.0f, -1.0f};
    float worldScale = 1.0f;

    GizmoSpace currentSpace = GizmoSpace::WORLD;
    glm::mat3 gizmoOrientation{1.0f};
    glm::vec3 axisX{1, 0, 0};
    glm::vec3 axisY{0, 1, 0};
    glm::vec3 axisZ{0, 0, 1};

    GizmoConfig config;

    GizmoParams prevParams;
    glm::vec2 prevMousePos{-1.0f};
    GizmoResult prevResult;

    glm::vec2 vpPos() const { return canvas->absolutePosition; }
    glm::vec2 vpSize() const { return canvas->absoluteSize; }

    GizmoAxis hitTestTranslate(glm::vec2 mouse);
    GizmoAxis hitTestRotate(glm::vec2 mouse);
    GizmoAxis hitTestScale(glm::vec2 mouse);

    void drawTranslate(GizmoAxis hovered, bool active, glm::vec2 mouse);
    void drawRotate(GizmoAxis hovered, bool active);
    void drawScale(GizmoAxis hovered, bool active, glm::vec2 mouse);

    glm::vec3 computeTranslationDelta(glm::vec2 mouse);
    float computeRotationDelta(glm::vec2 mouse);
    glm::vec3 computeScaleDelta(glm::vec2 mouse);

    bool shouldSnap() const;
    float applySnap(float value, float snapSize) const;

    void computeWorldScale();
    void updateOrientation(const glm::mat4 &objectTransform, GizmoSpace space);
    glm::vec3 getOrientedAxis(GizmoAxis axis) const;
    void getPlaneHandleQuad(GizmoAxis axis, glm::vec3 corners[4]);
};

void Gizmo::Impl::computeWorldScale()
{
    float distance = glm::length(gizmoCenter - cameraPos);
    float targetScreenPixels = 120.0f * config.sizeFactor / 0.15f;

    float focalLength = projMatrix[1][1];
    if (focalLength > 0.0f) {
        worldScale = (targetScreenPixels / vpSize().y) * distance * 2.0f / focalLength;
    } else {
        worldScale = distance * 0.15f;
    }

    worldScale = glm::clamp(worldScale, 0.01f, 1000.0f);
}

void Gizmo::Impl::updateOrientation(const glm::mat4 &objectTransform, GizmoSpace space)
{
    currentSpace = space;
    if (space == GizmoSpace::LOCAL) {
        axisX = glm::normalize(glm::vec3(objectTransform[0]));
        axisY = glm::normalize(glm::vec3(objectTransform[1]));
        axisZ = glm::normalize(glm::vec3(objectTransform[2]));
        gizmoOrientation = glm::mat3(axisX, axisY, axisZ);
    } else {
        axisX = glm::vec3(1, 0, 0);
        axisY = glm::vec3(0, 1, 0);
        axisZ = glm::vec3(0, 0, 1);
        gizmoOrientation = glm::mat3(1.0f);
    }
}

glm::vec3 Gizmo::Impl::getOrientedAxis(GizmoAxis axis) const
{
    switch (axis) {
    case GizmoAxis::X: return axisX;
    case GizmoAxis::Y: return axisY;
    case GizmoAxis::Z: return axisZ;
    default: return glm::vec3(0.0f);
    }
}

void Gizmo::Impl::getPlaneHandleQuad(GizmoAxis axis, glm::vec3 corners[4])
{
    float offset = worldScale * 0.3f;
    float size = worldScale * 0.15f;

    glm::vec3 dir1, dir2;
    switch (axis) {
    case GizmoAxis::XY: dir1 = axisX; dir2 = axisY; break;
    case GizmoAxis::XZ: dir1 = axisX; dir2 = axisZ; break;
    case GizmoAxis::YZ: dir1 = axisY; dir2 = axisZ; break;
    default: return;
    }

    corners[0] = gizmoCenter + dir1 * offset + dir2 * offset;
    corners[1] = gizmoCenter + dir1 * (offset + size) + dir2 * offset;
    corners[2] = gizmoCenter + dir1 * (offset + size) + dir2 * (offset + size);
    corners[3] = gizmoCenter + dir1 * offset + dir2 * (offset + size);
}

GizmoAxis Gizmo::Impl::hitTestTranslate(glm::vec2 mouse)
{
    float threshold = config.pickRadius;
    glm::vec2 center = s_worldToScreen(gizmoCenter, viewProjMatrix, vpPos(), vpSize());

    glm::vec2 xScreen = s_worldToScreen(gizmoCenter + axisX * worldScale, viewProjMatrix, vpPos(), vpSize());
    glm::vec2 yScreen = s_worldToScreen(gizmoCenter + axisY * worldScale, viewProjMatrix, vpPos(), vpSize());
    glm::vec2 zScreen = s_worldToScreen(gizmoCenter + axisZ * worldScale, viewProjMatrix, vpPos(), vpSize());

    auto testPlane = [&](GizmoAxis a) -> bool {
        glm::vec3 corners[4];
        getPlaneHandleQuad(a, corners);
        glm::vec2 quad[4];
        for (int i = 0; i < 4; i++) {
            quad[i] = s_worldToScreen(corners[i], viewProjMatrix, vpPos(), vpSize());
        }
        return s_pointInQuad2D(mouse, quad);
    };

    if (testPlane(GizmoAxis::XY)) return GizmoAxis::XY;
    if (testPlane(GizmoAxis::XZ)) return GizmoAxis::XZ;
    if (testPlane(GizmoAxis::YZ)) return GizmoAxis::YZ;

    float distX = s_distanceToSegment2D(mouse, center, xScreen);
    float distY = s_distanceToSegment2D(mouse, center, yScreen);
    float distZ = s_distanceToSegment2D(mouse, center, zScreen);

    float minDist = threshold;
    GizmoAxis result = GizmoAxis::NONE;

    if (distX < minDist) { minDist = distX; result = GizmoAxis::X; }
    if (distY < minDist) { minDist = distY; result = GizmoAxis::Y; }
    if (distZ < minDist) { minDist = distZ; result = GizmoAxis::Z; }

    return result;
}

GizmoAxis Gizmo::Impl::hitTestRotate(glm::vec2 mouse)
{
    float threshold = config.pickRadius * 1.5f;
    float ringRadius = worldScale * config.ringRadius;

    auto distanceToRing3D = [&](const glm::vec3 &axis) -> float {
        glm::vec3 up = glm::normalize(axis);
        glm::vec3 right;
        if (std::abs(up.y) < 0.99f) {
            right = glm::normalize(glm::cross(up, glm::vec3(0, 1, 0)));
        } else {
            right = glm::normalize(glm::cross(up, glm::vec3(1, 0, 0)));
        }
        glm::vec3 forward = glm::cross(right, up);

        float minDist = 1e10f;
        glm::vec2 prevScreen;
        bool hasPrev = false;

        constexpr int hitSegments = 24;
        for (int i = 0; i <= hitSegments; i++) {
            float angle = (static_cast<float>(i) / hitSegments) * TWO_PI;
            glm::vec3 worldPt = gizmoCenter + (right * std::cos(angle) + forward * std::sin(angle)) * ringRadius;
            glm::vec2 screenPt = s_worldToScreen(worldPt, viewProjMatrix, vpPos(), vpSize());

            if (screenPt.x > -9000.0f) {
                if (hasPrev && prevScreen.x > -9000.0f) {
                    float dist = s_distanceToSegment2D(mouse, prevScreen, screenPt);
                    minDist = std::min(minDist, dist);
                }
                prevScreen = screenPt;
                hasPrev = true;
            }
        }
        return minDist;
    };

    float distX = distanceToRing3D(axisX);
    float distY = distanceToRing3D(axisY);
    float distZ = distanceToRing3D(axisZ);

    float minDist = threshold;
    GizmoAxis result = GizmoAxis::NONE;

    if (distX < minDist) { minDist = distX; result = GizmoAxis::X; }
    if (distY < minDist) { minDist = distY; result = GizmoAxis::Y; }
    if (distZ < minDist) { minDist = distZ; result = GizmoAxis::Z; }

    return result;
}

GizmoAxis Gizmo::Impl::hitTestScale(glm::vec2 mouse)
{
    float threshold = config.pickRadius;
    glm::vec2 center = s_worldToScreen(gizmoCenter, viewProjMatrix, vpPos(), vpSize());

    if (s_distanceToPoint2D(mouse, center) < config.handleSize * 1.5f) {
        return GizmoAxis::XYZ;
    }

    auto testPlane = [&](GizmoAxis a) -> bool {
        glm::vec3 corners[4];
        getPlaneHandleQuad(a, corners);
        glm::vec2 quad[4];
        for (int i = 0; i < 4; i++) {
            quad[i] = s_worldToScreen(corners[i], viewProjMatrix, vpPos(), vpSize());
        }
        return s_pointInQuad2D(mouse, quad);
    };

    if (testPlane(GizmoAxis::XY)) return GizmoAxis::XY;
    if (testPlane(GizmoAxis::XZ)) return GizmoAxis::XZ;
    if (testPlane(GizmoAxis::YZ)) return GizmoAxis::YZ;

    glm::vec2 xScreen = s_worldToScreen(gizmoCenter + axisX * worldScale, viewProjMatrix, vpPos(), vpSize());
    glm::vec2 yScreen = s_worldToScreen(gizmoCenter + axisY * worldScale, viewProjMatrix, vpPos(), vpSize());
    glm::vec2 zScreen = s_worldToScreen(gizmoCenter + axisZ * worldScale, viewProjMatrix, vpPos(), vpSize());

    if (s_distanceToPoint2D(mouse, xScreen) < config.handleSize * 1.2f) return GizmoAxis::X;
    if (s_distanceToPoint2D(mouse, yScreen) < config.handleSize * 1.2f) return GizmoAxis::Y;
    if (s_distanceToPoint2D(mouse, zScreen) < config.handleSize * 1.2f) return GizmoAxis::Z;

    float distX = s_distanceToSegment2D(mouse, center, xScreen);
    float distY = s_distanceToSegment2D(mouse, center, yScreen);
    float distZ = s_distanceToSegment2D(mouse, center, zScreen);

    float minDist = threshold;
    GizmoAxis result = GizmoAxis::NONE;

    if (distX < minDist) { minDist = distX; result = GizmoAxis::X; }
    if (distY < minDist) { minDist = distY; result = GizmoAxis::Y; }
    if (distZ < minDist) { minDist = distZ; result = GizmoAxis::Z; }

    return result;
}

glm::vec3 Gizmo::Impl::computeTranslationDelta(glm::vec2 mouse)
{
    glm::vec3 rayOrigin, rayDir;
    s_screenToWorldRay(mouse, invViewProjMatrix, vpPos(), vpSize(), rayOrigin, rayDir);

    float t = s_rayPlaneIntersect(rayOrigin, rayDir, dragStartHitPoint, dragPlaneNormal);
    if (t < 0) return glm::vec3(0.0f);

    glm::vec3 currentHitPoint = rayOrigin + rayDir * t;
    glm::vec3 worldDelta = currentHitPoint - dragStartHitPoint;

    glm::vec3 delta(0.0f);
    switch (activeAxis) {
    case GizmoAxis::X: delta = axisX * glm::dot(worldDelta, axisX); break;
    case GizmoAxis::Y: delta = axisY * glm::dot(worldDelta, axisY); break;
    case GizmoAxis::Z: delta = axisZ * glm::dot(worldDelta, axisZ); break;
    case GizmoAxis::XY: delta = axisX * glm::dot(worldDelta, axisX) + axisY * glm::dot(worldDelta, axisY); break;
    case GizmoAxis::XZ: delta = axisX * glm::dot(worldDelta, axisX) + axisZ * glm::dot(worldDelta, axisZ); break;
    case GizmoAxis::YZ: delta = axisY * glm::dot(worldDelta, axisY) + axisZ * glm::dot(worldDelta, axisZ); break;
    default: delta = worldDelta; break;
    }

    if (shouldSnap()) {
        float snapX = applySnap(glm::dot(delta, axisX), config.snap.translate);
        float snapY = applySnap(glm::dot(delta, axisY), config.snap.translate);
        float snapZ = applySnap(glm::dot(delta, axisZ), config.snap.translate);
        delta = axisX * snapX + axisY * snapY + axisZ * snapZ;
    }

    dragStartHitPoint = currentHitPoint;
    accumulatedTranslation += delta;

    return delta;
}

float Gizmo::Impl::computeRotationDelta(glm::vec2 mouse)
{
    glm::vec2 center = s_worldToScreen(gizmoCenter, viewProjMatrix, vpPos(), vpSize());

    float currentAngle = std::atan2(mouse.y - center.y, mouse.x - center.x);
    float delta = dragCurrentAngle - currentAngle;

    while (delta > PI) delta -= TWO_PI;
    while (delta < -PI) delta += TWO_PI;

    if (shouldSnap()) {
        float snapRad = config.snap.rotate * PI / 180.0f;
        delta = applySnap(delta, snapRad);
    }

    dragCurrentAngle = currentAngle;
    accumulatedRotation -= delta;

    return delta;
}

glm::vec3 Gizmo::Impl::computeScaleDelta(glm::vec2 mouse)
{
    glm::vec2 center = s_worldToScreen(gizmoCenter, viewProjMatrix, vpPos(), vpSize());

    float currentDist = s_distanceToPoint2D(mouse, center);
    float scaleFactor = (dragStartDistance > 0.001f) ? currentDist / dragStartDistance : 1.0f;
    scaleFactor = glm::clamp(scaleFactor, 0.01f, 100.0f);

    if (shouldSnap()) {
        scaleFactor = applySnap(scaleFactor, config.snap.scale);
        if (scaleFactor < 0.01f) scaleFactor = 0.01f;
    }

    dragStartDistance = currentDist;

    glm::vec3 result(1.0f);
    switch (activeAxis) {
    case GizmoAxis::X: result.x = scaleFactor; accumulatedScale.x *= scaleFactor; break;
    case GizmoAxis::Y: result.y = scaleFactor; accumulatedScale.y *= scaleFactor; break;
    case GizmoAxis::Z: result.z = scaleFactor; accumulatedScale.z *= scaleFactor; break;
    case GizmoAxis::XY:
        result.x = scaleFactor; result.y = scaleFactor;
        accumulatedScale.x *= scaleFactor; accumulatedScale.y *= scaleFactor;
        break;
    case GizmoAxis::XZ:
        result.x = scaleFactor; result.z = scaleFactor;
        accumulatedScale.x *= scaleFactor; accumulatedScale.z *= scaleFactor;
        break;
    case GizmoAxis::YZ:
        result.y = scaleFactor; result.z = scaleFactor;
        accumulatedScale.y *= scaleFactor; accumulatedScale.z *= scaleFactor;
        break;
    case GizmoAxis::XYZ:
        result = glm::vec3(scaleFactor);
        accumulatedScale *= scaleFactor;
        break;
    default: break;
    }

    return result;
}

bool Gizmo::Impl::shouldSnap() const
{
    return config.snap.enabled;
}

float Gizmo::Impl::applySnap(float value, float snapSize) const
{
    return std::round(value / snapSize) * snapSize;
}

static void s_drawArrow(Canvas &c, glm::vec2 start, glm::vec2 end, Color4 color, float thickness, float arrowSize)
{
    glm::vec2 canvasStart = start - c.absolutePosition;
    glm::vec2 canvasEnd = end - c.absolutePosition;

    c.drawLine(canvasStart, canvasEnd, color, thickness);

    glm::vec2 dir = canvasEnd - canvasStart;
    float len = glm::length(dir);
    if (len < 0.001f) return;

    dir /= len;
    glm::vec2 perp(-dir.y, dir.x);
    glm::vec2 base = canvasEnd - dir * arrowSize;
    glm::vec2 left = base + perp * arrowSize * 0.4f;
    glm::vec2 right = base - perp * arrowSize * 0.4f;

    c.drawTriangleFilled(canvasEnd, left, right, color);
}

static void s_drawPlaneHandle(Canvas &c, const glm::vec2 quad[4], Color4 color)
{
    glm::vec2 offset = c.absolutePosition;
    c.drawQuadFilled(quad[0] - offset, quad[1] - offset, quad[2] - offset, quad[3] - offset, color);
    Color4 borderColor = Color4::fromLinear(color.r, color.g, color.b, 1.0f);
    c.drawQuadStroke(quad[0] - offset, quad[1] - offset, quad[2] - offset, quad[3] - offset, borderColor, 1.5f);
}

static void s_drawScaleHandle(Canvas &c, glm::vec2 pos, float size, Color4 color)
{
    glm::vec2 local = pos - c.absolutePosition;
    float hs = size * 0.5f;
    c.drawQuadFilled(local + glm::vec2(-hs, -hs), local + glm::vec2(hs, -hs),
                     local + glm::vec2(hs, hs), local + glm::vec2(-hs, hs), color);
}

static void s_drawValueLabel(Canvas &c, glm::vec2 pos, const char *text, Color4 textColor)
{
    glm::vec2 local = pos - c.absolutePosition;
    glm::vec2 labelPos = local + glm::vec2(20.0f, -10.0f);
    c.drawText(text, labelPos, 14.0f, textColor, 32);
}

static void s_draw3DRing(Canvas &c, const glm::vec3 &center, const glm::vec3 &axis, float radius,
                         const glm::mat4 &viewProj, glm::vec2 vpPos, glm::vec2 vpSize,
                         Color4 color, float thickness)
{
    glm::vec3 up = glm::normalize(axis);
    glm::vec3 right;
    if (std::abs(up.y) < 0.99f) {
        right = glm::normalize(glm::cross(up, glm::vec3(0, 1, 0)));
    } else {
        right = glm::normalize(glm::cross(up, glm::vec3(1, 0, 0)));
    }
    glm::vec3 forward = glm::cross(right, up);

    glm::vec2 screenCenter = s_worldToScreen(center, viewProj, vpPos, vpSize);
    if (screenCenter.x < -9000.0f) return;

    glm::vec2 screenRight = s_worldToScreen(center + right * radius, viewProj, vpPos, vpSize);
    glm::vec2 screenForward = s_worldToScreen(center + forward * radius, viewProj, vpPos, vpSize);
    if (screenRight.x < -9000.0f || screenForward.x < -9000.0f) return;

    glm::vec2 a = screenRight - screenCenter;
    glm::vec2 b = screenForward - screenCenter;

    float A = glm::dot(a, a) + glm::dot(b, b);
    float B = 2.0f * (a.x * a.y + b.x * b.y);
    float C = a.x * a.x + b.x * b.x - a.y * a.y - b.y * b.y;

    float disc = std::sqrt(C * C + B * B);
    float semiMajor = std::sqrt((A + disc) * 0.5f);
    float semiMinor = std::sqrt(std::max((A - disc) * 0.5f, 0.01f));
    float angle = 0.5f * std::atan2(B, C);

    glm::vec2 local = screenCenter - c.absolutePosition;
    c.drawEllipseStroke(local, semiMajor, semiMinor, angle * 180.0f / PI, color, thickness);
}

static void s_draw3DRotationArc(Canvas &c, const glm::vec3 &center, const glm::vec3 &axis, float radius,
                                float startAngle, float deltaAngle, const glm::mat4 &viewProj,
                                glm::vec2 vpPos, glm::vec2 vpSize, Color4 fillColor)
{
    if (std::abs(deltaAngle) < 0.001f) return;

    glm::vec3 up = glm::normalize(axis);
    glm::vec3 right;
    if (std::abs(up.y) < 0.99f) {
        right = glm::normalize(glm::cross(up, glm::vec3(0, 1, 0)));
    } else {
        right = glm::normalize(glm::cross(up, glm::vec3(1, 0, 0)));
    }
    glm::vec3 forward = glm::cross(right, up);

    int segments = std::max(8, std::min(32, static_cast<int>(std::abs(deltaAngle) / PI * 24.0f)));
    glm::vec2 offset = c.absolutePosition;

    glm::vec2 centerScreen = s_worldToScreen(center, viewProj, vpPos, vpSize) - offset;
    glm::vec2 prevPt = centerScreen;

    for (int i = 0; i <= segments; i++) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float angle = startAngle + t * deltaAngle;
        glm::vec3 worldPt = center + (right * std::cos(angle) + forward * std::sin(angle)) * radius;
        glm::vec2 screenPt = s_worldToScreen(worldPt, viewProj, vpPos, vpSize);

        if (screenPt.x > -9000.0f && i > 0) {
            c.drawTriangleFilled(centerScreen, prevPt, screenPt - offset, fillColor);
        }

        prevPt = (screenPt.x > -9000.0f) ? screenPt - offset : prevPt;
    }
}

void Gizmo::Impl::drawTranslate(GizmoAxis hovered, bool active, glm::vec2 mouse)
{
    glm::vec2 center = s_worldToScreen(gizmoCenter, viewProjMatrix, vpPos(), vpSize());

    glm::vec2 xScreen = s_worldToScreen(gizmoCenter + axisX * worldScale, viewProjMatrix, vpPos(), vpSize());
    glm::vec2 yScreen = s_worldToScreen(gizmoCenter + axisY * worldScale, viewProjMatrix, vpPos(), vpSize());
    glm::vec2 zScreen = s_worldToScreen(gizmoCenter + axisZ * worldScale, viewProjMatrix, vpPos(), vpSize());

    auto drawPlaneHandle = [&](GizmoAxis a) {
        glm::vec3 corners[4];
        getPlaneHandleQuad(a, corners);
        glm::vec2 quad[4];
        for (int i = 0; i < 4; i++) {
            quad[i] = s_worldToScreen(corners[i], viewProjMatrix, vpPos(), vpSize());
        }
        bool isHovered = (hovered == a) || (active && activeAxis == a);
        Color4 color = s_getPlaneColor(a, isHovered, active && activeAxis == a);
        s_drawPlaneHandle(*canvas, quad, color);
    };

    drawPlaneHandle(GizmoAxis::XY);
    drawPlaneHandle(GizmoAxis::XZ);
    drawPlaneHandle(GizmoAxis::YZ);

    bool xActive = active && activeAxis == GizmoAxis::X;
    bool yActive = active && activeAxis == GizmoAxis::Y;
    bool zActive = active && activeAxis == GizmoAxis::Z;

    s_drawArrow(*canvas, center, xScreen, s_getAxisColor(GizmoAxis::X, hovered == GizmoAxis::X, xActive), config.thickness, config.arrowSize);
    s_drawArrow(*canvas, center, yScreen, s_getAxisColor(GizmoAxis::Y, hovered == GizmoAxis::Y, yActive), config.thickness, config.arrowSize);
    s_drawArrow(*canvas, center, zScreen, s_getAxisColor(GizmoAxis::Z, hovered == GizmoAxis::Z, zActive), config.thickness, config.arrowSize);

    if (active && activeOp == GizmoOperation::TRANSLATE) {
        char text[64];
        glm::vec3 t = accumulatedTranslation;
        if (activeAxis == GizmoAxis::X) {
            snprintf(text, sizeof(text), "X: %.2f", t.x);
        } else if (activeAxis == GizmoAxis::Y) {
            snprintf(text, sizeof(text), "Y: %.2f", t.y);
        } else if (activeAxis == GizmoAxis::Z) {
            snprintf(text, sizeof(text), "Z: %.2f", t.z);
        } else {
            snprintf(text, sizeof(text), "%.2f, %.2f, %.2f", t.x, t.y, t.z);
        }
        s_drawValueLabel(*canvas, mouse, text, COL_WHITE);
    }
}

void Gizmo::Impl::drawRotate(GizmoAxis hovered, bool active)
{
    float ringRadius = worldScale * config.ringRadius;

    bool xActive = active && activeAxis == GizmoAxis::X;
    bool yActive = active && activeAxis == GizmoAxis::Y;
    bool zActive = active && activeAxis == GizmoAxis::Z;

    if (active) {
        if (activeAxis == GizmoAxis::X) {
            s_draw3DRing(*canvas, gizmoCenter, axisX, ringRadius, viewProjMatrix, vpPos(), vpSize(),
                         s_getAxisColor(GizmoAxis::X, hovered == GizmoAxis::X, xActive), config.thickness);
        } else if (activeAxis == GizmoAxis::Y) {
            s_draw3DRing(*canvas, gizmoCenter, axisY, ringRadius, viewProjMatrix, vpPos(), vpSize(),
                         s_getAxisColor(GizmoAxis::Y, hovered == GizmoAxis::Y, yActive), config.thickness);
        } else if (activeAxis == GizmoAxis::Z) {
            s_draw3DRing(*canvas, gizmoCenter, axisZ, ringRadius, viewProjMatrix, vpPos(), vpSize(),
                         s_getAxisColor(GizmoAxis::Z, hovered == GizmoAxis::Z, zActive), config.thickness);
        }
    } else {
        s_draw3DRing(*canvas, gizmoCenter, axisX, ringRadius, viewProjMatrix, vpPos(), vpSize(),
                     s_getAxisColor(GizmoAxis::X, hovered == GizmoAxis::X, xActive), config.thickness);
        s_draw3DRing(*canvas, gizmoCenter, axisY, ringRadius, viewProjMatrix, vpPos(), vpSize(),
                     s_getAxisColor(GizmoAxis::Y, hovered == GizmoAxis::Y, yActive), config.thickness);
        s_draw3DRing(*canvas, gizmoCenter, axisZ, ringRadius, viewProjMatrix, vpPos(), vpSize(),
                     s_getAxisColor(GizmoAxis::Z, hovered == GizmoAxis::Z, zActive), config.thickness);
    }

    if (active && std::abs(accumulatedRotation) > 0.001f) {
        glm::vec3 axis = getOrientedAxis(activeAxis);
        Color4 arcColor = {1.0f, 0.863f, 0.251f, 0.471f};
        s_draw3DRotationArc(*canvas, gizmoCenter, axis, ringRadius * 0.9f, dragStartAngle, accumulatedRotation,
                            viewProjMatrix, vpPos(), vpSize(), arcColor);

        glm::vec2 centerScreen = s_worldToScreen(gizmoCenter, viewProjMatrix, vpPos(), vpSize());
        float degrees = accumulatedRotation * 180.0f / PI;
        char text[32];
        snprintf(text, sizeof(text), "%.1f deg", degrees);

        float labelAngle = dragStartAngle + accumulatedRotation * 0.5f;
        glm::vec2 labelPos = centerScreen + glm::vec2(std::cos(labelAngle) * 80.0f, std::sin(labelAngle) * 80.0f);
        s_drawValueLabel(*canvas, labelPos, text, COL_WHITE);
    }
}

void Gizmo::Impl::drawScale(GizmoAxis hovered, bool active, glm::vec2 mouse)
{
    glm::vec2 center = s_worldToScreen(gizmoCenter, viewProjMatrix, vpPos(), vpSize());

    glm::vec2 xScreen = s_worldToScreen(gizmoCenter + axisX * worldScale, viewProjMatrix, vpPos(), vpSize());
    glm::vec2 yScreen = s_worldToScreen(gizmoCenter + axisY * worldScale, viewProjMatrix, vpPos(), vpSize());
    glm::vec2 zScreen = s_worldToScreen(gizmoCenter + axisZ * worldScale, viewProjMatrix, vpPos(), vpSize());

    auto drawPlaneHandle = [&](GizmoAxis a) {
        glm::vec3 corners[4];
        getPlaneHandleQuad(a, corners);
        glm::vec2 quad[4];
        for (int i = 0; i < 4; i++) {
            quad[i] = s_worldToScreen(corners[i], viewProjMatrix, vpPos(), vpSize());
        }
        bool isHovered = (hovered == a) || (active && activeAxis == a);
        Color4 color = s_getPlaneColor(a, isHovered, active && activeAxis == a);
        s_drawPlaneHandle(*canvas, quad, color);
    };

    drawPlaneHandle(GizmoAxis::XY);
    drawPlaneHandle(GizmoAxis::XZ);
    drawPlaneHandle(GizmoAxis::YZ);

    bool xActive = active && activeAxis == GizmoAxis::X;
    bool yActive = active && activeAxis == GizmoAxis::Y;
    bool zActive = active && activeAxis == GizmoAxis::Z;
    bool allActive = active && activeAxis == GizmoAxis::XYZ;

    glm::vec2 offset = canvas->absolutePosition;

    canvas->drawLine(center - offset, xScreen - offset, s_getAxisColor(GizmoAxis::X, hovered == GizmoAxis::X, xActive), config.thickness);
    canvas->drawLine(center - offset, yScreen - offset, s_getAxisColor(GizmoAxis::Y, hovered == GizmoAxis::Y, yActive), config.thickness);
    canvas->drawLine(center - offset, zScreen - offset, s_getAxisColor(GizmoAxis::Z, hovered == GizmoAxis::Z, zActive), config.thickness);

    s_drawScaleHandle(*canvas, xScreen, config.handleSize, s_getAxisColor(GizmoAxis::X, hovered == GizmoAxis::X, xActive));
    s_drawScaleHandle(*canvas, yScreen, config.handleSize, s_getAxisColor(GizmoAxis::Y, hovered == GizmoAxis::Y, yActive));
    s_drawScaleHandle(*canvas, zScreen, config.handleSize, s_getAxisColor(GizmoAxis::Z, hovered == GizmoAxis::Z, zActive));

    Color4 centerCol = (hovered == GizmoAxis::XYZ || allActive) ? COL_ACTIVE : COL_WHITE;
    s_drawScaleHandle(*canvas, center, config.handleSize * 1.2f, centerCol);

    if (active && activeOp == GizmoOperation::SCALE) {
        char text[64];
        glm::vec3 s = accumulatedScale;
        if (activeAxis == GizmoAxis::X) {
            snprintf(text, sizeof(text), "X: %.2f", s.x);
        } else if (activeAxis == GizmoAxis::Y) {
            snprintf(text, sizeof(text), "Y: %.2f", s.y);
        } else if (activeAxis == GizmoAxis::Z) {
            snprintf(text, sizeof(text), "Z: %.2f", s.z);
        } else if (activeAxis == GizmoAxis::XYZ) {
            snprintf(text, sizeof(text), "%.2f", s.x);
        } else {
            snprintf(text, sizeof(text), "%.2f, %.2f, %.2f", s.x, s.y, s.z);
        }
        s_drawValueLabel(*canvas, mouse, text, COL_WHITE);
    }
}

Gizmo::Gizmo(UIBase2D *parent) : m_impl(std::make_unique<Impl>())
{
    auto canvasOwned = std::make_unique<GizmoCanvas>();
    canvasOwned->size = UDim2::fromScale(1.0f, 1.0f);
    canvasOwned->backgroundTransparency = 1.0f;
    canvasOwned->zIndex = 100;
    m_impl->canvas = static_cast<GizmoCanvas *>(parent->addChild(std::move(canvasOwned)));
}

Gizmo::~Gizmo()
{
    if (m_impl && m_impl->canvas && m_impl->canvas->parent) {
        m_impl->canvas->parent->removeChild(m_impl->canvas);
    }
}
Gizmo::Gizmo(Gizmo &&) noexcept = default;
Gizmo &Gizmo::operator=(Gizmo &&) noexcept = default;

void Gizmo::reset()
{
    m_impl->state = Impl::State::IDLE;
    m_impl->hoveredAxis = GizmoAxis::NONE;
    m_impl->activeAxis = GizmoAxis::NONE;
    m_impl->firstFrame = true;
    m_impl->accumulatedRotation = 0.0f;
    m_impl->accumulatedTranslation = glm::vec3(0.0f);
    m_impl->accumulatedScale = glm::vec3(1.0f);
}

Canvas &Gizmo::canvas() { return *m_impl->canvas; }
GizmoConfig &Gizmo::config() { return m_impl->config; }
const GizmoConfig &Gizmo::config() const { return m_impl->config; }

GizmoResult Gizmo::update(const GizmoParams &params)
{
    GizmoResult result;
    result.operation = params.operation;

    if (m_impl->vpSize().x < 1.0f || m_impl->vpSize().y < 1.0f) {
        return result;
    }

    bool hasInput = m_impl->canvas->mouseDown || m_impl->canvas->mouseUp ||
                    m_impl->canvas->mousePos != m_impl->prevMousePos;
    bool paramsChanged = params != m_impl->prevParams;

    if (!hasInput && !paramsChanged && !m_impl->firstFrame && m_impl->state != Impl::State::DRAGGING) {
        return m_impl->prevResult;
    }

    m_impl->prevParams = params;
    m_impl->prevMousePos = m_impl->canvas->mousePos;

    m_impl->viewMatrix = params.view;
    m_impl->projMatrix = params.projection;
    m_impl->viewProjMatrix = params.projection * params.view;
    m_impl->invViewProjMatrix = glm::inverse(m_impl->viewProjMatrix);

    m_impl->gizmoCenter = glm::vec3(params.objectTransform * glm::vec4(params.pivot, 1.0f));
    m_impl->updateOrientation(params.objectTransform, params.space);

    float centerDist = glm::length(m_impl->gizmoCenter - m_impl->lastGizmoCenter);
    if (m_impl->firstFrame || centerDist > 0.001f) {
        if (m_impl->state != Impl::State::DRAGGING) {
            m_impl->state = Impl::State::IDLE;
            m_impl->hoveredAxis = GizmoAxis::NONE;
            m_impl->activeAxis = GizmoAxis::NONE;
        }
        m_impl->lastGizmoCenter = m_impl->gizmoCenter;
        m_impl->firstFrame = false;
    }

    glm::mat4 invView = glm::inverse(params.view);
    m_impl->cameraPos = glm::vec3(invView[3]);
    m_impl->cameraDir = -glm::normalize(glm::vec3(invView[2]));

    m_impl->computeWorldScale();

    glm::vec2 mouse = m_impl->canvas->mousePos;
    bool mouseClicked = m_impl->canvas->mouseDown;
    bool mouseReleased = m_impl->canvas->mouseUp;
    m_impl->canvas->mouseDown = false;
    m_impl->canvas->mouseUp = false;

    glm::vec2 vp = m_impl->vpPos();
    glm::vec2 vs = m_impl->vpSize();
    bool inViewport = mouse.x >= vp.x && mouse.x <= vp.x + vs.x &&
                      mouse.y >= vp.y && mouse.y <= vp.y + vs.y;

    GizmoOperation op = params.operation;

    switch (m_impl->state) {
    case Impl::State::IDLE:
        if (inViewport) {
            switch (op) {
            case GizmoOperation::TRANSLATE:
                m_impl->hoveredAxis = m_impl->hitTestTranslate(mouse);
                break;
            case GizmoOperation::ROTATE:
                m_impl->hoveredAxis = m_impl->hitTestRotate(mouse);
                break;
            case GizmoOperation::SCALE:
                m_impl->hoveredAxis = m_impl->hitTestScale(mouse);
                break;
            case GizmoOperation::COMBINED:
                m_impl->hoveredAxis = m_impl->hitTestTranslate(mouse);
                if (m_impl->hoveredAxis == GizmoAxis::NONE) m_impl->hoveredAxis = m_impl->hitTestScale(mouse);
                if (m_impl->hoveredAxis == GizmoAxis::NONE) m_impl->hoveredAxis = m_impl->hitTestRotate(mouse);
                break;
            }

            if (m_impl->hoveredAxis != GizmoAxis::NONE) {
                m_impl->state = Impl::State::HOVERING;
            }
        }
        break;

    case Impl::State::HOVERING:
        switch (op) {
        case GizmoOperation::TRANSLATE:
            m_impl->hoveredAxis = m_impl->hitTestTranslate(mouse);
            break;
        case GizmoOperation::ROTATE:
            m_impl->hoveredAxis = m_impl->hitTestRotate(mouse);
            break;
        case GizmoOperation::SCALE:
            m_impl->hoveredAxis = m_impl->hitTestScale(mouse);
            break;
        case GizmoOperation::COMBINED:
            m_impl->hoveredAxis = m_impl->hitTestTranslate(mouse);
            if (m_impl->hoveredAxis == GizmoAxis::NONE) m_impl->hoveredAxis = m_impl->hitTestScale(mouse);
            if (m_impl->hoveredAxis == GizmoAxis::NONE) m_impl->hoveredAxis = m_impl->hitTestRotate(mouse);
            break;
        }

        if (m_impl->hoveredAxis == GizmoAxis::NONE) {
            m_impl->state = Impl::State::IDLE;
        } else if (mouseClicked) {
            m_impl->state = Impl::State::DRAGGING;
            m_impl->activeAxis = m_impl->hoveredAxis;
            m_impl->activeOp = op;

            m_impl->accumulatedRotation = 0.0f;
            m_impl->accumulatedTranslation = glm::vec3(0.0f);
            m_impl->accumulatedScale = glm::vec3(1.0f);

            if (op == GizmoOperation::TRANSLATE || op == GizmoOperation::SCALE || op == GizmoOperation::COMBINED) {
                if (m_impl->activeAxis == GizmoAxis::XY) {
                    m_impl->dragPlaneNormal = m_impl->axisZ;
                } else if (m_impl->activeAxis == GizmoAxis::XZ) {
                    m_impl->dragPlaneNormal = m_impl->axisY;
                } else if (m_impl->activeAxis == GizmoAxis::YZ) {
                    m_impl->dragPlaneNormal = m_impl->axisX;
                } else {
                    glm::vec3 axisDir = m_impl->getOrientedAxis(m_impl->activeAxis);
                    glm::vec3 toCamera = glm::normalize(m_impl->cameraPos - m_impl->gizmoCenter);
                    glm::vec3 perp = glm::cross(axisDir, toCamera);
                    if (glm::length(perp) > 0.001f) {
                        m_impl->dragPlaneNormal = glm::normalize(glm::cross(axisDir, perp));
                    } else {
                        m_impl->dragPlaneNormal = m_impl->axisY;
                    }
                }
            }

            glm::vec3 rayOrigin, rayDir;
            s_screenToWorldRay(mouse, m_impl->invViewProjMatrix, m_impl->vpPos(), m_impl->vpSize(), rayOrigin, rayDir);
            float t = s_rayPlaneIntersect(rayOrigin, rayDir, m_impl->gizmoCenter, m_impl->dragPlaneNormal);
            m_impl->dragStartHitPoint = (t > 0) ? rayOrigin + rayDir * t : m_impl->gizmoCenter;

            glm::vec2 center = s_worldToScreen(m_impl->gizmoCenter, m_impl->viewProjMatrix, m_impl->vpPos(), m_impl->vpSize());
            m_impl->dragStartAngle = std::atan2(mouse.y - center.y, mouse.x - center.x);
            m_impl->dragCurrentAngle = m_impl->dragStartAngle;
            m_impl->dragStartDistance = s_distanceToPoint2D(mouse, center);
        }
        break;

    case Impl::State::DRAGGING:
        if (mouseReleased) {
            m_impl->state = Impl::State::IDLE;
            m_impl->activeAxis = GizmoAxis::NONE;
        } else {
            result.active = true;
            result.axis = m_impl->activeAxis;

            switch (m_impl->activeOp) {
            case GizmoOperation::TRANSLATE:
            case GizmoOperation::COMBINED:
                result.deltaPosition = m_impl->computeTranslationDelta(mouse);
                break;
            case GizmoOperation::ROTATE: {
                float rotDelta = m_impl->computeRotationDelta(mouse);
                result.rotationDegrees = rotDelta * 180.0f / PI;
                result.deltaRotation = s_getAxisVector(m_impl->activeAxis) * rotDelta;
            } break;
            case GizmoOperation::SCALE:
                result.deltaScale = m_impl->computeScaleDelta(mouse);
                break;
            }
        }
        break;
    }

    result.hovered = (m_impl->state == Impl::State::HOVERING || m_impl->state == Impl::State::DRAGGING);
    if (!result.active) {
        result.axis = m_impl->hoveredAxis;
    }

    m_impl->canvas->clear();

    bool isActive = (m_impl->state == Impl::State::DRAGGING);

    switch (op) {
    case GizmoOperation::TRANSLATE:
        m_impl->drawTranslate(m_impl->hoveredAxis, isActive, mouse);
        break;
    case GizmoOperation::ROTATE:
        m_impl->drawRotate(m_impl->hoveredAxis, isActive);
        break;
    case GizmoOperation::SCALE:
        m_impl->drawScale(m_impl->hoveredAxis, isActive, mouse);
        break;
    case GizmoOperation::COMBINED:
        m_impl->drawTranslate(m_impl->hoveredAxis, isActive, mouse);
        m_impl->drawRotate(m_impl->hoveredAxis, isActive);
        m_impl->drawScale(m_impl->hoveredAxis, isActive, mouse);
        break;
    }

    m_impl->prevResult = result;
    return result;
}

} // namespace Amethyst
