# Sliders, Drags, and Color Pickers — Plan

Implementation plan for three related additions: refitting the existing sliders, adding an
ImGui-style `Drag*` family, and adding `ColorPicker3` / `ColorPicker4`. Design only here —
the detailed color-picker design handoff lives in
[color_picker_design_prompt.md](color_picker_design_prompt.md).

## Current state

- **`Slider*` (`slider.cpp`)** already does position-mapping (drag thumb → value clamped to
  `[min, max]`), but `thumbWidth` is derived from `speed` / `range`, and `speed` only makes
  sense for a drag. There is no track fill and no click-to-jump on the track.
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

## Workstream 1 — Slider refit (do first; defines the contrast with drags)

- Remove `speed` from `Slider*`; size the thumb from a style prop instead of from
  `speed` / `range`.
- Add a fill `Frame` (track filled up to the thumb) plus style props for fill color.
- Add click-on-track to jump the value; keep drag for fine adjustment.
- Mostly edits to `slider.{h,cpp}` and `SliderStyleProperties`; no new scope types.

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

## Workstream 3 — Color pickers (`ColorPicker3` / `ColorPicker4`)

- Swatch trigger button → popup panel in the `OverlayLayer` (mirror `Dropdown`).
- Picking surface options: saturation/value square + hue bar (+ alpha bar for `Color4`),
  and an HSV triangle-in-hue-ring variant.
- Detail levels: minimal (just the picking surface, no text), standard, extended (inline
  RGB / hex / alpha entry + extras).
- Prereq: add HSV↔RGB helpers to `color.{h,cpp}`. The SV square and hue bar are natural
  fits for the existing **gradient** system, avoiding custom shaders where possible.
- New: `ColorPickerStyleProperties`, the classes, `ColorPicker*Scope` builders.
- Full design questions (detail-level mechanism, surface variants, trigger button looks and
  press/open states, popup behavior) are deferred to the design prompt and should each come
  back with multiple options.

## Suggested order

Slider refit → drags (cheap, high reuse) → HSV helpers → color picker minimal →
extended / triangle.
