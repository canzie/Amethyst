# Backend GPU Resource Redesign Plan

## Goal

Adding a GPU resource to the backend today means hand-writing virtuals, member blocks, creation code, upload code, growth
code, descriptor wiring and teardown, and doing it slightly differently each time. This plan replaces all of that with a
uniform handle-based resource layer: the abstract `AmethystBackend` exposes **generic buffer and texture primitives**, and
all policy (preallocation, suballocation, growth, dirty tracking) lives in **one core-side layer** that every resource
shares. The validating use case is the upcoming **gradient rendering** resource (stop SSBO and/or ramp texture), which must
be addable with a struct, a table entry and a shader binding - nothing else.

This is a plan only, and its scope is the **resource layer itself** - no gradient code. Gradients are just the use case
kept in view; their final encoding (CPU-baked ramp texture, a stop SSBO, or the compact `shapeData` packing) is chosen
after this refactor lands. Do not implement until the worked example at the bottom reads as obviously cheap.

---

## Status: IMPLEMENTED (2026-06-12, steps 1-4)

The refactor landed on `backend_refactor` as designed, with the following deviations and resolutions:

- **`GpuArena` holds no CPU mirror and no `DirtyRange`.** Producers stay the canonical CPU data and dirty tracking
  (`GeometryRegistry::m_sortedBuffer`, `GlyphBuffer`'s three mirrors); `GpuArena::upload()` takes caller pointers, so the
  doubled-host-memory cost this plan accepted never materialized. `GpuArena` is pure bookkeeping: `AmBufferId`, byte-granular
  free list, tail bump, `GrowthPolicy { maxBytes }` (0 = fixed). Grow-copies happen backend-side, old mapped to new mapped,
  and the descriptor rebind uses the `shaderBinding` recorded in the buffer's desc.
- **The draw list is passed explicitly**: `record(cmd, const FrameDrawList &)`, sourced from `AmethystContext::getDrawList()`.
  `FrameDrawList` lives in `rendering/frame_draw_list.h` and carries `indexBuffer` plus per-registry
  `{ firstInstance, instanceCount, glyphBase, lineBase, sliceBase }`. App loops reorder to `draw -> sync -> record` - required,
  since uploads moved out of `record` into `sync` and would otherwise lag a frame.
- **`setDestroyCb` is deleted, but a core-side hook remains**: `~GeometryRegistry` notifies `GpuResourceHub::active()`, a
  static pointer registered by `GpuResourceHub::init` (consistent with the registry list itself being static). The hub frees
  the registry's blocks into the arena free lists; no backend involvement.
- **Renames**: `VkBackend` -> `AmVulkanBackend`, `VulkanInitInfo` -> `AmVulkanInitInfo`, `GLFWInitInfo` -> `AmGlfwInitInfo`
  (nothing in Amethyst may carry a `Vk`/`Vulkan`/`GLFW` prefix that reads as a third-party API type); the backend's record
  tables are plain `BufferRecord` / `TextureRecord` inside the namespace.
- **Texture handles**: `AmTextureId` doubles as the resource handle as planned; the backend keys `TextureRecord`s by bindless
  slot in a map, so externally registered textures (`registerTexture`) share the same id space without records.
- **Open questions resolved**: the shared 10 MB cap became per-arena `GrowthPolicy` (instances 8 MB doubling to 64 MB,
  binding 0; glyph/line/slice arenas sized as multiples of one `GlyphBuffer` block, bindings 2-4); `FrameDrawList` landed
  inside step 4, so `record` never iterated registries in a transitional state.
- `AmethystContext` gained `getGlyphAtlasTexture()` (the test app's atlas debug tab previously read it off the backend).

Steps 5-6 (gradients on a new `SideBuffer`, optional `GlyphBuffer` rebase) remain future work as planned.

---

## What is bespoke today

### The interface scales linearly in virtuals

`amethyst_backend.h:19-25` is six virtuals for two textures: `createAtlasTexture` / `uploadAtlasData` /
`getAtlasTextureId`, then the same triple again with `Svg` in the name. Every new texture costs three more virtuals; every
new buffer would cost its own ad-hoc set (or, as actually happened with the glyph SSBOs, gets smuggled around the
interface entirely - see below). `AmethystContext::init` and `sync` (`amethyst_context.cpp`) then repeat the per-resource
pattern once more: one `create*` + `setTextureId` pair per atlas in `init`, one `isDirty -> upload* -> clearDirty` block
per atlas in `sync`.

### The Vulkan backend duplicates ~110 lines per texture

- `amethyst__vk13_glfw.h:128-150`: ten members per atlas (image, memory, view, sampler, texture id, staging buffer,
  staging memory, staging mapped pointer, width, height), copy-pasted for the SVG atlas. Twenty members for two textures.
- `createAtlasTexture` (`amethyst__vk13_glfw.cpp:1252-1357`) and `createSvgAtlasTexture` (`cpp:1414-1519`) are ~105 lines
  each and differ only in `VK_FORMAT_R8_UNORM` vs `VK_FORMAT_R8G8B8A8_UNORM` and bytes-per-pixel.
- `uploadAtlasData` (`cpp:1359-1412`) and `uploadSvgAtlasData` (`cpp:1521-1570`) duplicate the same barrier + copy +
  barrier sequence. Both upload the **entire** atlas every time (TODO at `cpp:1361`); there is no dirty-rect plumbing
  because each upload path is bespoke and adding it would mean writing it twice.
- `shutdown` (`cpp:253-347`) is ~95 lines of member-by-member destruction. Forgetting one line when adding a resource is a
  silent leak.

### Six named arenas, four named free lists, one growth function with a pointer switch

- `amethyst__vk13_glfw.h:153-162`: six named `BufferArena` members (`m_staticArena`, `m_dynamicArena`, `m_streamArena`,
  `m_glyphArena`, `m_lineArena`, `m_sliceArena`). `m_streamArena` is declared but never used. Each suballocated arena
  drags its own named free list (`h:169-172`).
- `allocateBufferArenas` (`cpp:480-512`) hand-writes a capacity formula per arena, reaching into core internals for
  constants (`4 * GlyphBuffer::GLYPH_CAPACITY * sizeof(GlyphQuad)` at `cpp:499`).
- `reallocBufferArena` (`cpp:940-1012`) maps an arena back to its descriptor binding by **pointer comparison**
  (`cpp:987-995`: `if (&arena == &m_glyphArena) binding = 2; else if ...`). The instance arena (`m_dynamicArena`,
  binding 0) is silently the `binding = 0` default of that switch. Adding an arena and forgetting this switch rebinds the
  wrong descriptor after growth.
- Growth policy is scattered: per-registry allocation doubling in `updateInstances` (`cpp:551`), arena doubling with a
  10 MB cap in `reallocBufferArena` (`cpp:943-948`, cap shared by all arenas regardless of what they store), fixed
  capacity + compact-once in `GlyphBuffer` (`glyph_buffer.h:150-152`), and atlases that can never grow at all.

### The glyph SSBOs bypass the interface entirely

The batched-text side buffers are the newest resource and the clearest symptom. They are **not in `AmethystBackend` at
all**:

- `VkBackend::record` reaches into core via `registry->getGlyphBuffer()` (`cpp:372-378`).
- `obtainTextAllocation` (`cpp:1041-1073`) and `updateTextBuffers` (`cpp:1075-1115`) hard-code core element types and
  capacities (`GlyphQuad`, `GlyphLine`, `GlyphSlice`, `GLYPH_CAPACITY`...).
- `AmethystContext::sync` knows nothing about them; atlases sync through the interface, text buffers sync inside the
  backend's `record`. Two resources, two completely different data paths.
- Per-registry cleanup needs the `GeometryRegistry::setDestroyCb` hook (`cpp:250`) so the backend can free what core
  destroyed - an ownership inversion that exists only because the suballocation map lives on the wrong side.

The `GlyphBuffer` internals themselves (`BlockAllocator` + CPU mirror + stable handles + `DirtyRange`) are exactly the
right shape - the problem is that this machinery is welded to glyphs instead of being the shared substrate.

### Descriptor bindings are hardcoded in five places

Binding numbers live in: the layout in `createPipeline` (`cpp:689-709`), the initial writes in `allocateDescriptorSet`
(`cpp:821-865`), the rebind switch in `reallocBufferArena` (`cpp:987-995`), `ui.vs.glsl:31,35`, and `ui.fs.glsl:22,36,40`.
A new SSBO touches all five.

### One-off upload machinery

`uploadToGpu` (`cpp:590-658`) creates a transient command pool + staging buffer and `vkQueueWaitIdle`s per call. Currently
only used for the 24-byte index buffer, but it is the only device-local upload path, so the next device-local resource
will either reuse this stall or grow yet another bespoke path.

**Net cost of adding one gradient SSBO today:** new arena member + free list + capacity formula + binding case in the
realloc switch + layout binding + descriptor write + shutdown entries + an obtain/update/free trio + `record` wiring +
push-constant offset. Roughly ten touch points across two files, none of them checked by the compiler.

---

## Options considered

### A. Generic device primitives in the backend + shared resource layer in core (recommended)

The backend interface shrinks to typed `createBuffer` / `uploadBufferRange` / `growBuffer` / `destroyBuffer` and
`createTexture` / `uploadTexture` / `destroyTexture`. Everything policy-shaped (preallocation table, suballocation, free
lists, growth decisions, dirty ranges, per-registry bookkeeping) moves into one core-side layer built on those primitives.

- Pros: one implementation of create/upload/grow/destroy per primitive kind, ever. New resource = struct + table entry +
  shader binding. Backends become small and dumb. The glyph path becomes first-class instead of a bypass. Policy is
  uniform and lives where the data producers live.
- Cons: core keeps CPU mirrors and the backend memcpys from them on upload, rather than core writing mapped memory
  directly. This is not a regression: it is exactly what `updateInstances` and `updateTextBuffers` already do today
  (registry allocation vector / GlyphBuffer mirrors -> memcpy -> flush). The mirror does double host memory for large
  arenas (an 8 MB instance mirror plus its GPU copy); accept that as the price of keeping core backend-agnostic, and do not
  "optimize" it by leaking a mapped pointer into core - that pointer is backend-owned and frame-versioned.

### B. Keep bespoke virtuals, factor helpers inside VkBackend

Deduplicate `create*AtlasTexture` etc. behind private helpers but keep the interface as-is.

- Pros: no interface change.
- Cons: the interface still grows three virtuals per resource, `AmethystContext` still grows per-resource plumbing, and
  the glyph bypass stays a bypass. This fixes the copy-paste, not the architecture. Rejected.

### C. Full command-stream abstraction

Backend receives an abstract frame description including resource ops as commands (ImGui-style draw data).

- Pros: maximal decoupling, trivially testable.
- Cons: a much larger rewrite for one backend, and it discards the cheap in-place incremental uploads the retained design
  is built around. Rejected for now; the `FrameDrawList` below is a small step in this direction that we can extend later
  if a second backend ever materializes.

**Decision: A.** It deletes the most duplication for the least churn, and it is the only option under which the gradient
resource is genuinely a three-touch-point addition.

---

## The design

### Handles and descriptors

In `components/common.h`, next to `AmTextureId`:

```cpp
struct AmBufferId {
    uint32_t id = UINT32_MAX;
    bool isValid() const { return id != UINT32_MAX; }
};

enum class AmBufferUsage { STORAGE, INDEX };
enum class AmBufferMemory { DEVICE_LOCAL, HOST_VISIBLE };
enum class AmTextureFormat { R8, RGBA8 };

struct AmBufferDesc {
    size_t initialCapacity = 0;
    AmBufferUsage usage = AmBufferUsage::STORAGE;
    AmBufferMemory memory = AmBufferMemory::HOST_VISIBLE;
    uint32_t shaderBinding = UINT32_MAX; // descriptor slot, UINT32_MAX = not shader-visible
};

struct AmTextureDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    AmTextureFormat format = AmTextureFormat::R8;
};
```

`AmTextureId` stays the bindless slot id (what `InstanceData::textureId` already consumes) and doubles as the resource
handle for texture ops, so there is a single texture id space.

### The new AmethystBackend

```cpp
class AmethystBackend {
  public:
    virtual ~AmethystBackend() = default;

    virtual AmBufferId createBuffer(const AmBufferDesc &desc) = 0;
    virtual bool growBuffer(AmBufferId id, size_t newCapacity) = 0;
    virtual void uploadBufferRange(void *cmdBuffer, AmBufferId id, const void *data, size_t offsetBytes, size_t sizeBytes) = 0;
    virtual void destroyBuffer(AmBufferId id) = 0;

    virtual AmTextureId createTexture(const AmTextureDesc &desc) = 0;
    virtual void uploadTexture(void *cmdBuffer, AmTextureId id, const uint8_t *pixels) = 0;
    virtual void destroyTexture(AmTextureId id) = 0;
};
```

Seven virtuals total, fixed forever. The bespoke atlas triples are deleted. Semantics:

- `uploadBufferRange` on a HOST_VISIBLE buffer is the memcpy-into-mapped + 64-byte-aligned flush that `updateInstances`
  and `updateTextBuffers` each hand-roll today, written once. On a DEVICE_LOCAL buffer it is the staged copy that
  `uploadToGpu` does today (still acceptable to stall there; only the static index buffer uses it, at init). `cmdBuffer` is
  unused on the HOST_VISIBLE path and only drives the DEVICE_LOCAL staging copy; document that so the signature does not
  read as a bug.
- `growBuffer` recreates, copies live bytes, and **rebinds the descriptor recorded in the resource** - the pointer-compare
  switch dies because `shaderBinding` lives in the buffer record.
- `uploadTexture` can later grow an optional region parameter for dirty-rect atlas uploads; with one implementation that
  optimization is finally worth writing.

### Core side: one arena module

Extract `BlockAllocator` and `DirtyRange` from `glyph_buffer.h` into their own module (`src/rendering/gpu_arena.h/.cpp`),
then build:

1. **`GpuArena`** - owns an `AmBufferId`, a `FreeBlock`-style free list, a CPU mirror, a `DirtyRange`, and a
   `GrowthPolicy { DOUBLE_UNTIL(cap), FIXED_COMPACT }`. This subsumes `BufferArena` + the four free-list members +
   `allocateFromArena` / `freeToArena` / `reallocBufferArena` (`cpp:867-1012`). Growth decisions happen here; the backend
   only executes `growBuffer`.

This refactor stops there. A second piece, **`SideBuffer`** (the `GlyphBuffer` skeleton generalized: stable handle table +
block allocator + mirror + dirty range, parameterized by element size), is deliberately **not built now** - there is only
one side-buffer consumer today (`GlyphBuffer`), and abstracting before the second one exists is exactly how the current
slop accreted. `SideBuffer` arrives when the first *new* side-buffer resource (gradients) lands, gets validated greenfield
there, and only then is a `GlyphBuffer` rebase considered. The rebase is the hard case - `GlyphBuffer` is three *coupled*
arenas (the slice table indexes the glyph/line arenas and `compact()` rewrites slice entries on relocation, glyph-specific
logic `SideBuffer` cannot absorb), so it stays last, optional, and never blocks this refactor. For now `GlyphBuffer` keeps
its internals and is only rerouted to allocate and upload through the hub.

A new **`GpuResourceHub`** (owned by `AmethystContext`, handed to it at `init`) holds the arena table and the
per-registry suballocation maps that currently live in the backend (`m_geometryAllocations`, `m_textAllocations`,
`h:166-167`). All preallocation lives in one function:

```cpp
void GpuResourceHub::init(AmethystBackend &backend)
{
    m_instances.init(backend, {8_MB,  STORAGE, HOST_VISIBLE, /*binding*/ 0}, GrowthPolicy::DOUBLE_UNTIL(64_MB));
    m_glyphs.init(backend,    {4 * GlyphBuffer::GLYPH_CAPACITY * sizeof(GlyphQuad), STORAGE, HOST_VISIBLE, 2}, ...);
    m_lines.init(backend,     {...,  STORAGE, HOST_VISIBLE, 3}, ...);
    m_slices.init(backend,    {...,  STORAGE, HOST_VISIBLE, 4}, ...);
    m_quadIndices.init(backend, {6 * sizeof(uint32_t), INDEX, DEVICE_LOCAL, UINT32_MAX}, GrowthPolicy::FIXED);
}
```

Because the hub lives in core, registry destruction frees its suballocations directly - the
`GeometryRegistry::setDestroyCb` hook (`cpp:250`) is deleted.

### How AmethystContext drives it

`init(backend)` creates the two atlas textures via `createTexture` and initializes the hub. `sync(cmdBuffer)` becomes the
**single** sync point for everything:

1. Dirty atlases -> `uploadTexture`.
2. For each visible registry: flush instance changes and glyph/line/slice dirty ranges into the hub's arenas ->
   `uploadBufferRange` per dirty range. This is the work `record` does today at `cpp:358-378`, moved off the backend.
3. Build a `FrameDrawList`: per visible registry, in order,
   `{ firstInstance, instanceCount, glyphBase, lineBase, sliceBase }` (the element-index math currently inlined in
   `record` at `cpp:418-426`).

`VkBackend::record(cmd)` shrinks to: bind pipeline/descriptors/index buffer once, then per draw-list entry set push
constants and `vkCmdDrawIndexed`. It no longer includes `GeometryRegistry`, `GlyphBuffer` or their element types.

### What VkBackend becomes

- Twenty atlas members -> two `std::vector` record tables (`VkBufferRecord { buffer, memory, mapped, capacity, desc }`,
  `VkTextureRecord { image, memory, view, sampler, staging*, desc, bindlessSlot }`) indexed by handle, with a free-slot
  list each.
- `createAtlasTexture` + `createSvgAtlasTexture` (~210 lines) -> one `createTexture` (~105 lines, format-parameterized).
  Same collapse for the two uploads.
- `shutdown` -> loop over the two record tables plus pipeline objects.
- Keeps: pipeline, descriptor set layout (bindings 0-4 unchanged), bindless `registerTexture` / `unregisterTexture`
  (`cpp:1212-1250`, already the right shape - `createTexture` calls it), GLFW glue.

Shaders are untouched by the migration itself; binding numbers stay 0-4 and remain declared in `AmBufferDesc` on the core
side, so backend code never mentions them again.

### Frame and synchronization model

The standalone `amethyst__vk13_glfw` backend uses a **single frame in flight**. The UI is retained-mode and dirty-tracked,
so the CPU only re-uploads on change and there is no throughput motivation for CPU/GPU overlap; one frame removes the
entire write-while-GPU-reads hazard class by construction. Buffer growth keeps its current `vkDeviceWaitIdle` before
recreate + rebind (`cpp:981`), a rare and acceptable stall at one frame in flight.

Frames-in-flight is a **backend concern, kept behind `AmBufferId`** - core never learns of it. The engine (RaptureVk)
draws the UI inside its own multi-FIF loop through a different backend; that backend satisfies the same interface by
ring-buffering its host-visible arenas internally, writing the current frame's copy in `uploadBufferRange` and using a
per-frame descriptor offset/set, all invisible to core. This is only possible because:

- `GpuArena` (core) holds exactly **one logical CPU mirror** and does not ring. Physical copy count is the backend's call.
- `FrameDrawList` carries **element indices** (`firstInstance`, `glyphBase`, `lineBase`...), never raw byte pointers into a
  specific physical buffer, so the backend maps them onto whichever frame copy it owns.
- Descriptor binding and any rebind-on-grow live entirely in the backend behind the handle.

Contract: producers mutate mirrors and mark dirty; `sync(cmd)` flushes dirty ranges via `uploadBufferRange`; `record(cmd)`
draws; the backend guarantees an upload issued in `sync` is visible to that frame's `record`. A 1-FIF backend satisfies
this trivially; a multi-FIF backend satisfies it by ringing. (This supersedes the earlier note that ring buffering would
slot into `GpuArena`: it slots into the backend, not core.)

---

## Worked example: adding gradients (illustrative, not part of this work)

> **Superseded:** the actual gradient design is now specified in `gradients_plan.md` (stop SSBO, fixed-stride records,
> global buffer at binding 5, gradients carried on Color3/4 as a heap-owned def with a lazily resolved, copied-as-is GPU
> slot). The sketches below are kept as the original cost argument only. Note for step 5: `SideBuffer` is *not* introduced
> with gradients after all - fixed-stride records need no block allocator; extract `SideBuffer` only when a real
> variable-length consumer appears.

Gradients are **not implemented by this refactor**. This section only shows what adding them *would* cost once the layer
above exists - the test that the abstraction is genuinely cheap. The final encoding is chosen with the feature, after this
refactor lands. The candidates: (a) **compact** - pack a 2-stop / 4-corner gradient straight into `shapeData`, nice and
tiny but cannot express every case; (b) **grow `InstanceData` by 16 bytes** for more inline room - rejected, too much
per-instance cost for a minority feature; (c) a **CPU-baked ramp texture** through the existing bindless `textureId`;
(d) a **`GradientBuffer` stop SSBO**. (c) and (d) are the general options and either is cheap under the new layer; the two
sketches below exist only to demonstrate that.

### Option: GradientBuffer stop SSBO

```cpp
// src/modules/gradient_buffer.h
struct GradientStop {
    uint32_t color; // packed RGBA
    uint32_t tAxis; // t as half (low 16) | axis/angle as half (high 16), layout TBD with the feature
};
```

1. **Module**: `GradientBuffer` = one `SideBuffer` over `GradientStop` with `createGradient(stops) -> handle`,
   `updateGradient`, `destroyGradient`. ~100 lines, mostly packing, because allocation/dirty/handles are inherited.
2. **Table entry**: one line in `GpuResourceHub::init` - `m_gradients.init(backend, {GRADIENT_CAPACITY *
   sizeof(GradientStop), STORAGE, HOST_VISIBLE, /*binding*/ 5}, GrowthPolicy::FIXED_COMPACT);` - plus `gradientBase` in
   the `FrameDrawList` entry and `gradientOffset` in `PushConstants`.
3. **Instance + shader**: `INSTANCE_FLAG_GRADIENT` flag bit; gradient handle + stop count packed into
   `InstanceData::shapeData[1]` (`shapeData[0]` is the text slice; frames use no shapeData today, but slot 1 keeps slot 0
   free so batched text can gradient-fill later). `ui.fs.glsl` gains `layout(std430, binding = 5)` and a stop-walk in the
   fill-color path. Canvas primitives (`PRIMITIVE_CANVAS_*`) already use all four `shapeData` words for points
   (`canvas.cpp:179-215`), so gradient-filled canvas shapes are out of scope for this encoding; gradients apply to
   rect/circle fills.

Zero new virtuals. Zero new `VkBackend` members. Zero shutdown, descriptor or growth edits. The ten touch points from the
diagnosis become three, and two of them (module, shader) are the actual feature rather than plumbing.

### Option: 1D ramp texture

Bake stops to an `N x rows` RGBA8 strip: `createTexture({256, rowCount, RGBA8})` + `uploadTexture` on bake, sample by
computed t through the existing bindless `textureId`. Under the new interface this is two calls and no shader SSBO.

Trade-offs: the ramp gets free hardware interpolation and arbitrarily many stops at flat cost, but quantizes t to the
ramp width, needs row lifetime management (a mini-atlas), and re-bakes a full row on any stop edit. The SSBO interpolates
exactly, edits via `DirtyRange` for pennies, and matches the glyph-slice pattern the codebase already has. Both stay open
until the gradient feature begins; the only claim here is that the new layer makes either a three-touch-point addition
instead of ten.

---

## Files to touch

- `libamethyst/include/amethyst/amethyst_backend.h` - new seven-virtual interface, delete the atlas triples
- `libamethyst/src/components/common.h` - `AmBufferId`, descs, enums
- `libamethyst/src/rendering/gpu_arena.h/.cpp` (new) - `BlockAllocator` + `DirtyRange` (moved), `GpuArena`, `GrowthPolicy`
  (step 1); `SideBuffer` lands here with gradients (step 5)
- `libamethyst/src/rendering/gpu_resource_hub.h/.cpp` (new) - arena table, per-registry suballocation, `FrameDrawList`
- `libamethyst/include/amethyst/amethyst_context.h` / `src/amethyst_context.cpp` - hub ownership, fat `sync`
- `libamethyst/src/modules/glyph_buffer.h/.cpp` - rebase onto `SideBuffer`, drop the moved types
- `libamethyst/src/rendering/geometry_registry.h/.cpp` - delete `setDestroyCb`
- `backends/amethyst__vk13_glfw.h/.cpp` - record tables, generic primitives, thin `record`, mass deletion
- Later, with the feature: `src/modules/gradient_buffer.h/.cpp` (new), `rendering/instance_data.h`,
  `backends/shaders/glsl/ui.fs.glsl` / `ui.vs.glsl`

## Migration order

1. **Scaffolding**: move `BlockAllocator`/`DirtyRange` to `gpu_arena`, add handles/descs, add the seven new virtuals and
   their `VkBackend` record-table implementations **alongside** the old paths. Everything still renders via old code.
2. **Glyph atlas**: `AmethystContext::init`/`sync` switch to `createTexture`/`uploadTexture`; delete the glyph-atlas
   triple and its ten members.
3. **SVG atlas**: same switch; delete the duplicated ~210 lines and the second ten members. Old interface is now empty -
   delete the bespoke virtuals.
4. **Buffers**: stand up `GpuResourceHub` (instances + glyph/line/slice + index), move per-registry maps and upload logic
   into `sync`, introduce `FrameDrawList`, shrink `record`, delete `BufferArena`/`FreeBlock`/`allocateFromArena`/
   `freeToArena`/`reallocBufferArena`/`updateInstances`/`updateTextBuffers`/`obtain*`/`free*`/`uploadToGpu` and the
   `setDestroyCb` hook. `GlyphBuffer` keeps its current internals here - it is only rerouted to allocate/upload through the
   hub and interface, not rebased onto `SideBuffer`. This is the big step; steps 1-3 keep it from also being a texture
   migration.
The refactor ends at step 4. Everything below is **post-refactor follow-up, not part of this work** - listed only to show
the layer leads somewhere:

5. *(later)* **First new side-buffer resource (gradients)**: introduce `SideBuffer` greenfield and build the chosen
   gradient encoding on it - the validation that a new resource is a struct, a table entry and a shader binding. The
   encoding is decided then, not now.
6. *(later, optional)* **GlyphBuffer rebase**: only once `SideBuffer` is proven, rebase `GlyphBuffer`'s three coupled
   arenas onto it, preserving the slice -> glyph/line coordination and `compact()` fixups. Skip if not a clean win.

Validation at every step: the test app must render identically (text, atlases, frames, canvas), since each step swaps a
data path under code that is already working. Capture an instance-buffer / glyph-buffer dump before step 1 and diff after
each step where feasible; treat any pixel or buffer-content change as a regression in that step, not a later one.

## Open questions

- Frames in flight: **decided** - single frame in flight for the GLFW backend, and FIF kept behind `AmBufferId` as a
  backend concern so a multi-FIF engine backend can ring internally without touching core. See *Frame and synchronization
  model* under The design.
- The shared 10 MB `MAX_BUFFER_ARENA_SIZE` cap (`amethyst__vk13_glfw.h:20`) becomes a per-arena `GrowthPolicy` parameter;
  pick real caps per resource during step 4.
- Whether `FrameDrawList` lands inside step 4 or `record` temporarily keeps iterating registries while reading offsets
  from the hub. Prefer landing it: it is what finally removes core types from the backend's draw path.
