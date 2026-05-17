#include "components/tree_view.h"

#include "logging/log.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "utils/am_assert.h"
#include "utils/profiling.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Amethyst {

static void applyStyle(TreeView &tree)
{
    const auto &style = Style::instance();
    tree.backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::TREE_VIEW);
    tree.backgroundTransparency = style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::TREE_VIEW);
    tree.borderColor = style.get<Color3>(StyleProperty::BORDER_COLOR, ComponentType::TREE_VIEW);
    tree.borderTransparency = style.get<float>(StyleProperty::BORDER_TRANSPARENCY, ComponentType::TREE_VIEW);
    tree.borderPixelSize = style.get<float>(StyleProperty::BORDER_PIXEL_SIZE, ComponentType::TREE_VIEW);
    tree.cornerRadius = style.get<float>(StyleProperty::CORNER_RADIUS, ComponentType::TREE_VIEW);
    tree.rowHeight = style.get<float>(StyleProperty::ROW_HEIGHT, ComponentType::TREE_VIEW);
    tree.rowBackgroundColor = style.get<Color4>(StyleProperty::ROW_BACKGROUND_COLOR, ComponentType::TREE_VIEW);
    tree.rowAlternateColor = style.get<Color4>(StyleProperty::ROW_ALTERNATE_COLOR, ComponentType::TREE_VIEW);
    tree.rowHoverColor = style.get<Color4>(StyleProperty::ROW_HOVER_COLOR, ComponentType::TREE_VIEW);
    tree.rowSelectedColor = style.get<Color4>(StyleProperty::ROW_SELECTED_COLOR, ComponentType::TREE_VIEW);
    tree.indentPerLevel = style.get<float>(StyleProperty::INDENT_PER_LEVEL, ComponentType::TREE_VIEW);
    tree.disclosureTriangleSize = style.get<float>(StyleProperty::DISCLOSURE_TRIANGLE_SIZE, ComponentType::TREE_VIEW);
    tree.disclosureTrianglePadding = style.get<float>(StyleProperty::DISCLOSURE_TRIANGLE_PADDING, ComponentType::TREE_VIEW);
    tree.disclosureTriangleColor = style.get<Color4>(StyleProperty::DISCLOSURE_TRIANGLE_COLOR, ComponentType::TREE_VIEW);
}

inline static float calculateRowY(uint32_t row, float rowHeight)
{
    return static_cast<float>(row) * (rowHeight - 1.0f);
}

TreeView::TreeView()
{
    applyStyle(*this);
}

TreeView::~TreeView()
{
    for (auto &bg : m_rowBackgrounds) {
        bg->parent = nullptr;
    }
    for (auto &btn : m_rowDisclosures) {
        btn->parent = nullptr;
    }
}

uint32_t TreeView::allocateSlot()
{
    uint32_t index;
    if (!m_freelist.empty()) {
        index = m_freelist.back();
        m_freelist.pop_back();
        m_rows[index].generation++;
    } else {
        index = static_cast<uint32_t>(m_rows.size());
        m_rows.emplace_back();
    }

    TreeRow &r = m_rows[index];
    r.alive = true;
    r.parent = INVALID_ROW;
    r.firstChild = INVALID_ROW;
    r.lastChild = INVALID_ROW;
    r.nextSibling = INVALID_ROW;
    r.prevSibling = INVALID_ROW;
    r.firstCellIndex = INVALID_ROW;
    r.expanded = true;

    return index;
}

void TreeView::freeSlot(uint32_t index)
{
    TreeRow &r = m_rows[index];

    if (r.firstCellIndex != INVALID_ROW) {
        uint32_t endIdx = std::min(r.firstCellIndex + numCols, static_cast<uint32_t>(m_children.size()));
        std::vector<Instance *> toRemove;
        for (uint32_t i = r.firstCellIndex; i < endIdx; i++) {
            if (m_children[i]) {
                toRemove.push_back(m_children[i].get());
            }
        }
        for (auto *child : toRemove) {
            removeChild(child);
        }
    }

    r.alive = false;
    m_freelist.push_back(index);
}

bool TreeView::isValidRow(uint32_t index) const
{
    return index < m_rows.size() && m_rows[index].alive;
}

uint32_t TreeView::beginRow(uint32_t parentRow)
{
    AM_ASSERT(parentRow == INVALID_ROW || isValidRow(parentRow), "Parent row does not exist");

    uint32_t newIndex = allocateSlot();
    m_rows[newIndex].firstCellIndex = static_cast<uint32_t>(m_children.size());

    linkRowToParent(newIndex, parentRow);
    m_buildStack.push_back(newIndex);

    markDirty();
    return newIndex;
}

void TreeView::endRow()
{
    AM_ASSERT(!m_buildStack.empty(), "endRow() called without matching beginRow()");
    m_buildStack.pop_back();
}

void TreeView::linkRowToParent(uint32_t row, uint32_t parentRow)
{
    TreeRow &r = m_rows[row];
    r.parent = parentRow;

    if (parentRow == INVALID_ROW) {
        if (m_firstRoot == INVALID_ROW) {
            m_firstRoot = row;
            m_lastRoot = row;
        } else {
            m_rows[m_lastRoot].nextSibling = row;
            r.prevSibling = m_lastRoot;
            m_lastRoot = row;
        }
    } else {
        TreeRow &parent = m_rows[parentRow];
        if (parent.firstChild == INVALID_ROW) {
            parent.firstChild = row;
            parent.lastChild = row;
        } else {
            m_rows[parent.lastChild].nextSibling = row;
            r.prevSibling = parent.lastChild;
            parent.lastChild = row;
        }
    }
}

void TreeView::unlinkRow(uint32_t row)
{
    TreeRow &r = m_rows[row];

    if (r.prevSibling != INVALID_ROW) {
        m_rows[r.prevSibling].nextSibling = r.nextSibling;
    }
    if (r.nextSibling != INVALID_ROW) {
        m_rows[r.nextSibling].prevSibling = r.prevSibling;
    }

    if (r.parent == INVALID_ROW) {
        if (m_firstRoot == row) {
            m_firstRoot = r.nextSibling;
        }
        if (m_lastRoot == row) {
            m_lastRoot = r.prevSibling;
        }
    } else {
        TreeRow &parent = m_rows[r.parent];
        if (parent.firstChild == row) {
            parent.firstChild = r.nextSibling;
        }
        if (parent.lastChild == row) {
            parent.lastChild = r.prevSibling;
        }
    }

    r.parent = INVALID_ROW;
    r.prevSibling = INVALID_ROW;
    r.nextSibling = INVALID_ROW;
}

void TreeView::removeRow(uint32_t row)
{
    AM_ASSERT(isValidRow(row), "Invalid row index");

    TreeRow &r = m_rows[row];
    for (uint32_t child = r.firstChild; child != INVALID_ROW;) {
        uint32_t next = m_rows[child].nextSibling;
        removeRow(child);
        child = next;
    }

    unlinkRow(row);
    freeSlot(row);
    markDirty();
}

void TreeView::clear()
{
    m_children.clear();

    m_rows.clear();
    m_freelist.clear();
    m_firstRoot = INVALID_ROW;
    m_lastRoot = INVALID_ROW;
    m_buildStack.clear();
    m_rowBackgrounds.clear();
    m_rowDisclosures.clear();
    markDirty();
}

uint32_t TreeView::rowCount() const
{
    return static_cast<uint32_t>(m_rows.size() - m_freelist.size());
}

TreeRow &TreeView::row(uint32_t index)
{
    AM_ASSERT(isValidRow(index), "Invalid row index");
    return m_rows[index];
}

const TreeRow &TreeView::row(uint32_t index) const
{
    AM_ASSERT(isValidRow(index), "Invalid row index");
    return m_rows[index];
}

uint32_t TreeView::firstRootRow() const
{
    return m_firstRoot;
}

uint32_t TreeView::depth(uint32_t row) const
{
    AM_ASSERT(isValidRow(row), "Invalid row index");

    uint32_t d = 0;
    uint32_t current = m_rows[row].parent;
    while (current != INVALID_ROW) {
        d++;
        current = m_rows[current].parent;
    }
    return d;
}

bool TreeView::hasChildren(uint32_t row) const
{
    AM_ASSERT(isValidRow(row), "Invalid row index");
    return m_rows[row].firstChild != INVALID_ROW;
}

bool TreeView::isAncestorOf(uint32_t ancestor, uint32_t descendant) const
{
    AM_ASSERT(isValidRow(ancestor) && isValidRow(descendant), "Invalid row index");

    uint32_t current = m_rows[descendant].parent;
    while (current != INVALID_ROW) {
        if (current == ancestor) {
            return true;
        }
        current = m_rows[current].parent;
    }
    return false;
}

uint32_t TreeView::findRowContaining(Instance *child) const
{
    for (uint32_t i = 0; i < m_rows.size(); i++) {
        if (!m_rows[i].alive) {
            continue;
        }
        const TreeRow &r = m_rows[i];
        if (r.firstCellIndex == INVALID_ROW) {
            continue;
        }
        uint32_t endIndex = std::min(r.firstCellIndex + numCols, static_cast<uint32_t>(m_children.size()));
        for (uint32_t j = r.firstCellIndex; j < endIndex; j++) {
            if (m_children[j].get() == child) {
                return i;
            }
        }
    }
    return INVALID_ROW;
}

void TreeView::clearRowCells(DrawContext &ctx, uint32_t row)
{
    AM_PROFILE_FUNCTION();

    const TreeRow &r = m_rows[row];
    if (r.firstCellIndex == INVALID_ROW) return;

    uint32_t endIdx = std::min(r.firstCellIndex + numCols, static_cast<uint32_t>(m_children.size()));
    for (uint32_t i = r.firstCellIndex; i < endIdx; i++) {
        if (auto *obj = m_children[i]->as<UIObject>()) {
            obj->visible = false;
            obj->markDirty();
            obj->draw(ctx);
        }
    }
}

void TreeView::markRowDirty(uint32_t row)
{
    AM_PROFILE_FUNCTION();

    for (uint32_t c = m_rows[row].firstChild; c != INVALID_ROW; c = m_rows[c].nextSibling) {
        if (!m_rows[c].alive) continue;
        m_rows[c].isDirty = true;
        markRowDirty(c);
    }
}

void TreeView::toggle(uint32_t row)
{
    AM_PROFILE_FUNCTION();
    AM_ASSERT(isValidRow(row), "Invalid row index");

    m_rows[row].expanded = !m_rows[row].expanded;
    markRowDirty(row);
    markDirty();

    if (onRowToggled) {
        onRowToggled(row, m_rows[row].expanded);
    }
}

void TreeView::expand(uint32_t row)
{
    AM_ASSERT(isValidRow(row), "Invalid row index");

    if (!m_rows[row].expanded) {
        m_rows[row].expanded = true;
        markDirty();
        if (onRowToggled) {
            onRowToggled(row, true);
        }
    }
}

void TreeView::collapse(uint32_t row)
{
    AM_ASSERT(isValidRow(row), "Invalid row index");

    if (m_rows[row].expanded) {
        m_rows[row].expanded = false;
        markDirty();
        if (onRowToggled) {
            onRowToggled(row, false);
        }
    }
}

void TreeView::expandAll()
{
    for (auto &r : m_rows) {
        if (r.alive) {
            r.expanded = true;
        }
    }
    markDirty();
}

void TreeView::collapseAll()
{
    for (auto &r : m_rows) {
        if (r.alive) {
            r.expanded = false;
        }
    }
    markDirty();
}

bool TreeView::isRowVisible(uint32_t row) const
{
    if (!isValidRow(row)) {
        return false;
    }

    uint32_t p = m_rows[row].parent;
    while (p != INVALID_ROW) {
        if (!m_rows[p].expanded) {
            return false;
        }
        p = m_rows[p].parent;
    }
    return true;
}

float TreeView::getRowIndent(uint32_t row) const
{
    float indent = static_cast<float>(depth(row)) * indentPerLevel;
    if (showDisclosureTriangles) {
        indent += disclosureTriangleSize + disclosureTrianglePadding * 2.0f;
    }
    return indent;
}

void TreeView::drawDisclosureTriangle(DrawContext &ctx, uint32_t row, uint32_t slotIndex, float y, bool expanded,
                                      const glm::vec4 &childClip)
{
    TextButton *btn = m_rowDisclosures[slotIndex].get();

    if (!showDisclosureTriangles || !hasChildren(row)) {
        btn->visible = false;
        btn->interactable = false;
        btn->markDirty();
        btn->draw(ctx);
        return;
    }

    btn->visible = true;
    btn->interactable = true;

    float rowDepth = static_cast<float>(depth(row));
    float x = absolutePosition.x + rowDepth * indentPerLevel + disclosureTrianglePadding;
    float centerY = y + m_computedRowHeight * 0.5f;
    btn->zIndex = getZIndex() + 2;
    btn->backgroundColor = Color3(disclosureTriangleColor);
    btn->backgroundTransparency = 1.0f - disclosureTriangleColor.a;
    btn->rotation = expanded ? 90.0f : 0.0f;
    btn->anchorPoint = {0.5f, 0.5f};
    btn->clipRect = childClip;
    btn->onMouseButton1ClickCb = [this, row]() { toggle(row); return EventResult::CONSUMED; };
    btn->markDirty();

    glm::vec2 triSize = {disclosureTriangleSize, disclosureTriangleSize};
    glm::vec2 triPos = {x, centerY - disclosureTriangleSize * 0.5f};
    btn->computeAbsolutes(triSize, triPos + triSize * 0.5f, absoluteRotation);
    btn->draw(ctx);
}

void TreeView::drawRowContent(DrawContext &ctx, uint32_t row, uint32_t slotIndex, const std::vector<float> &colPositions,
                              const glm::vec4 &childClip)
{
    AM_PROFILE_FUNCTION();

    const TreeRow &r = m_rows[row];
    float rowY = calculateRowY(slotIndex, m_computedRowHeight);
    float indent = getRowIndent(row);

    Frame *bg = m_rowBackgrounds[slotIndex].get();

    Color4 bgColor = rowBackgroundColor;
    if (static_cast<int32_t>(row) == selectedRow) {
        bgColor = rowSelectedColor;
    } else if (static_cast<int32_t>(row) == hoveredRow) {
        bgColor = rowHoverColor;
    } else if (slotIndex % 2 == 1 && rowAlternateColor.a > 0.0f) {
        bgColor = rowAlternateColor;
    }

    bg->backgroundColor = Color3(bgColor);
    bg->backgroundTransparency = 1.0f - bgColor.a;
    bg->clipRect = childClip;
    bg->markDirty();
    bg->computeAbsolutes({absoluteSize.x, m_computedRowHeight}, absolutePosition + glm::vec2(0.0f, rowY), absoluteRotation);
    bg->draw(ctx);

    drawDisclosureTriangle(ctx, row, slotIndex, absolutePosition.y + rowY, r.expanded, childClip);

    if (r.firstCellIndex == INVALID_ROW) {
        return;
    }

    for (uint32_t col = 0; col < numCols; col++) {
        uint32_t childIndex = r.firstCellIndex + col;
        if (childIndex >= m_children.size()) {
            break;
        }

        Instance *child = m_children[childIndex].get();
        if (!child) {
            continue;
        }

        auto *drawable = child->as<UIObject>();
        if (!drawable) {
            continue;
        }

        drawable->visible = true;

        float cellX = colPositions[col];
        float cellWidth = colPositions[col + 1] - cellX;

        if (col == 0) {
            cellX += indent;
            cellWidth -= indent;
        }

        float paddedX = cellX + m_resolvedPadding.w;
        float paddedY = rowY + m_resolvedPadding.x;
        float paddedWidth = cellWidth - m_resolvedPadding.w - m_resolvedPadding.y;
        float paddedHeight = m_computedRowHeight - m_resolvedPadding.x - m_resolvedPadding.z;

        glm::vec2 cellSize = {paddedWidth, paddedHeight};
        glm::vec2 cellPos = absolutePosition + glm::vec2(paddedX, paddedY);

        drawable->clipRect = childClip;
        drawable->computeAbsolutes(cellSize, cellPos, absoluteRotation);
        drawable->draw(ctx);
    }
}

void TreeView::ensureSlotCapacity(uint32_t slotCount)
{
    while (m_rowBackgrounds.size() < slotCount) {
        auto frame = std::make_unique<Frame>();
        frame->parent = this;
        frame->zIndex = getZIndex();
        frame->size = UDim2::fromScale(1.0f, 1.0f);
        m_rowBackgrounds.push_back(std::move(frame));
    }
    while (m_rowDisclosures.size() < slotCount) {
        auto btn = std::make_unique<TextButton>();
        btn->parent = this;
        btn->zIndex = getZIndex() + 2;
        btn->size = UDim2::fromScale(1.0f, 1.0f);
        btn->autoButtonColor = false;
        m_rowDisclosures.push_back(std::move(btn));
    }
}

uint32_t TreeView::drawVisibleRows(DrawContext &ctx, const std::vector<float> &colPositions, const glm::vec4 &childClip,
                                   uint32_t slotCount)
{
    AM_PROFILE_FUNCTION();

    uint32_t slotIndex = 0;
    std::vector<std::pair<uint32_t, bool>> stack;

    for (uint32_t r = m_lastRoot; r != INVALID_ROW; r = m_rows[r].prevSibling) {
        if (m_rows[r].alive) {
            stack.push_back({r, true});
        }
    }

    while (!stack.empty()) {
        auto [row, ancestorVisible] = stack.back();
        stack.pop_back();

        TreeRow &r = m_rows[row];

        if (ancestorVisible) {
            if (slotIndex < slotCount) {
                drawRowContent(ctx, row, slotIndex, colPositions, childClip);
                slotIndex++;
            }
            r.isDirty = false;

            if (r.expanded) {
                for (uint32_t c = r.lastChild; c != INVALID_ROW; c = m_rows[c].prevSibling) {
                    if (m_rows[c].alive) {
                        stack.push_back({c, true});
                    }
                }
            } else {
                for (uint32_t c = r.lastChild; c != INVALID_ROW; c = m_rows[c].prevSibling) {
                    if (m_rows[c].alive && m_rows[c].isDirty) {
                        stack.push_back({c, false});
                    }
                }
            }
        } else {
            clearRowCells(ctx, row);
            r.isDirty = false;

            for (uint32_t c = r.lastChild; c != INVALID_ROW; c = m_rows[c].prevSibling) {
                if (m_rows[c].alive && m_rows[c].isDirty) {
                    stack.push_back({c, false});
                }
            }
        }
    }

    return slotIndex;
}

void TreeView::drawEmptyRows(DrawContext &ctx, const glm::vec4 &childClip, uint32_t fromSlot, uint32_t slotCount)
{
    AM_PROFILE_FUNCTION();

    for (uint32_t i = fromSlot; i < slotCount; i++) {
        float rowY = calculateRowY(i, m_computedRowHeight);
        Frame *bg = m_rowBackgrounds[i].get();
        Color4 bgColor = (i % 2 == 1 && rowAlternateColor.a > 0.0f) ? rowAlternateColor : rowBackgroundColor;
        bg->backgroundColor = Color3(bgColor);
        bg->backgroundTransparency = 1.0f - bgColor.a;
        bg->clipRect = childClip;
        bg->markDirty();
        bg->computeAbsolutes({absoluteSize.x, m_computedRowHeight}, absolutePosition + glm::vec2(0.0f, rowY), absoluteRotation);
        bg->draw(ctx);
    }
}

void TreeView::clearUnusedSlots(DrawContext &ctx, uint32_t fromSlot)
{
    AM_PROFILE_FUNCTION();

    for (uint32_t i = fromSlot; i < m_rowDisclosures.size(); i++) {
        TextButton *btn = m_rowDisclosures[i].get();
        btn->visible = false;
        btn->interactable = false;
        btn->markDirty();
        btn->draw(ctx);
    }
}

void TreeView::draw(DrawContext &ctx)
{
    AM_PROFILE_FUNCTION();

    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (numCols == 0) {
        flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
        return;
    }

    if (flags & FLAG_DIRTY) {
        updateSeparators();
        m_resolvedPadding = cellPadding.resolve(absoluteSize);
    }

    glm::vec4 childClip = computeChildClipRect();

    m_computedRowHeight = rowHeight > 0.0f ? rowHeight : 24.0f;
    uint32_t slotCount = static_cast<uint32_t>(std::ceil(absoluteSize.y / m_computedRowHeight)) + 1;

    ensureSlotCapacity(slotCount);

    std::vector<float> colPositions = computeColumnPositions(absoluteSize.x);

    for (auto &sep : m_separators) {
        sep->clipRect = childClip;
        sep->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        sep->draw(ctx);
    }

    uint32_t usedSlots = drawVisibleRows(ctx, colPositions, childClip, slotCount);

    if (fillRows) {
        drawEmptyRows(ctx, childClip, usedSlots, slotCount);
    }

    clearUnusedSlots(ctx, usedSlots);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

std::vector<Instance *> TreeView::getHittableInstances()
{
    std::vector<Instance *> result;
    result.reserve(m_rowBackgrounds.size() + m_rowDisclosures.size() + m_children.size());
    for (auto &btn : m_rowDisclosures) {
        result.push_back(btn.get());
    }

    for (auto &bg : m_rowBackgrounds) {
        result.push_back(bg.get());
    }
    for (auto &child : m_children) {
        result.push_back(child.get());
    }

    return result;
}

} // namespace Amethyst
