/**
 * @file table.h
 * @brief Table component for grid-based layouts
 *
 * Table arranges children in a grid where every numCols children form one row.
 * Supports column weights and separators.
 */

#ifndef AMETHYST__TABLE_H
#define AMETHYST__TABLE_H

#include "components/common.h"
#include "components/frame.h"
#include "components/instance.h"
#include "components/ui_object.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Amethyst {

/**
 * @brief Table component for grid-based layouts
 *
 * Children are laid out in rows: child i goes to row (i / numCols), column (i % numCols).
 */
class Table : public UIObject {
  public:
    Table() = default;
    Table(Instance *parent);
    virtual ~Table() = default;

    void draw(DrawContext &ctx) override;

  public:
    /**
     * @brief Number of columns in the table
     *
     * Children are arranged so that every numCols children form one row.
     */
    uint32_t numCols = 1;

    /**
     * @brief Column widths as fractions that sum to 1.0
     *
     * If empty, columns are equal width. Otherwise, each value represents
     * that column's fraction of total width (e.g., {0.25, 0.5, 0.25}).
     */
    std::vector<float> columnWeights;

    /**
     * @brief Fixed row height in pixels
     *
     * If 0, row height is determined by the tallest child in the row.
     */
    float rowHeight = 0.0f;

    bool showColumnSeparators = false;
    float columnSeparatorWidth = 1.0f;
    Color4 columnSeparatorColor = {0.3f, 0.3f, 0.3f, 1.0f};

  public:
    uint32_t getRowCount() const;
    std::vector<float> computeColumnPositions(float tableWidth) const;

  protected:
    virtual bool isRowVisible(uint32_t row) const;
    void updateSeparators();

    /**
     * @brief Get horizontal offset for first column (overridden by TreeView for indentation)
     */
    virtual float getRowIndent(uint32_t row) const;

  protected:
    float m_computedRowHeight = 0.0f;
    std::vector<std::unique_ptr<Frame>> m_separators;
};

} // namespace Amethyst

#endif // AMETHYST__TABLE_H
