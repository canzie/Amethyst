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
#include "modules/event_signal.h"
#include "modules/style.h"
#include "rendering/instance_data.h"
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

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

    virtual void computeAbsolutes(vec2 parentSize, vec2 parentPos, Degrees parentRotation);
    InstanceData createInstanceData() const;
    Window *getWindow();

    vec4 computeChildClipRect() const;

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

    /**
     * @brief Sets the propagate flag for the given category.
     * @param cat The interaction category to set.
     */
    void propagate(InteractionCategory cat);

    /**
     * @brief Sets the consume flag for the given category.
     * @param cat The interaction category to set.
     */
    void consume(InteractionCategory cat);

    /**
     * @brief Returns whether the consume flag is set for the given category.
     * @param cat The interaction category to query.
     * @return True if the consume flag is set.
     */
    bool consumes(InteractionCategory cat) const;

    /**
     * @brief Returns whether the propagate flag is set for the given category.
     * @param cat The interaction category to query.
     * @return True if the propagate flag is set.
     */
    bool propagates(InteractionCategory cat) const;

    bool setBaseProperties(BaseProperties props);
    const BaseProperties &getBaseProperties() const { return m_uiObjProps; }

    bool setBaseStyleProperties(BaseStyleProperties style);
    const BaseStyleProperties &getBaseStyleProperties() const { return m_baseStyle; }

    /**
     * @brief Re-resolve this node's styling from the global theme and its class set.
     *
     * Called from each component constructor and again whenever the class set changes.
     * The default implementation does nothing; concrete components override it to pull
     * their resolved style structs from Style::instance().
     */
    virtual void resolveStyle();

    /**
     * @brief Add a style class to this node and re-resolve.
     * @param name Class name; interned to a token and recorded for diagnostics
     */
    void addClass(std::string_view name);

    /**
     * @brief Remove a style class from this node and re-resolve.
     * @param name Class name to remove; a no-op if not present
     */
    void removeClass(std::string_view name);

    /**
     * @brief Test whether this node carries a style class.
     * @param name Class name to query
     * @return True if the class is present
     */
    bool hasClass(std::string_view name) const;

    /**
     * @brief Replace this node's class set and re-resolve once.
     * @param names Class names to apply
     */
    void setClasses(std::span<const std::string> names);

    /**
     * @brief Replace this node's class set and re-resolve once.
     * @param names Class names to apply
     */
    void setClasses(std::initializer_list<std::string_view> names);

    /**
     * @brief Access this node's class tokens.
     * @return Span over the interned class tokens
     */
    std::span<const StyleKey> getClasses() const { return m_classes; }

  protected:
    friend class Window;
    virtual EventResult onMouseEnter(void);
    virtual EventResult onMouseLeave(void);
    virtual EventResult onMouseMoved(int32_t x, int32_t y);
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
    BaseStyleProperties m_baseStyle;
    std::vector<StyleKey> m_classes;

  private:
    std::unordered_map<std::type_index, std::unique_ptr<UIExtension>> m_extensions;
    uint8_t m_eventConsumptionFlags;
};

} // namespace Amethyst

#endif // AMETHYST__UI_OBJECT_H
