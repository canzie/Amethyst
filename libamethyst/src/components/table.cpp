/*
 * Table implementation
 */

#include "components/table.h"

#include "rendering/draw_context.h"

namespace Amethyst {

Table::Table(Instance *parent) : UIObject()
{
    setParent(parent);
}

uint32_t Table::getRowCount() const
{
    if (numCols == 0) {
        return 0;
    }
    uint32_t childCount = static_cast<uint32_t>(children.size());
    return (childCount + numCols - 1) / numCols;
}

std::vector<float> Table::computeColumnPositions(float tableWidth) const
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

void Table::updateSeparators()
{
    m_separators.clear();
    if (!showColumnSeparators || numCols <= 1) {
        return;
    }

    float accumulatedScale = 0.0f;
    for (uint32_t i = 0; i < numCols - 1; ++i) {
        float colWeight = columnWeights.empty() ? (1.0f / numCols) : columnWeights[i];
        accumulatedScale += colWeight;

        auto sep = std::make_unique<Frame>();
        sep->position = UDim2(accumulatedScale, -columnSeparatorWidth / 2.0f, 0.0f, 0.0f);
        sep->size = UDim2(0.0f, columnSeparatorWidth, 1.0f, 0.0f);
        sep->backgroundColor = Color3(columnSeparatorColor);
        sep->backgroundTransparency = 1.0f - columnSeparatorColor.a;
        sep->zIndex = zIndex + 1;
        sep->markDirty();
        m_separators.push_back(std::move(sep));
    }
}

bool Table::isRowVisible(uint32_t row) const
{
    (void)row;
    return true;
}

float Table::getRowIndent(uint32_t row) const
{
    (void)row;
    return 0.0f;
}

void Table::draw(DrawContext &ctx)
{
    if (numCols == 0) {
        flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
        return;
    }

    if (flags & FLAG_DIRTY) {
        updateSeparators();
        m_resolvedPadding = cellPadding.resolve(absoluteSize);
    }

    float effectiveRowHeight = rowHeight;
    if (effectiveRowHeight <= 0.0f) {
        effectiveRowHeight = 30.0f; // TODO: compute from children
    }
    m_computedRowHeight = effectiveRowHeight;

    glm::vec4 childClip = clipRect;
    if (clipsDescendants) {
        childClip = {absolutePosition.x, absolutePosition.y,
                     absolutePosition.x + absoluteSize.x, absolutePosition.y + absoluteSize.y};
    }

    std::vector<float> columnPositions = computeColumnPositions(absoluteSize.x);
    uint32_t rowCount = getRowCount();
    uint32_t visibleRowIndex = 0;

    for (auto &sep : m_separators) {
        sep->clipRect = childClip;
        sep->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
        sep->draw(ctx);
    }

    for (uint32_t row = 0; row < rowCount; ++row) {
        if (!isRowVisible(row)) {
            continue;
        }

        float rowY = static_cast<float>(visibleRowIndex) * effectiveRowHeight;
        float indent = getRowIndent(row);

        for (uint32_t col = 0; col < numCols; ++col) {
            uint32_t childIndex = row * numCols + col;
            if (childIndex >= children.size()) {
                break;
            }

            Instance *child = children[childIndex];
            if (auto *drawable = child->as<UIObject>()) {
                float cellX = columnPositions[col];
                float cellWidth = columnPositions[col + 1] - cellX;

                if (col == 0) {
                    cellX += indent;
                    cellWidth -= indent;
                }

                float paddedX = cellX + m_resolvedPadding.w;
                float paddedY = rowY + m_resolvedPadding.x;
                float paddedWidth = cellWidth - m_resolvedPadding.w - m_resolvedPadding.y;
                float paddedHeight = effectiveRowHeight - m_resolvedPadding.x - m_resolvedPadding.z;

                glm::vec2 cellSize = {paddedWidth, paddedHeight};
                glm::vec2 cellPos = absolutePosition + glm::vec2(paddedX, paddedY);

                drawable->clipRect = childClip;
                drawable->computeAbsolutes(cellSize, cellPos, absoluteRotation);
                drawable->draw(ctx);
            }
        }

        visibleRowIndex++;
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
