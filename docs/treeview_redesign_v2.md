# TreeView Redesign v2

## Premise

TreeView is Table plus exactly two things:

1. Rows can be collapsed/expanded (a collapsed row hides all of its descendants).
2. Column 0 content is indented by the row's depth, and a disclosure icon sits in
   that indent region for rows that have children.

Everything else (columns, weights, flat cell storage, headers, separators, row
backgrounds, hover/select, culling) is Table behavior. The redesign exists because
the current implementation reinvents all of that with full LCRS topology,
generation handles, slot recycling, per-row dirty propagation, and a DFS that
runs inside the draw loop. That is the wrong altitude. We want the simplest thing
that mirrors Table.

The guiding decision: **do not store a tree at all in pointer form. Store rows in
build order with a `depth` field, and derive the "tree" from contiguous depth
runs.** This is how every fast tree widget (ImGui-style, file explorers) actually
works. It collapses 90% of the current code.

---

## 1. Data model

A row needs the bare minimum:

```cpp
struct TreeRow {
    uint16_t depth = 0;      // 0 = root level
    bool expanded = true;    // only meaningful if it has children
    bool hasChildren = false;// derived at build time, cached
};
```

That is it. No parent/firstChild/lastChild/sibling links, no generation, no
`alive`, no handles.

**Why no LCRS?** Rows are added in depth-first build order (the scope API forces
this: you open a row, then open its children inside it). In DFS order, a node's
children are exactly the contiguous run of following rows whose depth is greater
than the node's depth, stopping at the first row with depth `<=` the node's depth.
That single invariant gives us:

- "does row `i` have children" -> `rows[i+1].depth > rows[i].depth` (cache it as
  `hasChildren` at build time).
- "the subtree of row `i`" -> the half-open range `[i+1, j)` where `j` is the first
  index `> i` with `depth <= rows[i].depth`.
- depth, ancestry, traversal -> all fall out of the depth array.

**Stable indices vs handles/generations?** Stable integer indices into a vector,
period. `selectedRow`/`hoveredRow`/`onRowClicked` all speak plain `uint32_t`
indices, same as Table. We drop generations and the `TreeRowHandle` struct
entirely. Generations only matter if rows are individually removed and external
code holds stale references across removals; we are not supporting that (see §4).
Build the tree, clear it, rebuild. Indices are valid for the lifetime of one build.

**Minimum viable row struct:** the three fields above. `depth` is set by the
build cursor; `hasChildren` is fixed up after the build completes (or lazily on
first draw); `expanded` defaults true and is toggled by the user.

---

## 2. Collapse/expand and visible-row tracking

Two layers: the full row list (build order, never reordered) and a **visible
plan** — a flat `std::vector<uint32_t>` of row indices in display order, top to
bottom, containing only rows whose ancestors are all expanded.

**Maintain the plan incrementally? No — rebuild it on dirty.** A full rebuild of
the visible plan is a single linear scan with a skip:

```cpp
void rebuildVisiblePlan() {
    m_visible.clear();
    for (uint32_t i = 0; i < m_rows.size(); ) {
        m_visible.push_back(i);
        if (!m_rows[i].expanded && m_rows[i].hasChildren) {
            // skip the entire subtree: advance past all deeper rows
            uint16_t d = m_rows[i].depth;
            uint32_t j = i + 1;
            while (j < m_rows.size() && m_rows[j].depth > d) j++;
            i = j;
        } else {
            i++;
        }
    }
}
```

This is O(total rows) but it is a flat scan over a POD array with no allocation
beyond the one vector — on the order of microseconds for tens of thousands of
rows. It runs only when something actually changes (toggle, add, clear), not per
frame. **Collapse hides all descendants for free**: the skip jumps the cursor `j`
past the whole subtree using only `depth` comparisons — no tree walk, no
recursion, no per-descendant dirty flags. This is the single biggest simplification
over the current code, which recursively marks every descendant dirty on toggle.

An incremental splice (insert/remove a subtree's worth of indices at the toggle
point) is possible and is strictly an optimization. **Reject it for v1.** The flat
rebuild is already fast enough and is trivially correct; incremental splicing
introduces index-shift bugs for no measurable gain at our scale. Revisit only if
profiling on a real 100k-row tree says so.

`m_visible.size()` is the total displayed row count; `m_visible[k]` maps visual
position `k` -> logical row index. This is the tree analogue of Table's
`m_displayOrder`.

---

## 3. Culling

Identical strategy to the current code's good part, but driven by the visible
plan instead of a live DFS.

All visible rows have uniform height `rowHeight`, so visual position `k` is at
`y = headerHeight + k * rowHeight`. Given the clip rect (the on-screen viewport,
already computed via `computeChildClipRect()` and reflecting the parent
ScrollingFrame's scroll), the first and last visible visual indices are pure
arithmetic:

```cpp
float scrolledY  = max(0, clip.y - absolutePosition.y - headerHeight);
uint32_t first   = floor(scrolledY / rowHeight);
uint32_t count   = ceil(viewportHeight / rowHeight) + 1;   // +1 for partial row
uint32_t last    = min(first + count, m_visible.size());
```

Entering the window is **O(1)** — a divide, not a scan — because `m_visible` is a
flat random-access array. The draw loop iterates `k` from `first` to `last` and
pulls `logicalRow = m_visible[k]`. Rows outside `[first, last)` are never touched.

This is the reason the visible plan must be a flat vector and not a lazy DFS: a
DFS cannot O(1)-seek to "the 4,000th visible row." The current code pays for this
by running the DFS every frame and counting rows it then throws away.

Interaction with the plan: the plan defines the universe of drawable rows and
their order; culling just picks a contiguous sub-range of it. The two compose
cleanly with zero shared state.

---

## 4. Cell storage

Mirror Table exactly: one flat `std::vector<Instance*> m_cells`, cell `(row, col)`
at `m_cells[row * numCols + col]`, with the actual instances owned in
`m_children`. Cells are addressed by **logical row index**, never by visual
position, so collapse/expand and culling never touch cell storage.

**Row removal / slot reuse: do not support it.** The API is `addRow` (append-only
during a build) and `clear` (wipe everything). No `removeRow`, no freelist, no
generations. This is the same posture Table *could* take; for a tree it is the
right one because:

- Trees are built from a data model each time it changes (filesystem scan, scene
  graph snapshot). The natural update is "clear and rebuild," which is cheap.
- Supporting mid-tree removal is what forces LCRS links, freelists, generation
  counters, and the entire fragile apparatus we are deleting.

Because there is no removal, `m_cells` is a simple append-only flat array with no
holes. `clear()` empties `m_children`, `m_cells`, `m_rows`, and `m_visible`. Done.

(If a caller needs to remove one node, they `clear()` and rebuild. If that ever
becomes a real performance problem we add removal then — but only then, and the
flat-array + rebuild model makes it a contained change.)

---

## 5. Hit detection

Three hittable concerns per visible row. Each maps to recycled, window-sized pools
(NOT one-per-logical-row):

1. **Disclosure icon** — toggles expand/collapse. Only present when
   `hasChildren`. Use a pool of `InvisibleButton` (or `TextButton`, but
   InvisibleButton is the honest choice: it is transparent and exists purely to
   catch clicks; we draw the chevron glyph separately or set the button's own
   image). Pool size = visible window size (`count`), not row count. Each frame,
   button `p` is positioned over the disclosure region of visual row `first + p`,
   its `onClick` captures the **logical** row index for that slot, and it is made
   non-interactable/invisible for rows without children.

2. **Row background / hover / select** — a pool of `Frame`, same window size.
   Frame `p` is the full-width background strip for visual row `first + p`, colored
   by select > hover > alternate > base (same precedence as Table::drawRow). Its
   hit handler sets `selectedRow`/`hoveredRow` to the logical index and fires
   `onRowClicked`. This is exactly Table's `m_rowBackgrounds` mechanism.

3. **Cell content** — the user-supplied instances in `m_cells`. Already in
   `m_children`, already hittable via the base machinery. Interactive cells
   (buttons) work for free because they are real children positioned each frame.

**Staying in sync with the window:** because both pools are window-sized and
re-bound every draw to `m_visible[first + p]`, there is no per-row state to keep
coherent. `getHittableInstances()` returns the two pools plus `m_children`,
mirroring Table. The pools only need `ensureCapacity(count)` growth, never
shrinking. Unused pool slots (when fewer rows than the window) are hidden +
made non-interactable, exactly as Table/the current code already do.

Key change from current code: pools are sized to the **visible window** (~20–40),
not to logical row count and not to `slotCount` derived from a possibly-bogus
clip. One disclosure button and one background frame per on-screen row.

---

## 6. Draw loop overview

State is split into "rebuild on FLAG_DIRTY" vs "recompute every draw" (cheap,
position-dependent), matching Table.

On `draw(ctx)`:

1. If `absolutePosition`/`absoluteSize` changed since last frame, set FLAG_DIRTY
   (needed because a scrolling parent moves us without dirtying us — same trick
   the current code uses, keep it).
2. If FLAG_DIRTY:
   - `fixupHasChildren()` if the row list changed since last fixup (cheap flat
     pass; or do it once at end of build).
   - `rebuildVisiblePlan()` (§2).
   - `rebuildColumnPositions()` (copy Table).
   - `updateSeparators()` (copy Table).
   - resolve cell padding.
3. Every draw (these are O(window), not O(rows)):
   - compute `childClip`, `rowHeight`, and the cull window `[first, last)` (§3).
   - `ensureRowPoolCapacity(count)` for backgrounds and disclosure buttons.
   - draw header (copy Table::drawHeader verbatim).
   - draw separators (copy Table).
   - for `k` in `[first, last)`: `drawRow(m_visible[k], poolSlot = k - first, ...)`.
   - hide leftover pool slots (backgrounds + disclosure buttons) beyond the used
     count.
4. Clear FLAG_DIRTY | FLAG_CHILD_DIRTY.

`drawRow` is Table::drawRow plus two deltas (see §8): indent column 0 by
`depth * indentPerLevel + disclosureWidth`, and position+show the pooled
disclosure button when `hasChildren`.

**Recycling:** pools are `std::vector<std::unique_ptr<Frame>>` and
`std::vector<std::unique_ptr<InvisibleButton>>`, grown by `ensureCapacity`, never
freed until destruction — identical ownership model to Table's `m_rowBackgrounds`.
No object pool abstraction, no per-row unique_ptrs. Cell instances live in
`m_children`.

What is incremental: nothing needs to be. The expensive structural work
(`rebuildVisiblePlan`) is gated behind FLAG_DIRTY and is a flat scan; the
per-frame work is already culled to the window. Resist adding incremental plan
splicing (§2).

---

## 7. Scope API shape

Mirror `TableScope` / `TableRowScope`, adding nesting. The build cursor is a depth
stack: opening a row pushes depth, closing pops it. Children declared inside a
row's callback become child rows.

```cpp
struct TreeRowScope {
    TreeView &component;
    std::vector<std::function<void(UIScope &)>> m_pendingCells;
    std::vector<std::function<void(TreeRowScope &)>> m_pendingChildren;
    explicit TreeRowScope(TreeView &t);

    TreeRowScope &cell(std::function<void(UIScope &)> fn);          // like TableRowScope::cell
    TreeRowScope &row(std::function<void(TreeRowScope &)> fn);      // a CHILD row
};

struct TreeViewScope : UIScope {
    TreeView &component;
    bool m_columnsExplicit = false;
    explicit TreeViewScope(TreeView &tv);
    TreeViewScope &column(std::string header, float weight, ...);   // like TableScope::column
    TreeViewScope &row(std::function<void(TreeRowScope &)> fn);     // a ROOT row
};
```

Mapping to the component:

- `TreeView::addRow(uint16_t depth)` appends a `TreeRow{depth}` and moves the
  build cursor; `nextCell(unique_ptr)` writes the next cell at the cursor, padding
  the row to `numCols`. (Same shape as Table's `addRow`/`nextCell`.)
- The scope realizes a row by: collecting `m_pendingCells` and `m_pendingChildren`
  in the callback, then `addRow(currentDepth)`, emitting cells (wrapped in a
  `Container` sized to fill, exactly like `TableScope::row`), then recursing into
  each pending child with `currentDepth + 1`.

Recursion (`TreeRowScope::row` -> child `TreeRowScope`) is what produces the
depth-first build order that §1's depth-run model relies on. The depth is implicit
in the nesting structure; callers never compute parent indices. This deletes the
demo's entire `beginRow(parent)/endRow()` ceremony and the manual
`numCols`/`columnWeights` poking — those become `column(...)` calls and nested
`row(...)` blocks, reading like the Table demo.

A lower-level imperative API (`addRow(depth)` + `nextCell`) stays public for
callers that build from their own iterative traversal and don't want the lambda
nesting; the scope is sugar on top.

---

## 8. What changes vs. Table

TreeView should be *almost* a copy of Table. Concretely:

**Same as Table (copy/share):**
- Flat cell storage `row * numCols + col`, `nextCell`/`getCell`/`setCursor`.
- Column definitions, weights, `rebuildColumnPositions`, `updateSeparators`.
- Header rendering (`drawHeader`) and header labels.
- Row background pool, hover/select coloring precedence, alternate rows.
- `hoveredRow`/`selectedRow`/`onRowClicked` as plain indices.
- `getHittableInstances` = pools + children.
- FLAG_DIRTY gating; position-change-forces-dirty trick.
- Culling math (Table doesn't cull today, but the same window arithmetic applies).

**Added by TreeView (the entire delta):**
1. `TreeRow{depth, expanded, hasChildren}` per row, in DFS build order.
2. The **visible plan** `m_visible` and `rebuildVisiblePlan()` (the depth-run skip)
   replacing Table's `m_displayOrder` (Table's display order is a sort permutation;
   the tree's is a collapse filter — same role, different builder).
3. Column-0 indent: shift cell 0's x by `depth * indentPerLevel + disclosureRegion`
   and shrink its width correspondingly.
4. A disclosure-button pool + chevron, drawn/positioned only for `hasChildren`
   rows, wired to `toggle(logicalRow)`.
5. `toggle/expand/collapse/expandAll/collapseAll` -> flip `expanded`, mark dirty.
   No descendant walk (the plan rebuild handles hiding).

**Removed vs. current TreeView (the point of the rewrite):**
- LCRS links (parent/child/sibling), `m_firstRoot`/`m_lastRoot`, `linkRowToParent`,
  `unlinkRow`.
- Generations, `TreeRowHandle`, `alive`, freelist, `allocateSlot`/`freeSlot`.
- Per-row `isDirty` and recursive `markRowDirty`.
- In-draw DFS (`drawVisibleRows` with the visible/ancestor-visible stack).
- `removeRow` and slot reuse (replaced by clear+rebuild).
- `firstCellIndex` per row (cells are addressed by `row * numCols + col`).

Net effect: the ~1000-line LCRS implementation collapses to roughly Table-sized
code plus a ~15-line plan builder and a per-row indent/disclosure tweak in the
draw inner loop.

---

## Open questions (decide during implementation, not blocking)

- **Should TreeView subclass Table or duplicate it?** Inheritance is tempting but
  Table's `draw`/`drawRow` are not parameterized for indent/disclosure and its
  `m_displayOrder` semantics differ. Recommend: factor the shared cell-grid +
  column + header logic into a small base or free helpers, but if that fights the
  existing Table, just duplicate `drawHeader`/`rebuildColumnPositions`/separators.
  Duplication of ~80 lines beats a leaky base class. Lean toward duplicate-then-
  extract-if-it-hurts.
- **Disclosure glyph:** chevron via font glyph, SVG, or rotated triangle. Current
  code rotates a TextButton 90°. Simplest correct: an `ImageLabel`/SVG chevron for
  visuals layered under the transparent `InvisibleButton` for hits, or one
  `TextButton` with a chevron glyph if the font has one. Visual-only detail.
- **`hasChildren` fixup timing:** set it at end of build (scope knows) vs. lazily
  before first plan rebuild. End-of-build is cleaner; the imperative API needs a
  `finishBuild()`/it's computed in the first dirty `rebuildVisiblePlan` pass.
