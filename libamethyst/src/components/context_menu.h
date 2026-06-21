#ifndef AMETHYST__CONTEXT_MENU_H
#define AMETHYST__CONTEXT_MENU_H

#include "components/context_menu_item.h"
#include "components/popup.h"
#include "components/properties.h"
#include "modules/event_signal.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace Amethyst {

class TextButton;

class ContextMenu : public Popup {
  public:
    ContextMenu();

    void setItems(std::vector<ContextMenuItem> items);
    std::vector<ContextMenuItem> &items() { return m_items; }

    void show(UIObject *anchor);
    void hide();

    bool setContextMenuProperties(const ContextMenuStyleProperties &props);
    const ContextMenuStyleProperties &getContextMenuProperties() const { return m_cmProps; }

    bool setTextStyleProperties(const TextStyleProperties &props);
    const TextStyleProperties &getTextStyleProperties() const { return m_textProps; }

    std::function<void(std::string_view)> onItemSelected;
    std::function<void()> onOpenedCb;
    std::function<void()> onClosedCb;

  private:
    void closeSubmenuFrom(size_t depth = 0);
    void buildMainContent();
    void buildSubmenuAtPath(const std::vector<size_t> &path, UIObject *sourceRow);
    void buildContent(Popup *popup, const std::vector<size_t> &path);
    void addItemRows(Instance *container, const std::vector<size_t> &path = {});
    std::vector<ContextMenuItem> &itemsAtPath(const std::vector<size_t> &path);
    std::string buildItemText(const ContextMenuItem &item) const;
    float computeTotalHeight(const std::vector<ContextMenuItem> &items) const;

  public:
    int32_t maxVisibleItems;
    float itemHeight;
    float popupWidth;

  protected:
    TextStyleProperties m_textProps{};

  private:
    std::vector<ContextMenuItem> m_items;
    ContextMenuStyleProperties m_cmProps{};

    std::vector<ContextMenu *> m_submenuStack;
    std::vector<TextButton *> m_submenuSourceRows;
    OverlayLayer *m_overlayPtr;
    EventConnection m_pressConn;
};

} // namespace Amethyst

#endif // AMETHYST__CONTEXT_MENU_H
