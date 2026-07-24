#ifndef AMETHYST__CONTEXT_MENU_H
#define AMETHYST__CONTEXT_MENU_H

#include "components/frame.h"
#include "components/popup.h"
#include "components/properties.h"
#include "modules/event_signal.h"
#include "utils/am_assert.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Amethyst {

class RadioGroup;

class ContextMenu : public Popup {
  public:
    ContextMenu();

    void resolveStyle() override;

    enum class Kind {
        ACTION,    // simple fire cb on click
        TOGGLE,    // checkbox
        SEPARATOR, // duh
        SUBMENU,   // pure submenu entry
        RADIO      // radio button style
    };

    class ItemData {
      public:
        explicit ItemData(Kind kind) : kind(kind) {}
        virtual ~ItemData() = default;

        /**
         * @brief Cast this item to T, dynamic_cast-checked in debug, free in release.
         */
        template <typename T> T &as()
        {
            AM_ASSERT(dynamic_cast<T *>(this) != nullptr, "ItemData cast to the wrong concrete type");
            return static_cast<T &>(*this);
        }

        const Kind kind;
    };

    class ItemView {
      public:
        explicit ItemView(Kind kind) : kind(kind) {}
        virtual ~ItemView() = default;

        virtual Frame *create(ContextMenu &owner);
        virtual void bind(ItemData &item) { m_boundItem = &item; }

        Frame *row() const { return m_row; }

        const Kind kind;

      protected:
        ContextMenu *m_owner = nullptr;
        Frame *m_row = nullptr;
        ItemData *m_boundItem = nullptr;
    };

    class ActionItemData : public ItemData {
      public:
        ActionItemData() : ItemData(Kind::ACTION) {}

        std::function<void()> onActivate;
    };

    class ToggleItemData : public ItemData {
      public:
        explicit ToggleItemData(std::function<void(bool)> cb = {}) : ItemData(Kind::TOGGLE), onToggled(std::move(cb)) {}

        bool value = false;
        std::function<void(bool)> onToggled;
    };

    class SeparatorItemData : public ItemData {
      public:
        SeparatorItemData() : ItemData(Kind::SEPARATOR) {}
    };

    class SubmenuItemData : public ItemData {
      public:
        SubmenuItemData() : ItemData(Kind::SUBMENU) {}

        std::vector<std::unique_ptr<ItemData>> items;
    };

    class RadioItemData : public ItemData {
      public:
        RadioItemData() : ItemData(Kind::RADIO) {}

        RadioGroup *group = nullptr;
        int32_t value = 0;
    };

    class ActionItemView : public ItemView {
      public:
        ActionItemView() : ItemView(Kind::ACTION) {}

        Frame *create(ContextMenu &owner) override;

      protected:
        void activate(ContextMenu &owner, ActionItemData &action);
    };

    class ToggleItemView : public ItemView {
      public:
        ToggleItemView() : ItemView(Kind::TOGGLE) {}

        Frame *create(ContextMenu &owner) override;

      protected:
        void toggle(ToggleItemData &item);
    };

    class SeparatorItemView : public ItemView {
      public:
        SeparatorItemView() : ItemView(Kind::SEPARATOR) {}
    };

    class SubmenuItemView : public ItemView {
      public:
        SubmenuItemView() : ItemView(Kind::SUBMENU) {}

        Frame *create(ContextMenu &owner) override;

      protected:
        void openSubmenu(ContextMenu &owner, SubmenuItemData &submenu, Frame *sourceRow);
    };

    class RadioItemView : public ItemView {
      public:
        RadioItemView() : ItemView(Kind::RADIO) {}

        Frame *create(ContextMenu &owner) override;

      protected:
        void select(RadioItemData &item);
    };

    /**
     * @brief One row factory per Kind. Unset fields fall back to default view for that kind, set a field to have
     * this ContextMenu build your own ItemView subclass for that kind instead.
     */
    struct RowFactories {
        std::function<std::unique_ptr<ItemView>()> action;
        std::function<std::unique_ptr<ItemView>()> toggle;
        std::function<std::unique_ptr<ItemView>()> separator;
        std::function<std::unique_ptr<ItemView>()> submenu;
        std::function<std::unique_ptr<ItemView>()> radio;
    };

    void setItems(std::vector<std::unique_ptr<ItemData>> items);
    std::vector<std::unique_ptr<ItemData>> &items() { return *m_itemsPtr; }

    /**
     * @brief Override the row factory for one or more kinds; unset fields keep whatever this menu was already using (its own
     * built-in defaults, initially).
     * @param factories Per-kind factory overrides.
     */
    void setRowFactories(RowFactories factories);

    void show(UIObject *anchor);
    void showAt(vec2 pos);
    void hide();

    /**
     * @brief Open a nested popup for a SUBMENU item.
     * @param submenu The submenu item being opened.
     * @param sourceRow The row instance that triggered the open, for positioning.
     */
    void openSubmenu(SubmenuItemData &submenu, Frame *sourceRow);

    /**
     * @brief Close whatever submenu is currently open, if any.
     */
    void closeSubmenu();

    bool setContextMenuProperties(const ContextMenuStylePropertiesArgs &props);
    const ContextMenuStyleProperties &getContextMenuProperties() const { return m_cmProps; }

    bool setTextStyleProperties(const TextStylePropertiesArgs &props);
    const TextStyleProperties &getTextStyleProperties() const { return m_textProps; }

    std::function<void(std::string_view)> onItemSelected;
    std::function<void()> onOpenedCb;
    std::function<void()> onClosedCb;

  public:
    int32_t maxVisibleItems;
    float itemHeight;
    float popupWidth;

  protected:
    TextStyleProperties m_textProps{};

  private:
    bool prepareShow();
    void buildMainContent();

    std::vector<std::unique_ptr<ItemData>> m_items;                // this menu's own item vector
    std::vector<std::unique_ptr<ItemData>> *m_itemsPtr = &m_items; // vector currently being shown: m_items,
                                                                   // or a submenu item's items when opened via openSubmenu()
    ContextMenuStyleProperties m_cmProps{};
    RowFactories m_rowFactories;
    std::vector<std::unique_ptr<ItemView>> m_views;

    ContextMenu *m_submenu = nullptr;
    Frame *m_submenuSourceRow = nullptr;

    OverlayLayer *m_overlayPtr;
    EventConnection m_pressConn;
};

} // namespace Amethyst

#endif // AMETHYST__CONTEXT_MENU_H
