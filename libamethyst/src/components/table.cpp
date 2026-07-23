#include "components/table.h"

#include "modules/style.h"
#include "rendering/draw_context.h"
#include "utils/am_assert.h"

#include <algorithm>

namespace Amethyst {

static constexpr float FALLBACK_ROW_HEIGHT = 24.0f;

static constexpr int32_t Z_ROW_BG = 0;
static constexpr int32_t Z_ABOVE_CONTENT = 2;

static bool s_showColumnSeparators(TableSeparatorMode mode)
{
    return mode == TableSeparatorMode::COLUMNS || mode == TableSeparatorMode::BOTH;
}

static bool s_showRowSeparators(TableSeparatorMode mode)
{
    return mode == TableSeparatorMode::ROWS || mode == TableSeparatorMode::BOTH;
}

Table::Table()
{
    m_tProps.cellPadding = UDim4{};
    m_tProps.separatorMode = TableSeparatorMode::COLUMNS;
    m_tProps.separatorWidth = 1.0f;
    m_tProps.separatorColor = Color4{0.3f, 0.3f, 0.3f, 1.0f};
    m_tProps.showHeader = 1;
    m_tProps.headerHeight = 28.0f;
    m_tProps.headerColor = Color3{0.25f, 0.25f, 0.28f};
    m_tProps.header.fontSize = 14.0f;
    m_tProps.header.textColor = Color4{1.0f, 1.0f, 1.0f, 1.0f};
    m_tProps.rowBackgroundColor = Color4{0.18f, 0.18f, 0.2f, 1.0f};
    m_tProps.rowAlternateColor = Color4{0.22f, 0.22f, 0.24f, 1.0f};

    resolveStyle();
}

void Table::resolveStyle()
{
    resolveBaseStyle(ComponentType::TABLE);

    auto &style = Style::instance();
    std::span<const StyleKey> classes = getClasses();
    TableStyleProperties oldBaseline = style.getTableStyle(ComponentType::TABLE, classes, m_lastResolvedGuiState);
    TableStyleProperties resolved = style.getTableStyle(ComponentType::TABLE, classes, effectiveGuiState());
    reconcileStyleOverrides(oldBaseline, resolved, m_tProps, [this](const TableStyleProperties &next) { setTableProperties(next); });
}

bool Table::setTableProperties(const TableStyleProperties &props)
{
    bool changed = m_tProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

void Table::resizeColumns(uint32_t newCount)
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

void Table::addColumn(TableColumn col)
{
    AM_ASSERT(m_cells.empty(), "Cannot add columns after rows have been added");
    m_columns.push_back(std::move(col));
    markDirty();
}

void Table::setColumns(std::vector<TableColumn> cols)
{
    AM_ASSERT(m_cells.empty(), "Cannot set columns after rows have been added");
    m_columns = std::move(cols);
    markDirty();
}

uint32_t Table::columnCount() const
{
    return static_cast<uint32_t>(m_columns.size());
}

uint32_t Table::addRow()
{
    uint32_t cols = columnCount();
    AM_ASSERT(cols > 0, "Must add columns before adding rows");

    uint32_t rowIndex;
    if (!m_rowFreelist.empty()) {
        rowIndex = m_rowFreelist.back();
        m_rowFreelist.pop_back();
        for (uint32_t i = 0; i < cols; i++) {
            m_cells[rowIndex * cols + i] = nullptr;
        }
    } else {
        rowIndex = static_cast<uint32_t>(m_cells.size() / cols);
        m_cells.resize(m_cells.size() + cols, nullptr);
    }

    m_displayOrder.push_back(rowIndex);
    m_cursorRow = rowIndex;
    m_cursorCol = 0;
    markDirty();
    return rowIndex;
}

Instance *Table::nextCell(std::unique_ptr<Instance> child)
{
    uint32_t cols = columnCount();
    AM_ASSERT(m_cursorCol < cols, "nextCell() called but cursor is past the last column");
    AM_ASSERT(m_cursorRow * cols + m_cursorCol < m_cells.size(), "Cursor row is out of bounds");

    uint32_t idx = m_cursorRow * cols + m_cursorCol;

    if (m_cells[idx] != nullptr) {
        removeChild(m_cells[idx]);
        m_cells[idx] = nullptr;
    }

    Instance *raw = addChild(std::move(child));
    m_cells[idx] = raw;
    m_cursorCol++;
    markDirty();
    return raw;
}

void Table::setCursor(uint32_t row, uint32_t col)
{
    uint32_t cols = columnCount();
    AM_ASSERT(col < cols, "Column index out of bounds");
    AM_ASSERT(row * cols + col < m_cells.size(), "setCursor position out of bounds");
    m_cursorRow = row;
    m_cursorCol = col;
}

void Table::setCell(uint32_t row, uint32_t col, std::unique_ptr<Instance> child)
{
    uint32_t cols = columnCount();
    AM_ASSERT(col < cols, "Column index out of bounds");
    uint32_t idx = row * cols + col;
    AM_ASSERT(idx < m_cells.size(), "Row index out of bounds");

    if (m_cells[idx] != nullptr) {
        removeChild(m_cells[idx]);
    }

    Instance *raw = addChild(std::move(child));
    m_cells[idx] = raw;
    markDirty();
}

Instance *Table::getCell(uint32_t row, uint32_t col) const
{
    uint32_t cols = columnCount();
    AM_ASSERT(col < cols, "Column index out of bounds");
    uint32_t idx = row * cols + col;
    if (idx >= m_cells.size()) {
        return nullptr;
    }
    return m_cells[idx];
}

void Table::removeRow(uint32_t row)
{
    uint32_t cols = columnCount();
    AM_ASSERT(row * cols < m_cells.size(), "Row index out of bounds");

    for (uint32_t col = 0; col < cols; col++) {
        uint32_t idx = row * cols + col;
        if (m_cells[idx] != nullptr) {
            removeChild(m_cells[idx]);
            m_cells[idx] = nullptr;
        }
    }

    m_displayOrder.erase(std::remove(m_displayOrder.begin(), m_displayOrder.end(), row), m_displayOrder.end());
    m_rowFreelist.push_back(row);
    markDirty();
}

void Table::clear()
{
    for (Instance *cell : m_cells) {
        if (cell != nullptr) {
            removeChild(cell);
        }
    }
    for (Frame *bg : m_rowBackgrounds) {
        removeChild(bg);
    }

    m_cells.clear();
    m_displayOrder.clear();
    m_rowFreelist.clear();
    m_rowBackgrounds.clear();
    m_rowBgInputConns.clear();
    m_cursorRow = 0;
    m_cursorCol = 0;
    m_selectedDisplayIndex = -1;
    markDirty();
}

uint32_t Table::rowCount() const
{
    return static_cast<uint32_t>(m_displayOrder.size());
}

void Table::sort(std::function<bool(uint32_t rowA, uint32_t rowB)> comparator)
{
    std::sort(m_displayOrder.begin(), m_displayOrder.end(), comparator);
    markDirty();
}

void Table::rebuildColumnPositions()
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

void Table::ensureColumnSeparatorCapacity(uint32_t count)
{
    while (m_columnSeparators.size() < count) {
        m_columnSeparators.push_back(add<Frame>());
    }
}

void Table::ensureRowSeparatorCapacity(uint32_t count)
{
    while (m_rowSeparators.size() < count) {
        m_rowSeparators.push_back(add<Frame>());
    }
}

void Table::updateSeparators()
{
    uint32_t cols = columnCount();
    uint32_t neededColumnSeps = (s_showColumnSeparators(m_tProps.separatorMode) && cols > 1) ? cols - 1 : 0;
    ensureColumnSeparatorCapacity(neededColumnSeps);

    for (uint32_t i = 0; i < static_cast<uint32_t>(m_columnSeparators.size()); i++) {
        Frame *sep = m_columnSeparators[i];
        if (i >= neededColumnSeps) {
            sep->setBaseProperties({.visible = false});
            continue;
        }

        float xPos = m_columnPositions[i + 1];
        sep->setBaseStyleProperties({
            .backgroundColor = Color3(m_tProps.separatorColor),
            .backgroundTransparency = 1.0f - m_tProps.separatorColor.a,
        });
        sep->setBaseProperties({
            .position = UDim2(0.0f, xPos - m_tProps.separatorWidth / 2.0f, 0.0f, 0.0f),
            .size = UDim2(0.0f, m_tProps.separatorWidth, 1.0f, 0.0f),
            .visible = true,
            .zIndex = Z_ABOVE_CONTENT,
        });
    }

    uint32_t rows = rowCount();
    uint32_t neededRowSeps = (s_showRowSeparators(m_tProps.separatorMode) && rows > 1) ? rows - 1 : 0;
    ensureRowSeparatorCapacity(neededRowSeps);

    float dataStartY = m_tProps.showHeader ? m_tProps.headerHeight : 0.0f;
    float rowStride = m_computedRowHeight + m_tProps.separatorWidth;

    for (uint32_t i = 0; i < static_cast<uint32_t>(m_rowSeparators.size()); i++) {
        Frame *sep = m_rowSeparators[i];
        if (i >= neededRowSeps) {
            sep->setBaseProperties({.visible = false});
            continue;
        }

        float yPos = dataStartY + static_cast<float>(i) * rowStride + m_computedRowHeight;
        sep->setBaseStyleProperties({
            .backgroundColor = Color3(m_tProps.separatorColor),
            .backgroundTransparency = 1.0f - m_tProps.separatorColor.a,
        });
        sep->setBaseProperties({
            .position = UDim2(0.0f, 0.0f, 0.0f, yPos),
            .size = UDim2(1.0f, 0.0f, 0.0f, m_tProps.separatorWidth),
            .visible = true,
            .zIndex = Z_ABOVE_CONTENT,
        });
    }
}

void Table::ensureHeaderCapacity()
{
    uint32_t cols = columnCount();

    if (m_headerBackground == nullptr) {
        m_headerBackground = add<Frame>();
    }

    while (m_headerLabels.size() < cols) {
        m_headerLabels.push_back(add<TextLabel>());
    }
}

void Table::ensureRowBackgroundCapacity(uint32_t count)
{
    while (m_rowBackgrounds.size() < count) {
        uint32_t slot = static_cast<uint32_t>(m_rowBackgrounds.size());
        Frame *bg = add<Frame>();
        m_rowBgInputConns.push_back(bg->onInputBeganCb.connect([this, slot](const InputObject &io) {
            if (io.type != InputType::MOUSE_BUTTON_1) {
                return;
            }
            m_selectedDisplayIndex = static_cast<int32_t>(slot);
            if (onRowSelected) {
                onRowSelected(slot);
            }
            if (m_tProps.selectedRowColor.a > 0.0f) {
                markDirty();
            }
        }));
        m_rowBackgrounds.push_back(bg);
    }
}

void Table::arrangeHeader(const vec4 &childClip)
{
    uint32_t cols = columnCount();
    ensureHeaderCapacity();

    m_headerBackground->setBaseStyleProperties({
        .backgroundColor = m_tProps.headerColor,
        .backgroundTransparency = 0.0f,
    });
    m_headerBackground->clipRect = childClip;
    m_headerBackground->computeAbsolutes({absoluteSize.x, m_tProps.headerHeight}, absolutePosition, absoluteRotation);
    m_headerBackground->arrange();

    for (uint32_t col = 0; col < cols; col++) {
        TextLabel *lbl = m_headerLabels[col];
        lbl->setBaseStyleProperties({.backgroundTransparency = 1.0f});
        lbl->setBaseProperties({
            .size = UDim2::fromScale(1.0f, 1.0f),
            .zIndex = Z_ABOVE_CONTENT,
        });
        lbl->setTextStyleProperties({
            .fontSize = m_tProps.header.fontSize,
            .textColor = m_tProps.header.textColor,
            .textXAlignment = TextXAlignment::LEFT,
            .textYAlignment = TextYAlignment::CENTER,
        });
        lbl->setText(m_columns[col].header);
        lbl->clipRect = childClip;

        float cellX = m_columnPositions[col] + m_resolvedPadding.w;
        float cellWidth = m_columnPositions[col + 1] - m_columnPositions[col] - m_resolvedPadding.w - m_resolvedPadding.y;

        lbl->computeAbsolutes({cellWidth, m_tProps.headerHeight}, absolutePosition + vec2(cellX, 0.0f), absoluteRotation);
        lbl->arrange();
    }
}

void Table::arrangeSeparators(const vec4 &childClip)
{
    for (Frame *sep : m_columnSeparators) {
        sep->clipRect = childClip;
        sep->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        sep->arrange();
    }
    for (Frame *sep : m_rowSeparators) {
        sep->clipRect = childClip;
        sep->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        sep->arrange();
    }
}

void Table::arrangeRow(uint32_t logicalRow, uint32_t visualIndex, float y, const vec4 &childClip, bool drawBackground)
{
    uint32_t cols = columnCount();

    if (drawBackground) {
        Frame *bg = m_rowBackgrounds[visualIndex];
        Color4 bgColor = m_tProps.rowBackgroundColor;
        if (visualIndex % 2 == 1 && m_tProps.rowAlternateColor.a > 0.0f) {
            bgColor = m_tProps.rowAlternateColor;
        }
        if (static_cast<int32_t>(visualIndex) == m_selectedDisplayIndex && m_tProps.selectedRowColor.a > 0.0f) {
            bgColor = m_tProps.selectedRowColor;
        }

        bg->setBaseStyleProperties({
            .backgroundColor = Color3(bgColor),
            .backgroundTransparency = 1.0f - bgColor.a,
        });
        bg->setBaseProperties({
            .size = UDim2::fromScale(1.0f, 1.0f),
            .zIndex = Z_ROW_BG,
        });
        bg->clipRect = childClip;
        bg->computeAbsolutes({absoluteSize.x, m_computedRowHeight}, absolutePosition + vec2(0.0f, y), absoluteRotation);
        bg->arrange();
    }

    for (uint32_t col = 0; col < cols; col++) {
        Instance *cell = m_cells[logicalRow * cols + col];
        if (cell == nullptr) {
            continue;
        }

        auto *obj = cell->asUiObject();
        if (obj == nullptr) {
            continue;
        }

        float cellX = m_columnPositions[col];
        float cellWidth = m_columnPositions[col + 1] - cellX;

        float paddedX = cellX + m_resolvedPadding.w;
        float paddedY = y + m_resolvedPadding.x;
        float paddedWidth = cellWidth - m_resolvedPadding.w - m_resolvedPadding.y;
        float paddedHeight = m_computedRowHeight - m_resolvedPadding.x - m_resolvedPadding.z;

        obj->clipRect = childClip;
        obj->computeAbsolutes({paddedWidth, paddedHeight}, absolutePosition + vec2(paddedX, paddedY), absoluteRotation);
        obj->arrange();
    }
}

void Table::arrange()
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    uint32_t cols = columnCount();
    if (cols == 0) {
        return;
    }

    m_computedRowHeight = m_tProps.rowHeight > 0.0f ? m_tProps.rowHeight : FALLBACK_ROW_HEIGHT;

    if (flags & FLAG_DIRTY) {
        rebuildColumnPositions();
        updateSeparators();
        m_resolvedPadding = m_tProps.cellPadding.resolve(absoluteSize);
    }

    vec4 childClip = computeChildClipRect();
    float dataStartY = m_tProps.showHeader ? m_tProps.headerHeight : 0.0f;
    float rowStride = m_computedRowHeight + (s_showRowSeparators(m_tProps.separatorMode) ? m_tProps.separatorWidth : 0.0f);

    if (m_tProps.showHeader) {
        arrangeHeader(childClip);
    }

    arrangeSeparators(childClip);

    bool showRowBackgrounds = m_tProps.rowBackgroundColor.a > 0.0f || m_tProps.rowAlternateColor.a > 0.0f;
    uint32_t visibleCount = static_cast<uint32_t>(m_displayOrder.size());
    uint32_t neededBackgrounds = showRowBackgrounds ? visibleCount : 0;
    ensureRowBackgroundCapacity(neededBackgrounds);

    for (uint32_t vi = 0; vi < visibleCount; vi++) {
        float rowY = dataStartY + static_cast<float>(vi) * rowStride;
        arrangeRow(m_displayOrder[vi], vi, rowY, childClip, showRowBackgrounds);
    }

    for (uint32_t i = neededBackgrounds; i < m_rowBackgrounds.size(); i++) {
        m_rowBackgrounds[i]->setBaseProperties({.visible = false});
    }
}

void Table::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (columnCount() == 0) {
        flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.setPrimitiveType(PRIMITIVE_RECT);
        pushData(ctx.geometry, data);
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

std::vector<Instance *> Table::getHittableInstances()
{
    std::vector<Instance *> result;
    result.reserve(m_children.size());

    for (Frame *bg : m_rowBackgrounds) {
        result.push_back(bg);
    }
    if (m_headerBackground != nullptr) {
        result.push_back(m_headerBackground);
    }
    for (TextLabel *lbl : m_headerLabels) {
        result.push_back(lbl);
    }
    for (Frame *sep : m_columnSeparators) {
        result.push_back(sep);
    }
    for (Frame *sep : m_rowSeparators) {
        result.push_back(sep);
    }
    for (Instance *cell : m_cells) {
        if (cell != nullptr) {
            result.push_back(cell);
        }
    }

    return result;
}

} // namespace Amethyst
