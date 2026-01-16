/*
 * Base class for UI extensions (drag detectors, gradients, layouts, etc.)
 * Extensions add optional functionality to UIObjects without inheritance.
 */

#ifndef AMETHYST__UI_EXTENSION_H
#define AMETHYST__UI_EXTENSION_H

namespace Amethyst {

class UIObject;

class UIExtension {
  public:
    virtual ~UIExtension() = default;

    UIExtension(const UIExtension &) = delete;
    UIExtension &operator=(const UIExtension &) = delete;

    UIObject *getOwner() const { return m_owner; }

  public:
    bool enabled = true;

  protected:
    explicit UIExtension(UIObject *owner) : m_owner(owner) {}

    UIObject *m_owner;
};

} // namespace Amethyst

#endif // AMETHYST__UI_EXTENSION_H
