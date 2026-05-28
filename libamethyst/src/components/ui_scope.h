#ifndef AMETHYST__UI_SCOPE_H
#define AMETHYST__UI_SCOPE_H

#include "components/docking_layer.h"
#include "components/dropdown_item.h"
#include "components/instance.h"
#include "components/properties.h"
#include "components/table.h"

#include <functional>
#include <string_view>
#include <vector>

namespace Amethyst {

class Canvas;
class Checkbox;
class Dropdown;
class MenuBar;
class CollapsibleHeader;
class Frame;
class ImageButton;
class ImageLabel;
class InvisibleButton;
class ScrollingFrame;
class SliderFloat;
class SliderInt;
class SliderVec2;
class SliderVec3;
class TabBar;
class TextButton;
class TextInput;
class TextLabel;
class TreeView;

struct CanvasScope;
struct DropdownScope;
struct MenuBarScope;
struct FrameScope;
struct ScrollingFrameScope;
struct SliderFloatScope;
struct SliderIntScope;
struct SliderVec2Scope;
struct SliderVec3Scope;
struct TabScope;
struct TabBarScope;
struct TextInputScope;
struct TextLabelScope;
struct TextButtonScope;
struct ImageLabelScope;
struct ImageButtonScope;
struct InvisibleButtonScope;
struct CheckboxScope;
struct CollapsibleHeaderScope;
struct TableRowScope;
struct TableScope;
struct TreeViewScope;

class UIScope {
  public:
    explicit UIScope(Instance &parent) : m_parent(&parent) {}

    UIScope &canvas(BaseProperties base = {}, std::function<void(CanvasScope &)> fn = {});
    UIScope &dropdown(BaseProperties base = {}, TextProperties text = {}, DropdownProperties ddProps = {},
                      std::function<void(DropdownScope &)> fn = {});
    UIScope &menuBar(BaseProperties base = {}, MenuBarProperties props = {}, std::function<void(MenuBarScope &)> fn = {});
    UIScope &frame(BaseProperties props = {}, std::function<void(FrameScope &)> fn = {});
    UIScope &scrollingFrame(BaseProperties base = {}, ScrollingFrameProperties scroll = {},
                            std::function<void(ScrollingFrameScope &)> fn = {});
    UIScope &textLabel(BaseProperties base = {}, TextProperties text = {}, std::function<void(TextLabelScope &)> fn = {});
    UIScope &textButton(BaseProperties base = {}, TextProperties text = {}, ButtonProperties button = {},
                        std::function<void(TextButtonScope &)> fn = {});
    UIScope &imageLabel(BaseProperties base = {}, ImageProperties image = {}, std::function<void(ImageLabelScope &)> fn = {});
    UIScope &imageButton(BaseProperties base = {}, ImageProperties image = {}, ButtonProperties button = {},
                         std::function<void(ImageButtonScope &)> fn = {});
    UIScope &invisibleButton(BaseProperties props = {}, std::function<void(InvisibleButtonScope &)> fn = {});
    UIScope &checkbox(BaseProperties base = {}, CheckboxProperties props = {}, std::function<void(CheckboxScope &)> fn = {});
    UIScope &collapsibleHeader(BaseProperties base = {}, CollapsibleHeaderProperties props = {},
                               std::function<void(CollapsibleHeaderScope &)> fn = {});
    UIScope &tabBar(BaseProperties base = {}, TabBarProperties props = {}, std::function<void(TabBarScope &)> fn = {});
    UIScope &table(BaseProperties base = {}, TableProperties props = {}, std::function<void(TableScope &)> fn = {});
    UIScope &textInput(BaseProperties base = {}, TextInputProperties props = {}, std::function<void(TextInputScope &)> fn = {});
    UIScope &sliderFloat(BaseProperties base = {}, SliderProperties props = {}, std::function<void(SliderFloatScope &)> fn = {});
    UIScope &sliderInt(BaseProperties base = {}, SliderProperties props = {}, std::function<void(SliderIntScope &)> fn = {});
    UIScope &sliderVec2(BaseProperties base = {}, SliderProperties props = {}, std::function<void(SliderVec2Scope &)> fn = {});
    UIScope &sliderVec3(BaseProperties base = {}, SliderProperties props = {}, std::function<void(SliderVec3Scope &)> fn = {});
    UIScope &treeView(BaseProperties base = {}, TreeViewProperties props = {}, std::function<void(TreeViewScope &)> fn = {});

    Instance &get() const { return *m_parent; }

  protected:
    Instance *m_parent;
};

struct CanvasScope : UIScope {
    Canvas &component;
    explicit CanvasScope(Canvas &c);
};

struct FrameScope : UIScope {
    Frame &component;
    explicit FrameScope(Frame &f);
};

struct ScrollingFrameScope : UIScope {
    ScrollingFrame &component;
    explicit ScrollingFrameScope(ScrollingFrame &sf);
};

struct TextLabelScope : UIScope {
    TextLabel &component;
    explicit TextLabelScope(TextLabel &tl);
};

struct TextButtonScope : UIScope {
    TextButton &component;
    explicit TextButtonScope(TextButton &tb);
};

struct ImageLabelScope : UIScope {
    ImageLabel &component;
    explicit ImageLabelScope(ImageLabel &il);
};

struct ImageButtonScope : UIScope {
    ImageButton &component;
    explicit ImageButtonScope(ImageButton &ib);
};

struct InvisibleButtonScope : UIScope {
    InvisibleButton &component;
    explicit InvisibleButtonScope(InvisibleButton &ib);
};

struct CheckboxScope : UIScope {
    Checkbox &component;
    explicit CheckboxScope(Checkbox &cb);
};

struct CollapsibleHeaderScope : UIScope {
    CollapsibleHeader &component;
    explicit CollapsibleHeaderScope(CollapsibleHeader &ch);
    CollapsibleHeaderScope &header(std::function<void(FrameScope &)> fn);
    CollapsibleHeaderScope &indicator(std::function<void(UIScope &)> fn);
};

struct TabScope {
    Frame &labelFrame;
    Frame &contentFrame;
    TabScope(Frame &lf, Frame &cf);
    TabScope &label(std::function<void(FrameScope &)> fn);
    TabScope &content(std::function<void(FrameScope &)> fn);
};

struct TabBarScope : UIScope {
    TabBar &component;
    explicit TabBarScope(TabBar &tb);
    TabBarScope &tab(std::string_view label, std::function<void(FrameScope &)> fn = {});
    TabBarScope &tab(std::function<void(TabScope &)> fn);
};

struct TableRowScope {
    Table &component;
    std::vector<std::function<void(UIScope &)>> m_pendingCells;
    explicit TableRowScope(Table &t);
    TableRowScope &cell(std::function<void(UIScope &)> fn);
};

struct TableScope : UIScope {
    Table &component;
    bool m_columnsExplicit = false;
    explicit TableScope(Table &t);
    TableScope &column(std::string header, float weight, TableColumnSizing sizing = TableColumnSizing::STRETCH);
    TableScope &column(TableColumn col);
    TableScope &row(std::function<void(TableRowScope &)> fn);
};

struct TextInputScope : UIScope {
    TextInput &component;
    explicit TextInputScope(TextInput &ti);
};

struct SliderFloatScope : UIScope {
    SliderFloat &component;
    explicit SliderFloatScope(SliderFloat &s);
};

struct SliderIntScope : UIScope {
    SliderInt &component;
    explicit SliderIntScope(SliderInt &s);
};

struct SliderVec2Scope : UIScope {
    SliderVec2 &component;
    explicit SliderVec2Scope(SliderVec2 &s);
};

struct SliderVec3Scope : UIScope {
    SliderVec3 &component;
    explicit SliderVec3Scope(SliderVec3 &s);
};

struct TreeViewScope : UIScope {
    TreeView &component;
    explicit TreeViewScope(TreeView &tv);
};

struct DropdownScope {
    Dropdown &component;
    std::vector<DropdownItem> pendingItems;

    explicit DropdownScope(Dropdown &d);

    DropdownScope &action(std::string label, std::function<void()> onSelect = {});
    DropdownScope &toggle(std::string label, std::function<void(bool)> onToggle);
    DropdownScope &separator();
    DropdownScope &submenu(std::string label, std::function<void(DropdownScope &)> fn);
    DropdownScope &items(std::vector<std::string> labels);
};

struct MenuBarScope : UIScope {
    MenuBar &component;
    explicit MenuBarScope(MenuBar &mb);
    MenuBarScope &menuItem(std::string label, std::function<void(DropdownScope &)> fn = {});
};

struct DockScope {
    /**
     * @brief Begin a declarative layout build for an empty DockingLayer.
     * @param layer The DockingLayer to populate. Must have no existing nodes.
     */
    explicit DockScope(DockingLayer &layer);

    /**
     * @brief Split this node into two children along the given axis.
     * @param axis HORIZONTAL splits top/bottom, VERTICAL splits left/right.
     * @param ratio Fraction of space given to the first child, in [0, 1].
     * @param fnFirst Callback to build the first child subtree.
     * @param fnSecond Callback to build the second child subtree.
     */
    DockScope &split(SplitAxis axis, float ratio, std::function<void(DockScope &)> fnFirst,
                     std::function<void(DockScope &)> fnSecond);

    /**
     * @brief Declare this node as a leaf panel with a TabBar.
     * @param fn Optional callback to populate the TabBar with tabs.
     */
    DockScope &panel(std::function<void(TabBarScope &)> fn = {});

  private:
    DockScope(DockingLayer &l, int32_t node) : m_layer(l), m_nodeIndex(node) {}

    DockingLayer &m_layer;
    int32_t m_nodeIndex;
};

} // namespace Amethyst

#endif // AMETHYST__UI_SCOPE_H
