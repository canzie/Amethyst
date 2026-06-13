# Color Picker Design Prompt

A handoff prompt for a visual design pass on a color picker UI component. The designer works
in HTML/CSS and should produce **visual mockups**, offering **multiple options for every
decision** so we can pick and mix. It is purely about look and interaction — no code beyond
the mockup markup, no specific framework or library.

---

## Prompt

**Design a color picker UI component. Produce visual mockups in HTML/CSS. For every decision
below, give multiple distinct options with trade-offs — I want a menu to choose from, not a
single finished verdict.**

There are two flavors of the component:

- An **RGB picker** (edits a red/green/blue color, no transparency).
- An **RGBA picker** (edits a color plus an alpha / transparency channel).

Design both, and show how the RGBA one adds the alpha controls on top of the RGB layout.

### 1. Detail levels (give several, as a ladder)

Design a range of detail levels, from bare to fully featured, so the same picker can sit in
a tight inspector row or a roomy panel. At minimum cover:

- **Minimal** — just the color field / picking surface, **no text, no readouts, no extra
  chrome**. Pure visual picking.
- **Standard** — the picking surface plus the essential readout (propose what that is).
- **Extended** — full inline RGB / hex (and alpha) entry fields, plus any extras worth
  having (eyedropper, recent-colors swatches, copy-hex button, etc. — suggest options).

### 2. Picking-surface variants (give multiple)

Propose several layouts for the actual color-picking surface, for example:

- **Square + bars** — a saturation/value square with a hue bar (and an alpha bar for the
  RGBA flavor) alongside it.
- **Triangle** — an HSV triangle inside a hue ring: a rotating hue wheel with a
  saturation/value triangle in the center.
- Any other worthwhile forms — hue wheel + value slider, vertical strips, radial, etc.

For each, show the draggable thumb / handle on the surface and how it reads as the current
selection.

### 3. Trigger — the closed state (most important — give lots of options)

Design the **collapsed trigger**: what the user sees *before* opening anything. This is the
default resting state and it has two jobs at once — show the **current color** and signal
"you can click here to change it." Picture it sitting in a property row or a table cell,
e.g. `imageColor │ [ trigger ]`, where it has to read clearly at a small size.

Offer many distinct looks for the **color representation**, for example:

- A filled swatch box.
- A swatch + the hex code (`■ #ffffff`).
- A swatch + the RGB / RGBA tuple.
- Just the hex / value text with a colored underline or left color bar.
- A small ring / dot of the color.
- A swatch that fills the whole cell vs a small chip with padding around it.

Then cover:

- **Affordance** — how it signals it is clickable and editable (a caret / chevron, a subtle
  border, an edit/eyedropper glyph on hover) vs staying minimal. Show options for both
  "obviously interactive" and "clean until hovered."
- **Alpha display** (RGBA flavor) — checkerboard behind the swatch, a split solid/alpha
  swatch, a separate alpha pip, etc.
- **Hover / press / open states** — what changes on hover, on press, and while the picker is
  open (border highlight, inset, darken, outline, active caret). Make pressed and open
  clearly distinct from idle.

Show each trigger variant rendered in a mock property row / table cell so they can be
compared side by side at realistic size.

### 4. Open behavior

Show how the picker opens relative to the trigger — a floating popover anchored to the
trigger vs an inline panel that expands in place — and what the full open state looks like
for each detail level.

### Output

Self-contained HTML/CSS mockups, multiple variants laid out so they can be compared at a
glance. Aim for a clean, modern dark-themed editor / inspector aesthetic.
