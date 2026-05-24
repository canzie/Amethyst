# SVG Support Plan

## Goal

Add SVG icon/image support to Amethyst by rasterizing SVGs to an atlas texture, fitting cleanly into the existing instanced-quad + bindless-texture pipeline.

## Approach: Rasterize-to-Atlas

SVGs are parsed and rasterized to RGBA bitmaps at a requested pixel size, packed into an atlas texture, and referenced via `AmTextureId`. No architectural changes to the render loop, draw ordering, or pipeline.

This mirrors how GlyphAtlas works: CPU-side atlas with skyline packing, dirty flag, backend uploads when dirty.

## Why not GPU-native path rendering

We explored three GPU-native approaches and ruled them out for now:

- **SDF composition** (evaluate SVG primitives as SDF chains in fragment shader) fits the architecture but cubic beziers have no closed-form SDF, making `<path>` elements impractical.
- **Compute-based rasterization** (Vello-style) produces a texture anyway, so the integration point is identical to CPU rasterization but with enormous implementation cost.
- **Stencil-and-cover** requires per-path draw calls and a stencil attachment, breaks the single-pipeline instanced draw model.

The atlas approach handles 100% of SVG icon use cases. Complex animated/dynamic SVGs are out of scope.

## Library Choice

**lunasvg**
- Full SVG 1.1 support (paths, gradients, transforms, clip-paths, groups)
- C++ library, CMake-friendly, actively maintained
- Rasterizes to RGBA bitmap at arbitrary size
- ~5k LOC, no dependencies (bundles its own plutovg rasterizer)
- MIT license

## Architecture

### Shared AtlasPacker

Extract the skyline bottom-left packing algorithm from GlyphAtlas into a reusable class. Both GlyphAtlas and SvgAtlas use the same packing logic, maintaining it in one place avoids divergence.

```
libamethyst/src/modules/atlas_packer.h
libamethyst/src/modules/atlas_packer.cpp
```

```
AtlasPacker
  - AtlasPacker(width, height)
  - packRect(width, height) -> optional<PackedRect>
  - reset()                  // clear all allocations (for re-rasterization)

PackedRect
  - x, y, width, height
```

GlyphAtlas refactored to use AtlasPacker internally. No API changes to GlyphAtlas.

### SvgAtlas

```
libamethyst/src/modules/svg_atlas.h
libamethyst/src/modules/svg_atlas.cpp
```

```
SvgAtlas
  - loadSvg(path, width, height) -> SvgIcon*
  - loadSvgFromMemory(data, len, width, height) -> SvgIcon*
  - getIcon(name) -> SvgIcon*
  - isDirty() / clearDirty()
  - getPixels() / getWidth() / getHeight()
  - setTextureId() / getTextureId()
```

```
SvgIcon
  - atlasX, atlasY       (position in atlas)
  - width, height         (pixel dimensions)
  - uvRect                (normalized [0,1] coordinates for shader)
  - name                  (lookup key)
```

Key properties:
- RGBA (4 channels), 2048x2048 default
- Keyed by `(name/hash, width, height)` for cache dedup
- On-demand rasterization: first request rasterizes + packs, subsequent requests return cached
- Dirty flag set on new entries, backend syncs next frame

### Registration / API

SVGs are loaded on-demand, similar to how glyphs work. Components request icons from the atlas:

```cpp
// ImageLabel gains an svg-aware setter
imageLabel.setSvgData(svgStringData);
// Internally: hash the SVG data, check atlas cache,
// rasterize at component's absoluteSize if miss,
// store atlas region, set textureId + uvRect

// Or explicit pre-loading for bulk icon sets
svgAtlas->loadSvg("assets/icons/settings.svg", 24, 24);
svgAtlas->loadSvg("assets/icons/close.svg", 24, 24);
// Then reference by name
imageLabel.setSvgIcon(svgAtlas->getIcon("settings"));
```

Resolution selection: rasterize at the component's resolved pixel size. Since UI icons are typically fixed-size (e.g. `size=[0, 24, 0, 24]`), this is usually a one-shot rasterization. For responsive-sized SVGs, snap to discrete size buckets (nearest multiple of 8px) to avoid thrashing the atlas on sub-pixel layout changes.

### Integration points

**DrawContext** - add `SvgAtlas *svgAtlas` pointer.

**Backend (VkBackend)**:
- `createSvgAtlasTexture(width, height)` using VK_FORMAT_R8G8B8A8_UNORM
- `uploadSvgAtlasData(cmd, pixels, width, height)` using same staging buffer pattern as glyph atlas
- Sync in `beginFrame()` when `svgAtlas->isDirty()`

**ImageLabel / ImageButton** - add `setSvgData()` and `setSvgIcon()` methods. Internally sets `textureId` to the SVG atlas texture and stores the UV sub-region for the shader.

**Canvas** - future work. Could add `drawSvg(svgData, position, size)` to the canvas command list, which would rasterize to the atlas and emit a textured quad.

### UV sub-region support

Current ImageLabel uses the full texture. For atlas-packed icons, we need UV sub-region support.

Add UV rect to InstanceData (or repurpose existing `shapeData` bits when `textureId` is set) so the vertex/fragment shader can sample a sub-region of the atlas. This also benefits the glyph atlas which already encodes atlas positions.

### Color tinting

For monochrome icons (most UI icons): rasterize as white-on-transparent, multiply by `fillColor` in the fragment shader. The existing `fillColor` field in InstanceData handles this.

For full-color SVGs: render at natural colors, use white fillColor (no tint).

Convention: monochrome icons loaded as white. A flag or naming convention (`icon_mono_*.svg`) could distinguish.

### Dirty system

- `SvgAtlas::isDirty()` returns true when new icons are packed since last `clearDirty()`
- Backend checks dirty flag each frame in `beginFrame()`, uploads full atlas if dirty
- Future optimization: dirty-rect tracking to upload only changed regions (same TODO as glyph atlas)
- On DPI/scale change: `SvgAtlas::reset()` clears all packing, re-rasterizes all registered SVGs at new scale, marks dirty for full re-upload

## Implementation Steps

### Phase 1: AtlasPacker extraction
1. Extract skyline packing from GlyphAtlas into AtlasPacker
2. Refactor GlyphAtlas to use AtlasPacker
3. Verify existing text rendering still works

### Phase 2: Core SvgAtlas
4. Add lunasvg as CMake dependency (FetchContent)
5. Implement SvgAtlas class using AtlasPacker
6. `loadSvg()` / `loadSvgFromMemory()` parse + rasterize + pack
7. Cache and dirty flag tracking

### Phase 3: Backend integration
8. Add `createSvgAtlasTexture()` to VkBackend (RGBA format)
9. Add `uploadSvgAtlasData()` with staging buffer
10. Register atlas texture, sync dirty state in `beginFrame()`

### Phase 4: UV sub-region support
11. Add UV rect to InstanceData
12. Update vertex/fragment shaders to use UV rect when sampling textured quads
13. Consider unifying glyph atlas UV handling with the same mechanism

### Phase 5: Component integration
14. Add `SvgAtlas*` to DrawContext
15. Wire up in Window/testapp initialization
16. Add `setSvgData()` / `setSvgIcon()` to ImageLabel
17. Add testapp demo with icon grid

## Open questions

- **Atlas overflow**: Grow atlas (reallocate larger texture) or error? GlyphAtlas currently errors.
- **Icon size variants**: Cache multiple sizes per icon, or re-rasterize on demand? One size per load call is probably fine for fixed-size icons.
- **Multi-atlas**: Grayscale glyph atlas + RGBA SVG atlas = two texture slots. Fine. Could unify later (wastes 3x memory for glyphs though).
