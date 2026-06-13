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
- **rgb defaults to white and is a tint, not an approximation.** When a color carries a gradient its rgb is `(1,1,1)` by
  default - the multiplicative identity - so the gradient renders exactly as authored. `fromGradient(def, tint)` lets the
  user pass a non-white rgb that multiplies in as a genuine tint, for free. This replaces the earlier "mean of stops" rule,
  which tried to be both the diff key and the value the shader multiplies by, and only avoided double-tinting via a
  gradient-specific branch in the draw path. White is also the flat fallback for paths that cannot take the encoding (canvas
  primitives) or are not wired yet: a flat white rect reads as "gradient unsupported here" rather than a muddy approximation
  that looks intentional. Because every gradient color now shares rgb = white, the gradient-equivalence term in `operator==`
  is *mandatory* (rgb compare alone cannot tell two gradients apart), not just an optimization.
- **Flat color stays a modulator - with no gradient branch.** Default white means the existing flat-color path
  (`fillColor = rgb * (1 - backgroundTransparency)`) already does the right thing for gradients; there is no "write white
  instead of rgb" special case. `background-transparency`, hover auto-darkening and any other flat-color manipulation keep
  working on gradients multiplicatively, with zero "but what if it is a gradient" branches.

## GPU side

```cpp
// 64-byte record: struct-of-arrays so the whole thing is one 64-byte line on AMD / two aligned 32-byte
// sectors on NVIDIA. The earlier interleaved {color, t} layout was 80 bytes -> 3 misaligned NVIDIA sectors
// (an 80-byte stride is not 32-aligned, so successive records straddle).
struct GpuGradient {
    uint32_t header;        // type (8: LINEAR; RADIAL reserved) | stopCount (8) | flags (8) | spare (8)
    float    angle;         // linear axis angle
    uint32_t radialCenter;  // reserved: packHalf2x16(cx, cy)
    float    radialRadius;  // reserved
    uint32_t stopT[4];      // 8 positions, packHalf2x16 -> 2 per word (16 B)
    uint32_t stopColor[8];  // 8 packed RGBA8 colors, 1 per word    (32 B)
};
// 16 words = 64 bytes, std430-clean, 64-byte aligned stride -> no straddle. MAX_GRADIENT_STOPS = 8.
```

`t` drops from float32 to half (`unpackHalf2x16` in the shader); over `[0,1]` half resolves to well under 0.001 (finer
near 0), past sub-pixel for any real gradient. The CPU `GradientDef` keeps float32 `t` as the authoring/normalization form;
the half packing happens only when `resolve()` writes the GPU record.

- New `layout(std430, binding = 5)` readonly buffer in `ui.fs.glsl`.
- New `INSTANCE_FLAG_GRADIENT` bit; gradient slot index in `shapeData[1]` (`shapeData[0]` stays the text slice; canvas
  primitives use all four words and are excluded - they fall back to the flat white rgb).
- Fragment path: project the local fragment position onto the angle axis to get t, walk at most 8 stops (`unpackHalf2x16`
  the position words, `unpackUnorm4x8` the color words), `mix` between the two neighbors, multiply by `fragFillColor`.
  Placement relative to the border/texture mix follows the existing texture-mix
  pattern; exact position decided with a visual check during implementation.
- Descriptor layout gains binding 5 in `createPipeline`/the layout array; because buffers self-register their binding on
  `createBuffer`/`growBuffer`, nothing else backend-side changes. Zero new virtuals, zero new members.

## CPU side

### GradientDef (new, immutable)

```cpp
struct GradientStop {
    uint32_t color;  // packed RGBA8
    float t;         // 0..1, float32 here (authoring form); packed to half when the GPU record is written
};

struct GradientDef {
    GradientType type;            // LINEAR (RADIAL later)
    float angleDegrees;
    // fixed inline array + count; no heap inside the def
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
- `Color3::fromGradient(def, tint = white)` stores the def and sets rgb to the tint (white by default, the multiplicative
  identity). No mean is computed.
- `operator==`: rgb compare plus gradient equivalence (same pointer, or both non-null with equal definition contents). The
  gradient term is *mandatory* - every gradient color shares rgb = white, so rgb alone cannot distinguish two gradients.
- Color arithmetic (lerp, scale, darken) produces flat colors and drops the def; hover-style effects act through the
  modulator instead.
- `propIsSet` is untouched (NaN sentinel rgb never coexists with a def, since `fromGradient` always writes a real tint).

### GradientBuffer (new module, `src/modules/gradient_buffer.h/.cpp`)

Global, owned by `GpuResourceHub`, created in `init` at binding 5:

- `GpuArena`-backed buffer: initial 64 records (5 KB), `GrowthPolicy::doubleUntil(4096 records)`; growth rebinds via the
  recorded `shaderBinding` like every other buffer.
- `resolveShared(const shared_ptr<const GradientDef> &) -> uint32_t`: pointer-keyed slot table
  (`unordered_map<const GradientDef *, uint32_t>`). On miss: take a slot from the free list (or append), write the
  `GpuGradient` record, mark a `DirtyRange`, store a `weak_ptr` for the slot. This is the path for `STATIC` nodes (see
  *Mobility* in the draw path) - interned and shared across every color holding the same def.
- `resolveMutable(const Instance &, const GradientDef &) -> uint32_t`: the `DYNAMIC` path. A private slot keyed by node
  identity, *not* interned, seeded from the def and rewritten in place via `DirtyRange` on change. Animating a dynamic node
  never reallocates per frame and never touches the shared slot any other node may share. Dormant for now - nothing sets
  `DYNAMIC` yet.
- `sweep()`: called from `GpuResourceHub::sync`; any shared slot whose `weak_ptr` expired goes back to the free list and its
  map entry is erased. Refcounting does all lifetime work - when the last color holding a def dies, its record is reclaimed
  on the next sweep. Mutable slots are released when their owning node is destroyed.
- Synced in `GpuResourceHub::sync` like the other arenas: one `uploadBufferRange` per dirty range per frame.
- On the plan's `SideBuffer` question: with fixed-stride records this module degenerates to a flat slot table + intern map.
  That is *simpler* than the planned `SideBuffer` (no block allocator, no relocation). Do NOT force a `SideBuffer`
  abstraction out of it; extract one later only when a real variable-length consumer appears. The redesign doc's step 5 is
  amended accordingly.

### Mobility

`Mobility { STATIC, DYNAMIC }` is a property of `Instance` (base class, default `STATIC`), not of the gradient - it says
whether a node changes at runtime, which is reusable later for batching, transform baking and skip-reupload decisions. It is
the Unreal `Mobility` idea. Nothing sets `DYNAMIC` yet, so it is fully dormant and behavior is identical to today; the
gradient path is its first consumer. A `STATIC` node interns/shares its gradient record (`resolveShared`); a `DYNAMIC` node
gets a private mutable slot (`resolveMutable`) so animating it does not touch other users of the same def. The copy-on-write
is driven by the owning node's mobility, never baked into the shared def.

### Draw path

Single wiring point: `UIObject::createInstanceData()` (where `fillColor` is built today). If
`m_baseStyle.backgroundColor.gradient` is set and the primitive supports it: set `INSTANCE_FLAG_GRADIENT`, write the gradient
slot into `shapeData[1]` (`resolveShared(def)` for a `STATIC` node, `resolveMutable(node, *def)` for a `DYNAMIC` one), and
build `fillColor` through the *unchanged* flat path (`rgb * (1 - backgroundTransparency)`, rgb defaulting to white). There is
no gradient-specific modulator branch. Otherwise: exactly today's path. Components never see any of this.

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

- `src/modules/color.h` (+ a small .cpp if the deep-copy lands out of line) - def pointer, white-default tint, factories
- `src/modules/gradient_buffer.h/.cpp` (new) - records, intern table, mutable slots, dirty range
- `src/rendering/gpu_resource_hub.h/.cpp` - own the GradientBuffer, init at binding 5, sync it
- `src/rendering/instance_data.h` - `INSTANCE_FLAG_GRADIENT`, `setGradientSlot` (shapeData[1])
- `src/components/instance.h` - `Mobility { STATIC, DYNAMIC }` member, default `STATIC`
- `src/components/ui_object.cpp` - the single draw-path branch in `createInstanceData`
- `.ams` parser module - `linear-gradient(...)` grammar
- `backends/amethyst__vk13_glfw.cpp` - binding 5 in the descriptor set layout (one array entry)
- `backends/shaders/glsl/ui.fs.glsl` - SSBO declaration + stop walk + modulation

## Implementation order

1. `GradientDef` + color extension + factories. Pure CPU, unit-testable (==, deep copy, white-default tint, normalization).
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
- Animated gradients: the mechanism is settled (`Mobility::DYNAMIC` -> `resolveMutable`, a private slot rewritten in place,
  no per-frame realloc and no contamination of shared static slots) but is left dormant - nothing sets `DYNAMIC` yet. What
  is *not* decided: the animation/tween API that would flip a node to `DYNAMIC` and the copy-on-write moment when a node
  starts animating a currently-shared static def. Design that with the animation feature, not now.
