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

## Workstream 2 — `Drag*` family (`DragFloat` / `DragInt` / `DragVec2` / `DragVec3`)

- Built on **`UIButton`**, using `onMouseButton1Down` / `onMouseMoved` /
  `onMouseButton1Up` directly. **No `UIDragDetector`.**
- Interaction: on press, capture mouse x and enter scrub mode. On each move while
  scrubbing, add `(x - lastX) * speed` to the value, clamp to optional soft `min` / `max`
  (unset = unbounded), update `lastX`, fire `onValueChanged`. On release, leave scrub mode.
- Display is just a value box (label + formatted number) — no thumb, no fill track.
  Double-click swaps the number for a `TextInput` to type an exact value.
- `DragVec2` / `DragVec3` are N boxes side by side, each scrubbing its own component.
- Shares value/format/style plumbing with the sliders, but a completely different input
  model (no track geometry, no position-to-value mapping).
- New: `DragStyleProperties`, the `Drag*` classes, `Drag*Scope` builders +
  `ui_scope.h` registration + a `Style::getDragStyle` entry.

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

## Workstream 4 — Color pickers (`Color3Picker` / `Color4Picker`)

Naming: `Color3Picker` edits a `Color3` (rgb), `Color4Picker` edits a `Color4` (rgba). The
`3` / `4` belongs to the color, not the picker.

These are **only the picker core** — a headless inline surface, no chrome, no popup of their
own:

- Surface = saturation/value square + hue bar (+ alpha bar for `Color4Picker`), each with a
  draggable thumb. This is the "minimal" picker. An HSV triangle-in-hue-ring is an alternate
  surface variant.
- **Square vs triangle is a `ColorPickerSurface` enum on the core, not a separate component**
  — both edit the same color / HSV state and differ only in surface geometry, exactly like
  `SliderVec2`/`SliderVec3` use `ValueControlLayout` for stacked vs side-by-side. Keep one
  class, one scope, one style struct; factor each surface's geometry / hit-test / thumb math
  into a static helper (`s_buildSquareSurface` / `s_buildTriangleSurface`) and dispatch on
  the enum in `draw` / `updateComponents`.

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

Popup/Portal + migrate Dropdown → HSV helpers → picker core (`Color3Picker` /
`Color4Picker`, = minimal) → Standard / Extended widgets. (Slider refit + drags are
independent and can slot in whenever.)
