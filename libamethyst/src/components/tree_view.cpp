#include "components/tree_view.h"

#include "amethyst/icons.h"
#include "components/image_button.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "utils/am_assert.h"

#include <algorithm>
#include <cmath>

namespace Amethyst {

static constexpr float DEFAULT_ROW_HEIGHT = 24.0f;

static constexpr int32_t Z_ABOVE_CONTENT = 2;
static constexpr int32_t Z_TOP = 3;

TreeView::TreeView()
{
    m_tvProps.rowHeight = 0.0f;
    m_tvProps.cellPadding = {};
    m_tvProps.showColumnSeparators = 0;
    m_tvProps.columnSeparatorWidth = 1.0f;
    m_tvProps.columnSeparatorColor = {0.3f, 0.3f, 0.3f, 1.0f};
    m_tvProps.indentPerLevel = 16.0f;
    m_tvProps.showDisclosureTriangles = 1;
    m_tvProps.disclosureTriangleSize = 16.0f;
    m_tvProps.disclosureTrianglePadding = 4.0f;
    m_tvProps.disclosureTriangleColor = {0.7f, 0.7f, 0.7f, 1.0f};
    m_tvProps.rowBackgroundColor = {0.18f, 0.18f, 0.2f, 1.0f};
    m_tvProps.rowAlternateColor = {0.22f, 0.22f, 0.24f, 1.0f};
    m_tvProps.rowHoverColor = {0.3f, 0.3f, 0.35f, 1.0f};
    m_tvProps.rowSelectedColor = {0.25f, 0.4f, 0.65f, 1.0f};
    m_tvProps.fillRows = 1;
    m_tvProps.showHeader = 0;
    m_tvProps.headerHeight = 28.0f;
    m_tvProps.headerColor = Color3{0.25f, 0.25f, 0.28f};
    m_tvProps.header.fontSize = 14.0f;
    m_tvProps.header.textColor = Color4{1.0f, 1.0f, 1.0f, 1.0f};

    resolveStyle();
}

void TreeView::resolveStyle()
{
    auto &style = Style::instance();
    setBaseStyleProperties(style.getBaseStyle(ComponentType::TREE_VIEW, getClasses()));
    setTreeViewProperties(style.getTreeViewStyle(ComponentType::TREE_VIEW, getClasses()));
}

TreeView::~TreeView()
{
    for (auto &bg : m_rowBackgrounds) {
        bg->parent = nullptr;
    }
    if (m_headerBackground) {
        m_headerBackground->parent = nullptr;
    }
    for (auto &lbl : m_headerLabels) {
        lbl->parent = nullptr;
    }
    for (auto &sep : m_separators) {
        sep->parent = nullptr;
    }
}

bool TreeView::setTreeViewProperties(const TreeViewStyleProperties &props)
{
    bool changed = m_tvProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

void TreeView::resizeColumns(uint32_t newCount)
{
    uint32_t oldCount = columnCount();
    if (newCount <= oldCount) {
        return;
    }

    while (static_cast<uint32_t>(m_columns.size()) < newCount) {
        m_columns.push_back({.weight = 1.0f});
    }

    if (m_cells.empty()) {
        markDirty();
        return;
    }

    uint32_t numSlots = static_cast<uint32_t>(m_cells.size()) / oldCount;
    std::vector<Instance *> newCells(numSlots * newCount, nullptr);
    for (uint32_t r = 0; r < numSlots; r++) {
        for (uint32_t c = 0; c < oldCount; c++) {
            newCells[r * newCount + c] = m_cells[r * oldCount + c];
        }
    }
    m_cells = std::move(newCells);
    markDirty();
}

void TreeView::addColumn(TreeColumn col)
{
    AM_ASSERT(m_cells.empty(), "Cannot add columns after rows have been added");
    m_columns.push_back(std::move(col));
    markDirty();
}

void TreeView::setColumns(std::vector<TreeColumn> cols)
{
    AM_ASSERT(m_cells.empty(), "Cannot set columns after rows have been added");
    m_columns = std::move(cols);
    markDirty();
}

uint32_t TreeView::columnCount() const
{
    return static_cast<uint32_t>(m_columns.size());
}

uint32_t TreeView::addRow(uint16_t depth)
{
    uint32_t cols = columnCount();
    AM_ASSERT(cols > 0, "Must add columns before adding rows");

    uint32_t rowIndex = static_cast<uint32_t>(m_rows.size());
    m_rows.push_back(TreeRow{depth, true, false});
    m_cells.resize(m_cells.size() + cols, nullptr);

    m_cursorRow = rowIndex;
    m_cursorCol = 0;
    markDirty();
    return rowIndex;
}

Instance *TreeView::nextCell(std::unique_ptr<Instance> child)
{
    uint32_t cols = columnCount();
    AM_ASSERT(m_cursorCol < cols, "nextCell() called but cursor is past the last column");
    AM_ASSERT(m_cursorRow * cols + m_cursorCol < m_cells.size(), "Cursor row is out of bounds");

    uint32_t idx = m_cursorRow * cols + m_cursorCol;

    if (m_cells[idx]) {
        Instance *old = m_cells[idx];
        if (old->parent) {
            old->parent->removeChild(old);
        }
        m_cells[idx] = nullptr;
    }

    Instance *raw = addChild(std::move(child));
    m_cells[idx] = raw;
    m_cursorCol++;
    markDirty();
    return raw;
}

void TreeView::setCursor(uint32_t row, uint32_t col)
{
    uint32_t cols = columnCount();
    AM_ASSERT(col < cols, "Column index out of bounds");
    AM_ASSERT(row * cols + col < m_cells.size(), "setCursor position out of bounds");
    m_cursorRow = row;
    m_cursorCol = col;
}

void TreeView::setCell(uint32_t row, uint32_t col, std::unique_ptr<Instance> child)
{
    uint32_t cols = columnCount();
    AM_ASSERT(col < cols, "Column index out of bounds");
    uint32_t idx = row * cols + col;
    AM_ASSERT(idx < m_cells.size(), "Row index out of bounds");

    if (m_cells[idx]) {
        Instance *old = m_cells[idx];
        if (old->parent) {
            old->parent->removeChild(old);
        }
    }

    Instance *raw = addChild(std::move(child));
    m_cells[idx] = raw;
    markDirty();
}

Instance *TreeView::getCell(uint32_t row, uint32_t col) const
{
    uint32_t cols = columnCount();
    AM_ASSERT(col < cols, "Column index out of bounds");
    uint32_t idx = row * cols + col;
    if (idx >= m_cells.size()) {
        return nullptr;
    }
    return m_cells[idx];
}

void TreeView::clear()
{
    m_children.clear();
    m_cells.clear();
    m_rows.clear();
    m_visible.clear();
    m_rowBackgrounds.clear();
    m_disclosures.clear();
    m_rowBySlot.clear();
    m_rowHoverConns.clear();
    m_rowInputConns.clear();
    m_cursorRow = 0;
    m_cursorCol = 0;
    markDirty();
}

uint32_t TreeView::rowCount() const
{
    return static_cast<uint32_t>(m_rows.size());
}

void TreeView::computeAbsolutes(vec2 parentSize, vec2 parentPos, Degrees parentRotation)
{
    UIObject::computeAbsolutes(parentSize, parentPos, parentRotation);
    if (flags & FLAG_DIRTY) {
        rebuildVisiblePlan();
    }
    float rowH = m_tvProps.rowHeight > 0.0f ? m_tvProps.rowHeight : DEFAULT_ROW_HEIGHT;
    float headerH = static_cast<bool>(m_tvProps.showHeader) ? m_tvProps.headerHeight : 0.0f;
    absoluteSize.y = headerH + static_cast<float>(m_visible.size()) * rowH;
}

uint16_t TreeView::depth(uint32_t row) const
{
    AM_ASSERT(row < m_rows.size(), "Row index out of bounds");
    return m_rows[row].depth;
}

bool TreeView::hasChildren(uint32_t row) const
{
    AM_ASSERT(row < m_rows.size(), "Row index out of bounds");
    return m_rows[row].hasChildren;
}

bool TreeView::isExpanded(uint32_t row) const
{
    AM_ASSERT(row < m_rows.size(), "Row index out of bounds");
    return m_rows[row].expanded;
}

void TreeView::toggle(uint32_t row)
{
    AM_ASSERT(row < m_rows.size(), "Row index out of bounds");
    m_rows[row].expanded = !m_rows[row].expanded;
    markDirty();
    if (onRowToggled) {
        onRowToggled(row, m_rows[row].expanded);
    }
}

void TreeView::expand(uint32_t row)
{
    AM_ASSERT(row < m_rows.size(), "Row index out of bounds");
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
    AM_ASSERT(row < m_rows.size(), "Row index out of bounds");
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
        r.expanded = true;
    }
    markDirty();
}

void TreeView::collapseAll()
{
    for (auto &r : m_rows) {
        r.expanded = false;
    }
    markDirty();
}

void TreeView::rebuildVisiblePlan()
{
    uint32_t n = static_cast<uint32_t>(m_rows.size());
    for (uint32_t i = 0; i < n; i++) {
        m_rows[i].hasChildren = (i + 1 < n && m_rows[i + 1].depth > m_rows[i].depth);
    }
    m_visible.clear();
    for (uint32_t i = 0; i < n;) {
        m_visible.push_back(i);
        if (!m_rows[i].expanded && m_rows[i].hasChildren) {
            uint16_t d = m_rows[i].depth;
            uint32_t j = i + 1;
            while (j < n && m_rows[j].depth > d) {
                j++;
            }
            i = j;
        } else {
            i++;
        }
    }
}

void TreeView::rebuildColumnPositions()
{
    m_columnPositions.clear();
    uint32_t cols = columnCount();
    m_columnPositions.reserve(cols + 1);
    m_columnPositions.push_back(0.0f);

    float totalWidth = absoluteSize.x;
    float weightTotal = 0.0f;
    for (const auto &col : m_columns) {
        weightTotal += col.weight;
    }
    if (weightTotal <= 0.0f) {
        weightTotal = 1.0f;
    }

    float x = 0.0f;
    for (const auto &col : m_columns) {
        x += (col.weight / weightTotal) * totalWidth;
        m_columnPositions.push_back(x);
    }
}

void TreeView::updateSeparators()
{
    for (auto &sep : m_separators) {
        sep->parent = nullptr;
    }
    m_separators.clear();
    uint32_t cols = columnCount();
    if (!static_cast<bool>(m_tvProps.showColumnSeparators) || cols <= 1) {
        return;
    }

    for (uint32_t i = 0; i < cols - 1; i++) {
        float xPos = m_columnPositions[i + 1];

        auto sep = std::make_unique<Frame>();
        sep->parent = this;
        sep->setBaseStyleProperties({
            .backgroundColor = Color3(m_tvProps.columnSeparatorColor),
            .backgroundTransparency = 1.0f - m_tvProps.columnSeparatorColor.a,
        });
        sep->setBaseProperties({
            .position = UDim2(0.0f, xPos - m_tvProps.columnSeparatorWidth / 2.0f, 0.0f, 0.0f),
            .size = UDim2(0.0f, m_tvProps.columnSeparatorWidth, 1.0f, 0.0f),
            .zIndex = Z_TOP,
        });
        sep->markDirty();
        m_separators.push_back(std::move(sep));
    }
}

void TreeView::ensureHeaderCapacity()
{
    uint32_t cols = columnCount();

    if (!m_headerBackground) {
        m_headerBackground = std::make_unique<Frame>();
        m_headerBackground->parent = this;
    }

    while (m_headerLabels.size() < cols) {
        auto lbl = std::make_unique<TextLabel>();
        lbl->parent = this;
        m_headerLabels.push_back(std::move(lbl));
    }
}

void TreeView::ensurePoolCapacity(uint32_t count)
{
    while (m_rowBackgrounds.size() < count) {
        auto frame = std::make_unique<Frame>();
        frame->parent = this;

        auto disc = std::make_unique<ImageButton>();
        disc->setSvg(Icons::ARROW);
        disc->setBaseStyleProperties({.backgroundTransparency = 1.0f});
        m_disclosures.push_back(disc.get());
        frame->addChild(std::move(disc));

        uint32_t poolSlot = static_cast<uint32_t>(m_rowBackgrounds.size());
        m_rowHoverConns.push_back(frame->onHoverChanged.connect([this, poolSlot](bool hovered) {
            uint32_t logicalRow = m_rowBySlot[poolSlot];
            if (hovered) {
                hoveredRow = static_cast<int32_t>(logicalRow);
            } else if (hoveredRow == static_cast<int32_t>(logicalRow)) {
                hoveredRow = -1;
            }
            markDirty();
        }));
        m_rowInputConns.push_back(frame->onInputBeganCb.connect([this, poolSlot](const InputObject &io) {
            if (io.type != InputType::MOUSE_BUTTON_1) {
                return;
            }
            uint32_t logicalRow = m_rowBySlot[poolSlot];
            selectedRow = static_cast<int32_t>(logicalRow);
            if (onRowClicked) {
                onRowClicked(logicalRow);
            }
            markDirty();
        }));

        m_rowBackgrounds.push_back(std::move(frame));
        m_rowBySlot.push_back(INVALID_ROW);
    }
}

// WIP: header rendering is wired but off by default and unverified against all themes.
void TreeView::drawHeader(DrawContext &ctx, const vec4 &childClip)
{
    uint32_t cols = columnCount();
    ensureHeaderCapacity();

    m_headerBackground->setBaseStyleProperties({
        .backgroundColor = m_tvProps.headerColor,
        .backgroundTransparency = 0.0f,
    });
    m_headerBackground->setBaseProperties({.visible = true});
    m_headerBackground->clipRect = childClip;
    m_headerBackground->markDirty();
    m_headerBackground->computeAbsolutes({absoluteSize.x, m_tvProps.headerHeight}, absolutePosition, absoluteRotation);
    m_headerBackground->draw(ctx);

    for (uint32_t col = 0; col < cols; col++) {
        TextLabel *lbl = m_headerLabels[col].get();
        lbl->setBaseStyleProperties({.backgroundTransparency = 1.0f});
        lbl->setBaseProperties({
            .size = UDim2::fromScale(1.0f, 1.0f),
            .visible = true,
            .zIndex = Z_ABOVE_CONTENT,
        });
        lbl->setTextStyleProperties({
            .fontSize = m_tvProps.header.fontSize,
            .textColor = m_tvProps.header.textColor,
            .textXAlignment = TextXAlignment::LEFT,
            .textYAlignment = TextYAlignment::CENTER,
        });
        lbl->setText(m_columns[col].header);
        lbl->clipRect = childClip;
        lbl->markDirty();

        float cellX = m_columnPositions[col] + m_cellPaddingPx.w;
        float cellWidth = m_columnPositions[col + 1] - m_columnPositions[col] - m_cellPaddingPx.w - m_cellPaddingPx.y;

        lbl->computeAbsolutes({cellWidth, m_tvProps.headerHeight}, absolutePosition + vec2(cellX, 0.0f), absoluteRotation);
        lbl->draw(ctx);
    }
}

void TreeView::drawSeparators(DrawContext &ctx, const vec4 &childClip)
{
    for (auto &sep : m_separators) {
        sep->setBaseProperties({.visible = true});
        sep->clipRect = childClip;
        sep->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        sep->draw(ctx);
    }
}

void TreeView::drawRow(DrawContext &ctx, uint32_t logicalRow, uint32_t poolSlot, uint32_t visibleIndex, float y,
                       const vec4 &childClip)
{
    uint32_t cols = columnCount();
    uint32_t visualIndex = visibleIndex;

    Frame *bg = m_rowBackgrounds[poolSlot].get();

    Color4 bgColor = m_tvProps.rowBackgroundColor;
    if (static_cast<int32_t>(logicalRow) == selectedRow) {
        bgColor = m_tvProps.rowSelectedColor;
    } else if (static_cast<int32_t>(logicalRow) == hoveredRow) {
        bgColor = m_tvProps.rowHoverColor;
    } else if (visualIndex % 2 == 1 && m_tvProps.rowAlternateColor.a > 0.0f) {
        bgColor = m_tvProps.rowAlternateColor;
    }

    bg->setBaseStyleProperties({
        .backgroundColor = Color3(bgColor),
        .backgroundTransparency = 1.0f - bgColor.a,
    });
    bg->setBaseProperties({
        .interactable = true,
        .position = UDim2(0.0f, 0.0f, 0.0f, y),
        .size = UDim2(1.0f, 0.0f, 0.0f, m_rowHeightPx),
        .visible = true,
    });
    bg->clipRect = childClip;
    bg->markDirty();
    bg->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);

    ImageButton *disc = m_disclosures[poolSlot];
    if (!static_cast<bool>(m_tvProps.showDisclosureTriangles) || !m_rows[logicalRow].hasChildren) {
        disc->setBaseProperties({.interactable = false, .visible = false});
    } else {
        float rowDepth = static_cast<float>(m_rows[logicalRow].depth);
        float triSize = m_tvProps.disclosureTriangleSize;
        float x = rowDepth * m_tvProps.indentPerLevel + m_tvProps.disclosureTrianglePadding;

        disc->setBaseProperties({
            .anchorPoint = {0.5f, 0.5f},
            .interactable = true,
            .position = UDim2(0.0f, x + triSize * 0.5f, 0.5f, 0.0f),
            .size = UDim2::fromOffset(triSize, triSize),
            .rotation = m_rows[logicalRow].expanded ? 90.0f : 0.0f,
            .visible = true,
            .zIndex = Z_ABOVE_CONTENT,
        });
        disc->setImageStyleProperties({.imageColor = m_tvProps.disclosureTriangleColor});
        disc->onMouseButton1ClickCb = [this, logicalRow]() {
            toggle(logicalRow);
            return EventResult::CONSUMED;
        };
    }

    float indent = static_cast<float>(m_rows[logicalRow].depth) * m_tvProps.indentPerLevel;
    if (static_cast<bool>(m_tvProps.showDisclosureTriangles)) {
        indent += m_tvProps.disclosureTriangleSize + m_tvProps.disclosureTrianglePadding * 2.0f;
    }

    float totalWidth = absoluteSize.x;
    float padT = m_cellPaddingPx.x;
    float padR = m_cellPaddingPx.y;
    float padB = m_cellPaddingPx.z;
    float padL = m_cellPaddingPx.w;

    for (uint32_t col = 0; col < cols; col++) {
        Instance *cell = m_cells[logicalRow * cols + col];
        if (!cell) {
            continue;
        }

        auto *drawable = cell->as<UIObject>();
        if (!drawable) {
            continue;
        }

        float startFrac = totalWidth > 0.0f ? m_columnPositions[col] / totalWidth : 0.0f;
        float widthFrac = totalWidth > 0.0f ? (m_columnPositions[col + 1] - m_columnPositions[col]) / totalWidth : 0.0f;
        float indentPx = col == 0 ? indent : 0.0f;

        drawable->setBaseProperties({
            //.interactable = false,
            .position = UDim2(startFrac, padL + indentPx, 0.0f, padT),
            .size = UDim2(widthFrac, -(padL + padR + indentPx), 1.0f, -(padT + padB)),
            .visible = true,
        });
    }

    bg->draw(ctx);
}

void TreeView::draw(DrawContext &ctx)
{
    if (!isVisible()) {
        for (uint32_t slot = 0; slot < m_rowBackgrounds.size(); slot++) {
            hideSlot(ctx, slot);
        }
        if (m_headerBackground != nullptr && m_headerBackground->setBaseProperties({.visible = false})) {
            m_headerBackground->draw(ctx);
        }
        for (auto &lbl : m_headerLabels) {
            if (lbl->setBaseProperties({.visible = false})) {
                lbl->draw(ctx);
            }
        }
        for (auto &sep : m_separators) {
            if (sep->setBaseProperties({.visible = false})) {
                sep->draw(ctx);
            }
        }
        flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
        return;
    }

    if (absolutePosition != m_lastAbsolutePosition || absoluteSize != m_lastAbsoluteSize) {
        flags |= FLAG_DIRTY;
        m_lastAbsolutePosition = absolutePosition;
        m_lastAbsoluteSize = absoluteSize;
    }

    uint32_t cols = columnCount();
    if (cols == 0) {
        flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
        return;
    }

    if (flags & FLAG_DIRTY) {
        rebuildColumnPositions();
        updateSeparators();
        m_cellPaddingPx = m_tvProps.cellPadding.resolve(absoluteSize);
    }

    m_rowHeightPx = m_tvProps.rowHeight > 0.0f ? m_tvProps.rowHeight : DEFAULT_ROW_HEIGHT;

    vec4 childClip = computeChildClipRect();
    float headerH = static_cast<bool>(m_tvProps.showHeader) ? m_tvProps.headerHeight : 0.0f;

    float viewportHeight = childClip.w - childClip.y;
    if (viewportHeight <= 0.0f) {
        flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
        return;
    }

    float scrolledY = std::max(0.0f, childClip.y - absolutePosition.y - headerH);
    uint32_t first = static_cast<uint32_t>(std::floor(scrolledY / m_rowHeightPx));
    uint32_t count = static_cast<uint32_t>(std::ceil(viewportHeight / m_rowHeightPx)) + 2;
    uint32_t totalVisible = static_cast<uint32_t>(m_visible.size());
    if (first > totalVisible) {
        first = totalVisible;
    }
    uint32_t last = std::min(first + count, totalVisible);

    uint32_t windowCount = last - first;
    ensurePoolCapacity(windowCount);

    if (static_cast<bool>(m_tvProps.showHeader)) {
        drawHeader(ctx, childClip);
    }

    drawSeparators(ctx, childClip);

    for (uint32_t slot = 0; slot < m_rowBackgrounds.size(); slot++) {
        uint32_t desired = slot < windowCount ? m_visible[first + slot] : INVALID_ROW;
        if (m_rowBySlot[slot] != INVALID_ROW && m_rowBySlot[slot] != desired) {
            parkRowCells(ctx, m_rowBySlot[slot]);
            m_rowBySlot[slot] = INVALID_ROW;
        }
    }

    for (uint32_t k = first; k < last; k++) {
        uint32_t poolSlot = k - first;
        uint32_t logicalRow = m_visible[k];
        attachRowCells(poolSlot, logicalRow);
        m_rowBySlot[poolSlot] = logicalRow;
        float rowY = headerH + static_cast<float>(k) * m_rowHeightPx;
        drawRow(ctx, logicalRow, poolSlot, k, rowY, childClip);
    }

    for (uint32_t slot = windowCount; slot < m_rowBackgrounds.size(); slot++) {
        hideSlot(ctx, slot);
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void TreeView::attachRowCells(uint32_t poolSlot, uint32_t logicalRow)
{
    Frame *bg = m_rowBackgrounds[poolSlot].get();
    uint32_t cols = columnCount();
    for (uint32_t col = 0; col < cols; col++) {
        Instance *cell = m_cells[logicalRow * cols + col];
        if (cell == nullptr || cell->parent == bg) {
            continue;
        }
        cell->reparent(bg);
    }
}

void TreeView::parkRowCells(DrawContext &ctx, uint32_t logicalRow)
{
    uint32_t cols = columnCount();
    for (uint32_t col = 0; col < cols; col++) {
        Instance *cell = m_cells[logicalRow * cols + col];
        if (cell == nullptr) {
            continue;
        }
        if (auto *drawable = cell->as<UIObject>(); drawable && drawable->setBaseProperties({.visible = false})) {
            drawable->draw(ctx);
        }
        if (cell->parent != this) {
            cell->reparent(this);
        }
    }
}

void TreeView::hideSlot(DrawContext &ctx, uint32_t poolSlot)
{
    Frame *bg = m_rowBackgrounds[poolSlot].get();
    ImageButton *disc = m_disclosures[poolSlot];
    bool changed = bg->setBaseProperties({.interactable = false, .visible = false});
    changed |= disc->setBaseProperties({.interactable = false, .visible = false});
    if (changed) {
        bg->markDirty();
        bg->draw(ctx);
    }
}

std::vector<Instance *> TreeView::getHittableInstances()
{
    std::vector<Instance *> result;
    result.reserve(m_rowBackgrounds.size());
    for (auto &bg : m_rowBackgrounds) {
        result.push_back(bg.get());
    }
    return result;
}

} // namespace Amethyst
