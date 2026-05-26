#include "components/ui_scope.h"

#include "components/checkbox.h"
#include "components/collapsible_header.h"
#include "components/frame.h"
#include "components/image_button.h"
#include "components/image_label.h"
#include "components/invisible_button.h"
#include "components/scrolling_frame.h"
#include "components/tab_bar.h"
#include "components/text_button.h"
#include "components/text_label.h"
#include "components/tree_view.h"

namespace Amethyst {

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
TreeViewScope::TreeViewScope(TreeView &tv) : UIScope(tv), component(tv) {}

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

} // namespace Amethyst
