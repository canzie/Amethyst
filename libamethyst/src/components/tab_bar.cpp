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

static void applyStyle(TabBar &tabBar)
{
    const auto &style = Style::instance();
    tabBar.setBaseProperties({
        .backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::TAB_BAR),
        .backgroundTransparency = style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::TAB_BAR),
        .borderColor = style.get<Color3>(StyleProperty::BORDER_COLOR, ComponentType::TAB_BAR),
        .borderTransparency = style.get<float>(StyleProperty::BORDER_TRANSPARENCY, ComponentType::TAB_BAR),
        .borderPixelSize = style.get<float>(StyleProperty::BORDER_PIXEL_SIZE, ComponentType::TAB_BAR),
        .cornerRadius = style.get<float>(StyleProperty::CORNER_RADIUS, ComponentType::TAB_BAR),
    });
    tabBar.setTabBarProperties({
        .tabWidth = style.get<float>(StyleProperty::TAB_WIDTH, ComponentType::TAB_BAR),
        .tabSpacing = style.get<float>(StyleProperty::TAB_SPACING, ComponentType::TAB_BAR),
        .barThickness = style.get<float>(StyleProperty::BAR_THICKNESS, ComponentType::TAB_BAR),
        .tabColor = style.get<Color3>(StyleProperty::TAB_COLOR, ComponentType::TAB_BAR),
        .focussedTabColor = style.get<Color3>(StyleProperty::TAB_ACTIVE_COLOR, ComponentType::TAB_BAR),
        .hoveredTabColor = style.get<Color3>(StyleProperty::TAB_HOVERED_COLOR, ComponentType::TAB_BAR),
        .pressedTabColor = style.get<Color3>(StyleProperty::TAB_PRESSED_COLOR, ComponentType::TAB_BAR),
    });
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
    m_tbProps.selectedIndex = 0;
    m_tbProps.closeButtonVisibility = TabCloseButtonVisibility::HOVERED_OR_ACTIVE;

    applyStyle(*this);
}

bool TabBar::setTabBarProperties(const TabBarProperties &props)
{
    bool changed = false;
#define AM_APPLY(field)                                             \
    if (propIsSet(props.field) && m_tbProps.field != props.field) { \
        m_tbProps.field = props.field;                              \
        changed = true;                                             \
    }
    AM_APPLY(closeable)
    AM_APPLY(persistLayout)
    AM_APPLY(mode)
    AM_APPLY(tabPosition)
    AM_APPLY(visibility)
    AM_APPLY(barThickness)
    AM_APPLY(tabWidth)
    AM_APPLY(tabSpacing)
    AM_APPLY(tabOffset)
    AM_APPLY(selectedIndex)
    AM_APPLY(tabColor)
    AM_APPLY(focussedTabColor)
    AM_APPLY(hoveredTabColor)
    AM_APPLY(pressedTabColor)
    AM_APPLY(closeButtonVisibility)
#undef AM_APPLY
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

glm::vec2 TabBar::getContentOffset() const
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

glm::vec2 TabBar::getContentSizeAdjust() const
{
    if (m_tbProps.mode == TabBarMode::OUTSIDE) return {0.0f, 0.0f};

    float bar = getBarSize();
    return isVertical() ? glm::vec2{-bar, 0.0f} : glm::vec2{0.0f, -bar};
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
    if (m_tbProps.selectedIndex < 0 || m_tbProps.selectedIndex >= static_cast<int32_t>(m_tabs.size())) {
        return nullptr;
    }
    return m_tabs[m_tbProps.selectedIndex]->content.get();
}

void TabBar::select(int32_t index)
{
    if (index < 0 || index >= static_cast<int32_t>(m_tabs.size())) return;
    if (index == m_tbProps.selectedIndex) return;

    m_tabs[m_tbProps.selectedIndex]->labelFrame->setBaseProperties({.backgroundColor = m_tbProps.tabColor});
    m_tabs[m_tbProps.selectedIndex]->closeButton->setBaseProperties({
        .visible = static_cast<uint8_t>(s_closeButtonVisible(m_tbProps.closeButtonVisibility, false, false)),
    });

    m_lastSelectedIndex = m_tbProps.selectedIndex;
    m_tbProps.selectedIndex = index;

    m_tabs[m_tbProps.selectedIndex]->labelFrame->setBaseProperties({.backgroundColor = m_tbProps.focussedTabColor});
    m_tabs[m_tbProps.selectedIndex]->closeButton->setBaseProperties({
        .visible = static_cast<uint8_t>(s_closeButtonVisible(m_tbProps.closeButtonVisibility, true, false)),
    });

    if (onSelectionChanged) {
        onSelectionChanged(m_tbProps.selectedIndex);
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

void TabBar::setupTabButton(Tab &tab, int32_t index)
{
    tab.labelFrame = std::make_unique<Frame>();
    tab.labelFrame->parent = this;
    tab.labelFrame->setBaseProperties({
        .backgroundColor = (index == m_tbProps.selectedIndex) ? m_tbProps.focussedTabColor : m_tbProps.tabColor,
    });

    tab.button = std::make_unique<InvisibleButton>();
    tab.button->parent = this;

    auto *drag = tab.button->addExtension<UIDragDetector>();
    drag->mode = isVertical() ? DragMode::SOFT_VERTICAL : DragMode::SOFT_HORIZONTAL;

    Tab *tabPtr = &tab;

    drag->onDragStart = [this, tabPtr](glm::vec2) {
        m_draggedTab = tabPtr;
        m_tornOff = false;
        tabPtr->labelFrame->setBaseProperties({.zIndex = 100});
        tabPtr->button->setBaseProperties({.zIndex = 100});
    };

    drag->onDragUpdate = [this, tabPtr, drag](glm::vec2, glm::vec2 absPos) {
        if (drag->isSoftLockBroken()) {
            if (!m_tornOff) {
                m_tornOff = true;
                if (onTabTornOff) onTabTornOff(tabPtr->content.get());
            }
            if (onTornOffTabMoved) onTornOffTabMoved(tabPtr->content.get(), absPos);
            return;
        }

        int32_t currentIdx = findTabIndex(tabPtr);
        if (currentIdx < 0) return;

        glm::vec2 relPos = absPos - absolutePosition;
        float dragPos = isVertical() ? relPos.y : relPos.x;
        int32_t targetIdx = indexFromPosition(dragPos + m_tbProps.tabWidth * 0.5f);

        if (targetIdx != currentIdx) {
            if (m_tbProps.selectedIndex == currentIdx) {
                m_tbProps.selectedIndex = targetIdx;
                m_lastSelectedIndex = targetIdx;
            } else if (m_tbProps.selectedIndex == targetIdx) {
                m_tbProps.selectedIndex = currentIdx;
                m_lastSelectedIndex = currentIdx;
            }
            std::swap(m_tabs[currentIdx], m_tabs[targetIdx]);
            m_tabs[currentIdx]->labelFrame->markDirty();
            m_tabs[currentIdx]->button->markDirty();
            m_tabs[targetIdx]->labelFrame->markDirty();
            m_tabs[targetIdx]->button->markDirty();
        }
    };

    drag->onDragEnd = [this, tabPtr](glm::vec2 endPos) {
        bool wasTornOff = m_tornOff;
        Instance *content = tabPtr->content.get();

        m_draggedTab = nullptr;
        m_tornOff = false;
        tabPtr->labelFrame->setBaseProperties({.zIndex = 1});
        tabPtr->button->setBaseProperties({.zIndex = 1});

        if (wasTornOff && onTornOffTabReleased) {
            onTornOffTabReleased(content, endPos);
        }
    };

    tab.button->onMouseEnterCb = [this, tabPtr]() {
        int32_t idx = findTabIndex(tabPtr);
        if (idx != m_tbProps.selectedIndex) {
            tabPtr->labelFrame->setBaseProperties({.backgroundColor = m_tbProps.hoveredTabColor});
        }
        tabPtr->closeButton->setBaseProperties({
            .visible =
                static_cast<uint8_t>(s_closeButtonVisible(m_tbProps.closeButtonVisibility, idx == m_tbProps.selectedIndex, true)),
        });
        return EventResult::CONSUMED;
    };

    tab.button->onMouseLeaveCb = [this, tabPtr]() {
        int32_t idx = findTabIndex(tabPtr);
        tabPtr->labelFrame->setBaseProperties({
            .backgroundColor = (idx == m_tbProps.selectedIndex) ? m_tbProps.focussedTabColor : m_tbProps.tabColor,
        });
        tabPtr->closeButton->setBaseProperties({
            .visible =
                static_cast<uint8_t>(s_closeButtonVisible(m_tbProps.closeButtonVisibility, idx == m_tbProps.selectedIndex, false)),
        });
        return EventResult::CONSUMED;
    };

    tab.button->onMouseButton1DownCb = [this, tabPtr](uint32_t, uint32_t) {
        tabPtr->labelFrame->setBaseProperties({.backgroundColor = m_tbProps.pressedTabColor});
        int32_t idx = findTabIndex(tabPtr);
        if (idx >= 0) select(idx);
        return EventResult::CONSUMED;
    };

    tab.button->onMouseButton1UpCb = [this, tabPtr](uint32_t, uint32_t) {
        int32_t idx = findTabIndex(tabPtr);
        tabPtr->labelFrame->setBaseProperties({
            .backgroundColor = (idx == m_tbProps.selectedIndex) ? m_tbProps.focussedTabColor : m_tbProps.hoveredTabColor,
        });
        return EventResult::CONSUMED;
    };

    auto closeBtn = std::make_unique<TextButton>();
    closeBtn->setBaseProperties({
        .anchorPoint = {1.0f, 0.5f},
        .size = UDim2::fromOffset(20.0f, 20.0f),
        .position = UDim2{{1.0f, -5.0f}, {0.5f, 0.0f}},
        .visible = 0,
        .zIndex = 101,
    });
    closeBtn->setTextProperties({
        .textXAlignment = TextXAlignment::CENTER,
        .textYAlignment = TextYAlignment::CENTER,
        .text = "×",
    });
    closeBtn->onMouseButton1ClickCb = [this, tabPtr]() {
        if (onTabClosed) onTabClosed(tabPtr->content.get());
        removeTab(tabPtr->content.get());
        return EventResult::CONSUMED;
    };
    tab.closeButton = closeBtn.get();
    tab.button->addChild(std::move(closeBtn));
}

void TabBar::markAllTabsDirty()
{
    for (auto &tab : m_tabs) {
        tab->labelFrame->markDirty();
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
    lbl->setTextProperties({
        .textYAlignment = TextYAlignment::BOTTOM,
        .text = std::string(label),
    });
    tab->label = lbl.get();
    tab->labelFrame->addChild(std::move(lbl));

    m_tabs.push_back(std::move(tab));
    markAllTabsDirty();
    m_lastSelectedIndex = m_tbProps.selectedIndex;
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
    m_lastSelectedIndex = m_tbProps.selectedIndex;
    markDirty();
    return raw;
}

std::unique_ptr<Instance> TabBar::removeTab(Instance *content)
{
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(), [content](const auto &t) { return t->content.get() == content; });
    if (it == m_tabs.end()) return nullptr;

    int32_t removedIdx = static_cast<int32_t>(std::distance(m_tabs.begin(), it));
    (*it)->labelFrame->parent = nullptr;
    (*it)->button->parent = nullptr;

    auto removedContent = std::move((*it)->content);
    removedContent->parent = nullptr;
    m_tabs.erase(it);

    int32_t oldSelected = m_tbProps.selectedIndex;

    if (m_tabs.empty()) {
        m_tbProps.selectedIndex = 0;
    } else if (m_tbProps.selectedIndex == removedIdx) {
        m_tbProps.selectedIndex = std::min(m_tbProps.selectedIndex, static_cast<int32_t>(m_tabs.size()) - 1);
    } else if (m_tbProps.selectedIndex > removedIdx) {
        m_tbProps.selectedIndex--;
    }

    m_lastSelectedIndex = oldSelected;
    markAllTabsDirty();

    if (!m_tabs.empty()) {
        m_tabs[m_tbProps.selectedIndex]->labelFrame->setBaseProperties({.backgroundColor = m_tbProps.focussedTabColor});
    }

    if (m_tbProps.selectedIndex != oldSelected && onSelectionChanged) {
        onSelectionChanged(m_tbProps.selectedIndex);
    }

    markDirty();
    return removedContent;
}

void TabBar::layoutTabs()
{
    float offset = m_tbProps.tabOffset;

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        auto *frame = m_tabs[i]->labelFrame.get();
        auto *btn = m_tabs[i]->button.get();

        if (m_tabs[i].get() != m_draggedTab) {
            if (isVertical()) {
                float x = (m_tbProps.tabPosition == TabBarPosition::LEFT) ? 0.0f : absoluteSize.x - m_tbProps.barThickness;
                BaseProperties tabProps{
                    .size = UDim2::fromOffset(m_tbProps.barThickness, m_tbProps.tabWidth),
                    .position = UDim2::fromOffset(x, offset),
                    .rotation = (m_tbProps.tabPosition == TabBarPosition::LEFT) ? -90.0f : 90.0f,
                };
                frame->setBaseProperties(tabProps);
                btn->setBaseProperties(tabProps);
            } else {
                float y = (m_tbProps.tabPosition == TabBarPosition::TOP) ? 0.0f : absoluteSize.y - m_tbProps.barThickness;
                BaseProperties tabProps{
                    .size = UDim2::fromOffset(m_tbProps.tabWidth, m_tbProps.barThickness),
                    .position = UDim2::fromOffset(offset, y),
                    .rotation = 0.0f,
                };
                frame->setBaseProperties(tabProps);
                btn->setBaseProperties(tabProps);
            }
        }

        offset += m_tbProps.tabWidth + m_tbProps.tabSpacing;
    }
}

void TabBar::layoutContent()
{
    Instance *selectedContent = getSelectedContent();
    bool selectionChanged = (m_tbProps.selectedIndex != m_lastSelectedIndex);

    Instance *lastContent = nullptr;
    if (m_lastSelectedIndex >= 0 && m_lastSelectedIndex < static_cast<int32_t>(m_tabs.size())) {
        lastContent = m_tabs[m_lastSelectedIndex]->content.get();
    }

    glm::vec2 contentOffset = getContentOffset();
    glm::vec2 sizeAdjust = getContentSizeAdjust();

    for (auto &tab : m_tabs) {
        Instance *child = tab->content.get();
        bool isSelected = (child == selectedContent);
        bool wasSelected = (child == lastContent);

        if (auto *drawable = child->as<UIObject>()) {
            drawable->setBaseProperties({.visible = static_cast<uint8_t>(isSelected)});

            if (selectionChanged && (isSelected || wasSelected)) {
                drawable->markDirty();
            }

            if (isSelected) {
                drawable->setBaseProperties({
                    .position = UDim2{{0.0f, contentOffset.x}, {0.0f, contentOffset.y}},
                    .size = UDim2{{1.0f, sizeAdjust.x}, {1.0f, sizeAdjust.y}},
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
    }

    layoutTabs();
    layoutContent();

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.setPrimitiveType(PRIMITIVE_RECT);

        if (m_geometryAlloc == nullptr) {
            m_geometryAlloc = ctx.geometry->submit(data);
        } else {
            ctx.geometry->update(*m_geometryAlloc, data);
        }
    }

    glm::vec4 childClip = computeChildClipRect();

    if (shouldShowTabs()) {
        DrawContext buttonCtx = ctx;
        if (ctx.overlay) {
            buttonCtx.geometry = ctx.overlay;
        }
        for (auto &tab : m_tabs) {
            tab->labelFrame->clipRect = childClip;
            tab->labelFrame->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            tab->labelFrame->draw(buttonCtx);

            tab->button->clipRect = childClip;
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

    m_lastSelectedIndex = m_tbProps.selectedIndex;
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
