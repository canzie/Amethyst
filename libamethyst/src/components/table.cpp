#include "components/table.h"

#include "modules/style.h"
#include "rendering/draw_context.h"
#include "utils/am_assert.h"

#include <algorithm>

namespace Amethyst {

static void s_applyStyle(Table &table)
{
    const auto &style = Style::instance();
    table.backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::TABLE);
    table.backgroundTransparency = style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::TABLE);
    table.borderColor = style.get<Color3>(StyleProperty::BORDER_COLOR, ComponentType::TABLE);
    table.borderTransparency = style.get<float>(StyleProperty::BORDER_TRANSPARENCY, ComponentType::TABLE);
    table.borderPixelSize = style.get<float>(StyleProperty::BORDER_PIXEL_SIZE, ComponentType::TABLE);
    table.cornerRadius = style.get<float>(StyleProperty::CORNER_RADIUS, ComponentType::TABLE);
    table.rowHeight = style.get<float>(StyleProperty::ROW_HEIGHT, ComponentType::TABLE);
    table.columnSeparatorWidth = style.get<float>(StyleProperty::COLUMN_SEPARATOR_WIDTH, ComponentType::TABLE);
    table.columnSeparatorColor = style.get<Color4>(StyleProperty::COLUMN_SEPARATOR_COLOR, ComponentType::TABLE);
    table.headerColor = style.get<Color3>(StyleProperty::HEADER_COLOR, ComponentType::TABLE);
    table.headerHeight = style.get<float>(StyleProperty::HEADER_HEIGHT, ComponentType::TABLE);
    table.rowBackgroundColor = style.get<Color4>(StyleProperty::ROW_BACKGROUND_COLOR, ComponentType::TABLE);
    table.rowAlternateColor = style.get<Color4>(StyleProperty::ROW_ALTERNATE_COLOR, ComponentType::TABLE);
    table.rowHoverColor = style.get<Color4>(StyleProperty::ROW_HOVER_COLOR, ComponentType::TABLE);
    table.rowSelectedColor = style.get<Color4>(StyleProperty::ROW_SELECTED_COLOR, ComponentType::TABLE);
}

Table::Table()
{
    s_applyStyle(*this);
}

Table::~Table()
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

    if (m_cells[idx]) {
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

    if (m_cells[idx]) {
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
        if (m_cells[idx]) {
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
    m_children.clear();
    m_cells.clear();
    m_displayOrder.clear();
    m_rowFreelist.clear();
    m_rowBackgrounds.clear();
    m_cursorRow = 0;
    m_cursorCol = 0;
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
    float fixedTotal = 0.0f;
    float stretchTotal = 0.0f;

    for (const auto &col : m_columns) {
        if (col.sizing == TableColumnSizing::FIXED) {
            fixedTotal += col.width;
        } else {
            stretchTotal += col.width;
        }
    }

    float remaining = std::max(0.0f, totalWidth - fixedTotal);
    if (stretchTotal <= 0.0f) {
        stretchTotal = 1.0f;
    }

    float x = 0.0f;
    for (const auto &col : m_columns) {
        if (col.sizing == TableColumnSizing::FIXED) {
            x += col.width;
        } else {
            x += (col.width / stretchTotal) * remaining;
        }
        m_columnPositions.push_back(x);
    }
}

void Table::updateSeparators()
{
    m_separators.clear();
    uint32_t cols = columnCount();
    if (!showColumnSeparators || cols <= 1) {
        return;
    }

    for (uint32_t i = 0; i < cols - 1; i++) {
        float xPos = m_columnPositions[i + 1];

        auto sep = std::make_unique<Frame>();
        sep->position = UDim2(0.0f, xPos - columnSeparatorWidth / 2.0f, 0.0f, 0.0f);
        sep->size = UDim2(0.0f, columnSeparatorWidth, 1.0f, 0.0f);
        sep->backgroundColor = Color3(columnSeparatorColor);
        sep->backgroundTransparency = 1.0f - columnSeparatorColor.a;
        sep->zIndex = getZIndex() + 1;
        sep->markDirty();
        m_separators.push_back(std::move(sep));
    }
}

void Table::ensureHeaderCapacity()
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

void Table::ensureRowBackgroundCapacity(uint32_t count)
{
    while (m_rowBackgrounds.size() < count) {
        auto frame = std::make_unique<Frame>();
        frame->parent = this;
        m_rowBackgrounds.push_back(std::move(frame));
    }
}

void Table::drawHeader(DrawContext &ctx, const glm::vec4 &childClip)
{
    uint32_t cols = columnCount();
    ensureHeaderCapacity();

    m_headerBackground->backgroundColor = headerColor;
    m_headerBackground->backgroundTransparency = 0.0f;
    m_headerBackground->clipRect = childClip;
    m_headerBackground->zIndex = getZIndex();
    m_headerBackground->markDirty();
    m_headerBackground->computeAbsolutes({absoluteSize.x, headerHeight}, absolutePosition, absoluteRotation);
    m_headerBackground->draw(ctx);

    for (uint32_t col = 0; col < cols; col++) {
        TextLabel *lbl = m_headerLabels[col].get();
        lbl->text = m_columns[col].header;
        lbl->size = UDim2::fromScale(1.0f, 1.0f);
        lbl->textColor = headerTextColor;
        lbl->fontSize = headerFontSize;
        lbl->backgroundTransparency = 1.0f;
        lbl->textXAlignment = TextXAlignment::LEFT;
        lbl->textYAlignment = TextYAlignment::CENTER;
        lbl->clipRect = childClip;
        lbl->zIndex = getZIndex() + 1;
        lbl->markDirty();

        float cellX = m_columnPositions[col] + m_resolvedPadding.w;
        float cellWidth = m_columnPositions[col + 1] - m_columnPositions[col] - m_resolvedPadding.w - m_resolvedPadding.y;

        lbl->computeAbsolutes({cellWidth, headerHeight}, absolutePosition + glm::vec2(cellX, 0.0f), absoluteRotation);
        lbl->draw(ctx);
    }
}

void Table::drawSeparators(DrawContext &ctx, const glm::vec4 &childClip)
{
    for (auto &sep : m_separators) {
        sep->clipRect = childClip;
        sep->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        sep->draw(ctx);
    }
}

void Table::drawRow(DrawContext &ctx, uint32_t logicalRow, uint32_t visualIndex, float y, const glm::vec4 &childClip)
{
    uint32_t cols = columnCount();

    Frame *bg = m_rowBackgrounds[visualIndex].get();
    Color4 bgColor = rowBackgroundColor;
    if (static_cast<int32_t>(logicalRow) == selectedRow) {
        bgColor = rowSelectedColor;
    } else if (static_cast<int32_t>(logicalRow) == hoveredRow) {
        bgColor = rowHoverColor;
    } else if (visualIndex % 2 == 1 && rowAlternateColor.a > 0.0f) {
        bgColor = rowAlternateColor;
    }

    bg->backgroundColor = Color3(bgColor);
    bg->backgroundTransparency = 1.0f - bgColor.a;
    bg->clipRect = childClip;
    bg->zIndex = getZIndex();
    bg->markDirty();
    bg->computeAbsolutes({absoluteSize.x, m_computedRowHeight}, absolutePosition + glm::vec2(0.0f, y), absoluteRotation);
    bg->draw(ctx);

    for (uint32_t col = 0; col < cols; col++) {
        Instance *cell = m_cells[logicalRow * cols + col];
        if (!cell) {
            continue;
        }

        auto *drawable = cell->as<UIObject>();
        if (!drawable) {
            continue;
        }

        float cellX = m_columnPositions[col];
        float cellWidth = m_columnPositions[col + 1] - cellX;

        float paddedX = cellX + m_resolvedPadding.w;
        float paddedY = y + m_resolvedPadding.x;
        float paddedWidth = cellWidth - m_resolvedPadding.w - m_resolvedPadding.y;
        float paddedHeight = m_computedRowHeight - m_resolvedPadding.x - m_resolvedPadding.z;

        drawable->clipRect = childClip;
        drawable->computeAbsolutes({paddedWidth, paddedHeight}, absolutePosition + glm::vec2(paddedX, paddedY), absoluteRotation);
        drawable->draw(ctx);
    }
}

void Table::draw(DrawContext &ctx)
{
    uint32_t cols = columnCount();
    if (cols == 0) {
        flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
        return;
    }

    if (flags & FLAG_DIRTY) {
        rebuildColumnPositions();
        updateSeparators();
        m_resolvedPadding = cellPadding.resolve(absoluteSize);
    }

    m_computedRowHeight = rowHeight > 0.0f ? rowHeight : 24.0f;

    glm::vec4 childClip = computeChildClipRect();
    float dataStartY = showHeader ? headerHeight : 0.0f;

    if (showHeader) {
        drawHeader(ctx, childClip);
    }

    drawSeparators(ctx, childClip);

    uint32_t visibleCount = static_cast<uint32_t>(m_displayOrder.size());
    ensureRowBackgroundCapacity(visibleCount);

    for (uint32_t vi = 0; vi < visibleCount; vi++) {
        float rowY = dataStartY + static_cast<float>(vi) * m_computedRowHeight;
        drawRow(ctx, m_displayOrder[vi], vi, rowY, childClip);
    }

    for (uint32_t i = visibleCount; i < m_rowBackgrounds.size(); i++) {
        Frame *bg = m_rowBackgrounds[i].get();
        bg->visible = false;
        bg->markDirty();
        bg->draw(ctx);
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

std::vector<Instance *> Table::getHittableInstances()
{
    std::vector<Instance *> result;
    result.reserve(m_rowBackgrounds.size() + m_children.size());

    for (auto &bg : m_rowBackgrounds) {
        result.push_back(bg.get());
    }
    for (auto &child : m_children) {
        result.push_back(child.get());
    }

    return result;
}

} // namespace Amethyst
