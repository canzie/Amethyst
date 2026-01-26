/**
 * @file tree_view.h
 * @brief Hierarchical tree view with columns
 *
 * Displays a tree structure where each row can have child rows that can be
 * expanded/collapsed. Uses array-backed LCRS (left-child right-sibling) for
 * efficient traversal.
 */

#ifndef AMETHYST__TREE_VIEW_H
#define AMETHYST__TREE_VIEW_H

#include "components/frame.h"
#include "components/table.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Amethyst {

static constexpr uint32_t INVALID_ROW = UINT32_MAX;

struct TreeRow {
    uint32_t generation = 0;
    uint32_t parent = INVALID_ROW;
    uint32_t firstChild = INVALID_ROW;
    uint32_t lastChild = INVALID_ROW;
    uint32_t nextSibling = INVALID_ROW;
    uint32_t prevSibling = INVALID_ROW;
    uint32_t firstCellIndex = INVALID_ROW;
    bool expanded = true;
    bool alive = false;
};

struct TreeRowHandle {
    uint32_t index = INVALID_ROW;
    uint32_t generation = 0;

    bool operator==(const TreeRowHandle &) const = default;
};

class TreeView : public Table {
  public:
    TreeView() = default;
    TreeView(Instance *parent);
    virtual ~TreeView() = default;

    void draw(DrawContext &ctx) override;

    /**
     * @brief Start building a new row
     * @param parentRow Parent row index, or INVALID_ROW for root-level
     * @return Index of the new row
     *
     * After calling beginRow(), add exactly numCols children, then call endRow().
     */
    uint32_t beginRow(uint32_t parentRow = INVALID_ROW);

    /**
     * @brief Finish building the current row
     *
     * Validates that exactly numCols children were added since beginRow().
     */
    void endRow();

    void removeRow(uint32_t row);
    void clear();

    uint32_t rowCount() const;
    TreeRow &row(uint32_t index);
    const TreeRow &row(uint32_t index) const;
    uint32_t firstRootRow() const;

    uint32_t depth(uint32_t row) const;
    bool hasChildren(uint32_t row) const;
    bool isAncestorOf(uint32_t ancestor, uint32_t descendant) const;
    uint32_t findRowContaining(Instance *child) const;

    template <typename Fn> void forEachRow(Fn &&fn) const;
    template <typename Fn> void forEachVisibleRow(Fn &&fn) const;

    void toggle(uint32_t row);
    void expand(uint32_t row);
    void collapse(uint32_t row);
    void expandAll();
    void collapseAll();

  protected:
    bool isRowVisible(uint32_t row) const override;
    float getRowIndent(uint32_t row) const override;

  private:
    void drawRowContent(DrawContext &ctx, uint32_t row, uint32_t visibleIndex,
                        const std::vector<float> &colPositions);
    void drawDisclosureTriangle(DrawContext &ctx, uint32_t row, float y, bool expanded);
    void linkRowToParent(uint32_t row, uint32_t parentRow);
    void unlinkRow(uint32_t row);

  public:
    float indentPerLevel = 16.0f;

    bool showDisclosureTriangles = true;
    float disclosureTriangleSize = 10.0f;
    float disclosureTrianglePadding = 4.0f;
    Color4 disclosureTriangleColor = {0.7f, 0.7f, 0.7f, 1.0f};

    int32_t hoveredRow = -1;
    int32_t selectedRow = -1;
    Color4 rowBackgroundColor = {0.18f, 0.18f, 0.2f, 1.0f};
    Color4 rowAlternateColor = {0.22f, 0.22f, 0.24f, 1.0f};
    Color4 rowHoverColor = {0.3f, 0.3f, 0.35f, 1.0f};
    Color4 rowSelectedColor = {0.25f, 0.4f, 0.65f, 1.0f};

    std::function<void(uint32_t)> onRowClicked;
    std::function<void(uint32_t, bool)> onRowToggled;

  private:
    uint32_t allocateSlot();
    void freeSlot(uint32_t index);
    bool isValidRow(uint32_t index) const;

    std::vector<TreeRow> m_rows;
    std::vector<uint32_t> m_freelist;
    uint32_t m_firstRoot = INVALID_ROW;
    uint32_t m_lastRoot = INVALID_ROW;

    std::vector<uint32_t> m_buildStack;

    std::vector<std::unique_ptr<Frame>> m_rowBackgrounds;
    std::vector<std::unique_ptr<Frame>> m_disclosureFrames;
};

template <typename Fn> void TreeView::forEachRow(Fn &&fn) const
{
    std::vector<uint32_t> stack;

    for (uint32_t r = m_lastRoot; r != INVALID_ROW;) {
        if (m_rows[r].alive) {
            stack.push_back(r);
        }
        r = m_rows[r].prevSibling;
    }

    while (!stack.empty()) {
        uint32_t current = stack.back();
        stack.pop_back();

        fn(current);

        const TreeRow &r = m_rows[current];
        for (uint32_t c = r.lastChild; c != INVALID_ROW;) {
            if (m_rows[c].alive) {
                stack.push_back(c);
            }
            c = m_rows[c].prevSibling;
        }
    }
}

template <typename Fn> void TreeView::forEachVisibleRow(Fn &&fn) const
{
    std::vector<uint32_t> stack;

    for (uint32_t r = m_lastRoot; r != INVALID_ROW;) {
        if (m_rows[r].alive) {
            stack.push_back(r);
        }
        r = m_rows[r].prevSibling;
    }

    while (!stack.empty()) {
        uint32_t current = stack.back();
        stack.pop_back();

        fn(current);

        const TreeRow &r = m_rows[current];
        if (r.expanded) {
            for (uint32_t c = r.lastChild; c != INVALID_ROW;) {
                if (m_rows[c].alive) {
                    stack.push_back(c);
                }
                c = m_rows[c].prevSibling;
            }
        }
    }
}

} // namespace Amethyst

#endif // AMETHYST__TREE_VIEW_H
