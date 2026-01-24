/**
 * @file tree_view.h
 * @brief Hierarchical tree view built on Table
 *
 * TreeView extends Table to display hierarchical data with indentation,
 * disclosure triangles, and collapsible nodes. Useful for scene graphs,
 * file browsers, and other tree-structured data.
 */

#ifndef AMETHYST__TREE_VIEW_H
#define AMETHYST__TREE_VIEW_H

#include "components/table.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace Amethyst {

struct TreeRowMetadata {
    uint32_t depth = 0;
    bool expanded = true;
    bool hasChildren = false;
    bool visible = true;
};

/**
 * @brief Hierarchical tree view component
 *
 * TreeView is a Table configured for single-column hierarchical display.
 * Rows have depth levels that control indentation, and can be expanded
 * or collapsed to show/hide their descendants.
 */
class TreeView : public Table {
  public:
    TreeView() = default;
    TreeView(Instance *parent);
    virtual ~TreeView() = default;

    void draw(DrawContext &ctx) override;

    void setRowDepth(uint32_t row, uint32_t depth);
    void setRowExpanded(uint32_t row, bool expanded);
    void setRowHasChildren(uint32_t row, bool hasChildren);

    uint32_t getRowDepth(uint32_t row) const;
    bool isRowExpanded(uint32_t row) const;
    bool rowHasChildren(uint32_t row) const;

    void toggleRowExpanded(uint32_t row);
    void expandAll();
    void collapseAll();

    /**
     * @brief Expand a row and all its ancestors to make it visible
     */
    void revealRow(uint32_t row);

    int32_t getRowAtPosition(float y) const;

  public:
    float indentPerLevel = 16.0f;
    bool showDisclosureTriangles = true;
    float disclosureTriangleSize = 12.0f;
    Color4 disclosureTriangleColor = {0.7f, 0.7f, 0.7f, 1.0f};

    int32_t activeRow = -1;

    Color4 rowBackgroundColor = {0.0f, 0.0f, 0.0f, 0.0f};
    Color4 rowAlternateColor = {0.0f, 0.0f, 0.0f, 0.0f};
    Color4 rowHoverColor = {0.3f, 0.3f, 0.3f, 0.5f};
    Color4 rowActiveColor = {0.4f, 0.4f, 0.6f, 0.8f};
    Color4 rowActiveHoverColor = {0.5f, 0.5f, 0.7f, 0.8f};

    std::function<void(uint32_t row)> onRowClicked;
    std::function<void(uint32_t row)> onRowHovered;
    std::function<void(uint32_t row)> onRowDoubleClicked;
    std::function<void(uint32_t row, bool expanded)> onRowToggled;

  protected:
    bool isRowVisible(uint32_t row) const override;
    float getRowIndent(uint32_t row) const override;

    void onMouseButton1Down(uint32_t x, uint32_t y) override;
    void onMouseButton1Click() override;
    void onMouseMoved(uint32_t x, uint32_t y) override;
    void onMouseLeave() override;

    void recomputeVisibility();
    void ensureRowMetadata(uint32_t rowCount);
    bool isOnDisclosureTriangle(uint32_t row, float localX) const;
    Color4 getRowBackgroundColor(uint32_t row, uint32_t visibleRowIndex) const;

  private:
    std::vector<TreeRowMetadata> m_rowMetadata;
    int32_t m_hoveredRow = -1;
};

} // namespace Amethyst

#endif // AMETHYST__TREE_VIEW_H
