# What a Text Editor Needs From Amethyst

Recon, 2026-08-10. What is missing for someone to build a genuinely fast text editor on
Amethyst, what each gap costs, and where the line between library and application sits.

Supersedes the "TextView component" framing in `text_editor_plan.md` on the widget question.
The virtualization mechanics in that document still stand and are referenced rather than
repeated here.

## Verdict: ship tools, not a widget

Amethyst should provide **positioned, colored glyph runs; batched rects; measurement; and
viewport clipping**. It should not own the editor.

Everything that makes an editor an editor is opinionated and application-shaped: the text
storage (rope, gap buffer, piece table), undo/redo, the highlighting engine (tree-sitter or
otherwise), multi-cursor semantics, keybindings, IME and completion UI. Any of those baked
into the library becomes a constraint that a real editor eventually has to fight.

The dividing line that works: **Amethyst never holds the document.** It holds
viewport-proportional GPU-facing data and the primitives to fill it. The application decides
what text exists, what colour each span is, and where the cursors are; it hands Amethyst a
description of what is currently on screen.

That also keeps the memory story clean, which is the crux of the question below.

## Prior art worth copying

Checked against two implementations rather than invented. Neovim is terminal/cell-grid based
so its *rendering* does not transfer, but its data model does; RAD is doing the same job we
are.

**Neovim, `src/nvim/grid_defs.h`** ([source](https://github.com/neovim/neovim/blob/master/src/nvim/grid_defs.h)):

- The grid keeps **parallel arrays**, not fat cells: `chars[]` (`schar_T`), `attrs[]`
  (`sattr_T`, the highlight attribute per cell), `vcols[]`. Attributes live beside the
  characters rather than inside them. This is independent confirmation of the colour approach
  in Gap 1 — nobody widens the per-cell record to carry styling.
- `line_offset[n]` is the offset into those arrays for row `n`, and *"full screen scrolling is
  implemented by rotating the offsets in the line_offset array"*. Rows are never copied; an
  indirection table is permuted.
- `dirty_col` per line records the last column drawn, so only changed spans are redrawn and
  transmitted.

**We already have `line_offset`.** `GlyphSlice` is exactly that indirection — a stable handle
to a slice's current location, read once per instance by the vertex shader
(`glyph_buffer.h:43`). Scrolling can rotate slice entries and leave glyph data untouched.

**RAD Debugger, `src/font_cache/font_cache.h`** ([source](https://github.com/EpicGames/raddebugger/blob/master/src/font_cache/font_cache.h)):

- Three cache tiers: font handle -> style raster cache -> **run cache**. The style node is
  keyed on `style_hash` plus size and flags, and caches `ascent`, `descent` and
  **`column_width`** — the monospace advance, cached per style, which is what Gap 4's O(1)
  lookup needs.
- A run is a whole string's arrangement: `FNT_Run { FNT_PieceArray pieces; Vec2F32 dim;
  F32 ascent; F32 descent; }`, each piece carrying texture coordinates, position and advance.
  Entire shaped strings are cached and reused, keyed by hash. **Measured and rejected for us**
  — see the run cache section below: it is an immediate-mode optimisation.
- The atlas uses a **quadtree region allocator** (`FNT_AtlasRegionNode` with per-quadrant
  occupancy and free space) rather than a skyline packer, so regions *can* be freed.
- Separate `permanent_arena` / `raster_arena` / `frame_arena` plus a `frame_index`, i.e.
  frame-scoped reclamation.

**Terminal emulators, the closest match to our case** (few faces, small size range, font-size
changes). Both were checked because RAD's quadtree suggested eviction was necessary. It is not:

- **Alacritty**, `alacritty/src/renderer/text/atlas.rs`
  ([source](https://github.com/alacritty/alacritty/blob/master/alacritty/src/renderer/text/atlas.rs)):
  shelf packing in horizontal rows (`row_extent`, `row_baseline`, `row_tallest`), glyphs placed
  left-to-right, next row starts at the tallest glyph's baseline. **Individual glyphs cannot be
  freed** — the only reset is `clear()` on the whole atlas. Exhaustion is handled by allocating
  *another* atlas and continuing there.
- **Ghostty**, `src/font/Atlas.zig`
  ([source](https://github.com/ghostty-org/ghostty/blob/main/src/font/Atlas.zig)): **skyline
  packing per Jukka Jylanki's paper — the same algorithm Amethyst already uses.** Always square,
  `grow()`s on exhaustion by copying pixel data and adding a node for the new space, keeps a
  1px border against sampling artifacts. Explicitly: *"Individual regions cannot be freed or
  evicted."* Only `clear()`.

**Read the terminals for code, not for policy.** Both get away without eviction because a
terminal renders one face-set at one size at a time: `clear()` costs them almost nothing and
has nothing else to damage. That workload is not ours, so their policy does not transfer even
though their packing code does. RAD is the comparable case — many faces, many sizes, zoomable,
general UI — and RAD is the one that built reclamation.

### Sizing the decision

| | px^2 |
|---|---|
| one face, ASCII, 14 px (~128 glyphs at ~9x16) | 18 K |
| live working set, 8 faces x 6 sizes | 885 K |
| 1024^2 atlas | 1.05 M |
| 20 zoom levels x 8 faces | 2.9 M |
| 2048^2 atlas | 4.2 M |

The steady-state working set nearly fills a 1024^2 page before any accents, punctuation or CJK.
Zoom pushes past it within a session.

Clear-all fails for two reasons specific to us:

1. **Collateral invalidation.** A zoom step in the editor would discard the toolbar's 14 px
   labels, which did not change. A terminal has no other component to damage.
2. **A visible hitch.** Everything visible re-rasterizes afterwards — a few hundred glyphs at
   ~10-50 us each is milliseconds, i.e. one janky frame per zoom step, on the interaction that
   most needs to feel smooth.

### What RAD actually does, verified from source

Checked out at `~/dev/reference/raddebugger` rather than inferred:

- **`fnt_atlas_region_release` has zero call sites.** Declared (`font_cache.h:259`), defined
  (`font_cache.c:355`), never called. The quadtree's freeing ability is dead code; RAD never
  evicts a glyph.
- Atlas selection is **first-fit across a linked list**, not keyed by font or size, so every
  atlas holds an interleaved mix — which is precisely why an atlas could never be freed.
- Fixed **1024x1024**, up to 64 atlases: a 64 MB R8 ceiling.
- `fnt_reset()` releases every atlas texture and clears `raster_arena`. Call sites:
  `fnt_init()`, **DPI change** (`raddbg_core.c:5819`), and **`IncWindowFontSize` /
  `DecWindowFontSize`** (`:14233`, `:14245`).

So RAD's answer to zoom is a **global clear and repopulate**, triggered by the font-size command
itself. No reference implementation evicts per glyph: Alacritty adds another atlas, Ghostty
grows a square one, RAD adds pages and resets wholesale.

### Measured comparison

`tools/atlas_sim.py` simulates five allocators on identical seeded workloads (uniform / ui /
zoom / editing / mixed), reporting footprint, ops and failures. Run it before revisiting any of
this.

Footprint is the metric that matters — atlas pixels actually consumed, including a shelf's
row-band slack and the gaps under a skyline frontier. Summed reserved area hides both.

At 1024^2, grouped arrival (interleaved is within noise, so arrival order is a non-factor):

| allocator | ui fp | zoom fp | mixed fp | ops/glyph | fails (zoom / mixed) |
|---|---|---|---|---|---|
| skyline | 0.18 (17%) | 0.77 (73%) | 1.04 (100%) | 366-420 | 0 / 906 |
| shelf | 0.25 (23%) | 1.02 (98%) | 1.05 (100%) | 1 | 6 / 1228 |
| quadtree | 0.42 (40%) | 0.92 (88%) | 0.94 (90%) | 26 | 7 / 326 |
| blockcell | 0.40 (38%) | 0.62 (59%) | 0.76 (73%) | 3-4 | 0 / 0 |

Conclusions:

- **Shelf is not free.** Identical rounding overhead to skyline but 32-39% more footprint from
  row-band slack, which is what makes it fail in `zoom` where skyline does not. Its O(1)
  placement would matter if allocation were per-frame; it is not — allocation happens once per
  (glyph, font, size) on a cache miss, so skyline's ~420 ops/glyph is a one-off warmup cost of
  ~1-2 ms for a whole session against a permanent memory penalty.
- **Quadtree is dominated.** 183-300% overhead *and* it still fails 324 times in `mixed` after
  3582 evictions, because sibling regions never coalesce so freed small cells cannot serve a
  larger glyph. Copying RAD here would import their waste without their memory budget.
- **blockcell (per-(font,size) chunk groups) is the only allocator that never fails**, and has
  the smallest footprint under pressure, at 3-4 ops/glyph. Its high rounding overhead is
  misleading precisely because it reclaims.
- **At 2048^2 nothing fails at all.** `mixed` at 2048^2 with the current packer is 36% footprint,
  zero failures.

### The extreme session is what decides it

A long session - 6 faces, 40 zoom steps across a 0.6x-4x ladder, 400 CJK plus 200 Latin-1
glyphs every other step - separates the options in a way the shorter scenarios do not:

| | pages | vram | fails | rasterisations |
|---|---|---|---|---|
| skyline, single 1024^2 | 1 | 1.0 MB | **31 207** | 7 166 |
| skyline, single 2048^2 | 1 | 4.2 MB | **11 629** | 13 692 |
| quadtree, single 2048^2 | 1 | 4.2 MB | 48 | 58 512 |
| shelf pages @1024, cap 32 | 10 | 10.5 MB | 0 | 17 568 |
| skyline pages @1024, cap 32 | 9 | 9.4 MB | 0 | 17 568 |
| **blockcell @1024** | **1** | **1.0 MB** | **0** | 58 560 |

- **A single atlas of any packer is untenable**: tens of thousands of dropped glyphs. Growth or
  eviction is mandatory, not a backstop.
- **Paging without eviction costs ~10x the memory**, because every size the session ever touched
  accumulates. At an 8-page ceiling the paged variants still failed (1685-2630), so 8 is too low
  for this workload.
- **Keyed groups with group-level eviction hold it in one committed page.**
- Quadtree still fails at 2048^2 after 29 264 evictions, at 172% overhead. Dead.

### Rounding overhead does not matter

An atlas page is committed VRAM the moment the texture is created: 1024^2 is 1 MB whether it is
40% or 95% occupied. Internal rounding waste inside an already-committed page therefore costs
nothing. **The only costs are pages committed and glyphs dropped.**

This is why the `over` column must not be used for ranking - it was the main argument against
blockcell (75-114%) and it is free. Rank on `vramMB` and `fails`.

### Correction: measured under the real policy, keyed groups lose

The table above ran blockcell confined to a single page, so eviction was its only response to
exhaustion. Re-measured with the policy we would actually ship - reactive reclamation only,
stale-group hysteresis, grow before evicting - the result inverts, on `extreme` at 1024^2 with a
32 page ceiling:

| | pages | vram | used% | rasterisations | fails |
|---|---|---|---|---|---|
| skyline pages + affinity | 9 | **9.4 MB** | 96% | **17 568** | 0 |
| shelf pages | 10 | 10.5 MB | 93% | 17 568 | 0 |
| blockcell | 13 | 13.6 MB | 65% | 42 264 | 0 |

Two findings:

- **"Rounding waste is free" holds only inside an already-committed page.** blockcell packs to
  65% where skyline reaches 96%, so its cell-grid rounding *causes four extra pages*. Once waste
  forces growth it is paid in VRAM directly.
- **Reactive eviction almost never fires.** Sweeping the stale threshold, `0` gives 2 pages but
  `30` through `never` are identical at 13 pages with **zero** stale evictions: reclamation only
  runs on an allocation failure, and with room to grow nothing fails. With safe hysteresis the
  policy degenerates to pure paging - and then the denser packer wins outright.

`stale 0` does reach 2.1 MB, but that is eviction of anything untouched this instant: 58 560
rasterisations of thrash, and unsafe against live runs.

### Decision: skyline pages with (font,size) affinity

- Keep the existing skyline packer, one 1024^2 page to start, grow on demand.
- **Pin each `(font, size)` to a page.** A batched run is single-font-single-size by
  construction, so its glyphs never span pages and it keeps one `textureId`; the backend is
  bindless and `textureId` is per instance, so multi-page costs nothing in draw calls. Affinity
  is free on density: 9.10 vs 9.13 Mpx footprint against unpinned paging.
- Worst case measured is 9.4 MB on a deliberately adversarial session (6 faces, 40 zoom steps,
  CJK). The `ui` and `zoom` scenarios both sit at **one page**.
- **Reclaim by clearing on font-config change**, RAD's trigger (`fnt_reset()` on DPI change and
  font-size commands): a zoom step kills whole size sets at once and collapses the page count.
  Event-driven, so it satisfies the reactive-only constraint.
- **Never drop a glyph silently.** Today `rasterizeGlyphInfo` logs and fails
  (`glyph_atlas.cpp:47`); a single atlas drops 31 171 glyphs on `extreme`. Order on a miss:
  grow a page, then clear-and-repopulate, then fail loudly.

### The run cache (RAD's FNT_Run): the shaper is allocation-bound, fix that first

Benchmarked against the real `TextProcessor` on 200k lines of neovim C
(`tools/run_cache_bench.cpp`), across granularity, cache scope and access pattern. A
persistent, token-granularity cache wins:

| pattern | baseline | token + LRU | hit% | mem |
|---|---|---|---|---|
| page, jump a screenful | 71.2 ms | **29.8 ms (0.42x)** | 96.6% | 810 KB |
| jitter, random jumps | 72.3 ms | **40.9 ms (0.57x)** | 94.7% | 835 KB |
| sweep, scroll one line | 1.8 ms | **1.2 ms (0.70x)** | 92.5% | 128 KB |
| redraw, full invalidation | 64.5 ms | **1.2 ms (0.02x)** | 99.9% | 19 KB |

Frame-scoped caching loses everywhere except `redraw` (1.31-1.42x), so RAD's per-frame scoping
is the wrong choice here; persistent with LRU is what pays.

**But the baseline is the finding.** 825 093 glyphs over ~27 900 line-shapes in 71.2 ms is
**2.55 us per line**, roughly 85 ns per character to do an advance lookup and emit a 16-byte
quad - which should be 2-5 ns. `s_shapeText` allocates a `vector<vector<ShapedGlyph>>`, a
`lineWidths` vector and a reserved scratch buffer, then `layoutTextBatched` fills its own two
output vectors: about **five heap allocations per line shaped**.

So most of the cache's win is skipping five mallocs, not skipping the glyph loop.
**Removing the allocations from the shaper is the better fix**: bigger (plausibly 10-20x on the
same measurement), no hashing, no hash map, no 800 KB, and it speeds up the uncached path,
which a cache cannot. Do that first, then re-run this bench to see whether the cache still
earns its place.

Two harness artifacts had previously hidden this, both since fixed: a modelled shaper that
excluded those allocations (making it ~4x too cheap) and a linear-scan LRU eviction.

### Rejected, with the measurements: quadtree, shelf, guillotine

RAD caches whole shaped strings by hash under the style node. Benchmarked on 200k lines of
neovim C with a model of our shaper's inner loop (`tools/run_cache_bench.cpp`), across
granularity (line/word/token/chunk), scope (per-frame/LRU) and access pattern, it loses
everywhere that matters:

| page pattern | glyphs shaped | hit% | ms | vs baseline |
|---|---|---|---|---|
| shape every exposed line | 1 544 223 | - | **6.2** | 1.00x |
| line, per-frame | 1 508 944 | 11.5% | 10.9 | 1.75x |
| word, per-frame | 1 172 023 | 67.9% | 22.7 | 3.65x |
| token, per-frame | 781 958 | **79.1%** | 29.3 | **4.70x** |

A 79% hit rate is 4.7x *slower*. Shaping is an advance lookup plus a 16-byte quad emit per
character; a cache lookup hashes the bytes (reading all of them anyway), probes a map, and
copies the quads to place the run at its origin. The bookkeeping exceeds the work avoided,
and finer granularity makes it worse - `token` has the best hit rate and the worst time.

**Why it works for RAD and not for us:** the one winning configuration is re-shaping the
*same* lines every frame (`redraw`, per-line, persistent: 99.9% hit, **0.51x**). That is what
an immediate-mode UI does every frame, and is consistent with RAD's run cache being
frame-scoped. Amethyst is retained-mode and already skips re-shaping unchanged text via dirty
flags and `TextLayoutState`, so the run cache addresses a cost our architecture does not pay.

Revisit only if shaping itself becomes expensive - real kerning, HarfBuzz, ligatures, BiDi -
since the cache pays exactly when shaping cost greatly exceeds hashing cost.

(Caveat on the numbers: LRU rows' times are dominated by a linear-scan eviction in the
harness and are not comparable; their hit rates are valid.)

### Rejected, with the measurements

- **Keyed chunk groups (blockcell)**: 13.6 MB and 2.4x rasterisation, above. Its win at 1.0 MB
  only appears when confined to one page with aggressive eviction.
- **Quadtree (RAD's)**: 183-300% overhead and still dropped glyphs after tens of thousands of
  evictions, because freed cells never coalesce for a differently sized glyph.
- **Shelf (Alacritty's)**: near-tie, not a rejection. On `extreme` it costs exactly one extra
  page (10.5 vs 9.4 MB, ~10%) and is 48x cheaper per allocation - 6 ops/glyph against 291.
  Both differences are immaterial: the 5 M-op gap is ~10-25 ms across a 40 step session, and
  1 MB is nothing. Shelf is also simpler, three integers against a node list with insertion,
  shrinking and merging. **The tiebreak is only that `AtlasPacker` already implements skyline
  and it is denser** - so keeping it spends no effort, whereas switching spends effort and 1 MB
  to save CPU that is not on a hot path. Greenfield, shelf would be a defensible pick; if the
  packer ever needs rework, shelf is a legitimate simplification rather than a regression.
- **Guillotine**: densest of all, but 2000+ ops/glyph.

### Superseded: keyed groups, group-level eviction

- Storage keyed on `(fontId, pixelSize)`. Each group owns chunks of a **uniform cell grid**, the
  cell being that face's scaled bounding box, so cell alloc and free are O(1) off a free list.
- **Evict whole groups**, LRU by last-used frame. Never evict individual glyphs: that is exactly
  what made the quadtree fail - freed cells cannot coalesce to serve a differently-sized glyph -
  and it costs 36 154 evictions to still drop 7 239 glyphs.
- Groups are chunks **inside a shared page**, not a texture per group, so a batched run - single
  font, single size by construction - keeps one `textureId` and draws are never split.
- **Eviction is reactive only.** The sole trigger is an allocation that did not fit - a
  keystroke, a paste, a new glyph appearing. No periodic sweep and no GC pass, so idle memory
  is held rather than reclaimed. On a miss the order is: reclaim groups untouched for K frames,
  then commit a new page, then fail loudly.
- **Never evict an in-use group.** Its cells are referenced by live glyph runs, including
  off-screen ones in a retained tree, so freeing it would leave those runs pointing at cells
  reassigned to another font. K frames of hysteresis is what makes a group a safe candidate;
  eviction happens at a frame boundary and marks affected runs dirty so they re-shape.
- **Never drop a glyph silently.** Today `rasterizeGlyphInfo` logs and returns failure
  (`glyph_atlas.cpp:47`), so text loses characters once full. That is the real bug behind
  `library_review.md` item 3.

Prior art positions, for the record: Alacritty adds atlases, Ghostty grows one square atlas, RAD
adds up to 64 pages of 1024^2 and clears everything via `fnt_reset()` on DPI and font-size
changes. None of them evict, and none of them key storage by font or size - RAD explicitly
cannot, since its atlas selection is first-fit so every page holds an interleaved mix. Keying by
`(font, size)` is what buys reclamation, and it is the one place this design deliberately departs
from all three references.

### Caveat on the numbers

`glyphs`, `ink` and `resv` are **not comparable between freeing and non-freeing allocators**: the
simulated live-glyph cap only binds allocators that can free, so blockcell's rasterisation count
is inflated relative to the paged variants. `vramMB`, `pages` and `fails` are apples to apples.

## Memory: viewport-proportional, not document-proportional

Take a 1 M line file, a 1920x1080 viewport, a 14 px monospace font: ~120 visible rows of
~400 visible columns, so ~48 000 glyphs on screen.

| structure | scales with | 1 M line file |
|---|---|---|
| document bytes | document | 0 (application owns it) |
| line-start index | document | 8 MB at 8 B/line |
| glyph quads (with colour) | **viewport** | 48 000 x 20 B = 960 KB, x2 for the CPU mirror |
| row instances | **viewport** | 120 x 80 B = 9.6 KB |
| selection/caret rects | **viewport** | ~220 x 80 B = 18 KB |

So Amethyst-side cost is ~2 MB regardless of file size, and the only document-proportional
structure is the line index the application owns anyway. At 10 M lines that index wants
chunking (per-chunk base plus u32 offsets) to stay off 80 MB, but that is an application
decision, not a library one.

Compare the same 48 000 glyphs through today's two paths:

- **Batched** (`GlyphQuad`, 16 B): ~1.5 MB, one instance per run, no per-glyph sorting.
- **Rich** (`layoutTextAtlas`, one `InstanceData` per glyph): **~12.8 MB**, because each glyph
  becomes a registry slot — 80 B in `m_slotData`, another 80 B in `m_sortedBuffer`, 10 B of
  alive/dirty/index bookkeeping, and a 16 B handle. It also puts every glyph into the layer's
  `stable_sort` and dirty list individually.

`Canvas` is the only rich-text user today (`canvas.cpp:300`), and it additionally submits
`maxChars - glyphs.size()` **invisible** instances per text command to keep slots stable. That
is the right instinct (avoid churn) applied to the wrong primitive.

**Conclusion: per-glyph colour must come to the batched path.** The rich path should not be
the answer to syntax highlighting, and arguably should be retired once batched carries colour.

---

## Gap 1: per-glyph colour in the batched path

Colour lives on the instance (`InstanceData::fillColor`), and `GlyphQuad` is four u32s of
position and UV with no colour. One instance per run therefore means one colour per run.

**Do not widen `GlyphQuad`.** Adding a u32 makes it 20 B, and while std430 does give a
20-byte array stride for five u32s, 20 no longer divides a 64-byte cache line or matches an
aligned 16-byte load. Quads start straddling cache lines and coalesced fetches degrade, so in
practice it would be padded to 32 — the "+4 B" is really **+16 B, a doubling**: 768 KB to
1.5 MB at the viewport budget.

Repacking into the spare bits is also a trap. The atlas is 1024x1024 so UVs only need 10 bits
of each u16, and there are ~44 spare bits across the four words — but glyph Y is *bbox*
relative, and a tall multi-line label needs the full u16 even though an editor's row-relative
runs would not. Repacking would fix the editor and break labels.

**Fix: a parallel colour array**, indexed by the same glyph index as `glyphs[]`.

- `GlyphQuad` keeps its 16 B stride, so the hot position/UV loads keep aligned, coalesced
  access.
- Colour is a separate fetch, taken only when an instance flag says per-glyph colour.
- Runs that do not use it allocate nothing: the array is sized and bound per registry like the
  existing glyph/line/slice arenas.
- 48 000 x 4 B = 192 KB for the text that does use it.

If 4 B per glyph ever looks expensive, the same array can hold a 1-byte palette index with the
theme in a small uniform buffer — highlighting themes are tens of colours, not thousands. Not
worth doing up front.

The other alternative — one instance per colour run, which is what RAD does — keeps the quad
at 16 B but multiplies instance count by 5-10x for typical highlighted code and reintroduces
the per-run churn of Gap 6.

This is what makes syntax highlighting viable, and it stays small.

## Gap 2: the batched primitive is not public

The batched path exists but is welded to `TextLabel` and `UIInput`. An external editor has no
way to say "draw this run of positioned coloured glyphs here". `GlyphBuffer`,
`GlyphSliceHandle` and the instance wiring are all internal plumbing reachable only by
writing a component inside the library.

**Fix:** promote it to a first-class primitive, roughly:

```cpp
class GlyphRun {
  public:
    void setGlyphs(std::span<const GlyphQuad> glyphs, std::span<const GlyphLine> lines,
                   float lineHeightPx);
    void setOrigin(vec2 origin);        // translation-only update, no re-upload of glyphs
    void setClipRect(vec4 clip);
    void setVisible(bool visible);
};
```

`setOrigin` being separate from `setGlyphs` is the important part: it is what makes sub-line
scrolling cost a handful of bytes instead of a re-upload, and it is already how
`TextLabel::repositionGlyphs` works internally.

An editor then owns a ring buffer of `GlyphRun`s, one per visible row, and never creates or
destroys them while scrolling — which matters because of Gap 6.

## Gap 3: fixed compile-time capacities

`GlyphBuffer` allocates its arenas up front in its constructor, from compile-time constants
(`glyph_buffer.h:65-67`): 16 384 glyph quads (256 KB), 8 192 lines (64 KB), 2 048 slices
(32 KB) — ~352 KB of CPU mirror per registry that contains any text.

Two problems for an editor:

- **16 384 quads is about a third of one viewport.** The editor needs ~48 000-64 000.
- The GPU-side glyph arena is sized `4 x GLYPH_CAPACITY` (`gpu_resource_hub.cpp:51`) and each
  registry takes a full `GLYPH_CAPACITY` block (`:173`), so **only four registries with text
  can exist** before allocation fails. Raising the constant for the editor's benefit multiplies
  that fixed cost for every other layer.

**Fix:** make capacities runtime configuration rather than constants, set per registry (or per
context) so a text-heavy layer can be sized generously while a button layer stays small.

## Gap 4: the fragment shader binary-searches per fragment

For batched text the fragment shader locates its glyph with a binary search over the line's
glyph range (`ui.fs.glsl:310-320`). At ~400 glyphs per row that is ~9 iterations of dependent
SSBO loads *per fragment*. Over a 1920x1080 text area that is on the order of 18 M dependent
loads per frame — the single largest per-pixel cost in the editor case, and it grows with
line length.

**Fix:** a uniform-advance fast path. For monospace text the glyph index is
`floor(local.x / advance)` — O(1), no search, no dependent chain. Flag it on the instance and
carry the advance in the slice entry (there is room: `GlyphSlice::pad`).

This is probably the highest-leverage rendering change for an editor specifically, and it is
useless for general UI text, which is why it needs the flag rather than replacing the search.

## Gap 5: single caret, single selection span

`UIInput` draws exactly one caret and one selection rect. Multi-cursor and multi-select need
N of each — but crucially **only the visible ones**: cursors outside the visible line range
need no instances at all, so the count is viewport-bounded (~220 rects worst case, 18 KB),
not cursor-bounded. An editor with 10 000 cursors in a 1 M line file draws whatever is on
screen.

**Fix:** a pooled rect-batch primitive, the rect analogue of `GlyphRun`: a stable set of
instances whose contents and visibility are rewritten per frame, never released. Selection
becomes one rect per contiguous span per visible row.

The application needs to answer "which cursors/spans intersect rows [a, b)" cheaply, which is
its problem — a sorted cursor array plus binary search — not the library's.

## Gap 6: create/destroy churn forces a full layer rebuild

`GeometryRegistry::flush()` re-sorts every live slot and marks the whole layer for re-upload
whenever any instance is created or released (`library_review.md` item 6). An editor that
released and re-submitted row instances while scrolling would pay a full re-sort and
re-upload per scroll step.

This is why Gaps 2 and 5 both specify *stable pools with visibility toggling*. With that
discipline the existing incremental path (`getMutable` -> dirty slot -> one-instance upload)
is already the right shape, and item 6 stops being on the editor's critical path.

Worth fixing regardless, but the editor can be fast without it.

## Gap 7: one font, no bold or italic

`GlyphAtlas` keys glyphs on `(codepoint, pixelSize)` and holds a single `FontLoader`
(`glyph_atlas.h:124-137`). Syntax highlighting conventionally wants at least bold and italic,
and comments/keywords in italic is table stakes.

**Fix:** `(fontId, pixelSize, codepoint)` as the key, a font table on the atlas, and a font id
reachable from the glyph run.

Atlas *space* becomes a real constraint once faces multiply — see the sizing table under prior
art: 8 faces x 6 sizes is ~885 K px^2 against a 1024^2 atlas's 1.05 M, and zoom pushes well
past that. So this gap travels with the eviction decision and a 2048^2 baseline; they should
land together.

This is `library_review.md` item 3, and for an editor it is a feature blocker rather than
hygiene.

## Gap 8: clipping is fragment-discard only

There is no scissor and no CPU-side rejection (`library_review.md` item 8), so a text row
scrolled outside the viewport still runs the full per-fragment glyph search over its area
before discarding. With Gap 4 unfixed this is expensive; with Gap 4 fixed it is merely
wasteful. A scissor per draw entry plus rejecting rows outside the clip rect is
straightforward.

## Gap 9: measurement and hit-testing are not public

An editor needs, per visible line: byte offset -> x, x -> byte offset, and advances. These
exist as internals (`getCharAdvanceAtlas`, and the logic open-coded in `UIInput`). The
`TextMeasure` facade in `text_editor_plan.md` covers this; the monospace fast path makes both
directions O(1) arithmetic.

Already partly done: `GlyphAtlas::getAdvance` measures without rasterizing, so measurement no
longer consumes atlas space, and `measureTextAtlas` is allocation-free.

---

## Explicit non-goals

Stating these so they do not get built by accident:

- Document storage, editing, undo/redo.
- Syntax highlighting itself. Amethyst consumes colours, it does not compute them.
- Cursor/selection *semantics* (word motion, virtual whitespace, block select).
- Keybindings, IME, autocomplete UI.
- Complex shaping: ligatures, BiDi, Arabic/Indic. Code editors routinely disable ligatures,
  and the shaper is deliberately simple. If this is ever needed it wants a HarfBuzz seam at
  `s_shapeText`, not incremental hacks.
- Soft wrap in the editor path, per `text_editor_plan.md` — it breaks the O(1) visual-line
  to document-line mapping everything else depends on.

## Suggested order

Roughly by leverage per unit of work:

1. **Per-glyph colour in `GlyphQuad`** (Gap 1). Small, unblocks highlighting, retires the
   rich path as the answer.
2. **Monospace O(1) glyph lookup in the shader** (Gap 4). Small, and the largest per-pixel win.
3. **Public `GlyphRun` primitive** (Gap 2). The actual API an external editor needs.
4. **Configurable capacities** (Gap 3). Required before anything renders a full viewport.
5. **Pooled rect batches** (Gap 5). Multi-cursor and selection.
6. **`TextMeasure`** (Gap 9). Caret placement and click-to-position.
7. **Font ids** (Gap 7). Bold/italic spans.
8. **Scissor + row rejection** (Gap 8).

Steps 1-4 are what make a viewport render at all; 5-6 make it interactive; 7-8 make it good.

An editor built on this holds its own document and highlighter, keeps a ring of `GlyphRun`s
and a rect pool, and per frame writes ~120 runs and a couple hundred rects — a few hundred
kilobytes of traffic, independent of file size.
