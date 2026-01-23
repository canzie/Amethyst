#include "tab_bar.h"

#include "components/common.h"
#include "components/extensions/ui_drag_detector.h"
#include "logging/log.h"
#include "rendering/geometry_registry.h"

#include <algorithm>

namespace Amethyst {

TabBar::TabBar() {}

TabBar::TabBar(Instance *parent)
{
    setParent(parent);
}

TabBar::~TabBar()
{
    m_tabItems.clear();
}

void TabBar::addChild(Instance *child)
{
    Instance::addChild(child);

    if (auto *obj = child->as<UIObject>()) {
        obj->zIndex = zIndex + 1;
    }

    auto tabItem = std::make_unique<TabItem>();
    tabItem->content = child;
    tabItem->button = std::make_unique<TextButton>(nullptr);
    tabItem->button->parent = this;
    tabItem->button->backgroundColor = glm::vec3(0.0f);
    tabItem->button->text = child->name.empty() ? "Tab " + std::to_string(m_tabItems.size() + 1) : child->name;
    tabItem->button->textYAlignment = TextYAlignment::BOTTOM;
    tabItem->button->zIndex = zIndex + 2;

    bool isVertical = (tabPosition == TabBarPosition::LEFT || tabPosition == TabBarPosition::RIGHT);
    auto *drag = tabItem->button->addExtension<UIDragDetector>();
    drag->mode = isVertical ? DragMode::SOFT_VERTICAL : DragMode::SOFT_HORIZONTAL;

    TabItem *tabItemPtr = tabItem.get();
    drag->onDragStart = [this, tabItemPtr](glm::vec2) {
        m_draggedTab = tabItemPtr;
        m_tabTornOff = false;
    };

    drag->onDragUpdate = [this, tabItemPtr, drag](glm::vec2, glm::vec2 absPos) {
        if (drag->isSoftLockBroken()) {
            if (!m_tabTornOff) {
                m_tabTornOff = true;
                if (onTabTornOff) {
                    onTabTornOff(tabItemPtr->content);
                }
            }
            if (onTornOffTabMoved) {
                onTornOffTabMoved(tabItemPtr->content, absPos);
            }
            return;
        }

        int32_t currentIdx = findTabIndex(tabItemPtr);
        if (currentIdx < 0) return;

        glm::vec2 relPos = absPos - absolutePosition;
        bool isVert = (tabPosition == TabBarPosition::LEFT || tabPosition == TabBarPosition::RIGHT);
        float dragPos = isVert ? relPos.y : relPos.x;
        int32_t targetIdx = computeTargetIndex(dragPos + tabWidth * 0.5f);

        if (targetIdx != currentIdx) {
            if (selectedIndex == currentIdx) {
                selectedIndex = targetIdx;
            } else if (selectedIndex == targetIdx) {
                selectedIndex = currentIdx;
            }
            std::swap(m_tabItems[currentIdx], m_tabItems[targetIdx]);
        }
    };

    drag->onDragEnd = [this, tabItemPtr](glm::vec2 endPos) {
        bool wasTornOff = m_tabTornOff;
        Instance *content = tabItemPtr->content;
        m_draggedTab = nullptr;
        m_tabTornOff = false;

        if (wasTornOff && onTornOffTabReleased) {
            onTornOffTabReleased(content, endPos);
        }
    };

    tabItem->button->onMouseButton1DownCb = [this, tabItemPtr](uint32_t, uint32_t) {
        int32_t idx = findTabIndex(tabItemPtr);
        if (idx >= 0) {
            selectedIndex = idx;
            markDirty();
        }
    };

    tabItem->button->markDirty();
    m_tabItems.push_back(std::move(tabItem));
    markDirty();
}

void TabBar::removeChild(Instance *child)
{

    int32_t removedIndex = -1;
    auto it = std::find_if(m_tabItems.begin(), m_tabItems.end(), [child](const auto &item) { return item->content == child; });
    if (it != m_tabItems.end()) {

        removedIndex = static_cast<int32_t>(std::distance(m_tabItems.begin(), it));
        if ((*it)->button) {
            (*it)->button->parent = nullptr;
        }
        auto dyingItem = std::move(*it);
        m_tabItems.erase(it);

        if (selectedIndex == removedIndex) {
            selectedIndex = std::min(selectedIndex, static_cast<int32_t>(m_tabItems.size()) - 1);
        } else if (selectedIndex > removedIndex) {
            selectedIndex--;
        }
    }

    Instance::removeChild(child);
    markDirty();
}

std::vector<Instance *> TabBar::getHittableInstances()
{
    formatChildren();

    std::vector<Instance *> result;
    for (auto &tab : m_tabItems) {
        result.push_back(tab->button.get());
    }
    for (Instance *child : children) {
        result.push_back(child);
    }
    return result;
}

void TabBar::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    formatChildren();

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.primitiveType = PRIMITIVE_RECT;

        if (m_geometryAlloc == nullptr) {
            m_geometryAlloc = ctx.geometry->submit(data);
        } else {
            ctx.geometry->update(*m_geometryAlloc, data);
        }
    }

    bool showTabs = (visibility == TabBarVisibility::ALWAYS) || (visibility == TabBarVisibility::AUTO && children.size() > 1);

    if (showTabs) {
        for (auto &tab : m_tabItems) {
            tab->button->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            tab->button->draw(ctx);
        }
    }

    for (Instance *child : children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void TabBar::formatChildren()
{
    bool isVertical = (tabPosition == TabBarPosition::LEFT || tabPosition == TabBarPosition::RIGHT);
    bool showTabs = (visibility == TabBarVisibility::ALWAYS) || (visibility == TabBarVisibility::AUTO && children.size() > 1);

    float tabBarSize = showTabs ? tabThickness : 0.0f;

    float offset = 0.0f;
    for (size_t i = 0; i < m_tabItems.size(); ++i) {
        auto *btn = m_tabItems[i]->button.get();
        bool isSelected = (static_cast<int32_t>(i) == selectedIndex);
        bool isDragged = (m_tabItems[i].get() == m_draggedTab);

        if (!isDragged) {
            if (isVertical) {
                btn->size.offset = {tabThickness, tabWidth};
                btn->size.scale = {0.0f, 0.0f};
                if (tabPosition == TabBarPosition::LEFT) {
                    btn->position.offset = {0.0f, offset};
                    btn->rotation = -90.0f;
                } else {
                    btn->position.offset = {absoluteSize.x - tabThickness, offset};
                    btn->rotation = 90.0f;
                }
            } else {
                btn->size.offset = {tabWidth, tabThickness};
                btn->size.scale = {0.0f, 0.0f};
                if (tabPosition == TabBarPosition::TOP) {
                    btn->position.offset = {offset, 0.0f};
                } else {
                    btn->position.offset = {offset, absoluteSize.y - tabThickness};
                }
                btn->rotation = 0.0f;
            }
        }
        offset += tabWidth;

        btn->backgroundColor = isSelected ? Color3{0.0f, 0.0f, 1.0f} : Color3{0.5f, 0.5f, 0.5f};
        btn->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        btn->markDirty();
    }

    Instance *selectedContent = (selectedIndex >= 0 && selectedIndex < static_cast<int32_t>(m_tabItems.size()))
                                    ? m_tabItems[selectedIndex]->content
                                    : nullptr;

    for (size_t i = 0; i < children.size(); ++i) {
        if (auto *drawable = children[i]->as<UIObject>()) {
            bool isSelected = (children[i] == selectedContent);
            drawable->visible = isSelected;
            drawable->markDirty();
            if (!isSelected) continue;

            drawable->size.scale = {1.0f, 1.0f};

            if (mode == TabBarMode::INSIDE) {
                switch (tabPosition) {
                case TabBarPosition::TOP:
                    drawable->position.offset = {0.0f, tabBarSize};
                    drawable->size.offset = {0.0f, -tabBarSize};
                    break;
                case TabBarPosition::BOTTOM:
                    drawable->position.offset = {0.0f, 0.0f};
                    drawable->size.offset = {0.0f, -tabBarSize};
                    break;
                case TabBarPosition::LEFT:
                    drawable->position.offset = {tabBarSize, 0.0f};
                    drawable->size.offset = {-tabBarSize, 0.0f};
                    break;
                case TabBarPosition::RIGHT:
                    drawable->position.offset = {0.0f, 0.0f};
                    drawable->size.offset = {-tabBarSize, 0.0f};
                    break;
                }
            } else {
                drawable->position.offset = {0.0f, 0.0f};
                drawable->size.offset = {0.0f, 0.0f};
            }
        }
    }
}

int32_t TabBar::findTabIndex(TabItem *item) const
{
    for (size_t i = 0; i < m_tabItems.size(); ++i) {
        if (m_tabItems[i].get() == item) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

int32_t TabBar::computeTargetIndex(float dragPosition) const
{
    int32_t index = static_cast<int32_t>(dragPosition / tabWidth);
    return std::clamp(index, 0, static_cast<int32_t>(m_tabItems.size()) - 1);
}

} // namespace Amethyst
