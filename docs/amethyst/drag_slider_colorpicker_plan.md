# Sliders, Drags, and Color Pickers — Plan

Implementation plan for three related additions: refitting the existing sliders, adding an
ImGui-style `Drag*` family, and adding `ColorPicker3` / `ColorPicker4`. Design only here —
the detailed color-picker design handoff lives in
[color_picker_design_prompt.md](color_picker_design_prompt.md).

## Current state

- **`Slider*` (`slider.cpp`)** refit shipped — see Workstream 1 below. `SliderFloat` /
  `SliderInt` only, `ValueScale` instead of `speed`, `format` string, track-as-interactive-
  surface with click-to-jump, typed builders.
- **`UIDragDetector`** is for repositioning objects (free / axis-locked move). It is **not**
  used by the new Drag widgets.
- **`UIButton`** exposes `onMouseButton1Down(x, y)`, `onMouseMoved(x, y)`,
  `onMouseButton1Up(x, y)` plus hover — enough to build a value scrubber directly.
- **`Color3` / `Color4`** are solid value types (linear storage, `fromHex` / `fromRgb`,
  gradient support). No HSV helpers yet.
- **Popups**: `Dropdown` builds a floating panel in the `OverlayLayer` — the precedent for a
  color-picker popup.
- **Authoring API**: every component is exposed through a `*Scope` builder
  (`ui_scope.h`), a `*StyleProperties` struct (`properties.h`), and a `Style::getXStyle`
  hook. New components need all three layers.

## Workstream 1 — Slider refit (SHIPPED)

Done. Final shape differs from the original sketch (no fill, no side label, no vec sliders):

- **Only `SliderFloat` / `SliderInt`** remain — `SliderVec2` / `SliderVec3` deleted (drags
  will likewise be `DragFloat` / `DragInt`). A shared base `Slider` owns the single
  track / thumb / value-label, the `draw` loop, `getHittableInstances`, and a `layoutTrack`
  helper; the subclasses only differ in value type and the value↔position mapping.
- **`speed` removed.** Replaced by a `ValueScale { LINEAR, LOGARITHMIC }` enum (a plain public
  member on the slider, not a style prop). Drags will reuse this same enum.
- **Suffix removed.** Replaced by a printf-style `format` string (`"%.3f"`, `"%d"`), defaulted
  per type, applied with `snprintf` in a shared `s_formatNumber` helper.
- **No fill, no side label.** A caption is just a sibling `TextLabel` (composition), not a
  slider feature. `LabelSide` / `ValueControlLayout` enums and the `label*` / `value*` /
  `slider-color` / `track-corner-radius` style keys were all deleted.
- **`value` is a pointer** (renamed from `valueRef`) — live two-way binding to the caller's
  variable; changes also fire `onValueChanged`.
- **Styling:** the track is styled by the slider's **own `BaseStyleProperties`**
  (background = bar color, corner-radius = bar). `SliderStyleProperties` now holds an embedded
  `BaseStyleProperties thumb`, an embedded `TextStyleProperties text` (value text), and
  `trackHeight` / `thumbWidth` / `thumbHeight`. `trackHeight` is free of the box height; the
  thumb height is clamped to it.
- **Int is discrete:** snaps to stops, thumb width = `trackWidth / (max-min+1)` (the
  `width/6` notched look). Float is continuous.
- **Input:** the `UIDragDetector` lives on the **track** (press anywhere = jump, drag =
  scrub); the thumb is a passive, non-interactable indicator. The value label is a sibling of
  the track (child of the slider), drawn last and non-interactable so it never inherits the
  track's transient drag-nudge.
- **Builders:** typed `SliderFloatProperties` / `SliderIntProperties` DTOs carry
  `min` / `max` / `value` / `scale` / `format`; `onValueChanged` is still set in the scope
  lambda. Separate typed builders are required because each carries a typed `value` pointer.

Carries into Workstream 2: drags share `ValueScale`, the `format` string + `s_formatNumber`,
and the typed-per-type DTO/builder pattern — but **not** the track/thumb geometry or
`UIDragDetector` (a Drag scrubs by mouse delta and has no physical thing to drag along).

## Workstream 2 — `Drag*` family (`DragFloat` / `DragInt`) (SHIPPED)

Shipped `DragFloat` / `DragInt`; `DragVec2` / `DragVec3` deferred. Final shape diverged from
the sketch below:

- **`Drag : UIObject`** (not `UIButton` — it isn't a button), handling its own
  `onInputBegan` / `onInputEnded` / `onMouseMoved` and capturing the mouse on press. **No
  `UIDragDetector`.** A shared base owns the state machine, draw, and the edit field; the
  subclasses only carry the typed value + scrub/commit/format hooks.
- **Single-click vs drag, no double-click.** On press → `PENDING`; first move past a small px
  threshold → `SCRUBBING` (`value` changes by `pixelDelta * speed`, `LINEAR` additive /
  `LOGARITHMIC` multiplicative); release without crossing the threshold → `EDITING`. There is
  no double-click event in the input system; deferred (would be synthesized at the
  `InputInterface` boundary as a first-class `InputObject`, not sniffed in components).
- **Typed values are `double` / `int64_t`** (no raw `int`/`float`); each subclass owns its own
  `speed` (`double` / `int64_t`). `min` / `max` are **non-optional, defaulted to the type's
  full range**, so clamping is unconditional and unbounded scrubbing can't overflow. The
  `double→int64` conversion saturates (`s_saturateToInt64`); NaN is guarded; a debug
  `AM_ASSERT(min <= max)`.
- **The edit field is a single `NumberInput` child** that doubles as display (read-only,
  non-interactable, centered) and editor (made editable + focused on click). `NumberInput`
  only guarantees a valid numeric grammar (`acceptText`) and offers `asDouble()` / `asInt64()`;
  it has no value/commit signal of its own. Commit happens on Enter / focus-loss.
- New: `DragStyleProperties` (text only), the `Drag*` classes, `Drag*Scope` builders +
  `ui_scope.h` registration, a `Style::getDragStyle` entry + `ComponentType::DRAG`
  (`{DRAG, UI_OBJECT}`). Demo: `amethyst_demo_drag`.

### Input refactor (prerequisite, SHIPPED)

The edit field pulled in a clean split of the text-input core, plus a tick mechanism:

- **`UIInput`** base owns all text-editing (caret, selection, clipboard, focus/blink,
  rendering) behind a protected `drawInput()`; subclasses keep their own `draw()` /
  `resolveStyle()` and override the `acceptText` / `displayText` / `onCommit` hooks.
  `TextInput` is now a thin subclass; `NumberInput` adds the numeric grammar. Password / color
  inputs would be further subclasses (display override / composition), not a mode enum.
- **Tick subscription, not a tree walk** (draw only visits dirty nodes). On focus a `UIInput`
  calls `Window::registerTick`, getting a `TickHandle { window, id }` it unregisters on
  blur / destroy. `Window::tick(dt)` runs the (usually one) focused subscriber; the app calls
  it once per frame. A `Drag`'s field registers itself, so `Drag` needs no update plumbing.
- **`FreeList<T>`** (`utils/free_list.h`) backs the tick registry: vector + free list, stable
  ids, no shifting on erase (slot-map / `std::hive` idea).

Original sketch (kept for the deferred `DragVec2` / `DragVec3`): N boxes side by side, each
scrubbing its own component; shares the value/format/style plumbing.

## Workstream 3 — Generic Popup (do before the color picker)

Today `Dropdown` hand-rolls all popup behavior: it walks up to find the `OverlayLayer`,
spawns an `InvisibleButton` "eater" for outside-click dismiss, runs its own
`CLOSED / OPEN / PENDING_CLOSE` state machine, and positions its panel manually. Every
future popup would copy that. Users currently cannot make a popup at all except by cheating
absolute position + high z-index. Extract this into reusable components.

- **`Portal`** (low-level): mount arbitrary content into the `OverlayLayer` at a position /
  anchor. No dismiss policy of its own. This is the placement primitive.
- **`Popup`** = `Portal` + light-dismiss (outside-click eater) + escape-to-close + anchor to
  a trigger (or point) with edge-flipping when it would go offscreen + z-index / stacking.
- **Migrate `Dropdown` onto `Popup`** to validate the API and delete its bespoke popup code.
- Layering note: docking panels are **not** light-dismiss (a floating dock must not vanish on
  outside click), so docking uses `Portal` placement but keeps its own persistence — it does
  not ride on `Popup`. Menus / tooltips / combobox / color picker use `Popup`.
- This makes user-defined popovers a first-class thing instead of a z-index hack.

## Workstream 4 — Color pickers (`Color3Picker` / `Color4Picker`) (CORE SHIPPED)

Naming: `Color3Picker` edits a `Color3` (rgb), `Color4Picker` edits a `Color4` (rgba). The
`3` / `4` belongs to the color, not the picker.

Shipped: the minimal HSV square + bars core. Final shape diverged from the sketch below:

- **`ColorPicker` base (`: UIObject`)** holds the shared channels (`m_hue`, `m_saturation`, and a
  `union { m_value; m_lightness; }`) and the SV field; `Color3Picker` / `Color4Picker` add the RGB(A)
  binding plus the `pullValue` / `pushValue` / `updateComponents` overrides. Alpha lives entirely in
  `Color4Picker` (its own `m_alpha` + alpha bar) — the base is alpha-agnostic.
- **Parts are real children** (`add<Frame>()` / `add<SliderFloat>()`): the SV field is two stacked
  gradient `Frame`s (white→hue, transparent→black) + a ring `m_fieldThumb`; the hue / alpha **bars
  reuse the `SliderFloat` component** (continuous, `setFormat("")` to hide the readout, track =
  rainbow / alpha gradient). Default `getHittableInstances` + the child draw loop cover them.
- **Two enums, values reserved** (non-shipped ones log a warning + fall back): `ColorModel { HSV, HSL }`
  and `ColorPickerShape { SQUARE, TRIANGLE, WHEEL }`. `model` / `shape` / `fieldThumbRadius` are plain
  public members (like slider's `scale`), **not** a style block. There is **no
  `ColorPickerStyleProperties`** — bar styling is the slider's own `SliderStyleProperties`.
- **The picker draws its own panel** via the inherited `m_geometryAlloc` (same retained pattern as
  `Frame`) from its `BaseStyleProperties`, and lays children out in the **content** rect so
  `base.padding` insets them. Thumbs take the color under them (field = selected, hue = pure hue,
  alpha = color at its alpha) with a rounded white `OUTLINE` border.
- HSV↔RGB helpers live in `color.h`. `SQUARE_BARS` needed zero shader work.
- **Border-bleed shader fix (`ui.vs.glsl`):** outward border modes (`OUTLINE` / `MIDDLE`) were clipped
  to the quad; the vertex shader now grows the quad by the outward extent + 1px AA and extends `fragUV`
  so the shape still maps to UV `[0,1]` (gradients/textures unchanged, fragment stage untouched). The
  thumb rings depend on this. Packed-field reads went through `INST_*` macros.
- Builders `color3Picker` / `color4Picker` + `Color3PickerProperties` / `Color4PickerProperties`. Demo:
  `amethyst_demo_color_picker`.

Deferred: the HSL model, the triangle / hue-ring shapes (renderer notes below), and the Standard /
Extended widgets.

Original surface-variant sketch (for the deferred shapes):

- **Square vs triangle is a `ColorPickerShape` enum on the core, not a separate component** — both edit
  the same HSV state and differ only in surface geometry. Keep one class, one scope; factor each
  surface's geometry / hit-test / thumb math into a static helper and dispatch on the enum.

### Renderer support — all three surfaces are cheap

The SDF shader (`backends/shaders/glsl/ui.fs.glsl`, in-repo and editable) already has the
triangle SDF (`sdfTriangle` / `sdEquilateralTriangle`, dispatched for `PRIMITIVE_TRIANGLE`)
and applies the gradient per `fragUV` for **every** primitive (the `evalGradient` block runs
regardless of type). Surfaces are submitted as retained primitive instances the way `Frame`
submits `PRIMITIVE_RECT` (see `ui_object.cpp` submitting `PRIMITIVE_TRIANGLE`) — **not** via
`Canvas`, which is the immediate-mode path.

- **`SQUARE_BARS`** — zero shader work. SV square = two stacked rects (white→hue then
  transparent→black linear gradients), hue bar = multi-stop linear rainbow, alpha bar =
  linear gradient over a checker texture.
- **SV triangle** — small `ui.fs.glsl` addition: inside the triangle branch, blend
  `hue·w0 + white·w1 + black·w2` from the barycentric weights of the fragment against the
  three known triangle vertices (hue comes in as `fillColor`). No new buffers / backend
  changes — a few shader lines plus a fill-mode flag.
- **Hue ring** — `PRIMITIVE_CIRCLE` with a thick inset border for the ring shape (already
  supported), plus one new **conic** mode in `evalGradient`
  (`t = atan(uv.y-0.5, uv.x-0.5)/TAU + 0.5` through the rainbow stops). Small but a genuinely
  new gradient mode.

Verify first that linear gradients already render correctly on `PRIMITIVE_TRIANGLE` and
`PRIMITIVE_CIRCLE` instances (the shader path suggests they do) before adding the barycentric
and conic modes.
- Inline by default. To present it as a popup, the consumer wraps it in `Popup` (Workstream
  3). The picker itself does not own a trigger or a popup.
- Prereq: add HSV↔RGB helpers to `color.{h,cpp}`. The SV square and hue bar are natural fits
  for the existing **gradient** system, avoiding custom shaders where possible.
- New: `ColorPickerStyleProperties`, the core classes, `Color*PickerScope` builders. The
  core IS a styled core component.

### Detail levels live as widgets, not core components

Standard / Extended (header, hex / format row, opacity field, eyedropper, recent-swatch
strip, copy button) are **composition over the core**, so they live in
`libamethyst/src/components/widgets/` (same as `gizmo`), not as core styled components.

- They do **not** hook into the style system; they are plain compositions of the core picker
  + `Frame` / `TextInput` / `TextLabel` / `Popup`.
- Rationale: it is irrelevant whether a consumer (e.g. RaptureVk) builds these or the lib
  ships them as widgets — if it is not a core component it is harmless. Keeping them out of
  core keeps the core surface small.
- Optionally gate the widgets behind a compile flag so they can be left out entirely.

## Suggested order

Popup/Portal + migrate Dropdown (done) → HSV helpers + picker core (`Color3Picker` /
`Color4Picker`, minimal) **(done)** → Standard / Extended widgets, HSL model, triangle / hue-ring
shapes (remaining). Slider refit is done; drags (`DragFloat` / `DragInt`) are independent and not yet
started.
