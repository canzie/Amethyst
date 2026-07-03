#include "components/curve_editor.h"

#include "components/spline.h"
#include "components/text_label.h"
#include "components/tree_view.h"

#include <algorithm>
#include <memory>

namespace Amethyst {

static void s_place(UIObject *o, UDim2 position, UDim2 size)
{
    BaseProperties b{};
    b.position = position;
    b.size = size;
    o->setBaseProperties(b);
}

static void s_fill(UIObject *o, Color3 color, float transparency = 0.0f)
{
    BaseStyleProperties s{};
    s.backgroundColor = color;
    s.backgroundTransparency = transparency;
    o->setBaseStyleProperties(s);
}

static std::unique_ptr<TextLabel> s_cellLabel(const std::string &text, Color4 color)
{
    auto label = std::make_unique<TextLabel>();

    BaseProperties b{};
    b.interactable = false;
    b.size = UDim2::fromScale(1.0f, 1.0f);
    label->setBaseProperties(b);

    BaseStyleProperties s{};
    s.backgroundTransparency = 1.0f;
    label->setBaseStyleProperties(s);

    TextStyleProperties t{};
    t.fontSize = 12.0f;
    t.textColor = color;
    t.textYAlignment = TextYAlignment::CENTER;
    label->setTextStyleProperties(t);

    label->setText(text);
    return label;
}

CurveEditor::CurveEditor()
{
    s_fill(this, Color3::fromHex(0x181818));
    buildLayout();
}

void CurveEditor::buildLayout()
{
    m_side = add<Frame>();
    s_place(m_side, UDim2::fromOffset(0.0f, 0.0f), UDim2(0.0f, 268.0f, 1.0f, 0.0f));
    s_fill(m_side, Color3::fromHex(0x1f1f1f));

    Frame *header = m_side->add<Frame>();
    s_place(header, UDim2::fromOffset(0.0f, 0.0f), UDim2(1.0f, 0.0f, 0.0f, 30.0f));
    s_fill(header, Color3::fromHex(0x232323));
    TextLabel *headerLabel = header->add<TextLabel>();
    s_place(headerLabel, UDim2::fromOffset(12.0f, 0.0f), UDim2(1.0f, -12.0f, 1.0f, 0.0f));
    s_fill(headerLabel, Color3::fromHex(0x232323), 1.0f);
    {
        BaseProperties b{};
        b.interactable = false;
        b.position = UDim2::fromOffset(12.0f, 0.0f);
        b.size = UDim2(1.0f, -12.0f, 1.0f, 0.0f);
        headerLabel->setBaseProperties(b);
        TextStyleProperties t{};
        t.fontSize = 12.0f;
        t.textColor = Color4(0.78f, 0.78f, 0.82f, 1.0f);
        t.textYAlignment = TextYAlignment::CENTER;
        headerLabel->setTextStyleProperties(t);
        headerLabel->setText("Curves");
    }

    m_tree = m_side->add<TreeView>();
    s_place(m_tree, UDim2::fromOffset(0.0f, 30.0f), UDim2(1.0f, 0.0f, 1.0f, -30.0f));
    {
        TreeViewStyleProperties tp{};
        tp.rowHeight = 24.0f;
        tp.indentPerLevel = 16.0f;
        tp.rowBackgroundColor = Color4::fromHex(0x1f1f1f);
        tp.rowHoverColor = Color4(0.20f, 0.28f, 0.40f, 1.0f);
        tp.rowSelectedColor = Color4(0.20f, 0.36f, 0.55f, 1.0f);
        tp.fillRows = true;
        m_tree->setTreeViewProperties(tp);
    }
    m_tree->addColumn({.header = "Name", .sizing = TreeColumnSizing::STRETCH, .weight = 1.0f});
    m_tree->onRowClicked = [this](uint32_t row) {
        if (row < m_rowToCurve.size() && m_rowToCurve[row] >= 0) {
            selectCurve(static_cast<size_t>(m_rowToCurve[row]));
        }
    };

    Frame *main = add<Frame>();
    s_place(main, UDim2(0.0f, 268.0f, 0.0f, 0.0f), UDim2(1.0f, -268.0f, 1.0f, 0.0f));
    s_fill(main, Color3::fromHex(0x181818), 1.0f);

    m_toolbar = main->add<Frame>();
    s_place(m_toolbar, UDim2::fromOffset(0.0f, 0.0f), UDim2(1.0f, 0.0f, 0.0f, 42.0f));
    s_fill(m_toolbar, Color3::fromHex(0x232323));
    m_title = m_toolbar->add<TextLabel>();
    {
        BaseProperties b{};
        b.interactable = false;
        b.position = UDim2::fromOffset(14.0f, 0.0f);
        b.size = UDim2(1.0f, -14.0f, 1.0f, 0.0f);
        m_title->setBaseProperties(b);
        s_fill(m_title, Color3::fromHex(0x232323), 1.0f);
        TextStyleProperties t{};
        t.fontSize = 13.0f;
        t.textColor = Color4(0.90f, 0.90f, 0.92f, 1.0f);
        t.textYAlignment = TextYAlignment::CENTER;
        m_title->setTextStyleProperties(t);
        m_title->setText("No curve selected");
    }

    m_plot = main->add<Frame>();
    s_place(m_plot, UDim2(0.0f, 0.0f, 0.0f, 42.0f), UDim2(1.0f, 0.0f, 1.0f, -88.0f));
    s_fill(m_plot, Color3::fromHex(0x141414));

    m_inspector = main->add<Frame>();
    s_place(m_inspector, UDim2(0.0f, 0.0f, 1.0f, -46.0f), UDim2(1.0f, 0.0f, 0.0f, 46.0f));
    s_fill(m_inspector, Color3::fromHex(0x1f1f1f));
    TextLabel *insLabel = m_inspector->add<TextLabel>();
    {
        BaseProperties b{};
        b.interactable = false;
        b.position = UDim2::fromOffset(14.0f, 0.0f);
        b.size = UDim2(1.0f, -14.0f, 1.0f, 0.0f);
        insLabel->setBaseProperties(b);
        s_fill(insLabel, Color3::fromHex(0x1f1f1f), 1.0f);
        TextStyleProperties t{};
        t.fontSize = 12.0f;
        t.textColor = Color4(0.5f, 0.5f, 0.53f, 1.0f);
        t.textYAlignment = TextYAlignment::CENTER;
        insLabel->setTextStyleProperties(t);
        insLabel->setText("Select a curve, then drag its points to edit.");
    }

    buildGrid();
}

void CurveEditor::buildGrid()
{
    const float fracs[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (float f : fracs) {
        Color3 color = (f == 0.5f) ? Color3::fromHex(0x38383a) : Color3::fromHex(0x2a2a2a);

        Frame *vertical = m_plot->add<Frame>();
        {
            BaseProperties b{};
            b.interactable = false;
            b.position = UDim2(f, 0.0f, 0.0f, 0.0f);
            b.size = UDim2(0.0f, 1.0f, 1.0f, 0.0f);
            vertical->setBaseProperties(b);
        }
        s_fill(vertical, color);

        Frame *horizontal = m_plot->add<Frame>();
        {
            BaseProperties b{};
            b.interactable = false;
            b.position = UDim2(0.0f, 0.0f, f, 0.0f);
            b.size = UDim2(1.0f, 0.0f, 0.0f, 1.0f);
            horizontal->setBaseProperties(b);
        }
        s_fill(horizontal, Color3::fromHex(0x2a2a2a));
    }
}

Spline *CurveEditor::addCurve(const std::string &name, Color4 color, std::vector<vec2> knots, const std::string &group)
{
    Spline *spline = m_plot->add<Spline>();
    s_place(spline, UDim2::fromOffset(0.0f, 0.0f), UDim2::fromScale(1.0f, 1.0f));
    {
        SplineStyleProperties sp{};
        sp.type = CurveType::CATMULL_ROM;
        sp.thickness = 2.5f;
        sp.color = color;
        spline->setSplineProperties(sp);
    }
    spline->setKnots(std::move(knots));

    m_curves.push_back({spline, name, group, color});
    rebuildTree();
    refreshSelection();
    return spline;
}

void CurveEditor::rebuildTree()
{
    m_tree->clear();
    m_rowToCurve.clear();
    m_curveToRow.assign(m_curves.size(), 0);

    std::vector<std::string> groups;
    for (const CurveEntry &c : m_curves) {
        if (std::find(groups.begin(), groups.end(), c.group) == groups.end()) {
            groups.push_back(c.group);
        }
    }

    auto recordRow = [this](uint32_t row, int curve) {
        if (row >= m_rowToCurve.size()) {
            m_rowToCurve.resize(row + 1, -1);
        }
        m_rowToCurve[row] = curve;
    };

    for (const std::string &group : groups) {
        uint16_t curveDepth = 0;
        if (!group.empty()) {
            uint32_t row = m_tree->addRow(0);
            m_tree->nextCell(s_cellLabel(group, Color4(0.85f, 0.75f, 0.45f, 1.0f)));
            recordRow(row, -1);
            curveDepth = 1;
        }
        for (size_t i = 0; i < m_curves.size(); i++) {
            if (m_curves[i].group != group) {
                continue;
            }
            uint32_t row = m_tree->addRow(curveDepth);
            m_tree->nextCell(s_cellLabel(m_curves[i].name, m_curves[i].color));
            recordRow(row, static_cast<int>(i));
            m_curveToRow[i] = row;
        }
    }
}

void CurveEditor::refreshSelection()
{
    const Color3 plotBg = Color3::fromHex(0x141414);

    for (size_t i = 0; i < m_curves.size(); i++) {
        bool selected = (i == m_selected);

        // Dim by blending toward the plot background at full opacity, not by lowering alpha: overlapping
        // segment caps at each knot would otherwise double up and read as dots on the transparent curve.
        Color4 color = m_curves[i].color;
        if (!selected) {
            color = Color4(plotBg.r + (color.r - plotBg.r) * m_dimAlpha, plotBg.g + (color.g - plotBg.g) * m_dimAlpha,
                           plotBg.b + (color.b - plotBg.b) * m_dimAlpha, 1.0f);
        }

        SplineStyleProperties sp{};
        sp.color = color;
        sp.showKnots = selected;
        m_curves[i].spline->setSplineProperties(sp);

        BaseProperties b{};
        b.zIndex = selected ? 2 : 1;
        m_curves[i].spline->setBaseProperties(b);
    }

    if (m_title != nullptr) {
        m_title->setText(m_selected < m_curves.size() ? m_curves[m_selected].name : std::string("No curve selected"));
    }

    if (m_tree != nullptr && m_selected < m_curveToRow.size()) {
        m_tree->selectedRow = static_cast<int32_t>(m_curveToRow[m_selected]);
        m_tree->markDirty();
    }
}

void CurveEditor::selectCurve(size_t index)
{
    if (index >= m_curves.size()) {
        return;
    }
    m_selected = index;
    refreshSelection();
}

} // namespace Amethyst
