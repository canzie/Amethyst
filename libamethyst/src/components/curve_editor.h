/**
 * @file curve_editor.h
 * @brief Composite graph editor
 */

#ifndef AMETHYST__CURVE_EDITOR_H
#define AMETHYST__CURVE_EDITOR_H

#include "components/frame.h"

#include "math/math.h"
#include <string>
#include <vector>

namespace Amethyst {

class Spline;
class TreeView;
class TextLabel;

class CurveEditor : public Frame {
  public:
    CurveEditor();

    /**
     * @brief Add a curve to the plot and a leaf row to the tree.
     * @param name Display name shown in the tree and toolbar
     * @param color Curve stroke colour
     * @param knots Curve points in plot-local pixel coordinates
     * @param group Optional group the curve is nested under; empty places it at the root
     * @return The created Spline
     */
    Spline *addCurve(const std::string &name, Color4 color, std::vector<vec2> knots, const std::string &group = "");
    void selectCurve(size_t index);

  private:
    struct CurveEntry {
        Spline *spline;
        std::string name;
        std::string group;
        Color4 color;
    };

    void buildLayout();
    void buildGrid();
    void rebuildTree();
    void refreshSelection();

    Frame *m_side = nullptr;
    Frame *m_toolbar = nullptr;
    Frame *m_plot = nullptr;
    Frame *m_inspector = nullptr;
    TreeView *m_tree = nullptr;
    TextLabel *m_title = nullptr;

    std::vector<CurveEntry> m_curves;
    std::vector<int> m_rowToCurve;
    std::vector<uint32_t> m_curveToRow;
    size_t m_selected = 0;
    float m_dimAlpha = 0.26f;
};

} // namespace Amethyst

#endif // AMETHYST__CURVE_EDITOR_H
