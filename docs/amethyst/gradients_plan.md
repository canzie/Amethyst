# Gradient Rendering Plan

Status: PLANNED. Builds on the implemented backend resource layer (see `backend_resource_redesign.md`); this is the step 5
follow-up. No code exists yet.

## Goal

Gradient fills for rect/circle backgrounds, defined exactly like colors - inline in code or in `.ams` - with no new backend
work. A gradient is a value the user puts wherever a `Color3`/`Color4` goes; everything else (storage, upload, lifetime,
dedup) is internal.

```cpp
Color3 bg = Color3::fromGradient(Gradient::linear(45.0f, {{0.0f, Color3(...)}, {1.0f, Color3(...)}}));
```

```css
background-color: linear-gradient(45deg, #282828, #1a1a1a);
```

## Decisions (settled 2026-06-12)

- **Stop SSBO, not a ramp texture.** Exact interpolation, 16-byte edits via dirty range (matters for future :hover
  animation), does not burn `textureId` (image + gradient can coexist), and matches the arena idiom. The ramp texture
  remains a possible *optimization* behind the same API if the fragment stop-walk ever shows up in profiles - it is not an
  architecture choice anymore.
- **Fixed-capacity records, not variable-length runs.** No glyph-style slice indirection or block allocator: one gradient is
  one fixed-stride SSBO element, handle == slot index, stable forever. Most gradients have 2 stops; wasting the tail of an
  8-stop record costs ~56 bytes per gradient, irrelevant at UI scales (1000 gradients = 80 KB).
- **One global buffer, binding 5.** Theme gradients are created at parse time, before any registry exists, and are shared
  across layers; per-registry blocks would duplicate them. Global means: no `gradientBase` in `FrameDrawEntry`, no new push
  constant, plain index in the shader.
- **Gradients ride on the color type.** CSS precedent is the `background:` shorthand (a gradient is technically an
  `<image>` there); the closer model is SVG's *paint*: one value that is either a flat color or a gradient reference.
  Amethyst styles every surface through `Color3`/`Color4`, so the value lives there.
- **The color stores `shared_ptr<const GradientDef>`.** Copies are refcount bumps, move is a move; every copy of a theme
  color shares one def and therefore one GPU record. The def is pure data - no GPU slot, no renderer knowledge in
  `color.h`. The slot association and lifetime live entirely in `GradientBuffer`: a pointer-keyed slot table plus one
  `weak_ptr` per live slot; a sweep frees slots whose def expired. Nothing user-visible to register or destroy. Two
  *independently constructed* identical defs still get separate records - 80 wasted bytes, accepted.
- **Derived rgb rule.** When a color carries a gradient, its rgb is always *derived* by `fromGradient()` (mean of stop
  colors), never user-supplied and never arbitrary. This keeps `operator==`/diffing deterministic and provides the flat
  fallback for paths that cannot take the encoding (canvas primitives) or are not wired yet.
- **Flat color stays a modulator.** A gradient-filled instance writes `fillColor = white * (1 - backgroundTransparency)` and
  the shader multiplies the gradient result by `fragFillColor`. `background-transparency`, hover auto-darkening and any
  other flat-color manipulation keep working on gradients multiplicatively, with zero "but what if it is a gradient"
  branches.

## GPU side

```cpp
struct GradientStop {
    uint32_t color; // packed RGBA
    float t;        // 0..1 along the gradient axis
};

struct GpuGradient {
    uint32_t typeAndAngle;  // type (8 bits: LINEAR; RADIAL reserved) | angle as half (16) | 8 spare
    uint32_t stopCount;
    uint32_t pad[2];        // reserved for radial center/radius
    GradientStop stops[8];  // MAX_GRADIENT_STOPS = 8
};
// 80-byte stride, std430-clean (all 4-byte scalars)
```

- New `layout(std430, binding = 5)` readonly buffer in `ui.fs.glsl`.
- New `INSTANCE_FLAG_GRADIENT` bit; gradient slot index in `shapeData[1]` (`shapeData[0]` stays the text slice; canvas
  primitives use all four words and are excluded - they fall back to the derived flat rgb).
- Fragment path: project the local fragment position onto the angle axis to get t, walk at most 8 stops, `mix` between the
  two neighbors, multiply by `fragFillColor`. Placement relative to the border/texture mix follows the existing texture-mix
  pattern; exact position decided with a visual check during implementation.
- Descriptor layout gains binding 5 in `createPipeline`/the layout array; because buffers self-register their binding on
  `createBuffer`/`growBuffer`, nothing else backend-side changes. Zero new virtuals, zero new members.

## CPU side

### GradientDef (new, immutable)

```cpp
struct GradientDef {
    GradientType type;            // LINEAR (RADIAL later)
    float angleDegrees;
    // fixed inline array + count, mirroring the GPU record; no heap inside the def
    std::array<GradientStop, MAX_GRADIENT_STOPS> stops;
    uint32_t stopCount;
};
```

Created only through factories (`Gradient::linear(angle, stops)`) returning `shared_ptr<const GradientDef>`; validated and
normalized there (stops sorted by t, clamped to [0,1], count clamped to 8 with a warning). Immutable after construction.

### Color3 / Color4 extension

- New member: `std::shared_ptr<const GradientDef> gradient` (null = flat color, the overwhelmingly common case). Default
  copy/move work as-is. Size: Color3 12 -> 28 (32 with 8-byte alignment), Color4 16 -> 32; CPU-side style structs only -
  `InstanceData` and `CharacterInstance` store packed `uint32` colors, so nothing GPU-facing grows.
- `Color3::fromGradient(shared_ptr<const GradientDef>)` computes the derived rgb (mean of stops) and stores the def.
- `operator==`: rgb compare plus gradient equivalence (same pointer, or both non-null with equal definition contents).
- Color arithmetic (lerp, scale, darken) produces flat colors and drops the def; hover-style effects act through the
  modulator instead.
- `propIsSet` is untouched (NaN sentinel rgb never coexists with a def, since `fromGradient` always writes real rgb).

### GradientBuffer (new module, `src/modules/gradient_buffer.h/.cpp`)

Global, owned by `GpuResourceHub`, created in `init` at binding 5:

- `GpuArena`-backed buffer: initial 64 records (5 KB), `GrowthPolicy::doubleUntil(4096 records)`; growth rebinds via the
  recorded `shaderBinding` like every other buffer.
- `resolve(const shared_ptr<const GradientDef> &) -> uint32_t`: pointer-keyed slot table
  (`unordered_map<const GradientDef *, uint32_t>`). On miss: take a slot from the free list (or append), write the
  `GpuGradient` record, mark a `DirtyRange`, store a `weak_ptr` for the slot.
- `sweep()`: called from `GpuResourceHub::sync`; any slot whose `weak_ptr` expired goes back to the free list and its map
  entry is erased. Refcounting does all lifetime work - when the last color holding a def dies, its record is reclaimed on
  the next sweep.
- Synced in `GpuResourceHub::sync` like the other arenas: one `uploadBufferRange` per dirty range per frame.
- On the plan's `SideBuffer` question: with fixed-stride records this module degenerates to a flat slot table + intern map.
  That is *simpler* than the planned `SideBuffer` (no block allocator, no relocation). Do NOT force a `SideBuffer`
  abstraction out of it; extract one later only when a real variable-length consumer appears. The redesign doc's step 5 is
  amended accordingly.

### Draw path

Single wiring point: `UIObject::createInstanceData()` (where `fillColor` is built today). If
`m_baseStyle.backgroundColor.gradient` is set and the primitive supports it: set `INSTANCE_FLAG_GRADIENT`, write
`GradientBuffer.resolve(def)` into `shapeData[1]`, write the white-times-alpha modulator as fillColor. Otherwise: exactly
today's path using the (derived or real) rgb. Components never see any of this.

Scope: rect and circle background fills. Excluded for now: canvas primitives (shapeData exhausted - flat fallback), border
color (stays flat), text fill (`PRIMITIVE_TEXT`; `shapeData[1]` is free there so gradient text color is possible later,
explicitly out of scope now).

### .ams parser

- Grammar: `linear-gradient( <angle>deg , <color-stop> [, <color-stop>]+ )` where `<color-stop>` is any existing color form
  with an optional `<number>%` position; omitted positions distribute evenly (CSS behavior). 2..8 stops.
- Accepted by every property that parses into a color (the value type carries it; per-property support is implicit).
- Lives in the parser module per the format-parsing convention, producing a shared `GradientDef` and a `fromGradient`
  color. A theme rule parses once and every consumer shares that pointer, so one rule = one record; only separately
  *written* identical gradients duplicate a record.

## Files to touch

- `src/modules/color.h` (+ a small .cpp if the deep-copy lands out of line) - def pointer, derived rgb, factories
- `src/modules/gradient_buffer.h/.cpp` (new) - records, intern table, dirty range
- `src/rendering/gpu_resource_hub.h/.cpp` - own the GradientBuffer, init at binding 5, sync it
- `src/rendering/instance_data.h` - `INSTANCE_FLAG_GRADIENT`, `setGradientSlot` (shapeData[1])
- `src/components/ui_object.cpp` - the single draw-path branch in `createInstanceData`
- `.ams` parser module - `linear-gradient(...)` grammar
- `backends/amethyst__vk13_glfw.cpp` - binding 5 in the descriptor set layout (one array entry)
- `backends/shaders/glsl/ui.fs.glsl` - SSBO declaration + stop walk + modulation

## Implementation order

1. `GradientDef` + color extension + factories. Pure CPU, unit-testable (==, deep copy, derived rgb, normalization).
2. `GradientBuffer` + hub init/sync + descriptor binding 5 + shader stop-walk behind the flag. Hand-written test gradient
   via code API.
3. `createInstanceData` wiring + modulator. Visual check: gradient frame, transparency on a gradient, hover-darkened
   gradient button, gradient + bindless image on one rect.
4. `.ams` grammar + theme usage.
5. Validation: flat-color rendering must be pixel-identical to before (the gradient branch must be completely inert when no
   def is set).

## Open questions (decide during implementation)

- Interpolation space for the stop mix (sRGB vs linear); follow whatever the shader's existing color math assumes.
- Exact placement of the gradient mix relative to the border mix in the fragment shader (visual decision).
- Radial gradients: header has reserved space; out of scope until linear ships.
- Animated gradients (stop edits per frame): a new shared def per change means a record allocated per frame, reclaimed one
  sweep later - functional but churny. If animation becomes real, add a mutable-gradient path that rewrites its existing
  slot in place; do not pre-build it.
