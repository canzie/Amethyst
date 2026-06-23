#include "tab_bar.h"

#include "components/common.h"
#include "components/extensions/ui_drag_detector.h"
#include "components/text_label.h"
#include "components/ui_layer.h"
#include "modules/style.h"
#include "rendering/geometry_registry.h"
#include "utils/am_assert.h"

#include <algorithm>

namespace Amethyst {

static bool s_closeButtonVisible(TabCloseButtonVisibility vis, bool isSelected, bool isHovered)
{
    switch (vis) {
    case TabCloseButtonVisibility::ALWAYS:
        return true;
    case TabCloseButtonVisibility::NONE:
    case TabCloseButtonVisibility::HIDDEN:
        return false;
    case TabCloseButtonVisibility::ACTIVE_ONLY:
        return isSelected;
    case TabCloseButtonVisibility::HOVERED_OR_ACTIVE:
        return isSelected || isHovered;
    }
    return false;
}

TabBar::TabBar()
{
    m_tbProps.mode = TabBarMode::INSIDE;
    m_tbProps.tabPosition = TabBarPosition::TOP;
    m_tbProps.visibility = TabBarVisibility::ALWAYS;
    m_tbProps.barThickness = 30.0f;
    m_tbProps.tabWidth = 100.0f;
    m_tbProps.tabSpacing = 0.0f;
    m_tbProps.tabOffset = 0.0f;
    m_selectedIndex = 0;
    m_tbProps.closeButtonVisibility = TabCloseButtonVisibility::HOVERED_OR_ACTIVE;
    m_tbProps.tabTearOffEnabled = false;

    resolveStyle();
}

void TabBar::resolveStyle()
{
    auto &style = Style::instance();
    setBaseStyleProperties(style.getBaseStyle(ComponentType::TAB_BAR, getClasses()));
    setTabBarProperties(style.getTabBarStyle(ComponentType::TAB_BAR, getClasses()));
}

bool TabBar::setTabBarProperties(const TabBarStyleProperties &props)
{
    bool changed = m_tbProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

Instance *TabBar::addChild(std::unique_ptr<Instance>)
{
    AM_ASSERT(false, "TabBar does not support addChild, use addTab");
    return nullptr;
}

std::unique_ptr<Instance> TabBar::removeChild(Instance *)
{
    AM_ASSERT(false, "TabBar does not support removeChild, use removeTab");
    return nullptr;
}

bool TabBar::isVertical() const
{
    return m_tbProps.tabPosition == TabBarPosition::LEFT || m_tbProps.tabPosition == TabBarPosition::RIGHT;
}

bool TabBar::shouldShowTabs() const
{
    if (m_tbProps.visibility == TabBarVisibility::NEVER) return false;
    if (m_tbProps.visibility == TabBarVisibility::ALWAYS) return true;
    return m_tabs.size() > 1;
}

float TabBar::getBarSize() const
{
    return shouldShowTabs() ? m_tbProps.barThickness : 0.0f;
}

vec2 TabBar::getContentOffset() const
{
    if (m_tbProps.mode == TabBarMode::OUTSIDE) return {0.0f, 0.0f};

    float bar = getBarSize();
    switch (m_tbProps.tabPosition) {
    case TabBarPosition::TOP:
        return {0.0f, bar};
    case TabBarPosition::LEFT:
        return {bar, 0.0f};
    default:
        return {0.0f, 0.0f};
    }
}

vec2 TabBar::getContentSizeAdjust() const
{
    if (m_tbProps.mode == TabBarMode::OUTSIDE) return {0.0f, 0.0f};

    float bar = getBarSize();
    return isVertical() ? vec2{-bar, 0.0f} : vec2{0.0f, -bar};
}

int32_t TabBar::findTabIndex(const Tab *tab) const
{
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].get() == tab) return static_cast<int32_t>(i);
    }
    return -1;
}

int32_t TabBar::indexFromPosition(float pos) const
{
    float stride = m_tbProps.tabWidth + m_tbProps.tabSpacing;
    int32_t idx = static_cast<int32_t>(pos / stride);
    return std::clamp(idx, 0, static_cast<int32_t>(m_tabs.size()) - 1);
}

Instance *TabBar::getSelectedContent() const
{
    if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<int32_t>(m_tabs.size())) {
        return nullptr;
    }
    return m_tabs[m_selectedIndex]->content.get();
}

void TabBar::select(int32_t index)
{
    if (index < 0 || index >= static_cast<int32_t>(m_tabs.size())) return;
    if (index == m_selectedIndex) return;

    m_tabs[m_selectedIndex]->labelFrame->setBaseStyleProperties({.backgroundColor = m_tbProps.tabColor});
    m_tabs[m_selectedIndex]->closeButton->setBaseProperties({
        .visible = static_cast<int8_t>(s_closeButtonVisible(m_tbProps.closeButtonVisibility, false, false)),
    });

    m_lastSelectedIndex = m_selectedIndex;
    m_selectedIndex = index;

    m_tabs[m_selectedIndex]->labelFrame->setBaseStyleProperties({.backgroundColor = m_tbProps.focussedTabColor});
    m_tabs[m_selectedIndex]->closeButton->setBaseProperties({
        .visible = static_cast<int8_t>(s_closeButtonVisible(m_tbProps.closeButtonVisibility, true, false)),
    });

    if (onSelectionChanged) {
        onSelectionChanged(m_selectedIndex);
    }
}

void TabBar::select(Instance *content)
{
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->content.get() == content) {
            select(static_cast<int32_t>(i));
            return;
        }
    }
}

void TabBar::ensureTabComponents(Tab &tab)
{
    if (tab.button != nullptr) {
        return;
    }
    tab.button = std::make_unique<InvisibleButton>();

    auto newFrame = std::make_unique<Frame>();
    newFrame->setBaseProperties({
        .interactable = false,
        .position = UDim2::fromScale(0.0f),
        .size = UDim2::fromScale(1.0f),
    });
    tab.labelFrame = static_cast<Frame *>(tab.button->addChild(std::move(newFrame)));

    auto closeBtn = std::make_unique<TextButton>();
    closeBtn->setBaseProperties({
        .anchorPoint = {1.0f, 0.5f},
        .position = UDim2(1.0f, -5.0f, 0.5f, 0.0f),
        .size = UDim2::fromOffset(20.0f, 20.0f),
        .visible = false,
        .zIndex = 101,
    });
    closeBtn->setTextStyleProperties({
        .textXAlignment = TextXAlignment::CENTER,
        .textYAlignment = TextYAlignment::CENTER,
    });
    closeBtn->setText("×");
    tab.closeButton = closeBtn.get();
    tab.button->addChild(std::move(closeBtn));
}

void TabBar::setupTabDragCallbacks(Tab &tab)
{
    auto *drag = tab.button->getExtension<UIDragDetector>();
    if (drag == nullptr) {
        drag = tab.button->addExtension<UIDragDetector>();
    }
    drag->mode = m_tbProps.tabTearOffEnabled ? (isVertical() ? DragMode::SOFT_VERTICAL : DragMode::SOFT_HORIZONTAL)
                                             : (isVertical() ? DragMode::VERTICAL : DragMode::HORIZONTAL);

    Tab *tabPtr = &tab;

    drag->onDragStart = [this, tabPtr](vec2) {
        m_draggedTab = tabPtr;
        m_tornOff = false;
        tabPtr->labelFrame->setBaseProperties({.zIndex = 100});
        tabPtr->button->setBaseProperties({.zIndex = 100});
    };

    drag->onDragUpdate = [this, tabPtr, drag](vec2, vec2 absPos) {
        if (drag->isSoftLockBroken()) {
            if (!m_tornOff) {
                m_tornOff = true;
                if (onTabTornOff) onTabTornOff(tabPtr->content.get());
            }
            if (onTornOffTabMoved) {
                onTornOffTabMoved(tabPtr->content.get(), absPos);
            }
            return;
        }

        int32_t currentIdx = findTabIndex(tabPtr);
        if (currentIdx < 0) return;

        vec2 relPos = absPos - absolutePosition;
        float dragPos = isVertical() ? relPos.y : relPos.x;
        int32_t targetIdx = indexFromPosition(dragPos + m_tbProps.tabWidth * 0.5f);

        if (targetIdx != currentIdx) {
            if (m_selectedIndex == currentIdx) {
                m_selectedIndex = targetIdx;
                m_lastSelectedIndex = targetIdx;
            } else if (m_selectedIndex == targetIdx) {
                m_selectedIndex = currentIdx;
                m_lastSelectedIndex = currentIdx;
            }
            std::swap(m_tabs[currentIdx], m_tabs[targetIdx]);
            m_tabs[currentIdx]->labelFrame->markDirty();
            m_tabs[currentIdx]->button->markDirty();
            m_tabs[targetIdx]->labelFrame->markDirty();
            m_tabs[targetIdx]->button->markDirty();
        }
    };

    drag->onDragEnd = [this, tabPtr](vec2 endPos) {
        bool wasTornOff = m_tornOff;
        Instance *content = tabPtr->content.get();

        m_draggedTab = nullptr;
        m_tornOff = false;
        tabPtr->labelFrame->setBaseProperties({.zIndex = 1});
        tabPtr->button->setBaseProperties({.zIndex = 1});

        if (wasTornOff && onTornOffTabReleased) {
            onTornOffTabReleased(extractTab(content), endPos);
        }
    };
}

void TabBar::setupTabInteractionCallbacks(Tab &tab)
{
    Tab *tabPtr = &tab;

    tab.button->onMouseEnterCb = [this, tabPtr]() {
        int32_t idx = findTabIndex(tabPtr);
        if (idx != m_selectedIndex) {
            tabPtr->labelFrame->setBaseStyleProperties({.backgroundColor = m_tbProps.hoveredTabColor});
        }
        tabPtr->closeButton->setBaseProperties({
            .visible = static_cast<am_bool>(s_closeButtonVisible(m_tbProps.closeButtonVisibility, idx == m_selectedIndex, true)),
        });
        return EventResult::CONSUMED;
    };

    tab.button->onMouseLeaveCb = [this, tabPtr]() {
        int32_t idx = findTabIndex(tabPtr);
        tabPtr->labelFrame->setBaseStyleProperties({
            .backgroundColor = (idx == m_selectedIndex) ? m_tbProps.focussedTabColor : m_tbProps.tabColor,
        });
        tabPtr->closeButton->setBaseProperties({
            .visible = static_cast<am_bool>(s_closeButtonVisible(m_tbProps.closeButtonVisibility, idx == m_selectedIndex, false)),
        });
        return EventResult::CONSUMED;
    };

    tab.button->onMouseButton1DownCb = [this, tabPtr](int32_t, int32_t) {
        tabPtr->labelFrame->setBaseStyleProperties({.backgroundColor = m_tbProps.pressedTabColor});
        int32_t idx = findTabIndex(tabPtr);
        if (idx >= 0) select(idx);
        return EventResult::CONSUMED;
    };

    tab.button->onMouseButton1UpCb = [this, tabPtr](int32_t, int32_t) {
        int32_t idx = findTabIndex(tabPtr);
        tabPtr->labelFrame->setBaseStyleProperties({
            .backgroundColor = (idx == m_selectedIndex) ? m_tbProps.focussedTabColor : m_tbProps.hoveredTabColor,
        });
        return EventResult::CONSUMED;
    };

    tab.closeButton->onMouseButton1ClickCb = [this, tabPtr]() {
        if (onTabClosed) onTabClosed(tabPtr->content.get());
        removeTab(tabPtr->content.get());
        return EventResult::CONSUMED;
    };
}

void TabBar::setupTabButton(Tab &tab, int32_t index)
{
    ensureTabComponents(tab);
    tab.button->parent = this;
    tab.labelFrame->setBaseStyleProperties({
        .backgroundColor = (index == m_selectedIndex) ? m_tbProps.focussedTabColor : m_tbProps.tabColor,
    });
    setupTabDragCallbacks(tab);
    setupTabInteractionCallbacks(tab);
}

void TabBar::markAllTabsDirty()
{
    for (auto &tab : m_tabs) {
        tab->button->markDirty();
    }
}

Instance *TabBar::addTab(std::unique_ptr<Instance> content, std::string_view label)
{
    Instance *raw = content.get();

    auto existing = std::find_if(m_tabs.begin(), m_tabs.end(), [raw](const auto &t) { return t->content.get() == raw; });
    if (existing != m_tabs.end()) return raw;

    content->parent = this;

    if (auto *layer = raw->as<UILayer>()) {
        layer->setDisplayOrder(2);
    }

    auto tab = std::make_unique<Tab>();
    tab->content = std::move(content);
    setupTabButton(*tab, static_cast<int32_t>(m_tabs.size()));

    auto lbl = std::make_unique<TextLabel>();
    lbl->setBaseStyleProperties({.backgroundTransparency = 1.0f});
    lbl->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
    lbl->setTextStyleProperties({
        .textXAlignment = TextXAlignment::CENTER,
        .textYAlignment = TextYAlignment::CENTER,
    });
    lbl->setText(std::string(label));
    tab->label = lbl.get();
    tab->labelFrame->addChild(std::move(lbl));

    m_tabs.push_back(std::move(tab));
    markAllTabsDirty();
    markDirty();
    return raw;
}

Instance *TabBar::addTab(std::unique_ptr<Instance> content, std::function<void(Frame &)> labelSetup)
{
    Instance *raw = content.get();

    auto existing = std::find_if(m_tabs.begin(), m_tabs.end(), [raw](const auto &t) { return t->content.get() == raw; });
    if (existing != m_tabs.end()) return raw;

    content->parent = this;

    if (auto *layer = raw->as<UILayer>()) {
        layer->setDisplayOrder(2);
    }

    auto tab = std::make_unique<Tab>();
    tab->content = std::move(content);
    setupTabButton(*tab, static_cast<int32_t>(m_tabs.size()));

    labelSetup(*tab->labelFrame);

    m_tabs.push_back(std::move(tab));
    markAllTabsDirty();
    markDirty();
    return raw;
}

std::unique_ptr<Instance> TabBar::removeTab(Instance *content)
{
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(), [content](const auto &t) { return t->content.get() == content; });
    if (it == m_tabs.end()) return nullptr;

    int32_t removedIdx = static_cast<int32_t>(std::distance(m_tabs.begin(), it));
    (*it)->button->parent = nullptr;

    auto removedContent = std::move((*it)->content);
    removedContent->parent = nullptr;
    m_tabs.erase(it);

    int32_t oldSelected = m_selectedIndex;

    if (m_tabs.empty()) {
        m_selectedIndex = 0;
    } else if (m_selectedIndex == removedIdx) {
        m_selectedIndex = std::min(m_selectedIndex, static_cast<int32_t>(m_tabs.size()) - 1);
    } else if (m_selectedIndex > removedIdx) {
        m_selectedIndex--;
    }

    m_lastSelectedIndex = oldSelected;
    markAllTabsDirty();

    if (!m_tabs.empty()) {
        m_tabs[m_selectedIndex]->labelFrame->setBaseStyleProperties({.backgroundColor = m_tbProps.focussedTabColor});
    }

    if (m_selectedIndex != oldSelected && onSelectionChanged) {
        onSelectionChanged(m_selectedIndex);
    }

    markDirty();
    return removedContent;
}

Instance *TabBar::getTabContent(int32_t index) const
{
    if (index < 0 || index >= static_cast<int32_t>(m_tabs.size())) {
        return nullptr;
    }
    return m_tabs[index]->content.get();
}

std::vector<std::unique_ptr<TabBar::Tab>> TabBar::removeAllTabs()
{
    for (auto &tab : m_tabs) {
        tab->button->parent = nullptr;
        tab->content->parent = nullptr;
    }
    std::vector<std::unique_ptr<Tab>> result = std::move(m_tabs);
    m_tabs.clear();
    m_selectedIndex = 0;
    markDirty();
    return result;
}

std::unique_ptr<TabBar::Tab> TabBar::extractTab(Instance *content)
{
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(), [content](const auto &t) { return t->content.get() == content; });
    if (it == m_tabs.end()) {
        return nullptr;
    }

    int32_t removedIdx = static_cast<int32_t>(std::distance(m_tabs.begin(), it));
    (*it)->button->parent = nullptr;
    (*it)->content->parent = nullptr;

    auto extracted = std::move(*it);
    m_tabs.erase(it);

    if (m_tabs.empty()) {
        m_selectedIndex = 0;
    } else if (m_selectedIndex >= removedIdx) {
        m_selectedIndex = std::max(0, m_selectedIndex - 1);
    }

    markAllTabsDirty();
    markDirty();
    return extracted;
}

void TabBar::addTab(std::unique_ptr<Tab> tab)
{
    tab->content->parent = this;
    if (auto *layer = tab->content->as<UILayer>()) {
        layer->setDisplayOrder(2);
    }
    tab->content->markDirty();
    setupTabButton(*tab, static_cast<int32_t>(m_tabs.size()));
    m_tabs.push_back(std::move(tab));
    markAllTabsDirty();
    markDirty();
}

void TabBar::layoutTabs()
{
    float offset = m_tbProps.tabOffset;

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        auto *btn = m_tabs[i]->button.get();

        if (m_tabs[i].get() != m_draggedTab) {
            if (isVertical()) {
                float x = (m_tbProps.tabPosition == TabBarPosition::LEFT) ? 0.0f : absoluteSize.x - m_tbProps.barThickness;
                btn->setBaseProperties({
                    .position = UDim2::fromOffset(x, offset),
                    .size = UDim2::fromOffset(m_tbProps.barThickness, m_tbProps.tabWidth),
                    .rotation = (m_tbProps.tabPosition == TabBarPosition::LEFT) ? -90.0f : 90.0f,
                    .visible = isVisible(),
                });
            } else {
                float y = (m_tbProps.tabPosition == TabBarPosition::TOP) ? 0.0f : absoluteSize.y - m_tbProps.barThickness;
                btn->setBaseProperties({
                    .position = UDim2::fromOffset(offset, y),
                    .size = UDim2::fromOffset(m_tbProps.tabWidth, m_tbProps.barThickness),
                    .rotation = 0.0f,
                    .visible = isVisible(),
                });
            }
        }

        offset += m_tbProps.tabWidth + m_tbProps.tabSpacing;
    }
}

void TabBar::layoutContent()
{
    Instance *selectedContent = getSelectedContent();
    bool selectionChanged = (m_selectedIndex != m_lastSelectedIndex);

    Instance *lastContent = nullptr;
    if (m_lastSelectedIndex >= 0 && m_lastSelectedIndex < static_cast<int32_t>(m_tabs.size())) {
        lastContent = m_tabs[m_lastSelectedIndex]->content.get();
    }

    vec2 contentOffset = getContentOffset();
    vec2 sizeAdjust = getContentSizeAdjust();

    for (auto &tab : m_tabs) {
        Instance *child = tab->content.get();
        bool isSelected = (child == selectedContent);
        bool wasSelected = (child == lastContent);

        if (auto *drawable = child->as<UIObject>()) {
            drawable->setBaseProperties({.visible = static_cast<int8_t>(isSelected)});

            if (selectionChanged && (isSelected || wasSelected)) {
                drawable->markDirty();
            }

            if (isSelected) {
                drawable->setBaseProperties({
                    .position = UDim2(0.0f, contentOffset.x, 0.0f, contentOffset.y),
                    .size = UDim2(1.0f, sizeAdjust.x, 1.0f, sizeAdjust.y),
                });
            }
        } else if (auto *layer = child->as<UILayer>()) {
            layer->visible = isSelected;

            if (selectionChanged && (isSelected || wasSelected)) {
                layer->markDirty();
            }

            if (isSelected) {
                layer->absolutePosition = absolutePosition + contentOffset;
                layer->absoluteSize = absoluteSize + sizeAdjust;
            }
        }
    }
}

std::vector<Instance *> TabBar::getHittableInstances()
{
    std::vector<Instance *> result;
    result.reserve(m_tabs.size() * 2);

    for (auto &tab : m_tabs) {
        result.push_back(tab->button.get());
        result.push_back(tab->content.get());
    }

    return result;
}

void TabBar::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        markAllTabsDirty();
        if (auto *selected = getSelectedContent()) {
            selected->markDirty();
        }
    }

    layoutTabs();
    layoutContent();

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.setPrimitiveType(PRIMITIVE_RECT);

        pushData(ctx.geometry, data);
    }

    vec4 childClip = computeChildClipRect();

    if (shouldShowTabs()) {
        DrawContext buttonCtx = ctx;
        if (ctx.overlay) {
            buttonCtx.geometry = ctx.overlay;
        }
        for (auto &tab : m_tabs) {
            bool isTornOff = (tab.get() == m_draggedTab && m_tornOff);
            tab->button->clipRect = isTornOff ? vec4(0.0f) : childClip;
            tab->button->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            tab->button->draw(buttonCtx);
        }
    }

    for (auto &tab : m_tabs) {
        Instance *child = tab->content.get();
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        } else if (auto *layer = child->as<UILayer>()) {
            layer->clipRect = childClip;
            layer->draw(ctx);
        }
    }

    m_lastSelectedIndex = m_selectedIndex;
    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

TabBarConfig TabBar::saveConfig() const
{
    TabBarConfig cfg;
    if (auto *selected = getSelectedContent()) {
        cfg.selectedTab = selected->name;
    }
    return cfg;
}

void TabBar::applyConfig(const TabBarConfig &config)
{
    if (config.selectedTab.empty()) return;

    for (auto &tab : m_tabs) {
        if (tab->content->name == config.selectedTab) {
            select(tab->content.get());
            return;
        }
    }
}

} // namespace Amethyst
