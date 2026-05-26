#ifndef AMETHYST__UI_SCOPE_H
#define AMETHYST__UI_SCOPE_H

#include "components/instance.h"
#include "components/properties.h"

#include <functional>
#include <string_view>

namespace Amethyst {

class Checkbox;
class CollapsibleHeader;
class Frame;
class ImageButton;
class ImageLabel;
class InvisibleButton;
class ScrollingFrame;
class TabBar;
class TextButton;
class TextLabel;
class TreeView;

struct FrameScope;
struct ScrollingFrameScope;
struct TabScope;
struct TabBarScope;
struct TextLabelScope;
struct TextButtonScope;
struct ImageLabelScope;
struct ImageButtonScope;
struct InvisibleButtonScope;
struct CheckboxScope;
struct CollapsibleHeaderScope;
struct TreeViewScope;

class UIScope {
  public:
    explicit UIScope(Instance &parent) : m_parent(&parent) {}

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
    UIScope &tabBar(BaseProperties base = {}, TabBarProperties props = {},
                    std::function<void(TabBarScope &)> fn = {});
    UIScope &treeView(BaseProperties base = {}, TreeViewProperties props = {},
                      std::function<void(TreeViewScope &)> fn = {});

    Instance &get() const { return *m_parent; }

  protected:
    Instance *m_parent;
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

struct TreeViewScope : UIScope {
    TreeView &component;
    explicit TreeViewScope(TreeView &tv);
};

} // namespace Amethyst

#endif // AMETHYST__UI_SCOPE_H
