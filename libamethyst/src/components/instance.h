/*
 * Base class for all UI elements
 */

#ifndef AMETHYST__INSTANCE_H
#define AMETHYST__INSTANCE_H

#include "math/math.h"
#include "modules/event_signal.h"
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Amethyst {

class UIObject;
class UILayer;

enum InstanceFlags : uint8_t {
    FLAG_NONE = 0,
    FLAG_DIRTY = 1 << 0,
    FLAG_CHILD_DIRTY = 1 << 1,
};

/**
 * @brief Bitmask of the type branches a node belongs to
 */
enum InstanceKind : uint16_t {
    KIND_BASE = 0,
    KIND_UI_OBJECT = 1 << 0,
    KIND_UI_LAYER = 1 << 1,
};

class Instance {
  public:
    Instance() = default;
    virtual ~Instance();

    void reparent(Instance *newParent);
    virtual Instance *addChild(std::unique_ptr<Instance> child);
    virtual std::unique_ptr<Instance> removeChild(Instance *child);
    std::vector<std::unique_ptr<Instance>> removeAllChildren();

    template <typename T, typename... Args>
    requires std::derived_from<T, Instance>
    T *add(Args &&...args)
    {
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        T *raw = child.get();
        addChild(std::move(child));
        return raw;
    }

    virtual std::vector<Instance *> getHittableInstances();
    virtual int32_t getZIndex() const { return 0; }
    virtual bool containsPoint(const vec2 &) const { return false; }
    virtual bool isHitTestVisible() const { return false; }
    virtual bool getClipsDescendants() const { return false; }

    /**
     * @brief Marks this node for repaint and its ancestors for traversal.
     */
    void markDirty();

    /**
     * @brief Marks this node and every descendant for repaint.
     *
     * For changes descendants inherit, such as visibility, clipping or geometry.
     */
    void markSubtreeDirty();

    void markChildrenDirty();

    template <typename T> T *as() { return dynamic_cast<T *>(this); }
    template <typename T> const T *as() const { return dynamic_cast<const T *>(this); }

    UIObject *asUiObject() { return (kind & KIND_UI_OBJECT) ? reinterpret_cast<UIObject *>(this) : nullptr; }
    UILayer *asLayer() { return (kind & KIND_UI_LAYER) ? reinterpret_cast<UILayer *>(this) : nullptr; }

    Instance *findFirstChild(const std::string &childName) const;

    template <typename T> T *findFirstChildOfClass(const std::string &childName) const
    {
        for (auto &child : m_children) {
            if (auto *casted = dynamic_cast<T *>(child.get()); casted && child->name == childName) return casted;
        }
        return nullptr;
    }

    template <typename T> T *findFirstChildOfClass() const
    {
        for (auto &child : m_children) {
            if (auto *casted = dynamic_cast<T *>(child.get())) return casted;
        }
        return nullptr;
    }

    Instance *findFirstDescendant(const std::string &descendantName) const;

    const std::vector<std::unique_ptr<Instance>> &getChildren() const { return m_children; }

  public:
    uint8_t flags = FLAG_NONE;
    uint16_t kind = KIND_BASE;
    std::string name;
    Instance *parent = nullptr;
    EventSignal<void(Instance *)> onDestroy;

  protected:
    std::vector<std::unique_ptr<Instance>> m_children;
};

} // namespace Amethyst

#endif // AMETHYST__INSTANCE_H
