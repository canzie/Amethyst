#include "components/tree_view.h"

#include "logging/log.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "utils/am_assert.h"
#include "utils/profiling.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#define TREE_VIEW_MAX_SLOT_COUNT 100000u

namespace Amethyst {

static void s_applyStyle(TreeView &tree)
{
    const auto &style = Style::instance();
    tree.setBaseProperties({
        .backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::TREE_VIEW),
        .backgroundTransparency = style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::TREE_VIEW),
        .borderPixelSize = style.get<float>(StyleProperty::BORDER_PIXEL_SIZE, ComponentType::TREE_VIEW),
        .borderColor = style.get<Color3>(StyleProperty::BORDER_COLOR, ComponentType::TREE_VIEW),
        .borderTransparency = style.get<float>(StyleProperty::BORDER_TRANSPARENCY, ComponentType::TREE_VIEW),
        .cornerRadius = style.get<float>(StyleProperty::CORNER_RADIUS, ComponentType::TREE_VIEW),
    });
    tree.setTreeViewProperties({
        .rowHeight = style.get<float>(StyleProperty::ROW_HEIGHT, ComponentType::TREE_VIEW),
        .disclosureTriangleSize = style.get<float>(StyleProperty::DISCLOSURE_TRIANGLE_SIZE, ComponentType::TREE_VIEW),
        .disclosureTrianglePadding = style.get<float>(StyleProperty::DISCLOSURE_TRIANGLE_PADDING, ComponentType::TREE_VIEW),
        .disclosureTriangleColor = style.get<Color4>(StyleProperty::DISCLOSURE_TRIANGLE_COLOR, ComponentType::TREE_VIEW),
        .indentPerLevel = style.get<float>(StyleProperty::INDENT_PER_LEVEL, ComponentType::TREE_VIEW),
        .rowBackgroundColor = style.get<Color4>(StyleProperty::ROW_BACKGROUND_COLOR, ComponentType::TREE_VIEW),
        .rowAlternateColor = style.get<Color4>(StyleProperty::ROW_ALTERNATE_COLOR, ComponentType::TREE_VIEW),
        .rowHoverColor = style.get<Color4>(StyleProperty::ROW_HOVER_COLOR, ComponentType::TREE_VIEW),
        .rowSelectedColor = style.get<Color4>(StyleProperty::ROW_SELECTED_COLOR, ComponentType::TREE_VIEW),
    });
}

inline static float calculateRowY(uint32_t row, float rowHeight)
{
    return static_cast<float>(row) * (rowHeight - 1.0f);
}

TreeView::TreeView()
{
    m_tvProps.rowHeight = 0.0f;
    m_tvProps.cellPadding = {};
    m_tvProps.showColumnSeparators = 0;
    m_tvProps.columnSeparatorWidth = 1.0f;
    m_tvProps.columnSeparatorColor = {0.3f, 0.3f, 0.3f, 1.0f};
    m_tvProps.indentPerLevel = 16.0f;
    m_tvProps.showDisclosureTriangles = 1;
    m_tvProps.disclosureTriangleSize = 10.0f;
    m_tvProps.disclosureTrianglePadding = 4.0f;
    m_tvProps.disclosureTriangleColor = {0.7f, 0.7f, 0.7f, 1.0f};
    m_tvProps.rowBackgroundColor = {0.18f, 0.18f, 0.2f, 1.0f};
    m_tvProps.rowAlternateColor = {0.22f, 0.22f, 0.24f, 1.0f};
    m_tvProps.rowHoverColor = {0.3f, 0.3f, 0.35f, 1.0f};
    m_tvProps.rowSelectedColor = {0.25f, 0.4f, 0.65f, 1.0f};
    m_tvProps.fillRows = 1;

    s_applyStyle(*this);
}

bool TreeView::setTreeViewProperties(const TreeViewProperties &props)
{
    bool changed = false;
#define AM_APPLY(field)                                             \
    if (propIsSet(props.field) && m_tvProps.field != props.field) { \
        m_tvProps.field = props.field;                              \
        changed = true;                                             \
    }
    AM_APPLY(rowHeight)
    AM_APPLY(cellPadding)
    AM_APPLY(showColumnSeparators)
    AM_APPLY(columnSeparatorWidth)
    AM_APPLY(columnSeparatorColor)
    AM_APPLY(showDisclosureTriangles)
    AM_APPLY(disclosureTriangleSize)
    AM_APPLY(disclosureTrianglePadding)
    AM_APPLY(disclosureTriangleColor)
    AM_APPLY(indentPerLevel)
    AM_APPLY(rowBackgroundColor)
    AM_APPLY(rowAlternateColor)
    AM_APPLY(rowHoverColor)
    AM_APPLY(rowSelectedColor)
    AM_APPLY(fillRows)
#undef AM_APPLY
    if (changed) {
        markDirty();
    }
    return changed;
}

std::vector<float> TreeView::computeColumnPositions(float tableWidth) const
{
    std::vector<float> positions;
    positions.reserve(numCols + 1);
    positions.push_back(0.0f);

    if (columnWeights.empty()) {
        float columnWidth = tableWidth / static_cast<float>(numCols);
        for (uint32_t i = 1; i <= numCols; ++i) {
            positions.push_back(columnWidth * static_cast<float>(i));
        }
    } else {
        float accumulatedWidth = 0.0f;
        for (uint32_t i = 0; i < numCols; ++i) {
            accumulatedWidth += columnWeights[i] * tableWidth;
            positions.push_back(accumulatedWidth);
        }
    }

    return positions;
}

void TreeView::updateSeparators()
{
    m_separators.clear();
    if (!static_cast<bool>(m_tvProps.showColumnSeparators) || numCols <= 1) {
        return;
    }

    float accumulatedScale = 0.0f;
    for (uint32_t i = 0; i < numCols - 1; ++i) {
        float colWeight = columnWeights.empty() ? (1.0f / numCols) : columnWeights[i];
        accumulatedScale += colWeight;

        auto sep = std::make_unique<Frame>();
        sep->setBaseProperties({
            .backgroundColor = Color3(m_tvProps.columnSeparatorColor),
            .backgroundTransparency = 1.0f - m_tvProps.columnSeparatorColor.a,
            .position = UDim2{{accumulatedScale, -m_tvProps.columnSeparatorWidth / 2.0f}, {0.0f, 0.0f}},
            .size = UDim2{{0.0f, m_tvProps.columnSeparatorWidth}, {1.0f, 0.0f}},
            .zIndex = getZIndex() + 1,
        });
        sep->markDirty();
        m_separators.push_back(std::move(sep));
    }
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
        auto *obj = m_children[i]->as<UIObject>();
        if (obj != nullptr && obj->getBaseProperties().visible != 0) {
            obj->setBaseProperties({.visible = 0});
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
    float indent = static_cast<float>(depth(row)) * m_tvProps.indentPerLevel;
    if (static_cast<bool>(m_tvProps.showDisclosureTriangles)) {
        indent += m_tvProps.disclosureTriangleSize + m_tvProps.disclosureTrianglePadding * 2.0f;
    }
    return indent;
}

void TreeView::drawDisclosureTriangle(DrawContext &ctx, uint32_t row, uint32_t bufferSlot, float y, bool expanded,
                                      const glm::vec4 &childClip)
{
    TextButton *btn = m_rowDisclosures[bufferSlot].get();

    if (!static_cast<bool>(m_tvProps.showDisclosureTriangles) || !hasChildren(row)) {
        btn->setBaseProperties({.interactable = 0, .visible = 0});
        btn->markDirty();
        btn->draw(ctx);
        return;
    }

    float rowDepth = static_cast<float>(depth(row));
    float x = absolutePosition.x + rowDepth * m_tvProps.indentPerLevel + m_tvProps.disclosureTrianglePadding;
    float centerY = y + m_computedRowHeight * 0.5f;

    btn->setBaseProperties({
        .anchorPoint = {0.5f, 0.5f},
        .backgroundColor = Color3(m_tvProps.disclosureTriangleColor),
        .backgroundTransparency = 1.0f - m_tvProps.disclosureTriangleColor.a,
        .interactable = 1,
        .rotation = expanded ? 90.0f : 0.0f,
        .visible = 1,
        .zIndex = getZIndex() + 2,
    });
    btn->clipRect = childClip;
    btn->onMouseButton1ClickCb = [this, row]() {
        toggle(row);
        return EventResult::CONSUMED;
    };
    btn->markDirty();

    float triSize = m_tvProps.disclosureTriangleSize;
    glm::vec2 triPos = {x, centerY - triSize * 0.5f};
    btn->computeAbsolutes({triSize, triSize}, triPos + glm::vec2(triSize * 0.5f), absoluteRotation);
    btn->draw(ctx);
}

void TreeView::drawRowContent(DrawContext &ctx, uint32_t row, uint32_t bufferSlot, uint32_t visualIndex,
                              const std::vector<float> &colPositions, const glm::vec4 &childClip)
{
    AM_PROFILE_FUNCTION();

    const TreeRow &r = m_rows[row];
    float rowY = calculateRowY(visualIndex, m_computedRowHeight);
    float indent = getRowIndent(row);

    Frame *bg = m_rowBackgrounds[bufferSlot].get();

    Color4 bgColor = m_tvProps.rowBackgroundColor;
    if (static_cast<int32_t>(row) == selectedRow) {
        bgColor = m_tvProps.rowSelectedColor;
    } else if (static_cast<int32_t>(row) == hoveredRow) {
        bgColor = m_tvProps.rowHoverColor;
    } else if (visualIndex % 2 == 1 && m_tvProps.rowAlternateColor.a > 0.0f) {
        bgColor = m_tvProps.rowAlternateColor;
    }

    bg->setBaseProperties({
        .backgroundColor = Color3(bgColor),
        .backgroundTransparency = 1.0f - bgColor.a,
    });
    bg->clipRect = childClip;
    bg->markDirty();
    bg->computeAbsolutes({absoluteSize.x, m_computedRowHeight}, absolutePosition + glm::vec2(0.0f, rowY), absoluteRotation);
    bg->draw(ctx);

    drawDisclosureTriangle(ctx, row, bufferSlot, absolutePosition.y + rowY, r.expanded, childClip);

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

        drawable->setBaseProperties({.visible = 1});

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
        frame->setBaseProperties({
            .size = UDim2::fromScale(1.0f, 1.0f),
            .zIndex = getZIndex(),
        });
        m_rowBackgrounds.push_back(std::move(frame));
    }
    while (m_rowDisclosures.size() < slotCount) {
        auto btn = std::make_unique<TextButton>();
        btn->parent = this;
        btn->setBaseProperties({
            .size = UDim2::fromScale(1.0f, 1.0f),
            .zIndex = getZIndex() + 2,
        });
        btn->setButtonProperties({.autoButtonColor = 0});
        m_rowDisclosures.push_back(std::move(btn));
    }
}

uint32_t TreeView::drawVisibleRows(DrawContext &ctx, const std::vector<float> &colPositions, const glm::vec4 &childClip,
                                   uint32_t firstVisibleSlot, uint32_t slotCount)
{
    AM_PROFILE_FUNCTION();

    uint32_t visibleCount = 0;
    uint32_t bufferSlot = 0;
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
            if (visibleCount >= firstVisibleSlot && bufferSlot < slotCount) {
                drawRowContent(ctx, row, bufferSlot, visibleCount, colPositions, childClip);
                bufferSlot++;
                r.isDirty = true;
            } else if (r.isDirty) {
                clearRowCells(ctx, row);
                r.isDirty = false;
            }
            visibleCount++;

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

    return bufferSlot;
}

void TreeView::drawEmptyRows(DrawContext &ctx, const glm::vec4 &childClip, uint32_t fromSlot, uint32_t slotCount,
                             uint32_t firstVisibleSlot)
{
    AM_PROFILE_FUNCTION();

    for (uint32_t i = fromSlot; i < slotCount; i++) {
        uint32_t visualIndex = firstVisibleSlot + i;
        float rowY = calculateRowY(visualIndex, m_computedRowHeight);
        Frame *bg = m_rowBackgrounds[i].get();
        Color4 bgColor = (visualIndex % 2 == 1 && m_tvProps.rowAlternateColor.a > 0.0f) ? m_tvProps.rowAlternateColor
                                                                                        : m_tvProps.rowBackgroundColor;
        bg->setBaseProperties({
            .backgroundColor = Color3(bgColor),
            .backgroundTransparency = 1.0f - bgColor.a,
        });
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
        btn->setBaseProperties({.interactable = 0, .visible = 0});
        btn->markDirty();
        btn->draw(ctx);
    }
}

void TreeView::draw(DrawContext &ctx)
{
    AM_PROFILE_FUNCTION();

    if (absolutePosition != m_lastAbsolutePosition || absoluteSize != m_lastAbsoluteSize) {
        flags |= FLAG_DIRTY;
        m_lastAbsolutePosition = absolutePosition;
        m_lastAbsoluteSize = absoluteSize;
    }

    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (numCols == 0) {
        flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
        return;
    }

    if (flags & FLAG_DIRTY) {
        updateSeparators();
        m_resolvedPadding = m_tvProps.cellPadding.resolve(absoluteSize);
    }

    glm::vec4 childClip = computeChildClipRect();

    m_computedRowHeight = m_tvProps.rowHeight > 0.0f ? m_tvProps.rowHeight : 24.0f;

    float viewportHeight = childClip.w - childClip.y;
    if (viewportHeight <= 0.0f) {
        flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
        return;
    }
    uint32_t slotCount = static_cast<uint32_t>(std::ceil(viewportHeight / m_computedRowHeight)) + 2;
    AM_ASSERT(slotCount <= TREE_VIEW_MAX_SLOT_COUNT, "TreeView slotCount is unreasonably large, likely a layout or clip bug");

    float scrolledY = std::max(0.0f, childClip.y - absolutePosition.y);
    uint32_t firstVisibleSlot = static_cast<uint32_t>(std::floor(scrolledY / m_computedRowHeight));

    ensureSlotCapacity(slotCount);

    std::vector<float> colPositions = computeColumnPositions(absoluteSize.x);

    for (auto &sep : m_separators) {
        sep->clipRect = childClip;
        sep->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        sep->draw(ctx);
    }

    uint32_t usedSlots = drawVisibleRows(ctx, colPositions, childClip, firstVisibleSlot, slotCount);

    if (static_cast<bool>(m_tvProps.fillRows)) {
        drawEmptyRows(ctx, childClip, usedSlots, slotCount, firstVisibleSlot);
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
