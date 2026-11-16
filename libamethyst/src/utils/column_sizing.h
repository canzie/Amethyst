/*
 * Column width distribution shared by Table and TreeView
 */

#ifndef AMETHYST__COLUMN_SIZING_H
#define AMETHYST__COLUMN_SIZING_H

#include <algorithm>
#include <cstddef>
#include <vector>

namespace Amethyst {

/**
 * @brief Splits a width between columns in proportion to their weights, whatever their sizing (sizing only says
 * whether the user may resize a column). Every column is held inside its minWidth and maxWidth (0 = no maximum); a
 * column pinned by a bound hands the difference back to the others.
 * @param columns Column definitions with weight, minWidth and maxWidth
 * @param totalWidth The width to split
 * @param positions Receives columns.size() + 1 edges, starting at 0
 */
template <typename Column> void distributeColumnWidths(const std::vector<Column> &columns, float totalWidth, std::vector<float> &positions)
{
    const size_t count = columns.size();
    auto bound = [](const Column &col, float width) {
        if (col.maxWidth > 0.0f) {
            width = std::min(width, col.maxWidth);
        }
        return std::max(width, col.minWidth);
    };

    std::vector<float> widths(count, 0.0f);
    std::vector<bool> settled(count, false);
    float remaining = totalWidth;

    // each round settles at least one column or finishes, so this ends within count rounds
    for (size_t round = 0; round <= count; round++) {
        float weightTotal = 0.0f;
        bool anyOpen = false;
        for (size_t i = 0; i < count; i++) {
            if (!settled[i]) {
                weightTotal += std::max(columns[i].weight, 0.0f);
                anyOpen = true;
            }
        }
        if (!anyOpen) {
            break;
        }

        float available = std::max(remaining, 0.0f);
        bool pinned = false;
        for (size_t i = 0; i < count; i++) {
            if (settled[i]) {
                continue;
            }
            float share = weightTotal > 0.0f ? available * std::max(columns[i].weight, 0.0f) / weightTotal : 0.0f;
            float clamped = bound(columns[i], share);
            widths[i] = share;
            if (clamped != share) {
                widths[i] = clamped;
                settled[i] = true;
                remaining -= clamped;
                pinned = true;
            }
        }
        if (!pinned) {
            break;
        }
        // the shares handed out this round assumed the pinned columns took theirs, so redo the rest
    }

    positions.clear();
    positions.reserve(count + 1);
    float x = 0.0f;
    positions.push_back(x);
    for (float w : widths) {
        x += w;
        positions.push_back(x);
    }
}

} // namespace Amethyst

#endif // AMETHYST__COLUMN_SIZING_H
