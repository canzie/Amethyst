# TreeView

Hierarchical tree component with collapsible rows and optional columns. Like `Table` but with nesting — rows can have child rows that expand/collapse.

## Basic usage

```cpp
UIScope(parent).treeView(
    {.size = UDim2::fromScale(1.0f, 1.0f)},
    {.rowHeight = 22.0f, .indentPerLevel = 18.0f},
    [](TreeViewScope &tv) {
        tv.columns({
            {"Name", 0.5f},
            {"Type", 0.3f},
            {"Value", 0.2f},
        });

        tv.row([](TreeRowScope &r) {
            r.cell([](UIScope &c) { c.textLabel({}, {.text = "Scene"}); })
             .cell([](UIScope &c) { c.textLabel({}, {.text = "Root"}); })
             .cell({});

            r.row([](TreeRowScope &child) {
                child.cell([](UIScope &c) { c.textLabel({}, {.text = "Camera"}); })
                     .cell([](UIScope &c) { c.textLabel({}, {.text = "Object"}); })
                     .cell({});
            });
        });
    });
```

Lambda nesting mirrors the tree structure. No row indices or begin/end pairs needed during construction.

## Columns

Defined once via `columns({...})` before any rows. Each column has a header string, a weight (normalized), and a sizing mode. `STRETCH` columns are resizable in the future; `FIXED` columns are not. Headers are rendered as a pinned row above the data if `showHeader` is set in properties.

If no `columns()` call is made, columns are inferred from the cell count of the first row (anonymous STRETCH columns, no headers).

## Row handles

`addRow()` (and the scope's `row()`) returns a `TreeRowHandle` — a generational handle that stays valid until `removeRow()` is called. Capturing a handle for later use:

```cpp
TreeRowHandle playerHandle;
tv.row([&](TreeRowScope &r) {
    playerHandle = r.handle;
    r.cell(...).cell(...).cell({});
});

// later:
treeView->selectedRow = playerHandle;
treeView->removeRow(playerHandle);
```

Handles are generational — after `removeRow`, the handle becomes invalid and will assert if used.

## Cells

Each cell is wrapped in a `Container` by the scope builder (same as `Table`). The container propagates all events so the tree's hit logic is not blocked. Pass a null lambda (`{}`) for an empty cell.

Cells in the first column are automatically indented based on the row's depth. Disclosure triangles are drawn as geometry in the first column's indented space.

## Hit testing

One `InvisibleButton` covers the entire data area. On click, the row is computed from the Y coordinate; if the click falls within the disclosure triangle zone (left edge of column 0, based on depth), the row is toggled. Otherwise `onRowClicked` fires. This means all rows share one constant-cost hit target regardless of row count.

Interactive cells (e.g. `TextInput`) sit at a higher z-index and consume their own clicks normally.

## Properties (`TreeViewProperties`)

| Field | Default | Notes |
|---|---|---|
| `rowHeight` | 24px | Height of each row |
| `indentPerLevel` | 16px | Additional X offset per depth level |
| `showDisclosureTriangles` | true | Whether to draw/handle collapse arrows |
| `disclosureTriangleSize` | 10px | Triangle glyph size |
| `disclosureTrianglePadding` | 4px | Padding around the triangle |
| `disclosureTriangleColor` | grey | Triangle fill color |
| `showHeader` | false | Render column headers |
| `headerHeight` | 28px | Height of header row |
| `headerColor` | dark grey | Header background color |
| `headerText` | `TextProperties` | Font/color for header labels |
| `showColumnSeparators` | false | Vertical lines between columns |
| `columnSeparatorWidth` | 1px | Separator line width |
| `columnSeparatorColor` | grey | Separator color |
| `rowBackgroundColor` | — | Default row background |
| `rowAlternateColor` | — | Alternating row tint (set alpha=0 to disable) |
| `rowHoverColor` | — | Hovered row background |
| `rowSelectedColor` | — | Selected row background |
| `fillRows` | true | Fill empty viewport space with alternating rows |
| `cellPadding` | 0 | Inner padding applied to each cell |

## Public API

```cpp
// Columns
void     addColumn(TreeColumn col);
void     setColumns(std::vector<TreeColumn> cols);
void     resizeColumns(uint32_t newCount);
uint32_t columnCount() const;

// Rows
TreeRowHandle addRow(TreeRowHandle parent = {});  // INVALID parent = root level
void          removeRow(TreeRowHandle row);
void          clear();
uint32_t      rowCount() const;

// Cells (cursor-based, mirrors Table)
Instance *nextCell(std::unique_ptr<Instance> child);
void      setCell(TreeRowHandle row, uint32_t col, std::unique_ptr<Instance> child);
Instance *getCell(TreeRowHandle row, uint32_t col) const;
void      setCursor(TreeRowHandle row, uint32_t col);

// Expansion
void toggle(TreeRowHandle row);
void expand(TreeRowHandle row);
void collapse(TreeRowHandle row);
void expandAll();
void collapseAll();
bool isExpanded(TreeRowHandle row) const;

// Queries
bool          hasChildren(TreeRowHandle row) const;
bool          isAncestorOf(TreeRowHandle ancestor, TreeRowHandle descendant) const;
TreeRowHandle firstRootRow() const;
TreeRowHandle handleOf(uint32_t index) const;

// Traversal
template <typename Fn> void forEachRow(Fn &&fn) const;
template <typename Fn> void forEachVisibleRow(Fn &&fn) const;

// State (public)
TreeRowHandle hoveredRow{};
TreeRowHandle selectedRow{};
std::function<void(TreeRowHandle)>       onRowClicked;
std::function<void(TreeRowHandle, bool)> onRowToggled;
```

## Design notes

- **Cells** are stored in a flat array `m_cells[row * columnCount + col]`, same as `Table`. Avoids the stale-index bug present in the old `firstCellIndex` design.
- **Visible rows** are maintained as a flat `m_visibleRows` vector (DFS order, only rows with all ancestors expanded). Expand/collapse splice or erase the affected subtree incrementally — no full rebuilds needed for interactive use.
- **Depth** is cached on each `TreeRow` struct, set once at link time. No per-frame O(depth) walks.
- **Single hit button** covers the full data area. Row and disclosure targets are computed from mouse Y/X coordinates at event time.
- TreeView is designed to live inside a `ScrollingFrame` — it does not manage its own scroll.
