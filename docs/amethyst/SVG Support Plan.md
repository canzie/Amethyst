# SVG Support Plan

## Goal

Add SVG icon/image support to Amethyst by rasterizing SVGs to an atlas texture at load time, fitting cleanly into the existing instanced-quad + bindless-texture pipeline.

## Approach: Rasterize-to-Atlas

SVGs are parsed and rasterized to RGBA bitmaps at a requested pixel size, packed into an atlas texture, and referenced via `AmTextureId` like any other image. No architectural changes to the render loop, draw ordering, or pipeline.

This mirrors how GlyphAtlas works: CPU-side atlas with skyline packing, dirty flag, backend uploads when dirty.

## Why not GPU-native path rendering

We explored three GPU-native approaches and ruled them out for now:

- **SDF composition** (evaluate SVG primitives as SDF chains in fragment shader) fits the architecture but cubic beziers have no closed-form SDF, making `<path>` elements impractical. Works for basic shapes but not real icon SVGs.
- **Compute-based rasterization** (Vello-style) produces a texture anyway, so the integration point is identical to CPU rasterization but with enormous implementation cost.
- **Stencil-and-cover** requires per-path draw calls and a stencil attachment, breaks the single-pipeline instanced draw model.

The atlas approach handles 100% of SVG icon use cases. Complex animated/dynamic SVGs are out of scope (and rare in UI).

## Library Choice

**lunasvg** (recommended)
- Full SVG 1.1 support (paths, gradients, transforms, clip-paths, groups)
- C++ library, CMake-friendly, actively maintained
- Rasterizes to RGBA bitmap at arbitrary size
- ~5k LOC, no dependencies (bundles its own plutovg rasterizer)
- MIT license

Alternatives considered: nanosvg (header-only but limited, no CSS, poor gradient support), plutosvg (smaller but less SVG coverage).

## Architecture

### New files

```
libamethyst/src/modules/svg_atlas.h
libamethyst/src/modules/svg_atlas.cpp
```

### SvgAtlas class

Follows the same pattern as GlyphAtlas: CPU-side RGBA atlas with skyline packing, dirty tracking, backend syncs to GPU.

```
SvgAtlas
  - loadSvg(path, width, height) -> SvgIcon*
  - loadSvgFromMemory(data, size, width, height) -> SvgIcon*
  - getIcon(id) -> SvgIcon*
  - isDirty() / clearDirty()
  - getPixels() / getWidth() / getHeight()
  - setTextureId() / getTextureId()
```

```
SvgIcon
  - atlasX, atlasY       (position in atlas)
  - width, height         (pixel dimensions)
  - id                    (lookup handle)
```

Key differences from GlyphAtlas:
- RGBA (4 channels) not grayscale (1 channel), since SVGs have color
- Larger default atlas (2048x2048), icons are bigger than glyphs
- Keyed by path/name + size, not codepoint + size
- One-shot load at init, not on-demand during draw

### Integration points

**DrawContext** - add `SvgAtlas *svgAtlas` pointer so components can look up icons.

**Backend (VkBackend)** - add parallel to glyph atlas:
- `createSvgAtlasTexture(width, height)` - RGBA format (VK_FORMAT_R8G8B8A8_UNORM) instead of R8
- `uploadSvgAtlasData(cmd, pixels, width, height)` - same staging buffer pattern
- Sync in `beginFrame()` when `svgAtlas->isDirty()`

**ImageLabel / ImageButton** - no changes needed. User sets `image = svgAtlas->getIcon("settings")->textureId` and the existing textured-quad rendering handles it. The icon's atlas region maps via UV coordinates.

**AML loader** - add `icon="name"` attribute support on ImageLabel/ImageButton that resolves via SvgAtlas lookup.

### UV mapping

Current ImageLabel uses the full texture (`textureId` references a standalone image). For atlas-packed icons, we need UV sub-region support. Two options:

**Option A: Store UV rect in InstanceData**
Add a `uvRect` field (or repurpose existing `shapeData` bits) so the vertex/fragment shader can sample a sub-region of the atlas. This is the clean solution and also benefits the glyph atlas.

**Option B: Separate VkImageView per icon region**
Create a VkImageView with component swizzle for each icon's sub-region. Wasteful (one descriptor slot per icon) but requires zero shader changes.

**Recommendation:** Option A. Add UV rect to InstanceData, similar to how text glyphs already encode their atlas position.

### Color tinting

For monochrome icons (most UI icons), rasterize as white-on-transparent and multiply by `fillColor` in the fragment shader. The existing `fillColor` field in InstanceData already supports this. For full-color SVGs, render at natural colors and use white fillColor (no tint).

A `tintable` flag (or convention: monochrome icons always loaded as white) controls this behavior.

## Implementation Steps

### Phase 1: Core SvgAtlas
1. Add lunasvg as CMake dependency (FetchContent)
2. Implement `SvgAtlas` class with skyline packing (can extract/share packing logic from GlyphAtlas)
3. `loadSvg()` / `loadSvgFromMemory()` parse + rasterize + pack into atlas
4. Dirty flag tracking

### Phase 2: Backend integration
5. Add `createSvgAtlasTexture()` to VkBackend (RGBA variant of glyph atlas)
6. Add `uploadSvgAtlasData()` with staging buffer
7. Register atlas texture via `registerTexture()`, store `AmTextureId`
8. Sync dirty atlas in `beginFrame()`

### Phase 3: UV sub-region support
9. Add UV rect to InstanceData (or a secondary mechanism)
10. Update vertex/fragment shaders to use UV rect when sampling textured quads
11. Update text rendering to use the same UV mechanism (unify with glyph atlas UVs)

### Phase 4: Component integration
12. Add `SvgAtlas*` to DrawContext
13. Wire up in Window/testapp initialization
14. Add `icon` property to ImageLabel for convenient SVG icon display
15. AML loader support for `icon="name"` attribute

### Phase 5: Testing
16. Add SVG loading tests (valid files, corrupt files, atlas overflow)
17. Add testapp demo with icon grid
18. Test DPI-aware re-rasterization

## Re-rasterization strategy

Icons are rasterized at a specific pixel size. On DPI/scale change:
- Clear and re-pack the atlas at the new scale
- All `SvgIcon*` pointers remain valid (atlas positions update)
- Backend re-uploads the full atlas

This is a rare event (display change, window moved to different monitor) so the cost is acceptable.

## Open questions

- **Shared packing logic**: Extract skyline packer from GlyphAtlas into a reusable `AtlasPacker` class? Or keep them separate (simpler, slight duplication)?
- **Atlas overflow**: Grow atlas (reallocate larger texture) or error? GlyphAtlas currently errors on overflow.
- **Icon size variants**: Cache multiple sizes per icon, or re-rasterize on demand? For a retained UI where icons have fixed sizes, one size per load call is probably fine.
- **Multi-atlas**: If we need both a grayscale glyph atlas and an RGBA SVG atlas, that's two texture slots. Fine for now. Could unify into a single RGBA atlas later (wastes 3x memory for glyphs though).
