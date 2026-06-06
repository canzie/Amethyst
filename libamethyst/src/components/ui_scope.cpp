#include "components/ui_scope.h"

#include "components/canvas.h"
#include "components/checkbox.h"
#include "components/collapsible_header.h"
#include "components/container.h"
#include "components/dropdown.h"
#include "components/frame.h"
#include "components/image_button.h"
#include "components/image_label.h"
#include "components/invisible_button.h"
#include "components/menu_bar.h"
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

UIScope &UIScope::canvas(CanvasProperties props, std::function<void(CanvasScope &)> fn)
{
    auto *c = m_parent->add<Canvas>();
    c->setBaseProperties(props.base);
    if (fn) {
        CanvasScope scope(*c);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::dropdown(DropdownProperties props, std::function<void(DropdownScope &)> fn)
{
    auto *d = m_parent->add<Dropdown>();
    d->setBaseProperties(props.base);
    d->setBaseStyleProperties(props.style);
    d->setTextStyleProperties(props.text);
    d->setText(std::move(props.label));
    d->setDropdownProperties(props.dropdown);
    if (fn) {
        DropdownScope scope(*d);
        fn(scope);
        d->setItems(std::move(scope.pendingItems));
    }
    return *this;
}

UIScope &UIScope::menuBar(MenuBarProperties props, std::function<void(MenuBarScope &)> fn)
{
    auto *mb = m_parent->add<MenuBar>();
    mb->setBaseProperties(props.base);
    mb->setBaseStyleProperties(props.style);
    mb->setMenuBarProperties(props.menuBar);
    if (fn) {
        MenuBarScope scope(*mb);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::frame(FrameProperties props, std::function<void(FrameScope &)> fn)
{
    auto *f = m_parent->add<Frame>();
    f->setBaseProperties(props.base);
    f->setBaseStyleProperties(props.style);
    if (fn) {
        FrameScope scope(*f);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::scrollingFrame(ScrollingFrameProperties props, std::function<void(ScrollingFrameScope &)> fn)
{
    auto *sf = m_parent->add<ScrollingFrame>();
    sf->setBaseProperties(props.base);
    sf->setBaseStyleProperties(props.style);
    sf->setScrollingFrameProperties(props.scroll);
    if (fn) {
        ScrollingFrameScope scope(*sf);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::textLabel(TextLabelProperties props, std::function<void(TextLabelScope &)> fn)
{
    auto *tl = m_parent->add<TextLabel>();
    tl->setBaseProperties(props.base);
    tl->setBaseStyleProperties(props.style);
    tl->setTextStyleProperties(props.text);
    tl->setText(std::move(props.label));
    if (fn) {
        TextLabelScope scope(*tl);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::textButton(TextButtonProperties props, std::function<void(TextButtonScope &)> fn)
{
    auto *tb = m_parent->add<TextButton>();
    tb->setBaseProperties(props.base);
    tb->setBaseStyleProperties(props.style);
    tb->setTextStyleProperties(props.text);
    tb->setText(std::move(props.label));
    tb->setButtonProperties(props.button);
    if (fn) {
        TextButtonScope scope(*tb);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::imageLabel(ImageLabelProperties props, std::function<void(ImageLabelScope &)> fn)
{
    auto *il = m_parent->add<ImageLabel>();
    il->setBaseProperties(props.base);
    il->setBaseStyleProperties(props.style);
    il->setImageStyleProperties(props.image);
    if (props.texture.isValid()) {
        il->setImage(props.texture);
    }
    if (!props.svg.empty()) {
        il->setSvg(std::move(props.svg));
    }
    if (fn) {
        ImageLabelScope scope(*il);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::imageButton(ImageButtonProperties props, std::function<void(ImageButtonScope &)> fn)
{
    auto *ib = m_parent->add<ImageButton>();
    ib->setBaseProperties(props.base);
    ib->setBaseStyleProperties(props.style);
    ib->setImageStyleProperties(props.image);
    if (props.texture.isValid()) {
        ib->setImage(props.texture);
    }
    if (!props.svg.empty()) {
        ib->setSvg(std::move(props.svg));
    }
    ib->setButtonProperties(props.button);
    if (fn) {
        ImageButtonScope scope(*ib);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::invisibleButton(InvisibleButtonProperties props, std::function<void(InvisibleButtonScope &)> fn)
{
    auto *ib = m_parent->add<InvisibleButton>();
    ib->setBaseProperties(props.base);
    if (fn) {
        InvisibleButtonScope scope(*ib);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::checkbox(CheckboxProperties props, std::function<void(CheckboxScope &)> fn)
{
    auto *cb = m_parent->add<Checkbox>();
    cb->setBaseProperties(props.base);
    cb->setCheckboxProperties(props.checkbox);
    if (fn) {
        CheckboxScope scope(*cb);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::collapsibleHeader(CollapsibleHeaderProperties props, std::function<void(CollapsibleHeaderScope &)> fn)
{
    auto *ch = m_parent->add<CollapsibleHeader>();
    ch->setBaseProperties(props.base);
    ch->setBaseStyleProperties(props.style);
    ch->setCollapsibleHeaderProperties(props.header);
    ch->setTitle(std::move(props.title));
    if (fn) {
        CollapsibleHeaderScope scope(*ch);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::tabBar(TabBarProperties props, std::function<void(TabBarScope &)> fn)
{
    auto *tb = m_parent->add<TabBar>();
    tb->setBaseProperties(props.base);
    tb->setBaseStyleProperties(props.style);
    tb->setTabBarProperties(props.tabBar);
    if (fn) {
        TabBarScope scope(*tb);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::table(TableProperties props, std::function<void(TableScope &)> fn)
{
    auto *t = m_parent->add<Table>();
    t->setBaseProperties(props.base);
    t->setTableProperties(props.table);
    if (fn) {
        TableScope scope(*t);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::textInput(TextInputProperties props, std::function<void(TextInputScope &)> fn)
{
    auto *ti = m_parent->add<TextInput>();
    ti->setBaseProperties(props.base);
    ti->setBaseStyleProperties(props.style);
    ti->setTextInputProperties(props.textInput);
    ti->setPlaceholder(std::move(props.placeholder));
    if (fn) {
        TextInputScope scope(*ti);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::sliderFloat(SliderProperties props, std::function<void(SliderFloatScope &)> fn)
{
    auto *s = m_parent->add<SliderFloat>();
    s->setBaseProperties(props.base);
    s->setBaseStyleProperties(props.style);
    s->setSliderProperties(props.slider);
    s->setLabel(std::move(props.label));
    s->setValueSuffix(std::move(props.valueSuffix));
    if (fn) {
        SliderFloatScope scope(*s);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::sliderInt(SliderProperties props, std::function<void(SliderIntScope &)> fn)
{
    auto *s = m_parent->add<SliderInt>();
    s->setBaseProperties(props.base);
    s->setBaseStyleProperties(props.style);
    s->setSliderProperties(props.slider);
    s->setLabel(std::move(props.label));
    s->setValueSuffix(std::move(props.valueSuffix));
    if (fn) {
        SliderIntScope scope(*s);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::sliderVec2(SliderProperties props, std::function<void(SliderVec2Scope &)> fn)
{
    auto *s = m_parent->add<SliderVec2>();
    s->setBaseProperties(props.base);
    s->setBaseStyleProperties(props.style);
    s->setSliderProperties(props.slider);
    s->setLabel(std::move(props.label));
    s->setValueSuffix(std::move(props.valueSuffix));
    if (fn) {
        SliderVec2Scope scope(*s);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::sliderVec3(SliderProperties props, std::function<void(SliderVec3Scope &)> fn)
{
    auto *s = m_parent->add<SliderVec3>();
    s->setBaseProperties(props.base);
    s->setBaseStyleProperties(props.style);
    s->setSliderProperties(props.slider);
    s->setLabel(std::move(props.label));
    s->setValueSuffix(std::move(props.valueSuffix));
    if (fn) {
        SliderVec3Scope scope(*s);
        fn(scope);
    }
    return *this;
}

UIScope &UIScope::treeView(TreeViewProperties props, std::function<void(TreeViewScope &)> fn)
{
    auto *tv = m_parent->add<TreeView>();
    tv->setBaseProperties(props.base);
    tv->setTreeViewProperties(props.treeView);
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
