#include "components/tree_view.h"

#include "logging/log.h"
#include "rendering/draw_context.h"
#include "utils/am_assert.h"

#include <algorithm>

namespace Amethyst {

TreeView::TreeView(Instance *parent)
{
    setParent(parent);
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
        uint32_t endIdx = std::min(r.firstCellIndex + numCols, static_cast<uint32_t>(children.size()));
        for (uint32_t i = r.firstCellIndex; i < endIdx; i++) {
            if (children[i]) {
                removeChild(children[i]);
            }
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
    m_rows[newIndex].firstCellIndex = static_cast<uint32_t>(children.size());

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
    for (Instance *child : children) {
        if (child) {
            removeChild(child);
        }
    }

    m_rows.clear();
    m_freelist.clear();
    m_firstRoot = INVALID_ROW;
    m_lastRoot = INVALID_ROW;
    m_buildStack.clear();
    m_rowBackgrounds.clear();
    m_disclosureFrames.clear();
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
        uint32_t endIndex = std::min(r.firstCellIndex + numCols, static_cast<uint32_t>(children.size()));
        for (uint32_t j = r.firstCellIndex; j < endIndex; j++) {
            if (children[j] == child) {
                return i;
            }
        }
    }
    return INVALID_ROW;
}

void TreeView::toggle(uint32_t row)
{
    AM_ASSERT(isValidRow(row), "Invalid row index");

    m_rows[row].expanded = !m_rows[row].expanded;
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

void TreeView::drawDisclosureTriangle(DrawContext &ctx, uint32_t row, float y, bool expanded, const glm::vec4 &childClip)
{
    if (!showDisclosureTriangles || !hasChildren(row)) {
        return;
    }

    float rowDepth = static_cast<float>(depth(row));
    float x = absolutePosition.x + rowDepth * indentPerLevel + disclosureTrianglePadding;
    float centerY = y + m_computedRowHeight * 0.5f;

    while (m_disclosureFrames.size() <= row) {
        auto frame = std::make_unique<Frame>();
        frame->zIndex = zIndex + 2;
        frame->size = UDim2::fromScale(1.0f, 1.0f);
        m_disclosureFrames.push_back(std::move(frame));
    }

    Frame *triangle = m_disclosureFrames[row].get();
    triangle->backgroundColor = Color3(disclosureTriangleColor);
    triangle->backgroundTransparency = 1.0f - disclosureTriangleColor.a;
    triangle->rotation = expanded ? 90.0f : 0.0f;
    triangle->anchorPoint = {0.5f, 0.5f};
    triangle->clipRect = childClip;
    triangle->markDirty();

    glm::vec2 triSize = {disclosureTriangleSize, disclosureTriangleSize};
    glm::vec2 triPos = {x, centerY - disclosureTriangleSize * 0.5f};
    triangle->computeAbsolutes(triSize, triPos + triSize * 0.5f, absoluteRotation);
    triangle->draw(ctx);
}

void TreeView::drawRowContent(DrawContext &ctx, uint32_t row, uint32_t visibleIndex, const std::vector<float> &colPositions, const glm::vec4 &childClip)
{
    const TreeRow &r = m_rows[row];
    float rowY = static_cast<float>(visibleIndex) * m_computedRowHeight;
    float indent = getRowIndent(row);

    while (m_rowBackgrounds.size() <= visibleIndex) {
        auto frame = std::make_unique<Frame>();
        frame->zIndex = zIndex;
        frame->size = UDim2::fromScale(1.0f, 1.0f);
        m_rowBackgrounds.push_back(std::move(frame));
    }

    Frame *bg = m_rowBackgrounds[visibleIndex].get();

    Color4 bgColor = rowBackgroundColor;
    if (static_cast<int32_t>(row) == selectedRow) {
        bgColor = rowSelectedColor;
    } else if (static_cast<int32_t>(row) == hoveredRow) {
        bgColor = rowHoverColor;
    } else if (visibleIndex % 2 == 1 && rowAlternateColor.a > 0.0f) {
        bgColor = rowAlternateColor;
    }

    bg->backgroundColor = Color3(bgColor);
    bg->backgroundTransparency = 1.0f - bgColor.a;
    bg->clipRect = childClip;
    bg->markDirty();
    bg->computeAbsolutes({absoluteSize.x, m_computedRowHeight}, absolutePosition + glm::vec2(0.0f, rowY), absoluteRotation);
    bg->draw(ctx);

    drawDisclosureTriangle(ctx, row, absolutePosition.y + rowY, r.expanded, childClip);

    if (r.firstCellIndex == INVALID_ROW) {
        return;
    }

    for (uint32_t col = 0; col < numCols; col++) {
        uint32_t childIndex = r.firstCellIndex + col;
        if (childIndex >= children.size()) {
            break;
        }

        Instance *child = children[childIndex];
        if (!child) {
            continue;
        }

        auto *drawable = child->as<UIObject>();
        if (!drawable) {
            continue;
        }

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

void TreeView::draw(DrawContext &ctx)
{

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

    glm::vec4 childClip = clipRect;
    if (clipsDescendants) {
        childClip = {absolutePosition.x, absolutePosition.y,
                     absolutePosition.x + absoluteSize.x, absolutePosition.y + absoluteSize.y};
    }

    float effectiveRowHeight = rowHeight;
    if (effectiveRowHeight <= 0.0f) {
        effectiveRowHeight = 24.0f;
    }
    m_computedRowHeight = effectiveRowHeight;

    std::vector<float> colPositions = computeColumnPositions(absoluteSize.x);

    for (auto &sep : m_separators) {
        sep->clipRect = childClip;
        sep->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        sep->draw(ctx);
    }

    uint32_t visibleIndex = 0;
    forEachVisibleRow([&](uint32_t row) {
        drawRowContent(ctx, row, visibleIndex, colPositions, childClip);
        visibleIndex++;
    });

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
