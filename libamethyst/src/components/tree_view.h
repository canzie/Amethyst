/**
 * @file tree_view.h
 * @brief Hierarchical tree view built on a flat depth-run model.
 *
 * TreeView is Table plus two things: collapse/expand (a collapsed row hides
 * all descendants) and column-0 indentation with a disclosure indicator for
 * rows that have children. Everything else mirrors Table.
 *
 * Rows are stored in DFS build order in m_rows. A visible plan (m_visible) is
 * rebuilt on FLAG_DIRTY via a linear depth-skip scan. No LCRS, no handles, no
 * per-row dirty flags. Culling is O(1) arithmetic over the flat visible plan.
 */

#ifndef AMETHYST__TREE_VIEW_H
#define AMETHYST__TREE_VIEW_H

#include "components/common.h"
#include "components/frame.h"
#include "components/input_events.h"
#include "components/properties.h"
#include "components/text_label.h"
#include "components/ui_object.h"
#include "modules/event_signal.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Amethyst {

class ImageButton;

enum class TreeColumnSizing {
    FIXED,
    STRETCH
};

struct TreeColumn {
    std::string header{};
    TreeColumnSizing sizing = TreeColumnSizing::STRETCH;
    float weight = 1.0f;
    float minWidth = 0.0f;
    float maxWidth = 0.0f;
};

struct TreeRow {
    uint16_t depth = 0;
    bool expanded = true;
    bool hasChildren = false;
};

class TreeView : public UIObject {
  public:
    TreeView();
    virtual ~TreeView();

    void draw(DrawContext &ctx) override;
    void arrange() override;
    void resolveStyle() override;
    void computeAbsolutes(vec2 parentSize, vec2 parentPos, Degrees parentRotation) override;

    /**
     * @brief Append a column definition. Must be called before adding rows.
     * @param col The column definition to add
     */
    void addColumn(TreeColumn col);

    /**
     * @brief Replace all column definitions. Must be called before adding rows.
     * @param cols The new set of column definitions
     */
    void setColumns(std::vector<TreeColumn> cols);

    /**
     * @brief Grow column count to newCount, re-striding existing cell data.
     * @param newCount New column count; no-op if <= current count
     *
     * New columns are anonymous STRETCH. Used by TreeViewScope to support
     * inferred column counts when no explicit columns are declared.
     */
    void resizeColumns(uint32_t newCount);

    /**
     * @brief Get the number of columns currently defined.
     * @return The column count
     */
    uint32_t columnCount() const;

    /**
     * @brief Append a new row at the given depth and move the build cursor to (row, 0).
     * @param depth Nesting depth (0 = root level)
     * @return Stable row index valid for the lifetime of this build
     */
    uint32_t addRow(uint16_t depth = 0);

    /**
     * @brief Add a cell at the cursor position and advance the cursor column. Ownership is transferred.
     * @param child The instance to place in the cell
     * @return Non-owning pointer to the added child
     */
    Instance *nextCell(std::unique_ptr<Instance> child);

    /**
     * @brief Move the build cursor to an arbitrary (row, col) position.
     * @param row Logical row index
     * @param col Column index, must be < columnCount()
     */
    void setCursor(uint32_t row, uint32_t col);

    /**
     * @brief Place a cell at a specific position, replacing any existing cell. Ownership is transferred.
     * @param row Logical row index
     * @param col Column index, must be < columnCount()
     * @param child The instance to place
     */
    void setCell(uint32_t row, uint32_t col, std::unique_ptr<Instance> child);

    /**
     * @brief Get the instance at a specific (row, col) position.
     * @param row Logical row index
     * @param col Column index
     * @return Pointer to the cell instance, or nullptr if empty
     */
    Instance *getCell(uint32_t row, uint32_t col) const;

    /**
     * @brief Wipe all rows, cells, visible plan, and pools. Column definitions are preserved.
     */
    void clear();

    /**
     * @brief Get the total number of rows.
     * @return m_rows.size()
     */
    uint32_t rowCount() const;

    /**
     * @brief Get the depth of a row.
     * @param row Logical row index
     * @return Nesting depth (0 = root)
     */
    uint16_t depth(uint32_t row) const;

    /**
     * @brief Whether a row has any children, derived from the depth-run invariant.
     * @param row Logical row index
     * @return true if the next row in build order has greater depth
     */
    bool hasChildren(uint32_t row) const;

    /**
     * @brief Whether a row is currently expanded.
     * @param row Logical row index
     */
    bool isExpanded(uint32_t row) const;

    /**
     * @brief Toggle the expanded state of a row and mark dirty.
     * @param row Logical row index
     *
     * No descendant walk. The visible plan rebuild handles hiding.
     */
    void toggle(uint32_t row);

    /**
     * @brief Expand a row if not already expanded.
     * @param row Logical row index
     */
    void expand(uint32_t row);

    /**
     * @brief Collapse a row if not already collapsed.
     * @param row Logical row index
     */
    void collapse(uint32_t row);

    /**
     * @brief Expand every row in the tree.
     */
    void expandAll();

    /**
     * @brief Collapse every row in the tree.
     */
    void collapseAll();

    /**
     * @brief Collect all hittable instances (disclosures, row backgrounds, cell children).
     * @return Vector of non-owning pointers to hittable instances
     */
    std::vector<Instance *> getHittableInstances() override;

    bool setTreeViewProperties(const TreeViewStylePropertiesArgs &props);
    const TreeViewStyleProperties &getTreeViewProperties() const { return m_tvProps; }

    static constexpr int32_t NO_ROW_SELECTION = -1;

  public:
    int32_t hoveredRow = NO_ROW_SELECTION;
    int32_t selectedRow = NO_ROW_SELECTION;

    std::function<void(uint32_t)> onRowClicked;
    std::function<void(uint32_t, vec2)> onRowRightClicked;
    std::function<void(uint32_t, bool)> onRowToggled;

  protected:
    TreeViewStyleProperties m_tvProps;

  private:
    static constexpr uint32_t INVALID_ROW = UINT32_MAX;

    void rebuildVisiblePlan();
    void rebuildColumnPositions();
    void updateSeparators();
    void ensureHeaderCapacity();
    void ensurePoolCapacity(uint32_t count);
    void arrangeHeader(const vec4 &childClip);
    void arrangeSeparators(const vec4 &childClip);
    void arrangeRow(uint32_t logicalRow, uint32_t poolSlot, uint32_t visibleIndex, float y, const vec4 &childClip);
    void attachRowCells(uint32_t poolSlot, uint32_t logicalRow);
    void parkRowCells(uint32_t logicalRow);
    void hideSlot(uint32_t poolSlot);

    std::vector<TreeColumn> m_columns;
    std::vector<Instance *> m_cells;
    std::vector<TreeRow> m_rows;
    std::vector<uint32_t> m_visible;

    uint32_t m_cursorRow = 0;
    uint32_t m_cursorCol = 0;

    float m_rowHeightPx = 0.0f;
    vec4 m_cellPaddingPx = {0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> m_columnPositions;

    vec2 m_lastAbsolutePosition = {0.0f, 0.0f};
    vec2 m_lastAbsoluteSize = {0.0f, 0.0f};

    std::vector<std::unique_ptr<Frame>> m_separators;
    std::vector<std::unique_ptr<Frame>> m_rowBackgrounds;
    std::vector<ImageButton *> m_disclosures;
    std::vector<uint32_t> m_rowBySlot;
    std::vector<EventConnection> m_rowHoverConns;
    std::vector<EventConnection> m_rowInputConns;

    std::unique_ptr<Frame> m_headerBackground;
    std::vector<std::unique_ptr<TextLabel>> m_headerLabels;
};

} // namespace Amethyst

#endif // AMETHYST__TREE_VIEW_H
