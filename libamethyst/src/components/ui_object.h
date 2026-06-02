/*
 * Base class for drawable UI objects (frames, buttons, labels)
 */

#ifndef AMETHYST__UI_OBJECT_H
#define AMETHYST__UI_OBJECT_H

#include "components/common.h"
#include "components/extensions/ui_extension.h"
#include "components/input_events.h"
#include "components/properties.h"
#include "components/ui_base_2d.h"
#include "rendering/instance_data.h"
#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace Amethyst {

class Window;

class UIObject : public UIBase2D {
  public:
    UIObject();
    virtual ~UIObject();
    UIObject(const UIObject &) = delete;
    UIObject &operator=(const UIObject &) = delete;
    UIObject(UIObject &&) = default;
    UIObject &operator=(UIObject &&) = default;

    void computeAbsolutes(glm::vec2 parentSize, glm::vec2 parentPos, Degrees parentRotation);
    InstanceData createInstanceData() const;
    Window *getWindow();

    glm::vec4 computeChildClipRect() const;

    template <typename T> T *getExtension()
    {
        auto it = m_extensions.find(std::type_index(typeid(T)));
        return it != m_extensions.end() ? static_cast<T *>(it->second.get()) : nullptr;
    }

    template <typename T, typename... Args> T *addExtension(Args &&...args)
    {
        auto key = std::type_index(typeid(T));
        auto [it, inserted] = m_extensions.try_emplace(key, std::make_unique<T>(this, std::forward<Args>(args)...));
        return static_cast<T *>(it->second.get());
    }

    template <typename T> void removeExtension() { m_extensions.erase(std::type_index(typeid(T))); }

    bool isVisible() const;
    int32_t getRelativeZIndex() const { return m_uiObjProps.zIndex; }
    int32_t getAbsoluteZIndex() const;
    int32_t getZIndex() const override;
    bool isHitTestVisible() const override { return m_uiObjProps.visible && m_uiObjProps.interactable; }
    bool getClipsDescendants() const override { return static_cast<bool>(m_uiObjProps.clipsDescendants); }

    bool setBaseProperties(BaseProperties props);
    const BaseProperties &getBaseProperties() const { return m_uiObjProps; }

  protected:
    friend class Window;
    virtual EventResult onMouseEnter(void);
    virtual EventResult onMouseLeave(void);
    virtual EventResult onMouseMoved(uint32_t x, uint32_t y);
    virtual EventResult onMouseScrollUp(void) { return EventResult::PROPAGATE; }
    virtual EventResult onMouseScrollDown(void) { return EventResult::PROPAGATE; }

    virtual EventResult onInputBegan(const InputObject &input);
    virtual EventResult onInputChanged(const InputObject &input);
    virtual EventResult onInputEnded(const InputObject &input);

  public:
    std::function<void(bool hovered)> onHoverChanged;
    std::function<EventResult(const InputObject &)> onInputBeganCb;
    std::function<EventResult(const InputObject &)> onInputChangedCb;
    std::function<EventResult(const InputObject &)> onInputEndedCb;

  protected:
    BaseProperties m_uiObjProps;

  private:
    std::unordered_map<std::type_index, std::unique_ptr<UIExtension>> m_extensions;
};

} // namespace Amethyst

#endif // AMETHYST__UI_OBJECT_H
