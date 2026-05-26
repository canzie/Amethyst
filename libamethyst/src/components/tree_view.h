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

#include "components/common.h"
#include "components/frame.h"
#include "components/properties.h"
#include "components/text_button.h"
#include "components/ui_object.h"

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
    bool isDirty = true;
};

struct TreeRowHandle {
    uint32_t index = INVALID_ROW;
    uint32_t generation = 0;

    bool operator==(const TreeRowHandle &) const = default;
};

class TreeView : public UIObject {
  public:
    TreeView();
    virtual ~TreeView();

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

    std::vector<Instance *> getHittableInstances() override;

    bool setTreeViewProperties(const TreeViewProperties &props);
    const TreeViewProperties &getTreeViewProperties() const { return m_tvProps; }

  public:
    uint32_t numCols = 1;
    std::vector<float> columnWeights;

    int32_t hoveredRow = -1;
    int32_t selectedRow = -1;

    std::function<void(uint32_t)> onRowClicked;
    std::function<void(uint32_t, bool)> onRowToggled;

  protected:
    TreeViewProperties m_tvProps;

  private:
    std::vector<float> computeColumnPositions(float tableWidth) const;
    void updateSeparators();

    bool isRowVisible(uint32_t row) const;
    float getRowIndent(uint32_t row) const;

    void drawRowContent(DrawContext &ctx, uint32_t row, uint32_t bufferSlot, uint32_t visualIndex, const std::vector<float> &colPositions, const glm::vec4 &childClip);
    void drawDisclosureTriangle(DrawContext &ctx, uint32_t row, uint32_t bufferSlot, float y, bool expanded, const glm::vec4 &childClip);
    void linkRowToParent(uint32_t row, uint32_t parentRow);
    void unlinkRow(uint32_t row);
    void markRowDirty(uint32_t row);
    void clearRowCells(DrawContext &ctx, uint32_t row);

    void ensureSlotCapacity(uint32_t slotCount);
    uint32_t drawVisibleRows(DrawContext &ctx, const std::vector<float> &colPositions, const glm::vec4 &childClip, uint32_t firstVisibleSlot, uint32_t slotCount);
    void drawEmptyRows(DrawContext &ctx, const glm::vec4 &childClip, uint32_t fromSlot, uint32_t slotCount, uint32_t firstVisibleSlot);
    void clearUnusedSlots(DrawContext &ctx, uint32_t fromSlot);

    uint32_t allocateSlot();
    void freeSlot(uint32_t index);
    bool isValidRow(uint32_t index) const;

    float m_computedRowHeight = 0.0f;
    glm::vec4 m_resolvedPadding = {0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec2 m_lastAbsolutePosition = {0.0f, 0.0f};
    glm::vec2 m_lastAbsoluteSize = {0.0f, 0.0f};
    std::vector<std::unique_ptr<Frame>> m_separators;

    std::vector<TreeRow> m_rows;
    std::vector<uint32_t> m_freelist;
    uint32_t m_firstRoot = INVALID_ROW;
    uint32_t m_lastRoot = INVALID_ROW;

    std::vector<uint32_t> m_buildStack;

    std::vector<std::unique_ptr<Frame>> m_rowBackgrounds;
    std::vector<std::unique_ptr<TextButton>> m_rowDisclosures;
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
