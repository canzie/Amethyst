/**
 * @file table.h
 * @brief Table component with explicit columns, rows, headers, and sorting.
 *
 * Cells are stored in a flat array indexed by (row * columnCount + col).
 * A separate display order array maps visual position to logical row index,
 * allowing sorting without moving cell data.
 */

#ifndef AMETHYST__TABLE_H
#define AMETHYST__TABLE_H

#include "components/common.h"
#include "components/frame.h"
#include "components/properties.h"
#include "components/scrolling_frame.h"
#include "components/text_label.h"
#include "components/ui_object.h"
#include "modules/event_signal.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Amethyst {

enum class TableColumnSizing {
    FIXED,
    STRETCH
};

struct TableColumn {
    std::string header{};
    TableColumnSizing sizing = TableColumnSizing::STRETCH;
    float weight = 1.0f;
    float minWidth = 0.0f;
    float maxWidth = 0.0f;
    TextXAlignment labelAlign = TextXAlignment::LEFT;
    UDim4 labelPadding{};
};

class Table : public UIObject {
  public:
    Table();

    void draw(DrawContext &ctx) override;
    void arrange() override;
    void resolveStyle() override;

    /**
     * @brief Append a column definition. Must be called before adding rows
     * @param col The column definition to add
     */
    void addColumn(TableColumn col);

    /**
     * @brief Replace all column definitions. Must be called before adding rows
     * @param cols The new set of column definitions
     */
    void setColumns(std::vector<TableColumn> cols);

    /**
     * @brief Grow column count to newCount, re-striding existing cell data.
     * @param newCount New column count; no-op if <= current count
     *
     * New columns are anonymous STRETCH. Used by TableScope to support
     * inferred column counts when no explicit columns are declared.
     */
    void resizeColumns(uint32_t newCount);

    /**
     * @brief Get the number of columns currently defined
     * @return The column count
     */
    uint32_t columnCount() const;

    /**
     * @brief Create a new row and move the build cursor to (row, 0)
     * @param height Height of this row in pixels, or 0 to use the table's rowHeight
     * @return Stable row index, never reused across remove/add cycles
     */
    uint32_t addRow(float height = 0.0f);

    /**
     * @brief Add a cell at the cursor position and advance the cursor column. Ownership is transferred to the table's internal
     * child list
     * @param child The instance to place in the cell
     * @return Non-owning pointer to the added child
     */
    Instance *nextCell(std::unique_ptr<Instance> child);

    /**
     * @brief Move the build cursor to an arbitrary position. Subsequent nextCell() calls write starting from this position
     * @param row Logical row index, must be alive
     * @param col Column index, must be < columnCount()
     */
    void setCursor(uint32_t row, uint32_t col);

    /**
     * @brief Place a cell at a specific position, replacing any existing cell. Ownership is transferred to the table's internal
     * child list
     * @param row Logical row index, must be alive
     * @param col Column index, must be < columnCount()
     * @param child The instance to place
     */
    void setCell(uint32_t row, uint32_t col, std::unique_ptr<Instance> child);

    /**
     * @brief Get the instance at a specific position
     * @param row Logical row index
     * @param col Column index
     * @return Pointer to the cell instance, or nullptr if empty
     */
    Instance *getCell(uint32_t row, uint32_t col) const;

    /**
     * @brief Remove a row and destroy all its cells. The slot is added to a freelist for reuse by future addRow() calls
     * @param row Logical row index to remove
     */
    void removeRow(uint32_t row);

    /**
     * @brief Remove all rows and cells. Column definitions are preserved
     */
    void clear();

    /**
     * @brief Get the number of alive (non-removed) rows
     * @return The row count
     */
    uint32_t rowCount() const;

    /**
     * @brief The height every alive row occupies together, row separators included
     * @return The content height in pixels
     */
    float contentHeight() const;

    /**
     * @brief Reorder rows by sorting the display order. Only permutes the visual ordering, cell data and row indices stay
     * unchanged. Use getCell() inside the comparator to inspect content
     * @param comparator Receives two logical row indices, returns true if the first should appear before the second
     */
    void sort(std::function<bool(uint32_t rowA, uint32_t rowB)> comparator);

    /**
     * @brief Collect hittable instances with cells last, so they win z-index ties against the row background
     * @return Vector of non-owning pointers to hittable instances
     */
    std::vector<Instance *> getHittableInstances() override;

    bool setTableProperties(const TableStylePropertiesArgs &props);
    const TableStyleProperties &getTableProperties() const { return m_tProps; }

    std::function<void(uint32_t)> onRowSelected;

  protected:
    TableStyleProperties m_tProps;

  private:
    void rebuildColumnPositions();
    void updateSeparators();
    void ensureHeaderCapacity();
    void ensureRowBackgroundCapacity(uint32_t count);
    void ensureColumnSeparatorCapacity(uint32_t count);
    void ensureRowSeparatorCapacity(uint32_t count);
    void arrangeHeader(const vec4 &childClip);
    void arrangeSeparators(const vec4 &childClip);
    void layoutRowBackground(uint32_t visualIndex, float y, float height);
    void arrangeRow(uint32_t logicalRow, float y, float height, vec2 bodyOrigin, const vec4 &childClip);

    /**
     * @brief The height a row lays out at, falling back to the table's rowHeight when it has none of its own
     * @param logicalRow Logical row index
     * @return The height in pixels
     */
    float resolveRowHeight(uint32_t logicalRow) const;

    /**
     * @brief Gives one row a height of its own, or drops the one it had when height is 0
     * @param logicalRow Logical row index
     * @param height Height in pixels, 0 to inherit the table's rowHeight
     */
    void setRowHeight(uint32_t logicalRow, float height);

  private:
    std::vector<TableColumn> m_columns;

    // Flat cell storage, cell (row, col) is at m_cells[row * columnCount() + col]. Non-owning pointers, the actual instances are
    // owned in m_children
    std::vector<Instance *> m_cells;

    // Row slots freed by removeRow(), reused by addRow()
    std::vector<uint32_t> m_rowFreelist;

    // Maps visual row position to logical row index. Sorting permutes this without moving cell data
    std::vector<uint32_t> m_displayOrder;

    // Only the rows whose height differs from the table's rowHeight, sorted by logical row index
    std::vector<std::pair<uint32_t, float>> m_rowHeights;

    uint32_t m_cursorRow = 0;
    uint32_t m_cursorCol = 0;

    int32_t m_selectedDisplayIndex = -1;

    vec4 m_resolvedPadding = {0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> m_columnPositions;

    std::vector<Frame *> m_columnSeparators;
    std::vector<Frame *> m_rowSeparators;
    std::vector<Frame *> m_rowBackgrounds;
    std::vector<EventConnection> m_rowBgInputConns;
    Frame *m_headerBackground = nullptr;
    std::vector<TextLabel *> m_headerLabels;

    ScrollingFrame *m_scrollFrame = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__TABLE_H
