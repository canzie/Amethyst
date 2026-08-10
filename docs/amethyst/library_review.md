# Library Review

Read-only audit of the library as of 2026-08-10 (`db62aed`). Nothing was built or run;
every claim below is from reading the source, with `file:line` pointers so each one can
be checked independently.

The foundation is sound: a retained tree, a slot-based instance registry with incremental
dirty upload, and batched text where one quad per label resolves its glyphs in the
fragment shader out of an SSBO (see `text_batching_plan.md`). The findings are almost all
at the edges of that design, not in it.

Findings are ordered by how much they block real work. The large-text/editor work has its
own document: `text_editor_plan.md`.

Items 1, 2 and 5 are **fixed**; see "What has been done" at the end for exactly what
changed. The rest are open.

---

## 1. Hard newlines do not exist in the shaper — FIXED

**Was blocking.** `s_shapeText` decoded codepoints and never tested for `\n`. It reached
`FontLoader::rasterizeGlyph`, where an unmapped codepoint falls back to glyph index 0
(`font_loader.cpp:91`) — `.notdef`, which in most fonts is a visible box.

So `\n` rendered as a tofu box and did **not** break the line. Consequences:

- `UIInput` inserts `"\n"` on Enter when `multiline` is set (`ui_input.cpp:327`), so
  multiline input was broken.
- `measureTextAtlas` returned `metrics.lineHeight` as the height unconditionally, so any
  auto-sizing over multi-line text was wrong.

## 2. Cursor and selection arithmetic is byte-indexed over UTF-8 — FIXED

**Was blocking for any non-ASCII text.** `UIInput` treated `m_cursorPosition` as a byte
offset and stepped it by one byte:

- `m_text.erase(m_cursorPosition - 1, 1)` (`ui_input.cpp:252`) splits multibyte sequences.
- `moveCursor` moves ±1 byte (`ui_input.cpp:424`).
- `getCursorFromMouseX` indexes `m_charPositions` by byte (`ui_input.cpp:453`).

Worse, the advance table was built with
`getCharAdvanceAtlas(static_cast<uint32_t>(shown[i]), ...)`. `char` is signed on this
platform, so any byte >= 0x80 sign-extended to a ~4-billion codepoint, which was then
rasterized and cached as a `.notdef` box.

Byte offsets remain the caret representation (they compose with the document model in
`text_editor_plan.md`), but movement is now by codepoint boundary. When `TextMeasure`
lands, its `offsetToX`/`xToOffset` should absorb this logic so it lives in one place.

## 3. Single font, structurally

`GlyphAtlas` keys its cache on `(codepoint, pixelSize)` only — `m_sizeTables` is keyed by
pixel size, and each table holds an ASCII array plus an extended map
(`glyph_atlas.h:124-137`). The atlas holds one `FontLoader*`, and neither `GlyphQuad` nor
`InstanceData` carries a font id.

Bold, italic, and fallback-for-missing-glyphs therefore cannot be added without reworking
the cache key, the atlas ownership model, and possibly the instance format. This is cheap
to fix now and expensive later: make the key `(fontId, pixelSize, codepoint)` and give
the atlas a font table rather than a single loader.

Related, in the same area:

- The atlas is a fixed 1024x1024 with a skyline packer that never evicts; on exhaustion it
  logs an error and drops the glyph (`glyph_atlas.cpp:45-49`). Every distinct pixel size
  ever touched (DPI changes, `textScaled`, size animations) permanently consumes space.
  Needs LRU or generation-based eviction, or multiple atlas pages.
- Font size is truncated to an integer at every entry point
  (`static_cast<uint32_t>(fontSize)`, e.g. `text_processor.cpp:114`,
  `text_label.cpp:113`), so fractional sizes snap. `textScaled` computes a non-integer
  size (`text_label.cpp:122`) and then loses it.
- No DPI / content-scale awareness anywhere in the library.
- `getKerning` exists (`glyph_atlas.cpp:109`) but is never called from the shaper, and it
  calls `setPixelSize` on every invocation, which re-runs `FT_Set_Pixel_Sizes`.

## 4. The glyph atlas re-uploads in full on every new glyph

`amethyst_context.cpp:36` guards only on `isDirty()`, and `uploadTexture` then copies the
whole image (`amethyst__vk13_glfw.cpp:1155`, where the TODO already says so). Packing one
new glyph costs a full 1 MB transfer. Track a dirty rect in `GlyphAtlas` and upload only
that. This matters most for text-heavy views, where first paint touches many new glyphs.

`uploadTexture` sets `oldLayout = VK_IMAGE_LAYOUT_UNDEFINED`
(`amethyst__vk13_glfw.cpp:1177`), discarding existing contents. That is correct only while
every upload is a full-image upload, so it has to change in the same commit as the dirty
rect.

Separately, `uploadDeviceLocal` creates a command pool, staging buffer, submit and
`vkQueueWaitIdle` per call (`amethyst__vk13_glfw.cpp:571-639`). Acceptable at init (index
buffer), a full device stall anywhere else.

## 5. `GeometryRegistry` leaks a handle per `submit()` — FIXED

`m_handlePool` was a `std::deque<GeometryAllocation>` that was only ever appended to,
returning `&m_handlePool.back()`. `release()` invalidated the handle's fields but never
returned the deque slot for reuse.

Any component that repeatedly submits and releases grew it without bound — the cursor
blink releasing and re-submitting its alloc (`ui_input.cpp:661-688`), scrollbars being
created and reset (`scrolling_frame.cpp:218`), rows appearing and disappearing while
scrolling.

The pool is now indexed by `slotId`: one handle per slot, reused whenever the slot is.
Bounded by peak slot count, which the slot free list already bounds. No separate handle
free list and no extra id on `GeometryAllocation` — a slot has exactly one live
allocation at a time, so the slot id is already the handle id.

## 6. `flush()` rebuilds and re-uploads everything when any allocation appears or disappears

**Still open, but its worst trigger is gone.** The caret used to release and re-submit its
instance on every blink, so a focused field forced this path twice a second forever.
`UIInput` now keeps the caret and selection instances alive and toggles their visibility, so
the remaining cost only fires on genuine create/destroy.

`m_needsRebuild` is set by `submit`, by `release`, and by any zIndex change
(`geometry_registry.cpp:84`, `:97`, `:132`). On the next `flush()` it re-collects live
slots, `stable_sort`s all of them, rewrites the whole sorted buffer, and sets
`m_fullDirty` (`geometry_registry.cpp:142-168`) — which makes `syncGeometry` upload the
entire registry (`gpu_resource_hub.cpp:213`).

So creating or destroying one instance costs a full sort and a full re-upload of that
layer. For anything that churns instances per frame (a scrolling editor rotating row
instances) this is a rebuild every frame.

Fix options, cheapest first:

- Have churning components keep their instances alive and toggle `setVisible` instead of
  releasing. The virtualized design in `text_editor_plan.md` does this anyway (ring
  buffer of row instances), which removes the problem for the editor specifically.
- Bucket slots by zIndex so appends land in a bucket without a global sort.
- Track whether the *sort order* actually changed, and skip `m_fullDirty` when a rebuild
  produced an identical order (common: a release leaves the rest in place).

## 7. Hit-testing allocates and sorts on every mouse move

`getHittableInstances()` builds a fresh `std::vector` per node (`window.cpp:45`), and both
`s_fillHoverStackRecursive` (`window.cpp:98`) and `s_dispatchRecursive` (`window.cpp:135`)
then **copy that vector again** and `stable_sort` it — recursively, for every mouse move,
every click, and every scroll.

Fix: cache a z-sorted child list on the node, invalidated when children change or a
zIndex changes, and pass `std::span` down the recursion instead of vectors by value.

## 8. Clipping is fragment-discard only

`fragClipRect` is tested per fragment with a `discard` (`ui.fs.glsl:298-303`). There is no
scissor rect and no CPU-side rejection of instances that fall entirely outside their clip
rect. A fully-clipped batched-text quad still runs the whole per-fragment glyph binary
search over its area (`ui.fs.glsl:305-343`).

One note on the same code: the bounds test mixes `<` on the min edges with `>` on the max
edges, making the rect inclusive on both, so it clips one pixel wider than the rect on the
max side.

(The `vec4(0)` = "no clipping" encoding is fine: a zero-area clip rect means a zero-area
node, which is culled before it can render.)

Fix: reject instances outside their clip rect on the CPU before submit, and set a real
scissor per draw entry in `record()` (`amethyst__vk13_glfw.cpp:297`).

## 9. Instance dirty upload collapses to a min/max span

`syncGeometry` takes `minmax_element` over the dirty indices and uploads one contiguous
range covering both (`gpu_resource_hub.cpp:216-220`). Two edits at opposite ends of a
layer upload everything between them.

Fix: sort the dirty indices and coalesce into runs, uploading a handful of ranges. Worth
doing only after item 4, since the per-frame-slot buffering changes the shape of this
code.

## 10. Smaller cleanups

- **`s_pushData` is copy-pasted four times**, verbatim except for which member it
  assigns: `text_label.cpp:15`, `text_button.cpp`, `ui_image.cpp`, `ui_input.cpp:16`. It
  is `UIBase2D::pushData` (`ui_base_2d.cpp:7`) with the member swapped for a parameter.
  Make it one free function taking `GeometryAllocation *&`.
- ~~**`TextProcessor::m_fontData` is never assigned**~~ — FIXED: the member, its accessor
  and the `ttf_types.h` include are gone.
- **`TextTruncate::AT_END` has no ellipsis** — still open. It now skips to the next hard
  break instead of abandoning the rest of the text, but there is no ellipsis glyph.
- ~~**`s_decodeUtf8` does not validate**~~ — FIXED: decoding moved to `utils/utf8.h`, which
  rejects bad continuation bytes, overlongs, surrogates and out-of-range codepoints,
  yielding U+FFFD and a single-byte step so the loop resynchronises.
- ~~**`getKerning` calls `setPixelSize` on every invocation**~~ — half fixed:
  `FontLoader::setPixelSize` now early-outs when the size is unchanged, so it no longer
  resets the face's size state (and FreeType's caches) on every glyph-cache miss. Kerning
  itself is still uncached: if it is ever wired into the shaper, cache kern pairs in
  `SizeGlyphTable` keyed on `(left << 32) | right` rather than calling FreeType per pair.
- **`GlyphAtlas::getGlyph` returns non-null for zero-size glyphs** (space, and `.notdef`
  when the bitmap is empty) because `rasterizeGlyphInfo` returns `true` with a zero rect
  (`glyph_atlas.cpp:37-43`). Callers must know to check `width > 0` separately
  (`text_processor.cpp:178`), which is easy to get wrong. Consider returning a distinct
  "blank but advancing" state.
- ~~**`UIInput` re-shapes its text on every draw.**~~ — FIXED: it now keeps a
  `TextLayoutState` and takes `TextLabel`'s cheap path (shift the quad's translation) when
  only the origin moved. `m_charPositions` is rebuilt only when the text or font size
  changed, with the total width cached alongside it for alignment.
- **`ScrollingFrame` scroll speed is in fixed pixels** (`scrolling_frame.cpp:290`), not
  lines, and `layoutChildren` runs twice whenever a scrollbar appears
  (`scrolling_frame.cpp:124`, `:142`). Culled children are still fully laid out — the flag
  only suppresses rendering (`scrolling_frame.cpp:108`). Scrollbar dragging is missing and
  already on `todo.md`.

---

## What has been done

New: `utils/utf8.{h,cpp}` — validating decode, encode, and boundary stepping
(`nextBoundary`, `prevBoundary`, `alignToBoundary`). One implementation for the shaper,
the inputs, and later the editor view.

`text_processor.{h,cpp}`:

- `s_shapeText` handles LF, CR and CRLF as hard breaks before the glyph lookup, so control
  codepoints never reach the atlas. A trailing break leaves an empty final line, so `"a\n"`
  is two lines — editor semantics, chosen deliberately.
- Tabs advance to the next tab stop instead of rasterizing. Stops are multiples of the
  space advance, controlled by the new `TextLayoutParams::tabSize` (default 4). A tab is
  also a wrap opportunity, like a space.
- `measureTextAtlas` runs the same shaper as layout, so its width is the widest line and
  its height covers every line. Wrapping is still not applied — documented on the method.
- Dead `m_fontData`/`fontData()` and the `ttf_types.h` include removed.

`ui_input.cpp`: backspace, delete and arrow movement operate on codepoints;
`setCursorPosition` snaps to a boundary; `getCursorFromMouseX` walks boundaries;
`m_charPositions` is built from decoded codepoints (killing the signed-`char` sign
extension) and stays byte-indexed, with every byte of a sequence holding that codepoint's
left edge; the hand-rolled UTF-8 encoder is replaced by `Utf8::encode`.

`geometry_registry.{h,cpp}`: handle pool indexed by `slotId`. `release()` is unchanged.

Measurement (perf + correctness):

- `measureTextAtlas` is an allocation-free metrics scan. It briefly went through the full
  shaper to gain newline handling, which meant a `vector<vector<ShapedGlyph>>` per call; it
  now walks codepoints directly, handling LF/CR/CRLF and tabs, with no allocation.
- It takes a `TextLayoutParams` overload, so `tabSize`/`letterSpacing` come from the same
  struct layout uses instead of a second set of defaults. Tab-stop width is computed by one
  shared `s_tabWidth` helper, so measured and laid-out widths cannot disagree.
- `GlyphAtlas::getAdvance` returns an advance via `FT_Get_Advance` without rendering or
  packing. Entries upgrade in place to packed if the glyph is later drawn, so measuring text
  that is never drawn no longer consumes the fixed-size atlas.
- `FontLoader::setPixelSize` early-outs when the size is unchanged. It was calling
  `FT_Set_Pixel_Sizes` on every cache miss, which resets the face's size state.
- The FreeType 26.6 and 16.16 fixed-point divisors are now named constants.

`ui_input.cpp` (perf): `drawCursor` and `drawSelection` keep their instances and toggle
`setVisible` instead of releasing and re-submitting. Both bail out early when there is
nothing to show and no instance exists yet, so a never-focused field still costs nothing.
This is what removed the twice-a-second full-layer rebuild described in item 6; the fix
belongs in the component, since visibility is component-owned state and liveness is the
registry's.

Not yet compiled or exercised at the time of writing.

## Suggested order for the rest

1. `TextLayoutState` caching in `UIInput` (item 10). Reuses machinery `TextLabel` already
   has, and it is the largest remaining per-interaction cost in text input.
2. Atlas dirty-rect upload (item 4), together with the `oldLayout` change it forces.
3. `TextMeasure` public API (`text_editor_plan.md`), absorbing the caret helpers from
   `UIInput`.
4. Font id in the atlas key (item 3), while it is still cheap.
5. Registry rebuild on genuine create/destroy (item 6), if it still shows up in a profile
   once the churn above is gone.
6. Hit-test allocations (item 7) — small, self-contained, and user-paced so the payoff is
   modest.
7. Virtualized text view (`text_editor_plan.md`).
