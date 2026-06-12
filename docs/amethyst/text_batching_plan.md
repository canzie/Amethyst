# Batched Text Rendering Plan

## Goal

Today every glyph is one `InstanceData` (rich-text style). A label of N glyphs costs
N instances through layout, z-sort, cull and upload. This plan makes **regular text one
instance per label**: a single bounding-box quad, with the per-glyph data living in a
shared GPU side-buffer. Each fragment resolves which glyph it sits in (and the atlas UV)
from that side-buffer.

Rich per-glyph text (per-glyph color/size/effects) stays on the existing path as a
fallback. Batched is the default.

## Assumptions for the batched path

- One fill color per label.
- One font size per label, so line height is uniform and `line = floor(y / lineHeight)`
  is exact. Mixed sizes -> rich path.
- All glyphs of a label live in the same atlas (already true; single `GlyphAtlas`).

## No new primitive type

We reuse `PRIMITIVE_TEXT`. Batched is the default; the **flag bit marks the rich
exception**, so a default-constructed instance is batched and the shader batches when the
bit is clear:

```cpp
// instance_data.h
enum GpuInstanceFlags : uint32_t {
    INSTANCE_FLAG_VISIBLE   = 0x00000001,
    INSTANCE_FLAG_TEXT_RICH = 0x00000002,
};
```

For a batched label `shapeData[0]` holds a **slice handle** (see below), not a glyph UV.
The rich path sets `INSTANCE_FLAG_TEXT_RICH` and keeps using `shapeData[0..1]` for the
single glyph's UV rect. So inside the `PRIMITIVE_TEXT` branch: `RICH` set -> per-glyph,
clear -> batched.

---

## Storage model

Three GPU buffers, owned **per `GeometryRegistry`, lazily**. A registry with no text
slices never creates them. They live and die with the registry, alongside its existing
per-layer instance buffer:

1. `glyphs[]`   - `GlyphQuad`, the per-glyph ink rects + atlas UV.
2. `lines[]`    - `GlyphLine`, per-line sub-ranges into a label's glyph run.
3. `slices[]`   - `GlyphSlice`, the indirection table: handle -> current location.

Handles are **registry-local**. An instance and the slice it references always live in the
same registry, so when the backend draws registry R it binds R's instance buffer and R's
`glyphs/lines/slices` together.

Why per-registry (not global): a destroyed layer drops its whole buffer, so there is never
any cross-layer fragmentation to compact away - the expensive "close the gaps across
everyone" plumbing simply does not exist. Lazy creation means text-free registries cost
nothing. (Compaction still exists for *intra*-registry churn - a long-lived layer with
many text edits - but teardown never needs it.)

Each buffer is allocated at a large fixed capacity on first use. **No GPU buffer resize.**
On exhaustion we try `compact()` once, and if still full `AM_ASSERT` + log and drop the
slice. How the backend physically stores/binds these is the backend's business; the core
just owns one logical set per registry.

### GlyphQuad (16 B)

```cpp
struct GlyphQuad {
    uint32_t posMin;  // 2x u16: ink rect min, px, relative to the label bbox top-left
    uint32_t posMax;  // 2x u16: ink rect max
    uint32_t uvMin;   // 2x u16: atlas pixel coords (0..atlasW/H)
    uint32_t uvMax;   // 2x u16
};
```

u16 integer pixels, not `packHalf2x16`: exact for atlas coords and for bbox-relative
positions, same size. Positions are relative to the label bbox top-left so they stay
small. Zero-area glyphs (spaces) are **not** emitted; their advance still shifts later
glyphs, and the gap is handled by the shader.

### GlyphLine (8 B)

```cpp
struct GlyphLine {
    uint32_t glyphStart;  // first glyph of this line, RELATIVE to the label's glyph run
    uint32_t glyphCount;
};
```

Glyphs are stored in reading order. Wrapping does not change storage: it just means later
glyphs get a higher `y` and a reset `x`, and the line table partitions the single run.
Within a line, `x` is monotonic (reading order), which is what lets the per-line binary
search work; across lines `x` resets, but we never search across lines.

Example, `"hello world"` wrapped to two lines, one contiguous run of 11 glyphs:

```
glyphs (relative):  h e l l o w o r l d   (space not emitted)
                    0 1 2 3 4 5 6 7 8 9
lines (relative):   [ {0,5}, {5,5} ]
```

### GlyphSlice (16 B) - the indirection table

```cpp
struct GlyphSlice {
    uint32_t glyphBase;   // absolute offset of the label's first GlyphQuad in glyphs[]
    uint32_t lineBase;    // absolute offset of the label's first GlyphLine in lines[]
    uint32_t packed;      // lineCount(low 16) | lineHeightPx as half(high 16)
    uint32_t _pad;
};
```

The **slice handle is the index into the registry's `slices[]`** and is stable for the
lifetime of the label within that registry. The instance stores only this handle
(`shapeData[0]`). The vertex shader reads the
slice once and forwards `glyphBase / lineBase / lineCount / lineHeight` as flat varyings.

This indirection is the whole answer to "how do holders learn something moved": they do
not. When a slice grows (realloc) or compaction relocates its data, only the
`GlyphSlice` entry's bases change. The handle, the instance, and the owning component are
untouched. Nobody needs notifying.

---

## The allocator: `GlyphBuffer`

New module `libamethyst/src/modules/glyph_buffer.{h,cpp}`. **One instance owned lazily by
each `GeometryRegistry`** (`std::unique_ptr<GlyphBuffer>`, created on the first
`createSlice`, destroyed with the registry). Owns the three CPU mirrors, two block
sub-allocators (one for the glyph arena, one for the line arena), a free list of
slice-handle ids, and dirty-range tracking per buffer.

### Public CPU types

```cpp
struct GlyphSliceHandle {
    uint32_t id = UINT32_MAX;
    bool isValid() const { return id != UINT32_MAX; }
};
```

### Public API

```cpp
class GlyphBuffer {
  public:
    GlyphSliceHandle createSlice();                 // reserves a slices[] id, no data yet
    void updateSlice(GlyphSliceHandle,              // (re)fill; grows blocks as needed
                     const GlyphQuad *glyphs, uint32_t glyphCount,
                     const GlyphLine *lines, uint32_t lineCount,
                     float lineHeightPx);
    void destroySlice(GlyphSliceHandle);            // frees blocks + handle id
    void reserve(GlyphSliceHandle, uint32_t glyphCapacity); // optional growth hint (inputs)

    void compact();                                 // defragment; transparent to holders

    // upload accessors (backend reads these in sync())
    const GlyphQuad *glyphData() const; const GlyphLine *lineData() const;
    const GlyphSlice *sliceData() const;
    DirtyRange glyphDirty() const, lineDirty() const, sliceDirty() const;
    void clearDirty();
};
```

### Internal per-slice record

```cpp
struct SliceRecord {
    Block glyph;   // {offset, capacity, count} into the glyph arena
    Block line;    // {offset, capacity, count} into the line arena
    bool  alive = false;
};
// Block = { uint32_t offset, capacity, count; }
```

`slices[handle]` (the GPU mirror) is derived from `SliceRecord` on every `updateSlice`.

### `BlockAllocator` (used for both arenas)

Fixed capacity `N`. Free list = vector of free spans `{offset, length}`, initially
`{0, N}`. Allocation rounds the request up to a small bucket (e.g. next multiple of 8)
to cut fragmentation and to leave slack for in-place growth.

- `alloc(n)`: first-fit over free spans for `length >= bucket(n)`; carve from the front;
  return `{offset, bucket(n)}`. If none fits -> attempt `compact()` once, retry, else fail.
- `free(block)`: insert span, coalesce with adjacent free spans.
- `grow(block, n)`: if the immediately following span is free and large enough, extend in
  place (capacity only, no copy, no base change). Otherwise `alloc(n)` a new block, the
  caller copies data over, then `free` the old block. The base change is absorbed by the
  slice table, never seen outside.

### `updateSlice` algorithm

```
1. rec = records[handle]
2. if rec.glyph.capacity < glyphCount: rec.glyph = grow(rec.glyph, glyphCount)   // may move + copy old
   if rec.line.capacity  < lineCount : rec.line  = grow(rec.line,  lineCount)
3. memcpy glyphs -> m_glyphs[rec.glyph.offset ..]; rec.glyph.count = glyphCount
   memcpy lines  -> m_lines [rec.line.offset  ..]; rec.line.count  = lineCount
4. slices[handle] = { rec.glyph.offset, rec.line.offset,
                      (lineCount & 0xFFFF) | (packHalf(lineHeightPx) << 16) }
5. mark dirty: glyph range [rec.glyph.offset, +glyphCount),
               line  range [rec.line.offset,  +lineCount),
               slice index  handle
```

Because step 4 rewrites the bases into the slice table, the instance (which only holds
`handle`) is never re-touched on growth or move.

### `reserve` for text inputs

Text inputs churn every keystroke. To avoid per-keystroke moves, the component calls
`reserve(handle, hint)` once (e.g. `hint = 64`, or clamped from `maxLength`). This sizes
`rec.glyph.capacity` up front. While `count <= capacity`, every keystroke is just step 3
(rewrite the used range) + step 4 (rewrite the slice entry) -> a partial upload, stable
base, no fragmentation. Past capacity, `grow` doubles (2x amortized), so even an
unbounded input pays only ~log(N) moves over its whole life. We do **not** pre-commit
`maxLength`; growth is driven by real typing, with the buckets/2x rule. The fixed buffer
capacity is the only hard ceiling.

### `compact`

Triggered on alloc failure (or callable manually). For each arena independently:

```
1. collect live blocks, sort by offset
2. cursor = 0
3. for each block in order:
     if block.offset != cursor:
        memmove arena[cursor .. cursor+count) <- arena[block.offset ..)
        block.offset = cursor
        mark this slice's slices[] entry dirty (base changed)
     cursor += block.capacity   // keep slack, or += count to fully tighten
4. free list = single span {cursor, N - cursor}
5. mark the moved arena range dirty for re-upload
```

Holders are not involved: only `slices[]` entries and arena data change, both owned here.

---

## Wiring it into the registry

- `GeometryRegistry`: add `std::unique_ptr<GlyphBuffer> m_glyphBuffer;` and
  `GlyphBuffer &glyphBuffer();` which lazily creates it on first call. Components reach it
  through the same registry they submit instances to (`ctx.geometry`), so the slice and the
  instance are guaranteed to share a registry. No change to `DrawContext` is needed - the
  registry is already passed.
- Lifecycle mirrors the existing `GeometryAllocation` model: a slice handle is released back
  to *its* registry's glyph buffer (the component already tracks which registry owns its
  instance allocation). On reparent across layers the component releases the old slice and
  creates a new one in the new registry, exactly as it already does for the instance.
- `AmethystBackend`: per-registry, mirror what it already does for the instance buffer -
  create/grow the registry's `glyphs/lines/slices` buffers, upload their dirty ranges in
  `sync()`, and bind them next to that registry's instance buffer in the descriptor set the
  `ui` shaders use. The core exposes one logical set per registry; the backend decides the
  physical layout (e.g. sub-allocating from a shared pool keyed by registry).

---

## Text layout

`TextProcessor`: add a sibling to `layoutTextAtlas` (reusing the same break/align/truncate
code) that returns the batched form instead of `vector<InstanceData>`:

```cpp
struct BatchedText {
    vec2 bboxPos;                  // top-left of the layout box (screen space)
    vec2 bboxSize;                 // width x (lineCount * lineHeightPx)
    float lineHeightPx;
    std::vector<GlyphQuad> glyphs; // bbox-relative ink rects + atlas uv
    std::vector<GlyphLine> lines;  // per-line ranges (relative)
};
BatchedText layoutTextBatched(const std::string &, const TextLayoutParams &) const;
```

Notes:
- bbox `y` origin = top of line 0's line-box; height = `lineCount * lineHeightPx`, so
  `line = floor(localY / lineHeightPx)` is exact. Glyph ink sits inside the line boxes.
- Spaces advance `x` but emit no `GlyphQuad`.
- Truncation: dropped glyphs are simply never emitted; the ellipsis is a normal glyph.
- Alignment (x center/right, y) shifts glyph rects; the line table is unaffected.

---

## Component wiring (text_label, text_button, text_input)

Each text component owns one `GeometryAllocation` (the bbox quad instance) and one
`GlyphSliceHandle`. On dirty redraw:

```
1. bt = ctx.textProcessor->layoutTextBatched(text, params)
2. if !handle.isValid(): handle = ctx.geometry->glyphBuffer().createSlice()
                         (text input: ctx.geometry->glyphBuffer().reserve(handle, 64))
3. ctx.geometry->glyphBuffer().updateSlice(handle, bt.glyphs..., bt.lines..., bt.lineHeightPx)
4. InstanceData inst{};
   inst.translation = bt.bboxPos + bt.bboxSize * 0.5;
   inst.scale       = bt.bboxSize;
   inst.setFillColor(color);
   inst.setPrimitiveType(PRIMITIVE_TEXT);
   inst.flags |= INSTANCE_FLAG_TEXT_BATCHED;
   inst.shapeData[0] = handle.id;
   inst.textureId    = atlasTextureId;
   inst.zIndex       = zIndex;
   inst.clipRect     = clip;
   submit-or-update the single instance
```

On destroy / empty text: `destroySlice(handle)` and release the instance.

### Text input specifics

Input is barely affected. Cursor placement, selection rect, and mouse hit-testing all run
off `m_charPositions` (a separate CPU array) and their own `PRIMITIVE_RECT` instances -
none of that reads glyph instances. So the only change in `drawText` is replacing the
"submit N glyph instances" loop with one `updateSlice` + one instance. `reserve(handle, 64)`
keeps the base stable across keystrokes; each edit is a partial re-upload of the used
glyph range plus the slice entry. (Pre-existing: `m_charPositions` is single-line only;
this change neither fixes nor regresses that.)

---

## Shaders

### `ui.vs.glsl`

Add the slice SSBO and, for batched text, look it up once and forward flat varyings; leave
`fragUV` as 0..1 (do **not** remap to atlas UV for batched).

```glsl
struct GlyphSlice { uint glyphBase; uint lineBase; uint packed; uint _pad; };
layout(std430, binding = 4) readonly buffer SliceBuffer { GlyphSlice slices[]; };

// new flat outs:
//   flat uint  fragGlyphBase, fragLineBase, fragLineCount;
//   flat float fragLineHeight;
//   flat uint  fragTextBatched;

uint primitiveType = (inst.cornerPrimitiveMode >> 16u) & 0xFFu;
bool batched = (primitiveType == PRIMITIVE_TEXT) &&
               ((inst.flags & INSTANCE_FLAG_TEXT_BATCHED) != 0u);
fragTextBatched = batched ? 1u : 0u;

if (batched) {
    GlyphSlice sl  = slices[inst.shapeData[0]];
    fragGlyphBase  = sl.glyphBase;
    fragLineBase   = sl.lineBase;
    fragLineCount  = sl.packed & 0xFFFFu;
    fragLineHeight = unpackHalf2x16(sl.packed).y;
    fragUV         = uvs[gl_VertexIndex];                 // 0..1, no remap
} else if (primitiveType == PRIMITIVE_TEXT || primitiveType == PRIMITIVE_SVG) {
    vec2 uvMin = unpackHalf2x16(inst.shapeData[0]);       // rich path unchanged
    vec2 uvMax = unpackHalf2x16(inst.shapeData[1]);
    fragUV = mix(uvMin, uvMax, uvs[gl_VertexIndex]);
}
```

### `ui.fs.glsl`

Add the glyph + line SSBOs; handle batched before the existing `PRIMITIVE_TEXT` branch.

```glsl
struct GlyphQuad { uint posMin; uint posMax; uint uvMin; uint uvMax; };
struct GlyphLine { uint glyphStart; uint glyphCount; };
layout(std430, binding = 2) readonly buffer GlyphBuffer { GlyphQuad glyphs[]; };
layout(std430, binding = 3) readonly buffer LineBuffer  { GlyphLine lines[];  };
const vec2 ATLAS_SIZE = vec2(1024.0); // or pass via push constant

if (fragPrimitiveType == PRIMITIVE_TEXT && fragTextBatched == 1u) {
    vec2 local = fragUV * fragSize;                          // px, bbox-relative, y-down
    uint line  = min(uint(local.y / fragLineHeight), fragLineCount - 1u);
    GlyphLine lr = lines[fragLineBase + line];

    // last glyph in this line with posMin.x <= local.x
    uint lo = lr.glyphStart, hi = lr.glyphStart + lr.glyphCount;
    while (lo < hi) {
        uint mid = (lo + hi) >> 1u;
        float xmin = float(glyphs[fragGlyphBase + mid].posMin & 0xFFFFu);
        if (xmin <= local.x) lo = mid + 1u; else hi = mid;
    }
    if (lo == lr.glyphStart) discard;                       // left of first glyph
    GlyphQuad g = glyphs[fragGlyphBase + (lo - 1u)];

    vec2 pmin = vec2(g.posMin & 0xFFFFu, g.posMin >> 16);
    vec2 pmax = vec2(g.posMax & 0xFFFFu, g.posMax >> 16);
    if (any(lessThan(local, pmin)) || any(greaterThanEqual(local, pmax))) discard; // in a gap

    vec2 luv  = (local - pmin) / (pmax - pmin);
    vec2 amin = vec2(g.uvMin & 0xFFFFu, g.uvMin >> 16);
    vec2 amax = vec2(g.uvMax & 0xFFFFu, g.uvMax >> 16);
    float cov = textureLod(gTextures[fragTextureId], mix(amin, amax, luv) / ATLAS_SIZE, 0.0).r;

    float a = cov * fragFillColor.a;
    if (a < 0.001) discard;
    outColor = vec4(fragFillColor.rgb, a);
    return;
}
// ... existing PRIMITIVE_TEXT (rich) branch unchanged below ...
```

`textureLod(..., 0.0)` is required: the sample is in data-dependent control flow, so
implicit derivatives are undefined. The atlas is non-mipmapped coverage, so LOD 0 is
correct and bilinear filtering preserves AA exactly as today.

### Per-pixel cost

`line` pick is O(1). The search is O(log glyphs-per-line), independent of total glyphs or
line count, so wrapped multi-line paragraphs stay cheap.

---

## Fixed capacities (no resize)

Per registry, allocated on first text use. Size for a typical layer, not a worst case -
buffers are cheap to skip (lazy) and most registries hold little text:

```
GLYPH_BUFFER_CAPACITY  // per registry, e.g. 1 << 16 glyphs = 1 MB
LINE_BUFFER_CAPACITY   // per registry, e.g. 1 << 12 lines  = 32 KB
SLICE_TABLE_CAPACITY   // per registry, e.g. 1 << 12 slices = 64 KB
```

On exhaustion: `compact()` once, retry, else `AM_ASSERT` + log and skip the slice.

---

## Removal / cleanup

- Delete the dead analytic text shaders `text.vs.glsl` / `text.fs.glsl` and their pipeline.
- Keep the rich per-glyph `PRIMITIVE_TEXT` path as the fallback for future per-glyph styling.

---

## Implementation order

1. Structs + `instance_data.h` flag/setters (`setBatchedText`, slice handle into shapeData[0]).
2. `GlyphBuffer` allocator (`BlockAllocator`, slice table, dirty ranges) + unit-ish coverage.
3. Lazy `GlyphBuffer` on `GeometryRegistry` + per-registry backend buffer creation, bind, sync.
4. Shader changes (VS lookup + flat outs, FS batched branch).
5. `TextProcessor::layoutTextBatched`.
6. Wire `TextLabel`, then `TextButton`, then `TextInput` (with `reserve`).
7. Delete dead analytic text shaders.
