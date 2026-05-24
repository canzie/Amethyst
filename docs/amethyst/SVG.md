# SVG Support

Amethyst supports rendering SVG icons and graphics via rasterization to an RGBA texture atlas. SVGs are parsed and rasterized using [lunasvg](https://github.com/sammycage/lunasvg), then packed into a shared atlas using a skyline bin-packing algorithm. The atlas is sampled in the fragment shader with UV remapping per instance.

## How It Works

1. User provides raw SVG markup to an `ImageLabel` or `ImageButton` (via constructor or `setSvg()`)
2. On first draw, the SVG is rasterized at the element's pixel size and packed into the atlas
3. A content hash (SVG data + render dimensions) deduplicates identical SVGs automatically
4. The fragment shader samples the RGBA atlas and tints with `imageColor`

## Usage

### ImageLabel with SVG

```cpp
auto *icon = parent->add<Amethyst::ImageLabel>(svgString);
icon->size = Amethyst::UDim2::fromOffset(32.0f, 32.0f);
icon->imageColor = {1.0f, 0.5f, 0.5f, 1.0f}; // tint color
icon->backgroundTransparency = 1.0f;
icon->markDirty();
```

### ImageButton with SVG

```cpp
auto *btn = parent->add<Amethyst::ImageButton>(svgString);
btn->size = Amethyst::UDim2::fromOffset(48.0f, 48.0f);
btn->imageColor = {1.0f, 1.0f, 1.0f, 1.0f};
btn->backgroundColor = {0.2f, 0.2f, 0.25f};
btn->cornerRadius = 8.0f;
btn->markDirty();
```

### Changing SVG at runtime

```cpp
icon->setSvg(newSvgString); // re-rasterizes and marks dirty
```

## SVG Requirements

- SVGs should use `fill="white"` for proper tinting (the shader multiplies texture color by `imageColor`)
- The SVG is rasterized at the element's absolute pixel size, so sizing the element correctly matters
- Complex SVGs with many paths work fine but consume more atlas space
- The atlas is 2048x2048 RGBA (16MB). If it fills up, new SVGs will fail to pack

## Architecture

- `SvgAtlas` owns the CPU-side pixel buffer and packing state
- `AtlasPacker` (shared with `GlyphAtlas`) handles skyline rect packing
- `PRIMITIVE_SVG` in the shader does UV remapping (like text) but samples RGBA and multiplies by `fillColor`
- `AmethystContext` owns the `SvgAtlas` and handles GPU texture creation/upload via `AmethystBackend`

## Caching

SVGs are cached by a hash of their content string combined with the render dimensions. The same SVG rendered at different sizes will occupy separate atlas entries. There is no eviction; the atlas grows until full.
