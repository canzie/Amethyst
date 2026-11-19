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
#include <optional>
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

    /**
     * @brief The size this object takes up inside a parent, with margin and constraints honoured
     * @param parentSize The size of the parent it is placed in
     * @return The size it will occupy
     */
    vec2 resolveSize(vec2 parentSize) const;

    void arrange() override;
    InstanceData createInstanceData() const;
    Window *getWindow();
    const Window *getWindow() const;

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

    template <typename T> const T *getExtension() const
    {
        auto it = m_extensions.find(std::type_index(typeid(T)));
        return it != m_extensions.end() ? static_cast<const T *>(it->second.get()) : nullptr;
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

    /**
     * @brief z values each node owns in its layer's geometry: local layer 0 is the node's own quad,
     * the rest are for what it draws on top of it (text, selection, caret).
     */
    static constexpr int32_t DRAW_LAYERS_PER_NODE = 4;

    int32_t getZIndex() const override { return m_uiObjProps.zIndex; }

    /**
     * @brief Position in its layer's draw order, assigned by UILayer::ensureDrawOrder().
     */
    uint32_t getDrawOrder() const { return m_drawOrder; }

    /**
     * @brief Called by the layer's draw order pass; marks the node dirty when the order moved.
     */
    void setDrawOrder(uint32_t order);

    /**
     * @brief The InstanceData::zIndex for one of this node's allocations.
     * @param localLayer Which of the node's own z values, below DRAW_LAYERS_PER_NODE
     */
    int32_t drawZ(uint32_t localLayer = 0) const
    {
        return static_cast<int32_t>(m_drawOrder) * DRAW_LAYERS_PER_NODE + static_cast<int32_t>(localLayer);
    }
    bool isHitTestVisible() const override { return m_uiObjProps.visible && m_uiObjProps.interactable; }
    bool getClipsDescendants() const override { return static_cast<bool>(m_uiObjProps.clipsDescendants); }

    /**
     * @brief This node receives events of the category, then lets them continue to what is behind
     * it and to its ancestors.
     * @param cat The interaction category to set.
     */
    void propagate(InteractionCategory cat);

    /**
     * @brief This node receives events of the category and stops them there. The default.
     * @param cat The interaction category to set.
     */
    void consume(InteractionCategory cat);

    /**
     * @brief This node is skipped for events of the category, as if it were not there: no hover
     * state, no signals, no tooltip. Its children are still hit-tested. For overlays that must stay
     * invisible to the mouse (drag ghosts, drop hints).
     * @param cat The interaction category to set.
     */
    void ignore(InteractionCategory cat);

    /**
     * @brief Returns whether the node is skipped for the given category.
     * @param cat The interaction category to query.
     * @return True if the ignore flag is set.
     */
    bool ignores(InteractionCategory cat) const;

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
     * @brief How far the content box sits in from each edge: the padding plus the part of the
     * border that lies inside the rect.
     * @param size The size the padding resolves against
     * @return The insets as (top, right, bottom, left)
     */
    vec4 resolveContentInsets(vec2 size) const;

    /**
     * @brief A size grown on the axes automaticSize names to fit this object's content inside its padding and border
     * @param size The size the object asked for, kept wherever it is already larger than the content
     * @return The size with automaticSize honoured
     */
    vec2 applyAutomaticSize(vec2 size) const;

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
     * @brief Bind this node as the given named sub-part (e.g. a TabBar's "tab") and re-resolve.
     * An own part always wins over one inherited through propagateClassesToChildren().
     * @param part The part this node is bound as, NONE to unbind
     * @param owner Component type owning the part, matched by `type#part`; defaults to the part's usual owner
     */
    void bindPart(ComponentPart part, std::optional<ComponentType> owner = std::nullopt);

    /**
     * @brief Hand this node's class set (own and inherited) and its part down to every descendant
     * UIObject as their inherited style. Descendants keep their own classes and own part, and a
     * later call replaces what was inherited before, so a class removed here leaves them too.
     */
    void propagateClassesToChildren();

    /**
     * @brief Replace the classes and part this node inherits from the component that owns it.
     * Inherited classes only match part rules (`.abc#tab`); the inherited part applies when the
     * node has no part of its own.
     * @param classes Class tokens of the owner
     * @param part Part inherited from the owner, NONE for none
     * @param owner Component type owning that part
     */
    void setInheritedStyle(std::span<const StyleKey> classes, ComponentPart part = ComponentPart::NONE,
                           ComponentType owner = ComponentType::UI_OBJECT);

    /**
     * @brief Classes this node inherited from its owner via setInheritedStyle().
     * @return Span over the inherited class tokens
     */
    std::span<const StyleKey> getInheritedClasses() const { return m_part.inheritedClasses; }

    /**
     * @brief The part this node resolves as: its own if bound, otherwise the inherited one.
     * @return The effective part, NONE if the node is not a part
     */
    ComponentPart getPart() const { return m_part.part; }

    /**
     * @brief Apply this node's current GuiState onto every descendant UIObject.
     */
    void propagateGuiStateToChildren();

    /**
     * @brief Add multiple style classes already interned as tokens to this node and re-resolve once.
     * @param tokens Interned class tokens to add
     */
    void addClasses(std::span<const StyleKey> tokens);

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
     * @brief Replace this node's class set with already-interned tokens (e.g. another node's
     * getClasses()) and re-resolve once.
     * @param tokens Interned class tokens to apply
     */
    void setClasses(std::span<const StyleKey> tokens);

    /**
     * @brief Access this node's class tokens.
     * @return Span over the interned class tokens
     */
    std::span<const StyleKey> getClasses() const { return m_classes; }

    /**
     * @brief Own and inherited classes together, deduplicated; what this node hands to descendants.
     * @return The combined class tokens
     */
    std::vector<StyleKey> getEffectiveClasses() const;

  protected:
    /**
     * @brief The size of what this object holds, which automaticSize fits it to: by default the extent of its
     * visible children as its list layout places them, or as they place themselves without one, their scale parts
     * resolving against nothing since the object is being sized by them
     * @return The content size in logical pixels
     */
    virtual vec2 automaticContentSize() const;

    /**
     * @brief Apply an attached layout extension
     */
    void applyLayoutExtensions();

    /**
     * @brief GuiState bits including the DISABLED bit derived from `!interactable`. Parts skip that
     * derivation: compositing widgets make their decorative parts non-interactable so the input
     * reaches the widget, which is structure rather than a disabled state, and parts get their
     * pseudo-state handed down from the owner instead.
     * @return The bitmask resolveStyle() should resolve against
     */
    uint16_t effectiveGuiState() const;

    /**
     * @brief Re-run resolveStyle() if the effective GuiState or the theme generation changed since
     * the last resolve. UIObject::arrange() calls it; arrange() overrides that do not chain up
     * must call it themselves, before laying out.
     */
    void refreshStyleIfStale();

    /**
     * @brief Resolve and merge BaseStyleProperties for a component type, honoring any
     * instance-level overrides pinned by a prior setBaseStyleProperties() call. Also applies
     * the theme padding unless padding was set through setBaseProperties().
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

    /**
     * @brief The press this node consumed was released somewhere else (over another node, or
     * outside every node). Lets press-tracking components drop their pressed state.
     */
    virtual void onPressCancelled(InputType type) { (void)type; }

  public:
    EventSignal<void(bool hovered)> onHoverChanged;
    /// fires during layout whenever the object's absolute position, size or rotation changes
    EventSignal<void()> onAbsoluteRectChanged;
    EventSignal<void(const InputObject &)> onInputBeganCb;
    EventSignal<void(const InputObject &)> onInputChangedCb;
    EventSignal<void(const InputObject &)> onInputEndedCb;

  protected:
    BaseProperties m_uiObjProps;
    BaseStyleProperties m_baseStyle;
    std::vector<StyleKey> m_classes;
    // effective part (own, else inherited) with the inherited classes; passed straight to the Style getters
    StylePart m_part;
    uint16_t m_guiState = GUI_STATE_NONE;
    uint16_t m_lastResolvedGuiState = GUI_STATE_NONE;
    uint64_t m_styleGeneration = 0;
    bool m_renderCulled = false;
    bool m_paddingPinned = false;
    uint32_t m_drawOrder = 0;

  private:
    void updateEffectivePart();

    ComponentPart m_ownPart = ComponentPart::NONE;
    ComponentType m_ownPartOwner = ComponentType::UI_OBJECT;
    ComponentPart m_inheritedPart = ComponentPart::NONE;
    ComponentType m_inheritedPartOwner = ComponentType::UI_OBJECT;

  private:
    std::unordered_map<std::type_index, std::unique_ptr<UIExtension>> m_extensions;
    uint8_t m_eventConsumptionFlags;
    uint8_t m_eventIgnoreFlags = 0;
    std::vector<EventConnection> m_connections;
};

} // namespace Amethyst

#endif // AMETHYST__UI_OBJECT_H
