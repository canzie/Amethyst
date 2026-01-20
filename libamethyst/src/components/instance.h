/*
 * Base class for all UI elements
 */

#ifndef AMETHYST__INSTANCE_H
#define AMETHYST__INSTANCE_H

#include <cstdint>
#include <string>
#include <vector>

namespace Amethyst {

enum InstanceFlags : uint8_t {
    FLAG_NONE = 0,
    FLAG_DIRTY = 1 << 0,
    FLAG_CHILD_DIRTY = 1 << 1,
};

class Instance {
  public:
    Instance() = default;
    Instance(Instance *parent) { setParent(parent); };
    virtual ~Instance();

    void setParent(Instance *newParent);
    virtual void addChild(Instance *child);
    virtual void removeChild(Instance *child);
    virtual std::vector<Instance *> getHittableInstances() { return children; }

    void markDirty();
    void markChildrenDirty();

    template <typename T> T *as() { return dynamic_cast<T *>(this); }

    template <typename T> const T *as() const { return dynamic_cast<const T *>(this); }

  public:
    uint8_t flags = FLAG_NONE;
    std::string name;
    Instance *parent = nullptr;
    std::vector<Instance *> children;
};

} // namespace Amethyst

#endif // AMETHYST__INSTANCE_H
