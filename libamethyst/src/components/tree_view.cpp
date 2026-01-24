/*
 * TreeView implementation
 */

#include "components/tree_view.h"

#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace Amethyst {

TreeView::TreeView(Instance *parent) : Table()
{
    setParent(parent);
    numCols = 1;
}

void TreeView::setRowDepth(uint32_t row, uint32_t depth)
{
    ensureRowMetadata(row + 1);
    m_rowMetadata[row].depth = depth;
    recomputeVisibility();
}

void TreeView::setRowExpanded(uint32_t row, bool expanded)
{
    ensureRowMetadata(row + 1);
    m_rowMetadata[row].expanded = expanded;
    recomputeVisibility();
    markDirty();
}

void TreeView::setRowHasChildren(uint32_t row, bool hasChildren)
{
    ensureRowMetadata(row + 1);
    m_rowMetadata[row].hasChildren = hasChildren;
}

uint32_t TreeView::getRowDepth(uint32_t row) const
{
    if (row >= m_rowMetadata.size()) {
        return 0;
    }
    return m_rowMetadata[row].depth;
}

bool TreeView::isRowExpanded(uint32_t row) const
{
    if (row >= m_rowMetadata.size()) {
        return true;
    }
    return m_rowMetadata[row].expanded;
}

bool TreeView::rowHasChildren(uint32_t row) const
{
    if (row >= m_rowMetadata.size()) {
        return false;
    }
    return m_rowMetadata[row].hasChildren;
}

void TreeView::toggleRowExpanded(uint32_t row)
{
    if (row >= m_rowMetadata.size()) {
        return;
    }
    setRowExpanded(row, !m_rowMetadata[row].expanded);
    if (onRowToggled) {
        onRowToggled(row, m_rowMetadata[row].expanded);
    }
}

void TreeView::expandAll()
{
    for (auto &meta : m_rowMetadata) {
        meta.expanded = true;
    }
    recomputeVisibility();
    markDirty();
}

void TreeView::collapseAll()
{
    for (auto &meta : m_rowMetadata) {
        meta.expanded = false;
    }
    recomputeVisibility();
    markDirty();
}

void TreeView::revealRow(uint32_t row)
{
    if (row >= m_rowMetadata.size()) {
        return;
    }

    uint32_t targetDepth = m_rowMetadata[row].depth;
    for (int32_t i = static_cast<int32_t>(row) - 1; i >= 0 && targetDepth > 0; --i) {
        if (m_rowMetadata[i].depth < targetDepth) {
            m_rowMetadata[i].expanded = true;
            targetDepth = m_rowMetadata[i].depth;
        }
    }
    recomputeVisibility();
    markDirty();
}

int32_t TreeView::getRowAtPosition(float y) const
{
    float effectiveRowHeight = rowHeight > 0.0f ? rowHeight : m_computedRowHeight;
    if (effectiveRowHeight <= 0.0f) {
        return -1;
    }

    float relativeY = y - absolutePosition.y;
    if (relativeY < 0.0f || relativeY >= absoluteSize.y) {
        return -1;
    }

    uint32_t visibleRowIndex = static_cast<uint32_t>(relativeY / effectiveRowHeight);
    uint32_t rowCount = getRowCount();
    uint32_t currentVisible = 0;

    for (uint32_t row = 0; row < rowCount; ++row) {
        if (!isRowVisible(row)) {
            continue;
        }
        if (currentVisible == visibleRowIndex) {
            return static_cast<int32_t>(row);
        }
        ++currentVisible;
    }

    return -1;
}

bool TreeView::isRowVisible(uint32_t row) const
{
    if (row >= m_rowMetadata.size()) {
        return true;
    }
    return m_rowMetadata[row].visible;
}

float TreeView::getRowIndent(uint32_t row) const
{
    if (row >= m_rowMetadata.size()) {
        return 0.0f;
    }
    return static_cast<float>(m_rowMetadata[row].depth) * indentPerLevel;
}

void TreeView::recomputeVisibility()
{
    uint32_t rowCount = getRowCount();
    ensureRowMetadata(rowCount);

    std::vector<bool> ancestorExpanded;

    for (uint32_t i = 0; i < rowCount; ++i) {
        auto &meta = m_rowMetadata[i];

        while (ancestorExpanded.size() > meta.depth) {
            ancestorExpanded.pop_back();
        }

        meta.visible = std::all_of(ancestorExpanded.begin(), ancestorExpanded.end(), [](bool b) { return b; });

        if (meta.hasChildren) {
            ancestorExpanded.push_back(meta.expanded);
        }
    }
}

void TreeView::ensureRowMetadata(uint32_t rowCount)
{
    if (m_rowMetadata.size() < rowCount) {
        m_rowMetadata.resize(rowCount);
    }
}

bool TreeView::isOnDisclosureTriangle(uint32_t row, float localX) const
{
    if (!showDisclosureTriangles || !rowHasChildren(row)) {
        return false;
    }
    float indent = getRowIndent(row);
    return localX >= indent && localX < indent + disclosureTriangleSize;
}

Color4 TreeView::getRowBackgroundColor(uint32_t row, uint32_t visibleRowIndex) const
{
    bool isActive = (static_cast<int32_t>(row) == activeRow);
    bool isHovered = (static_cast<int32_t>(row) == m_hoveredRow);

    if (isActive && isHovered) {
        return rowActiveHoverColor;
    } else if (isActive) {
        return rowActiveColor;
    } else if (isHovered) {
        return rowHoverColor;
    } else if (visibleRowIndex % 2 == 1 && rowAlternateColor.a > 0.0f) {
        return rowAlternateColor;
    }
    return rowBackgroundColor;
}

void TreeView::onMouseMoved(uint32_t x, uint32_t y)
{
    (void)x;
    int32_t newHovered = getRowAtPosition(static_cast<float>(y));
    if (newHovered != m_hoveredRow) {
        if (m_hoveredRow >= 0) {
            children[m_hoveredRow]->markDirty();
        }
        m_hoveredRow = newHovered;
        if (m_hoveredRow >= 0) {
            children[m_hoveredRow]->markDirty();
            if (onRowHovered) {
                onRowHovered(static_cast<uint32_t>(m_hoveredRow));
            }
        }
    }
}

void TreeView::onMouseLeave()
{
    if (m_hoveredRow >= 0) {
        children[m_hoveredRow]->markDirty();
        m_hoveredRow = -1;
    }
}

void TreeView::onMouseButton1Down(uint32_t x, uint32_t y)
{
    int32_t clickedRow = getRowAtPosition(static_cast<float>(y));
    if (clickedRow < 0) {
        return;
    }

    float localX = static_cast<float>(x) - absolutePosition.x;
    if (isOnDisclosureTriangle(static_cast<uint32_t>(clickedRow), localX)) {
        toggleRowExpanded(static_cast<uint32_t>(clickedRow));
        return;
    }

    if (activeRow >= 0 && activeRow != clickedRow) {
        children[activeRow]->markDirty();
    }
    activeRow = clickedRow;
    children[activeRow]->markDirty();
}

void TreeView::onMouseButton1Click()
{
    if (activeRow >= 0 && onRowClicked) {
        onRowClicked(static_cast<uint32_t>(activeRow));
    }
}

void TreeView::draw(DrawContext &ctx)
{
    if (numCols == 0) {
        flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
        return;
    }

    float effectiveRowHeight = rowHeight;
    if (effectiveRowHeight <= 0.0f) {
        effectiveRowHeight = 30.0f; // TODO: compute from children
    }
    m_computedRowHeight = effectiveRowHeight;

    ensureRowMetadata(getRowCount());

    std::vector<float> columnPositions = computeColumnPositions(absoluteSize.x);
    uint32_t rowCount = getRowCount();
    uint32_t visibleRowIndex = 0;

    for (uint32_t row = 0; row < rowCount; ++row) {
        if (!isRowVisible(row)) {
            continue;
        }

        float rowY = static_cast<float>(visibleRowIndex) * effectiveRowHeight;
        float indent = getRowIndent(row);

        Color4 bgColor = getRowBackgroundColor(row, visibleRowIndex);
        if (bgColor.a > 0.0f) {
            InstanceData rowData;
            rowData.transform =
                glm::translate(glm::mat4(1.0f), glm::vec3(absolutePosition.x, absolutePosition.y + rowY, 0.0f));
            rowData.transform = glm::scale(rowData.transform, glm::vec3(absoluteSize.x, effectiveRowHeight, 1.0f));
            rowData.fillColor = bgColor;
            rowData.borderColor = Color4(0.0f);
            rowData.borderThickness = 0.0f;
            rowData.cornerRadius = 0.0f;
            rowData.primitiveType = PRIMITIVE_RECT;
            rowData.borderMode = static_cast<uint32_t>(BorderMode::OUTLINE);
            rowData.zIndex = zIndex;

            ctx.geometry->submit(rowData);
        }

        if (showDisclosureTriangles && rowHasChildren(row)) {
            float triangleX = absolutePosition.x + indent;
            float triangleY = absolutePosition.y + rowY + (effectiveRowHeight - disclosureTriangleSize) / 2.0f;

            InstanceData triangleData;
            triangleData.transform = glm::translate(glm::mat4(1.0f), glm::vec3(triangleX, triangleY, 0.0f));
            triangleData.transform =
                glm::scale(triangleData.transform, glm::vec3(disclosureTriangleSize, disclosureTriangleSize, 1.0f));

            if (!isRowExpanded(row)) {
                triangleData.transform =
                    glm::rotate(triangleData.transform, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            }

            triangleData.fillColor = disclosureTriangleColor;
            triangleData.borderColor = Color4(0.0f);
            triangleData.borderThickness = 0.0f;
            triangleData.cornerRadius = 0.0f;
            triangleData.primitiveType = PRIMITIVE_TRIANGLE;
            triangleData.borderMode = static_cast<uint32_t>(BorderMode::OUTLINE);
            triangleData.zIndex = zIndex + 1;

            ctx.geometry->submit(triangleData);
        }

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
                    float contentIndent = indent + (showDisclosureTriangles ? disclosureTriangleSize : 0.0f);
                    cellX += contentIndent;
                    cellWidth -= contentIndent;
                }

                drawable->position = UDim2::fromOffset(cellX, rowY);
                drawable->size = UDim2::fromOffset(cellWidth, effectiveRowHeight);

                drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
                drawable->draw(ctx);
            }
        }

        visibleRowIndex++;
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
