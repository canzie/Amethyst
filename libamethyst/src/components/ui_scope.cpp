#include "components/ui_scope.h"

#include "components/canvas.h"
#include "components/dropdown.h"
#include "components/menu_bar.h"
#include "components/checkbox.h"
#include "components/collapsible_header.h"
#include "components/container.h"
#include "components/frame.h"
#include "components/image_button.h"
#include "components/image_label.h"
#include "components/invisible_button.h"
#include "components/scrolling_frame.h"
#include "components/slider.h"
#include "components/tab_bar.h"
#include "components/text_button.h"
#include "components/text_input.h"
#include "components/text_label.h"
#include "components/tree_view.h"

namespace Amethyst {

CanvasScope::CanvasScope(Canvas &c) : UIScope(c), component(c) {}
DropdownScope::DropdownScope(Dropdown &d) : component(d) {}
MenuBarScope::MenuBarScope(MenuBar &mb) : UIScope(mb), component(mb) {}
FrameScope::FrameScope(Frame &f) : UIScope(f), component(f) {}
TabScope::TabScope(Frame &lf, Frame &cf) : labelFrame(lf), contentFrame(cf) {}
ScrollingFrameScope::ScrollingFrameScope(ScrollingFrame &sf) : UIScope(sf), component(sf) {}
TabBarScope::TabBarScope(TabBar &tb) : UIScope(tb), component(tb) {}
TextLabelScope::TextLabelScope(TextLabel &tl) : UIScope(tl), component(tl) {}
TextButtonScope::TextButtonScope(TextButton &tb) : UIScope(tb), component(tb) {}
ImageLabelScope::ImageLabelScope(ImageLabel &il) : UIScope(il), component(il) {}
ImageButtonScope::ImageButtonScope(ImageButton &ib) : UIScope(ib), component(ib) {}
InvisibleButtonScope::InvisibleButtonScope(InvisibleButton &ib) : UIScope(ib), component(ib) {}
CheckboxScope::CheckboxScope(Checkbox &cb) : UIScope(cb), component(cb) {}
CollapsibleHeaderScope::CollapsibleHeaderScope(CollapsibleHeader &ch) : UIScope(ch), component(ch) {}
TextInputScope::TextInputScope(TextInput &ti) : UIScope(ti), component(ti) {}
SliderFloatScope::SliderFloatScope(SliderFloat &s) : UIScope(s), component(s) {}
SliderIntScope::SliderIntScope(SliderInt &s) : UIScope(s), component(s) {}
SliderVec2Scope::SliderVec2Scope(SliderVec2 &s) : UIScope(s), component(s) {}
SliderVec3Scope::SliderVec3Scope(SliderVec3 &s) : UIScope(s), component(s) {}
TreeViewScope::TreeViewScope(TreeView &tv) : UIScope(tv), component(tv) {}
TreeRowScope::TreeRowScope(TreeView &tv, uint16_t depth) : component(tv), depth(depth) {}

UIScope &UIScope::canvas(BaseProperties base, std::function<void(CanvasScope &)> fn)
{
    auto *c = m_parent->add<Canvas>();
    c->setBaseProperties(base);
    if (fn) {
        CanvasScope scope(*c);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::dropdown(BaseProperties base, TextProperties text, DropdownProperties ddProps,
                           std::function<void(DropdownScope &)> fn)
{
    auto *d = m_parent->add<Dropdown>();
    d->setBaseProperties(base);
    d->setTextProperties(text);
    d->setDropdownProperties(ddProps);
    if (fn) {
        DropdownScope scope(*d);
        fn(scope);
        d->setItems(std::move(scope.pendingItems));
    }
    return *this;
}

UIScope &UIScope::menuBar(BaseProperties base, MenuBarProperties props, std::function<void(MenuBarScope &)> fn)
{
    auto *mb = m_parent->add<MenuBar>();
    mb->setBaseProperties(base);
    mb->setMenuBarProperties(props);
    if (fn) {
        MenuBarScope scope(*mb);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::frame(BaseProperties props, std::function<void(FrameScope &)> fn)
{
    auto *f = m_parent->add<Frame>();
    f->setBaseProperties(props);
    if (fn) {
        FrameScope scope(*f);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::scrollingFrame(BaseProperties base, ScrollingFrameProperties scroll,
                                 std::function<void(ScrollingFrameScope &)> fn)
{
    auto *sf = m_parent->add<ScrollingFrame>();
    sf->setBaseProperties(base);
    sf->setScrollingFrameProperties(scroll);
    if (fn) {
        ScrollingFrameScope scope(*sf);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::textLabel(BaseProperties base, TextProperties text, std::function<void(TextLabelScope &)> fn)
{
    auto *tl = m_parent->add<TextLabel>();
    tl->setBaseProperties(base);
    tl->setTextProperties(text);
    if (fn) {
        TextLabelScope scope(*tl);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::textButton(BaseProperties base, TextProperties text, ButtonProperties button,
                             std::function<void(TextButtonScope &)> fn)
{
    auto *tb = m_parent->add<TextButton>();
    tb->setBaseProperties(base);
    tb->setTextProperties(text);
    tb->setButtonProperties(button);
    if (fn) {
        TextButtonScope scope(*tb);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::imageLabel(BaseProperties base, ImageProperties image, std::function<void(ImageLabelScope &)> fn)
{
    auto *il = m_parent->add<ImageLabel>();
    il->setBaseProperties(base);
    il->setImageProperties(image);
    if (fn) {
        ImageLabelScope scope(*il);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::imageButton(BaseProperties base, ImageProperties image, ButtonProperties button,
                              std::function<void(ImageButtonScope &)> fn)
{
    auto *ib = m_parent->add<ImageButton>();
    ib->setBaseProperties(base);
    ib->setImageProperties(image);
    ib->setButtonProperties(button);
    if (fn) {
        ImageButtonScope scope(*ib);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::invisibleButton(BaseProperties props, std::function<void(InvisibleButtonScope &)> fn)
{
    auto *ib = m_parent->add<InvisibleButton>();
    ib->setBaseProperties(props);
    if (fn) {
        InvisibleButtonScope scope(*ib);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::checkbox(BaseProperties base, CheckboxProperties props, std::function<void(CheckboxScope &)> fn)
{
    auto *cb = m_parent->add<Checkbox>();
    cb->setBaseProperties(base);
    cb->setCheckboxProperties(props);
    if (fn) {
        CheckboxScope scope(*cb);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::collapsibleHeader(BaseProperties base, CollapsibleHeaderProperties props,
                                    std::function<void(CollapsibleHeaderScope &)> fn)
{
    auto *ch = m_parent->add<CollapsibleHeader>();
    ch->setBaseProperties(base);
    ch->setCollapsibleHeaderProperties(props);
    if (fn) {
        CollapsibleHeaderScope scope(*ch);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::tabBar(BaseProperties base, TabBarProperties props, std::function<void(TabBarScope &)> fn)
{
    auto *tb = m_parent->add<TabBar>();
    tb->setBaseProperties(base);
    tb->setTabBarProperties(props);
    if (fn) {
        TabBarScope scope(*tb);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::table(BaseProperties base, TableProperties props, std::function<void(TableScope &)> fn)
{
    auto *t = m_parent->add<Table>();
    t->setBaseProperties(base);
    t->setTableProperties(props);
    if (fn) {
        TableScope scope(*t);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::textInput(BaseProperties base, TextInputProperties props, std::function<void(TextInputScope &)> fn)
{
    auto *ti = m_parent->add<TextInput>();
    ti->setBaseProperties(base);
    ti->setTextInputProperties(props);
    if (fn) {
        TextInputScope scope(*ti);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::sliderFloat(BaseProperties base, SliderProperties props, std::function<void(SliderFloatScope &)> fn)
{
    auto *s = m_parent->add<SliderFloat>();
    s->setBaseProperties(base);
    s->setSliderProperties(props);
    if (fn) {
        SliderFloatScope scope(*s);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::sliderInt(BaseProperties base, SliderProperties props, std::function<void(SliderIntScope &)> fn)
{
    auto *s = m_parent->add<SliderInt>();
    s->setBaseProperties(base);
    s->setSliderProperties(props);
    if (fn) {
        SliderIntScope scope(*s);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::sliderVec2(BaseProperties base, SliderProperties props, std::function<void(SliderVec2Scope &)> fn)
{
    auto *s = m_parent->add<SliderVec2>();
    s->setBaseProperties(base);
    s->setSliderProperties(props);
    if (fn) {
        SliderVec2Scope scope(*s);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::sliderVec3(BaseProperties base, SliderProperties props, std::function<void(SliderVec3Scope &)> fn)
{
    auto *s = m_parent->add<SliderVec3>();
    s->setBaseProperties(base);
    s->setSliderProperties(props);
    if (fn) {
        SliderVec3Scope scope(*s);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::treeView(BaseProperties base, TreeViewProperties props, std::function<void(TreeViewScope &)> fn)
{
    auto *tv = m_parent->add<TreeView>();
    tv->setBaseProperties(base);
    tv->setTreeViewProperties(props);
    if (fn) {
        TreeViewScope scope(*tv);
        fn(scope);
    }
    return *this;
}

TabScope &TabScope::label(std::function<void(FrameScope &)> fn)
{
    FrameScope scope(labelFrame);
    fn(scope);
    return *this;
}

TabScope &TabScope::content(std::function<void(FrameScope &)> fn)
{
    FrameScope scope(contentFrame);
    fn(scope);
    return *this;
}

TabBarScope &TabBarScope::tab(std::string_view label, std::function<void(FrameScope &)> fn)
{
    auto contentFrame = std::make_unique<Frame>();
    Frame *raw = contentFrame.get();
    component.addTab(std::move(contentFrame), label);
    if (fn) {
        FrameScope scope(*raw);
        fn(scope);
    }
    return *this;
}

TabBarScope &TabBarScope::tab(std::function<void(TabScope &)> fn)
{
    Frame *labelRaw = nullptr;
    auto contentFrame = std::make_unique<Frame>();
    Frame *contentRaw = contentFrame.get();
    component.addTab(std::move(contentFrame), [&](Frame &lf) { labelRaw = &lf; });
    TabScope scope(*labelRaw, *contentRaw);
    fn(scope);
    return *this;
}

CollapsibleHeaderScope &CollapsibleHeaderScope::header(std::function<void(FrameScope &)> fn)
{
    component.header([&fn](Frame &f) {
        FrameScope scope(f);
        fn(scope);
    });
    return *this;
}

CollapsibleHeaderScope &CollapsibleHeaderScope::indicator(std::function<void(UIScope &)> fn)
{
    component.indicator([&fn](UIObject &obj) {
        UIScope scope(obj);
        fn(scope);
    });
    return *this;
}

TableRowScope::TableRowScope(Table &t) : component(t) {}

TableRowScope &TableRowScope::cell(std::function<void(UIScope &)> fn)
{
    m_pendingCells.emplace_back(std::move(fn));
    return *this;
}

TableScope::TableScope(Table &t) : UIScope(t), component(t) {}

TableScope &TableScope::column(std::string header, float weight, TableColumnSizing sizing)
{
    m_columnsExplicit = true;
    component.addColumn({.header = header, .sizing = sizing, .weight = weight});
    return *this;
}

TableScope &TableScope::column(TableColumn col)
{
    m_columnsExplicit = true;
    component.addColumn(std::move(col));
    return *this;
}

TableScope &TableScope::row(std::function<void(TableRowScope &)> rowFn)
{
    TableRowScope rowScope(component);
    rowFn(rowScope);

    uint32_t cellCount = static_cast<uint32_t>(rowScope.m_pendingCells.size());
    if (cellCount == 0) {
        return *this;
    }

    if (!m_columnsExplicit && cellCount > component.columnCount()) {
        component.resizeColumns(cellCount);
    }

    component.addRow();

    for (auto &cellFn : rowScope.m_pendingCells) {
        auto container = std::make_unique<Container>();
        UIScope cellScope(*container);
        cellFn(cellScope);
        container->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
        component.nextCell(std::move(container));
    }

    return *this;
}

TreeRowScope &TreeRowScope::cell(std::function<void(UIScope &)> fn)
{
    pendingCells.emplace_back(std::move(fn));
    return *this;
}

TreeRowScope &TreeRowScope::row(std::function<void(TreeRowScope &)> fn)
{
    pendingChildRows.emplace_back(std::move(fn));
    return *this;
}

// Realizes a row depth-first: emit the parent's cells, then recurse into each child row at
// depth+1. This append-then-recurse order is what produces the DFS build order TreeView's
// depth-run model relies on.
static void s_realizeTreeRow(TreeView &tv, uint16_t depth, TreeRowScope &scope, bool columnsExplicit)
{
    uint32_t cellCount = static_cast<uint32_t>(scope.pendingCells.size());
    if (!columnsExplicit && cellCount > tv.columnCount()) {
        tv.resizeColumns(cellCount);
    }

    tv.addRow(depth);

    for (auto &cellFn : scope.pendingCells) {
        auto container = std::make_unique<Container>();
        if (cellFn) {
            UIScope cellScope(*container);
            cellFn(cellScope);
        }
        container->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
        tv.nextCell(std::move(container));
    }

    for (auto &childFn : scope.pendingChildRows) {
        TreeRowScope childScope(tv, static_cast<uint16_t>(depth + 1));
        childFn(childScope);
        s_realizeTreeRow(tv, static_cast<uint16_t>(depth + 1), childScope, columnsExplicit);
    }
}

TreeViewScope &TreeViewScope::column(std::string header, float weight, TreeColumnSizing sizing)
{
    columnsExplicit = true;
    component.addColumn({.header = header, .sizing = sizing, .weight = weight});
    return *this;
}

TreeViewScope &TreeViewScope::column(TreeColumn col)
{
    columnsExplicit = true;
    component.addColumn(std::move(col));
    return *this;
}

TreeViewScope &TreeViewScope::row(std::function<void(TreeRowScope &)> rowFn)
{
    TreeRowScope rowScope(component, 0);
    rowFn(rowScope);

    if (rowScope.pendingCells.empty()) {
        return *this;
    }

    s_realizeTreeRow(component, 0, rowScope, columnsExplicit);
    return *this;
}

DropdownScope &DropdownScope::action(std::string label, std::function<void()> onSelect)
{
    pendingItems.emplace_back(DropdownItem::action(std::move(label), std::move(onSelect)));
    return *this;
}

DropdownScope &DropdownScope::toggle(std::string label, std::function<void(bool)> onToggle)
{
    pendingItems.emplace_back(DropdownItem::toggle(std::move(label), std::move(onToggle)));
    return *this;
}

DropdownScope &DropdownScope::separator()
{
    pendingItems.emplace_back(DropdownItem::separator());
    return *this;
}

DropdownScope &DropdownScope::submenu(std::string label, std::function<void(DropdownScope &)> fn)
{
    DropdownScope sub(component);
    fn(sub);
    pendingItems.emplace_back(DropdownItem::submenu(std::move(label), std::move(sub.pendingItems)));
    return *this;
}

DropdownScope &DropdownScope::items(std::vector<std::string> labels)
{
    for (auto &lbl : labels) {
        DropdownItem item;
        item.label = lbl;
        item.payload = DropdownSelect{};
        pendingItems.emplace_back(std::move(item));
    }
    return *this;
}

MenuBarScope &MenuBarScope::menuItem(std::string label, std::function<void(DropdownScope &)> fn)
{
    Dropdown *entry = component.addMenu(std::move(label), {});
    if (fn) {
        DropdownScope scope(*entry);
        fn(scope);
        entry->setItems(std::move(scope.pendingItems));
    }
    return *this;
}

DockScope::DockScope(DockingLayer &layer) : m_layer(layer), m_nodeIndex(layer.createLeaf()) {}

DockScope &DockScope::split(SplitAxis axis, float ratio, std::function<void(DockScope &)> fnFirst,
                            std::function<void(DockScope &)> fnSecond)
{
    auto [first, second] = m_layer.splitLeaf(m_nodeIndex, axis, ratio);
    DockScope firstScope(m_layer, first);
    fnFirst(firstScope);
    DockScope secondScope(m_layer, second);
    fnSecond(secondScope);
    return *this;
}

DockScope &DockScope::panel(std::function<void(TabBarScope &)> fn)
{
    TabBar *tb = m_layer.obtainLeafTabBar(m_nodeIndex);
    if (fn) {
        TabBarScope scope(*tb);
        fn(scope);
    }
    return *this;
}

} // namespace Amethyst
