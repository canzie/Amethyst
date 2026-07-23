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
    void arrange() override;
    InstanceData createInstanceData() const;
    Window *getWindow();

    vec4 computeChildClipRect() const;

    /**
     * @brief Recurse the draw pass into children.
     * @param ctx Draw context forwarded to each child
     */
    void drawChildren(DrawContext &ctx);

    /**
     * @brief Adopts an EventConnection, tying its lifetime to this object.
     * The connection is disconnected automatically when this object is destroyed.
     * @param conn The connection to keep alive.
     */
    void track(EventConnection conn) { m_connections.push_back(std::move(conn)); }

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

    /**
     * @brief Layout-owned cull flag. Hides culled children without touching the user visible property.
     */
    void setRenderCulled(bool culled);
    bool isRenderCulled() const { return m_renderCulled; }

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

    bool setBaseProperties(BasePropertiesArgs props);
    const BaseProperties &getBaseProperties() const { return m_uiObjProps; }

    bool setBaseStyleProperties(BaseStylePropertiesArgs style);
    const BaseStyleProperties &getBaseStyleProperties() const { return m_baseStyle; }

    /**
     * @brief Re-resolve this node's styling from the global theme, its class set and pseudo-state.
     *
     * Called from each component constructor, whenever the class set changes, and from
     * arrange() when the node's effective GuiState changes since the last resolve. The
     * default implementation does nothing; concrete components override it to pull their
     * resolved style structs from Style::instance(), passing effectiveGuiState() through.
     */
    virtual void resolveStyle();

    /**
     * @brief Current GuiState bits set on this node (HOVERED/PRESSED/FOCUSED/CHECKED/SELECTED).
     * Does not include DISABLED, which is derived from `interactable`; see effectiveGuiState().
     * @return The raw, stored pseudo-state bitmask
     */
    uint16_t getGuiState() const { return m_guiState; }

    /**
     * @brief Overwrite this node's GuiState bits and mark it dirty if they changed.
     * @param state New bitmask; callers should only ever set/clear HOVERED/PRESSED/FOCUSED/CHECKED/SELECTED
     */
    void setGuiState(uint16_t state);

    /**
     * @brief Add a style class to this node and re-resolve.
     * @param name Class name; interned to a token and recorded for diagnostics
     */
    void addClass(std::string_view name);

    /**
     * @brief Add a class this component owns (e.g. a #part selector) that setClasses()/removeClass()
     * must never be able to strip. Structural classes sit ahead of the user-facing ones in getClasses().
     * Intended for a component to call on a Frame/UIObject it composes internally (e.g. TabBar on a
     * tab's label Frame), not on itself from a subclass constructor.
     * @param name Class name; interned to a token and recorded for diagnostics
     */
    void addStructuralClass(std::string_view name);

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
    /**
     * @brief Apply an attached layout extension
     */
    void applyLayoutExtensions();

    /**
     * @brief GuiState bits including the DISABLED bit derived from `!interactable`.
     * @return The bitmask resolveStyle() should resolve against
     */
    uint16_t effectiveGuiState() const;

    /**
     * @brief Resolve and merge BaseStyleProperties for a component type, honoring any
     * instance-level overrides pinned by a prior setBaseStyleProperties() call.
     * @param type Component type to resolve against
     */
    void resolveBaseStyle(ComponentType type);

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
    EventSignal<void(bool hovered)> onHoverChanged;
    EventSignal<void(const InputObject &)> onInputBeganCb;
    EventSignal<void(const InputObject &)> onInputChangedCb;
    EventSignal<void(const InputObject &)> onInputEndedCb;

  protected:
    BaseProperties m_uiObjProps;
    BaseStyleProperties m_baseStyle;
    std::vector<StyleKey> m_classes; // [0, m_structuralClassCount) are structural, the rest are user-facing
    size_t m_structuralClassCount = 0;
    uint16_t m_guiState = GUI_STATE_NONE;
    uint16_t m_lastResolvedGuiState = GUI_STATE_NONE;
    bool m_renderCulled = false;

  private:
    std::unordered_map<std::type_index, std::unique_ptr<UIExtension>> m_extensions;
    uint8_t m_eventConsumptionFlags;
    std::vector<EventConnection> m_connections;
};

} // namespace Amethyst

#endif // AMETHYST__UI_OBJECT_H
