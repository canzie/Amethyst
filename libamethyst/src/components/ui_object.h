/*
 * Base class for drawable UI objects (frames, buttons, labels)
 */

#ifndef AMETHYST__UI_OBJECT_H
#define AMETHYST__UI_OBJECT_H

#include "components/common.h"
#include "components/extensions/ui_extension.h"
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
    UIObject() = default;
    virtual ~UIObject();
    UIObject(const UIObject &) = delete;
    UIObject &operator=(const UIObject &) = delete;
    UIObject(UIObject &&) = default;
    UIObject &operator=(UIObject &&) = default;

    void computeAbsolutes(glm::vec2 parentSize, glm::vec2 parentPos, Degrees parentRotation);
    InstanceData createInstanceData() const;
    Window *getWindow();

    glm::vec4 computeChildClipRect() const
    {
        if (!clipsDescendants) {
            return clipRect;
        }
        glm::vec4 myBounds = {absolutePosition.x, absolutePosition.y, absolutePosition.x + absoluteSize.x,
                              absolutePosition.y + absoluteSize.y};
        if (clipRect == glm::vec4(0.0f)) {
            return myBounds;
        }
        return {glm::max(clipRect.x, myBounds.x), glm::max(clipRect.y, myBounds.y),
                glm::min(clipRect.z, myBounds.z), glm::min(clipRect.w, myBounds.w)};
    }

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

  protected:
    friend class Window;
    virtual void onMouseEnter(void);
    virtual void onMouseLeave(void);
    virtual void onMouseMoved(uint32_t x, uint32_t y);
    virtual void onMouseButton1Down(uint32_t x, uint32_t y);
    virtual void onMouseButton1Up(uint32_t x, uint32_t y);
    virtual void onMouseButton1Click(void) {}
    virtual void onMouseButton2Down(uint32_t, uint32_t) {}
    virtual void onMouseButton2Up(uint32_t, uint32_t) {}
    virtual void onMouseButton2Click(void) {}
    virtual bool onMouseScrollUp(void) { return false; }
    virtual bool onMouseScrollDown(void) { return false; }

  public:
    bool active = false;
    glm::vec2 anchorPoint = glm::vec2(0.0f);
    AutomaticSize automaticSize = AutomaticSize::NONE;
    Color3 backgroundColor = {1.0f, 1.0f, 1.0f};
    float backgroundTransparency = 0.0f;
    BorderMode borderMode = BorderMode::OUTLINE;
    float borderPixelSize = 0.0f;
    Color3 borderColor = {0.0f, 0.0f, 0.0f};
    float borderTransparency = 0.0f;
    bool clipsDescendants = false;
    float cornerRadius = 0.0f;
    GuiState guiState = GuiState::IDLE;
    bool interactable = true;
    LayoutOrder layoutOrder = 0;

    UDim4 padding;
    UDim4 margin;

    UDim2 position;
    UDim2 size;
    Degrees rotation = 0.0f;
    bool visible = true;
    int32_t zIndex = 1;

  private:
    std::unordered_map<std::type_index, std::unique_ptr<UIExtension>> m_extensions;
};

} // namespace Amethyst

#endif // AMETHYST__UI_OBJECT_H
