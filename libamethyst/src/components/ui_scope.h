#ifndef AMETHYST__UI_SCOPE_H
#define AMETHYST__UI_SCOPE_H

#include "components/docking_layer.h"
#include "components/dropdown_item.h"
#include "components/instance.h"
#include "components/properties.h"
#include "components/table.h"
#include "components/tree_view.h"

#include <functional>
#include <string_view>
#include <vector>

namespace Amethyst {

class Canvas;
class Checkbox;
class Color3Picker;
class Color4Picker;
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
class TabBar;
class TextButton;
class TextInput;
class TextLabel;
class TreeView;

struct CanvasScope;
struct Color3PickerScope;
struct Color4PickerScope;
struct DropdownScope;
struct MenuBarScope;
struct FrameScope;
struct ScrollingFrameScope;
struct SliderFloatScope;
struct SliderIntScope;
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

    UIScope &canvas(CanvasProperties props = {}, std::function<void(CanvasScope &)> fn = {});
    UIScope &dropdown(DropdownProperties props = {}, std::function<void(DropdownScope &)> fn = {});
    UIScope &menuBar(MenuBarProperties props = {}, std::function<void(MenuBarScope &)> fn = {});
    UIScope &frame(FrameProperties props = {}, std::function<void(FrameScope &)> fn = {});
    UIScope &scrollingFrame(ScrollingFrameProperties props = {}, std::function<void(ScrollingFrameScope &)> fn = {});
    UIScope &textLabel(TextLabelProperties props = {}, std::function<void(TextLabelScope &)> fn = {});
    UIScope &textButton(TextButtonProperties props = {}, std::function<void(TextButtonScope &)> fn = {});
    UIScope &imageLabel(ImageLabelProperties props = {}, std::function<void(ImageLabelScope &)> fn = {});
    UIScope &imageButton(ImageButtonProperties props = {}, std::function<void(ImageButtonScope &)> fn = {});
    UIScope &invisibleButton(InvisibleButtonProperties props = {}, std::function<void(InvisibleButtonScope &)> fn = {});
    UIScope &checkbox(CheckboxProperties props = {}, std::function<void(CheckboxScope &)> fn = {});
    UIScope &collapsibleHeader(CollapsibleHeaderProperties props = {}, std::function<void(CollapsibleHeaderScope &)> fn = {});
    UIScope &tabBar(TabBarProperties props = {}, std::function<void(TabBarScope &)> fn = {});
    UIScope &table(TableProperties props = {}, std::function<void(TableScope &)> fn = {});
    UIScope &textInput(TextInputProperties props = {}, std::function<void(TextInputScope &)> fn = {});
    UIScope &sliderFloat(SliderFloatProperties props = {}, std::function<void(SliderFloatScope &)> fn = {});
    UIScope &sliderInt(SliderIntProperties props = {}, std::function<void(SliderIntScope &)> fn = {});
    UIScope &treeView(TreeViewProperties props = {}, std::function<void(TreeViewScope &)> fn = {});
    UIScope &color3Picker(Color3PickerProperties props = {}, std::function<void(Color3PickerScope &)> fn = {});
    UIScope &color4Picker(Color4PickerProperties props = {}, std::function<void(Color4PickerScope &)> fn = {});

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

struct Color3PickerScope : UIScope {
    Color3Picker &component;
    explicit Color3PickerScope(Color3Picker &p);
};

struct Color4PickerScope : UIScope {
    Color4Picker &component;
    explicit Color4PickerScope(Color4Picker &p);
};

struct TreeRowScope {
    TreeView &component;
    uint16_t depth;
    std::vector<std::function<void(UIScope &)>> pendingCells;
    std::vector<std::function<void(TreeRowScope &)>> pendingChildRows;
    TreeRowScope(TreeView &tv, uint16_t depth);
    TreeRowScope &cell(std::function<void(UIScope &)> fn);
    TreeRowScope &row(std::function<void(TreeRowScope &)> fn);
};

struct TreeViewScope : UIScope {
    TreeView &component;
    bool columnsExplicit = false;
    explicit TreeViewScope(TreeView &tv);
    TreeViewScope &column(std::string header, float weight, TreeColumnSizing sizing = TreeColumnSizing::STRETCH);
    TreeViewScope &column(TreeColumn col);
    TreeViewScope &row(std::function<void(TreeRowScope &)> fn);
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
