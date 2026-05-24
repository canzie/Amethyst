# AmethystBackend

`AmethystBackend` is the abstract interface that rendering backends must implement. It lives in `libamethyst/include/amethyst/amethyst_backend.h`.

## Interface

```cpp
class AmethystBackend {
  public:
    virtual ~AmethystBackend() = default;

    virtual void createAtlasTexture(uint32_t width, uint32_t height) = 0;
    virtual void uploadAtlasData(void *cmdBuffer, const uint8_t *pixels, uint32_t width, uint32_t height) = 0;
    virtual AmTextureId getAtlasTextureId() const = 0;

    virtual void createSvgAtlasTexture(uint32_t width, uint32_t height) = 0;
    virtual void uploadSvgAtlasData(void *cmdBuffer, const uint8_t *pixels, uint32_t width, uint32_t height) = 0;
    virtual AmTextureId getSvgAtlasTextureId() const = 0;
};
```

## Methods

### createAtlasTexture

Creates the GPU texture for the glyph atlas. Called once during `AmethystContext::init()`.

- `width` / `height` - atlas dimensions in pixels (currently 1024x1024, single channel R8)

### uploadAtlasData

Uploads glyph atlas pixel data to the GPU. Called by `AmethystContext::sync()` when new glyphs have been rasterized.

- `cmdBuffer` - backend-specific command buffer (`VkCommandBuffer` for Vulkan, `nullptr` acceptable for backends that don't need one like OpenGL)
- `pixels` - R8 pixel data, `width * height` bytes
- `width` / `height` - atlas dimensions

### getAtlasTextureId

Returns the `AmTextureId` assigned to the glyph atlas texture. Used internally to bind glyphs to the correct texture slot.

### createSvgAtlasTexture

Creates the GPU texture for the SVG atlas. Called once during `AmethystContext::init()`.

- `width` / `height` - atlas dimensions in pixels (currently 2048x2048, RGBA8)

### uploadSvgAtlasData

Uploads SVG atlas pixel data to the GPU. Called by `AmethystContext::sync()` when new SVGs have been rasterized.

- `cmdBuffer` - same as `uploadAtlasData`
- `pixels` - RGBA8 pixel data, `width * height * 4` bytes
- `width` / `height` - atlas dimensions

### getSvgAtlasTextureId

Returns the `AmTextureId` assigned to the SVG atlas texture.

## Implementing a Backend

A backend implementation must:

1. Inherit from `AmethystBackend`
2. Allocate GPU textures in the `create*` methods
3. Register them in the bindless descriptor set and return valid `AmTextureId` values
4. Handle pixel uploads using the provided command buffer (or equivalent mechanism)

### Vulkan Example

The reference implementation is `VkBackend` in `backends/amethyst__vk13_glfw.h`. It:

- Creates `VkImage` + staging buffer in `createAtlasTexture` / `createSvgAtlasTexture`
- Casts `void *cmdBuffer` to `VkCommandBuffer` in upload methods
- Records pipeline barriers, buffer-to-image copies, and layout transitions
- Registers images in a bindless descriptor array (1024 slots)

### OpenGL Example (hypothetical)

An OpenGL backend would:

- Create `glTexImage2D` textures in `create*`
- Ignore the `cmdBuffer` parameter (pass `nullptr`)
- Call `glTexSubImage2D` in upload methods
- Return texture name wrapped in `AmTextureId`

## AmethystContext Integration

Users don't call backend methods directly. `AmethystContext` handles the lifecycle:

```cpp
Amethyst::AmethystContext amCtx;
amCtx.loadFont("font.ttf");
amCtx.init(backend);          // calls create* and get* internally

// render loop
amCtx.sync(cmdBuffer);        // calls upload* when atlases are dirty
amCtx.draw(window);
```
