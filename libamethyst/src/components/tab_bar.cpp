#include "tab_bar.h"

#include "components/common.h"
#include "components/extensions/ui_drag_detector.h"
#include "components/ui_layer.h"
#include "modules/style.h"
#include "rendering/geometry_registry.h"

#include <algorithm>

namespace Amethyst {

static void applyStyle(TabBar &tabBar)
{
    const auto &style = Style::instance();
    tabBar.backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::TAB_BAR);
    tabBar.backgroundTransparency = style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::TAB_BAR);
    tabBar.borderColor = style.get<Color3>(StyleProperty::BORDER_COLOR, ComponentType::TAB_BAR);
    tabBar.borderTransparency = style.get<float>(StyleProperty::BORDER_TRANSPARENCY, ComponentType::TAB_BAR);
    tabBar.borderPixelSize = style.get<float>(StyleProperty::BORDER_PIXEL_SIZE, ComponentType::TAB_BAR);
    tabBar.cornerRadius = style.get<float>(StyleProperty::CORNER_RADIUS, ComponentType::TAB_BAR);
    tabBar.tabWidth = style.get<float>(StyleProperty::TAB_WIDTH, ComponentType::TAB_BAR);
    tabBar.barThickness = style.get<float>(StyleProperty::BAR_THICKNESS, ComponentType::TAB_BAR);
}

TabBar::TabBar()
{
    applyStyle(*this);
}

bool TabBar::isVertical() const
{
    return tabPosition == TabBarPosition::LEFT || tabPosition == TabBarPosition::RIGHT;
}

bool TabBar::shouldShowTabs() const
{
    if (visibility == TabBarVisibility::NEVER) return false;
    if (visibility == TabBarVisibility::ALWAYS) return true;
    return m_children.size() > 1;
}

float TabBar::getBarSize() const
{
    return shouldShowTabs() ? barThickness : 0.0f;
}

glm::vec2 TabBar::getContentOffset() const
{
    if (mode == TabBarMode::OUTSIDE) return {0.0f, 0.0f};

    float bar = getBarSize();
    switch (tabPosition) {
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
    if (mode == TabBarMode::OUTSIDE) return {0.0f, 0.0f};

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
    int32_t idx = static_cast<int32_t>(pos / tabWidth);
    return std::clamp(idx, 0, static_cast<int32_t>(m_tabs.size()) - 1);
}

Instance *TabBar::getSelectedContent() const
{
    if (selectedIndex < 0 || selectedIndex >= static_cast<int32_t>(m_tabs.size())) {
        return nullptr;
    }
    return m_tabs[selectedIndex]->content;
}

void TabBar::select(int32_t index)
{
    if (index < 0 || index >= static_cast<int32_t>(m_tabs.size())) return;
    if (index == selectedIndex) return;

    m_lastSelectedIndex = selectedIndex;
    selectedIndex = index;
    markDirty();

    if (onSelectionChanged) {
        onSelectionChanged(selectedIndex);
    }
}

void TabBar::select(Instance *content)
{
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->content == content) {
            select(static_cast<int32_t>(i));
            return;
        }
    }
}

void TabBar::setupTabButton(Tab &tab, int32_t index)
{
    tab.button = std::make_unique<TextButton>();
    tab.button->parent = this;
    tab.button->backgroundColor = Color3{0.5f, 0.5f, 0.5f};
    tab.button->textYAlignment = TextYAlignment::BOTTOM;

    if (tab.content) {
        tab.button->text = tab.content->name.empty() ? "Tab " + std::to_string(index + 1) : tab.content->name;
    }

    auto *drag = tab.button->addExtension<UIDragDetector>();
    drag->mode = isVertical() ? DragMode::SOFT_VERTICAL : DragMode::SOFT_HORIZONTAL;

    Tab *tabPtr = &tab;

    drag->onDragStart = [this, tabPtr](glm::vec2) {
        m_draggedTab = tabPtr;
        m_tornOff = false;
    };

    drag->onDragUpdate = [this, tabPtr, drag](glm::vec2, glm::vec2 absPos) {
        if (drag->isSoftLockBroken()) {
            if (!m_tornOff) {
                m_tornOff = true;
                if (onTabTornOff) onTabTornOff(tabPtr->content);
            }
            if (onTornOffTabMoved) onTornOffTabMoved(tabPtr->content, absPos);
            return;
        }

        int32_t currentIdx = findTabIndex(tabPtr);
        if (currentIdx < 0) return;

        glm::vec2 relPos = absPos - absolutePosition;
        float dragPos = isVertical() ? relPos.y : relPos.x;
        int32_t targetIdx = indexFromPosition(dragPos + tabWidth * 0.5f);

        if (targetIdx != currentIdx) {
            if (selectedIndex == currentIdx) {
                selectedIndex = targetIdx;
                m_lastSelectedIndex = targetIdx;
            } else if (selectedIndex == targetIdx) {
                selectedIndex = currentIdx;
                m_lastSelectedIndex = currentIdx;
            }
            std::swap(m_tabs[currentIdx], m_tabs[targetIdx]);
            m_tabs[currentIdx]->button->markDirty();
            m_tabs[targetIdx]->button->markDirty();
        }
    };

    drag->onDragEnd = [this, tabPtr](glm::vec2 endPos) {
        bool wasTornOff = m_tornOff;
        Instance *content = tabPtr->content;

        m_draggedTab = nullptr;
        m_tornOff = false;
        tabPtr->button->markDirty();

        if (wasTornOff && onTornOffTabReleased) {
            onTornOffTabReleased(content, endPos);
        }
    };

    tab.button->onMouseButton1DownCb = [this, tabPtr](uint32_t, uint32_t) {
        int32_t idx = findTabIndex(tabPtr);
        if (idx >= 0) select(idx);
        return EventResult::CONSUMED;
    };
}

void TabBar::markAllTabsDirty()
{
    for (auto &tab : m_tabs) {
        tab->button->markDirty();
    }
}

Instance *TabBar::addChild(std::unique_ptr<Instance> child)
{
    Instance *raw = child.get();
    auto existing = std::find_if(m_tabs.begin(), m_tabs.end(), [raw](const auto &t) { return t->content == raw; });
    if (existing != m_tabs.end()) return raw;

    Instance::addChild(std::move(child));

    if (auto *layer = raw->as<UILayer>()) {
        layer->setDisplayOrder(2);
    }

    auto tab = std::make_unique<Tab>();
    tab->content = raw;
    setupTabButton(*tab, static_cast<int32_t>(m_tabs.size()));
    tab->button->markDirty();
    m_tabs.push_back(std::move(tab));

    markAllTabsDirty();
    m_lastSelectedIndex = selectedIndex;
    markDirty();
    return raw;
}

std::unique_ptr<Instance> TabBar::removeChild(Instance *child)
{
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(), [child](const auto &t) { return t->content == child; });

    if (it != m_tabs.end()) {
        int32_t removedIdx = static_cast<int32_t>(std::distance(m_tabs.begin(), it));
        (*it)->button->parent = nullptr;
        m_tabs.erase(it);

        int32_t oldSelected = selectedIndex;

        if (m_tabs.empty()) {
            selectedIndex = 0;
        } else if (selectedIndex == removedIdx) {
            selectedIndex = std::min(selectedIndex, static_cast<int32_t>(m_tabs.size()) - 1);
        } else if (selectedIndex > removedIdx) {
            selectedIndex--;
        }

        m_lastSelectedIndex = oldSelected;
        markAllTabsDirty();

        if (selectedIndex != oldSelected && onSelectionChanged) {
            onSelectionChanged(selectedIndex);
        }
    }

    auto result = Instance::removeChild(child);
    markDirty();
    return result;
}

void TabBar::layoutTabs()
{
    float offset = 0.0f;

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        auto *btn = m_tabs[i]->button.get();

        if (m_tabs[i].get() != m_draggedTab) {
            btn->size.scale = {0.0f, 0.0f};

            if (isVertical()) {
                btn->size.offset = {barThickness, tabWidth};
                btn->rotation = (tabPosition == TabBarPosition::LEFT) ? -90.0f : 90.0f;
                float x = (tabPosition == TabBarPosition::LEFT) ? 0.0f : absoluteSize.x - barThickness;
                btn->position.offset = {x, offset};
            } else {
                btn->size.offset = {tabWidth, barThickness};
                btn->rotation = 0.0f;
                float y = (tabPosition == TabBarPosition::TOP) ? 0.0f : absoluteSize.y - barThickness;
                btn->position.offset = {offset, y};
            }
        }

        offset += tabWidth;
    }
}

void TabBar::updateTabVisuals()
{
    bool selectionChanged = (selectedIndex != m_lastSelectedIndex);

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        auto *btn = m_tabs[i]->button.get();
        bool isSelected = (static_cast<int32_t>(i) == selectedIndex);

        btn->backgroundColor = isSelected ? Color3{0.0f, 0.0f, 1.0f} : Color3{0.5f, 0.5f, 0.5f};

        if (selectionChanged) {
            bool wasSelected = (static_cast<int32_t>(i) == m_lastSelectedIndex);
            if (isSelected || wasSelected) {
                btn->markDirty();
            }
        }
    }
}

void TabBar::layoutContent()
{
    Instance *selectedContent = getSelectedContent();
    bool selectionChanged = (selectedIndex != m_lastSelectedIndex);

    Instance *lastContent = nullptr;
    if (m_lastSelectedIndex >= 0 && m_lastSelectedIndex < static_cast<int32_t>(m_tabs.size())) {
        lastContent = m_tabs[m_lastSelectedIndex]->content;
    }

    glm::vec2 contentOffset = getContentOffset();
    glm::vec2 sizeAdjust = getContentSizeAdjust();

    for (auto &child : m_children) {
        bool isSelected = (child.get() == selectedContent);
        bool wasSelected = (child.get() == lastContent);

        if (auto *drawable = child->as<UIObject>()) {
            drawable->visible = isSelected;

            if (selectionChanged && (isSelected || wasSelected)) {
                drawable->markDirty();
            }

            if (isSelected) {
                drawable->position.offset = contentOffset;
                drawable->size.scale = {1.0f, 1.0f};
                drawable->size.offset = sizeAdjust;
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
    result.reserve(m_tabs.size() + m_children.size());

    for (auto &tab : m_tabs) {
        result.push_back(tab->button.get());
    }
    for (auto &child : m_children) {
        result.push_back(child.get());
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
    updateTabVisuals();
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
            tab->button->clipRect = childClip;
            tab->button->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            tab->button->draw(buttonCtx);
        }
    }

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        } else if (auto *layer = child->as<UILayer>()) {
            layer->clipRect = childClip;
            layer->draw(ctx);
        }
    }

    m_lastSelectedIndex = selectedIndex;
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

    for (auto &child : m_children) {
        if (child->name == config.selectedTab) {
            select(child.get());
            return;
        }
    }
}

} // namespace Amethyst
