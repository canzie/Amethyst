# Large-Text View Plan

## Goal

Support a text editor / log viewer panel over documents far larger than a screenful —
hundreds of thousands to millions of lines — with scrolling, caret movement and editing
that cost the same at line 1 as at line 900,000.

Also: promote text measurement to a real public API, since the editor needs it and
everything else already wants it.

Prerequisites from `library_review.md`: newline support in the shaper (item 1), UTF-8
caret movement (item 2), and ideally the frames-in-flight fix (item 4).

## Why the current model cannot get there

`text_batching_plan.md` gave us one instance per label with glyphs in a side buffer. That
is the right *rendering* model. The problem is that the *layout* model is document-sized:

- `TextLabel` shapes the entire string on any change (`text_label.cpp:186`).
- Glyph positions are **u16 pixels relative to the label bbox**
  (`text_processor.cpp:353`, clamped by `s_clampU16`), capping extent at 65535 px —
  about 4000 lines at a 16 px line height.
- The label's quad must cover the whole document, so the fragment shader runs over the
  document's area rather than the viewport's.
- `GLYPH_CAPACITY` is 16384 quads *per registry* (`glyph_buffer.h:65`), roughly 200 lines
  of code. `LINE_CAPACITY` is 8192 lines.

A million-line file is three orders of magnitude past those limits, and none of them move
by tuning a constant.

The fix is the one Ryan Fleury uses in the RAD Debugger, and it is a layout change rather
than a shader change: **never lay out the document, only the viewport.** Per-frame cost
becomes O(visible lines) and stops depending on document size. That independence — not a
faster inner loop — is what makes that kind of scrolling feel instant.

---

## Document side: a line index, not a string

The panel does not own a `std::string`. It reads through an interface so the application
can back it with whatever it already has (mmap'd file, log ring buffer, rope):

```cpp
class TextSource {
  public:
    virtual ~TextSource() = default;
    virtual uint64_t         lineCount() const = 0;
    virtual std::string_view line(uint64_t index) const = 0;  // without the terminator
    virtual uint64_t         byteSize() const = 0;

    // Change detection. Bumped on every edit; per-line so an edit invalidates one row.
    virtual uint64_t revision() const = 0;
    virtual uint64_t lineRevision(uint64_t index) const = 0;
};
```

**Change detection must be O(1), never a content comparison.** A row's cached layout is
valid iff its `lineRevision` is unchanged, so validating the whole viewport is ~120 integer
compares regardless of document size.

This is deliberately different from `UIInput`, which compares the rendered string itself
against the previous one. That is the right call for a single-line field — a `memcmp` over
tens of bytes, with no hash-collision risk, and the string can be moved in since
`displayText()` already returns by value — but it is O(n) per draw and does not scale. Do
not reuse it here.

The panel owns the line index: a `std::vector<uint64_t>` of line-start byte offsets, built
once by a `memchr` loop over the buffer. That scan runs at GB/s, so a 100 MB file indexes
in well under a second, and it is the only pass over the whole document that ever happens.

On edit, only offsets *after* the edit point shift, and they all shift by the same delta —
so an incremental patch is a `memmove` of the tail plus a constant add, not a rescan.

With the index in place, "line i starts at byte X" and `lineCount()` are O(1). That is
what makes scroll-to-anywhere instant: jumping to line 900,000 is an array index.

## Layout side: one instance per visible line

Instead of one quad for the document, emit one instance per **visible** line — 60 to 120
of them — each with its own glyph slice holding only that line's glyphs.

This one change fixes several things at once:

- Glyph positions become **viewport-relative**, so the u16 packing is never a constraint
  again.
- Budget becomes `visible lines x visible columns` ~ 120 x 400 = 48K quads: a fixed,
  predictable number independent of document size. `GLYPH_CAPACITY` becomes a real
  capacity rather than a document limit.
- The shader's `line = min(uint(local.y / fragLineHeight), fragLineCount - 1u)`
  (`ui.fs.glsl:307`) becomes trivially line 0, so the uniform-line-height assumption in
  `text_batching_plan.md` stops mattering for this component.
- Per-line dirty granularity: editing line 400 re-shapes one line.
- Horizontal culling is free — a row instance's quad is its line's extent.

Allocate the row instances and their glyph slices **once**, as a ring buffer, and never
release them. Scrolling rewrites their contents and translation instead of submitting and
releasing, which sidesteps the full-registry rebuild in `library_review.md` item 6
entirely. `GlyphBuffer::reserve` already exists for pre-sizing a slice
(`glyph_buffer.h:82`) and is exactly the right tool for the row slices.

## Scrolling: whole lines plus a remainder

Keep scroll state as two values, not one:

```cpp
uint64_t m_firstVisibleLine;  // index into the line table
float    m_subLineOffsetPx;   // [0, lineHeight)
```

- **Sub-line movement** touches only each row instance's `translation`. That is the
  existing cheap path — `getMutable` marks the slot dirty without a re-sort
  (`text_label.cpp:150`, `geometry_registry.cpp:108`) — so a scroll frame is ~100 float
  writes and no shaping at all.
- **Crossing a line boundary** rotates the ring buffer by one and re-shapes exactly the
  one newly exposed line.
- **A fling that jumps 50,000 lines** costs the same as scrolling one line: relayout of
  the ~120 visible rows.

Scroll input should be quantized in lines, not pixels (see `library_review.md` item 10 on
`scrollSpeed`).

## Monospace fast path

For a code or log panel, take the fixed-advance branch explicitly: when the font is
monospace and the line is ASCII, `x = column * advance` with no per-glyph shaping — a
tight loop over bytes with one atlas lookup each.

That makes column <-> pixel arithmetic in both directions, so caret placement,
click-to-position and selection rectangles are O(1) rather than the linear scan over
`m_charPositions` that `UIInput` does today (`ui_input.cpp:465`). Tab expansion belongs
here too.

## Per-glyph color for syntax highlighting

This is the one capability the batched path structurally lacks. Color lives on the
instance (`InstanceData::fillColor`), and `GlyphQuad` is 16 bytes of position and UV with
no color field (`glyph_buffer.h:22`). One instance per line therefore means one color per
line.

Two options:

1. **Widen `GlyphQuad`** to carry a packed RGBA (16 -> 20 B, or 24 B padded), and have the
   fragment shader prefer `g.color` when an instance flag says per-glyph color. Keeps one
   instance per line. Costs memory on every glyph in the buffer, including labels that do
   not need it.
2. **One instance per colored run** (what RAD does). Keeps the quad at 16 B, but
   multiplies instance count by 5-10x for typical highlighted code, and reintroduces the
   churn from `library_review.md` item 6.

**Recommendation: option 1.** At 48K quads the extra 8 bytes is ~400 KB, and keeping one
instance per line is what makes the ring-buffer approach simple.

## Also needed for a usable panel

- **Multi-line selection**: one rect per visible line, not the single rect
  `drawSelection` emits today (`ui_input.cpp:632`).
- **Caret as a byte offset** into the document, with UTF-8-aware movement
  (`library_review.md` item 2).
- **Horizontal scrolling**: track max line width lazily, over visible lines plus a
  high-water mark, rather than scanning the document.
- **Scrollbar dragging** — for a million-line file, wheel-only navigation is unusable.
  Already on `todo.md`.

## Decide early: soft wrapping

Wrapping breaks the O(1) "visual line -> document line" mapping that everything above
rests on, because visual line N's document line cannot be known without laying out
everything before it.

Editors resolve this either by not wrapping (RAD's choice, and the right one for a
debugger or log view) or by caching per-document-line wrap counts in a summed/prefix tree
so the mapping stays O(log n).

**Recommendation: no wrap in this component.** It removes an entire class of complexity,
and `TextLabel` keeps the wrapping path for ordinary labels. If wrap is needed later, the
summed-tree approach layers on top without changing the row-instance model.

---

## Text measurement API

Currently reachable only as `TextProcessor::measureTextAtlas` (`text_processor.h:133`),
which is not adequate to expose as-is:

- Ignores wrap and newlines; returns `metrics.lineHeight` as the height unconditionally.
- Takes `uint32_t pixelSize`, so fractional sizes are already lost at the boundary.
- No caching — layout code calls it repeatedly with identical arguments.
- Declared `const` while mutating the atlas through `m_glyphAtlas` (rasterizing on
  demand). A const lie that will bite as soon as anything is threaded.

Proposed facade on the context, over the existing shaper:

```cpp
struct TextMetrics {
    vec2     size;            // honours wrap and newlines
    float    ascender, descender, lineHeight;
    uint32_t lineCount;
    float    firstBaselineY;  // for aligning text against non-text widgets
};

class TextMeasure {
  public:
    TextMetrics measure(std::string_view text, const TextLayoutParams &params);

    float       lineHeight(float fontSize) const;
    FontMetrics fontMetrics(float fontSize) const;

    // caret / hit-testing: byte offsets in, pixels out, and back
    float  offsetToX(std::string_view line, size_t byteOffset, float fontSize) const;
    size_t xToOffset(std::string_view line, float x, float fontSize) const;

    // advance without forcing rasterization into the atlas
    float advance(uint32_t codepoint, float fontSize) const;
};
```

Two design points matter more than the signatures:

**Cache `measure`.** Key on a hash of `(text, fontSize, bounds.x, wrap, letterSpacing,
lineHeight)`. Constraint solving calls it repeatedly with identical arguments and today
re-shapes every time.

**Split measurement from rasterization.** Done: `GlyphAtlas::getAdvance` loads the advance
via `FT_Get_Advance` without rendering or packing, and cache entries upgrade in place to
packed if the glyph is later drawn. `measureTextAtlas` and `getCharAdvanceAtlas` both go
through it, so measuring text that is never drawn no longer consumes atlas space.

`measureTextAtlas` still is not `const`-honest — it mutates the atlas cache through the
pointer — but it no longer has the side effect that mattered. Making it truly `const` needs
the cache to be `mutable` or the atlas split into a metrics half and a packing half.

`offsetToX` / `xToOffset` are worth exposing before the editor exists: `UIInput` open-codes
that logic against a per-byte array it rebuilds every frame (`ui_input.cpp:612-628`), and
the editor needs the identical operation.

---

## Implementation order

1. `TextMeasure` facade with caching and a metrics-only path. Independent of the editor,
   immediately useful, and fixes the const lie.
2. Widen `GlyphQuad` with a per-glyph color plus the instance flag and shader branch.
   Small, and unblocks highlighting.
3. `TextSource` + line index, with the `memchr` scan and incremental patch on edit. Test
   standalone against a large file before any rendering exists.
4. `TextView` component: ring buffer of row instances, viewport-relative shaping, no wrap,
   monospace fast path.
5. Two-part scroll state and the sub-line translation-only path.
6. Caret, multi-line selection, horizontal scroll.
7. Editing, on top of the incremental line-index patch.
