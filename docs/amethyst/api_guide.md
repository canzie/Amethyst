# Amethyst API Guide (for engine integration / migration)

Practical reference for building UI with Amethyst's current (post-refactor) API. Written so another
agent can start a migration without reading every header. Source of truth for exact fields:
`libamethyst/src/components/properties.h` and `components/common.h`. For theming:
`libamethyst/src/modules/style_properties.def` (property list) and `assets/theme.ams` (example).

Status: the properties/builder refactor and the CSS-like styling system are landed. See
"Known gaps" before relying on edge behavior.

## Mental model

Amethyst is a retained-mode scene graph. Everything is an `Instance`; drawable UI nodes derive from
`UIObject`. You build a tree once (declaratively via scopes, or imperatively), the library keeps it,
and you call `draw(root)` each frame. Styling is two-layer: a **theme** (`.ams` stylesheet, resolved
by component type + CSS-like classes) provides defaults, and **instance overrides** set on a node win
on top.

## Include + namespace

```cpp
#include "amethyst/Amethyst.h"   // umbrella header: components, scopes, context, backend
using namespace Amethyst;        // everything is in namespace Amethyst
```

## Core value types (`components/common.h`)

| Type | What | Construct |
|------|------|-----------|
| `UDim` | scale (0..1 of parent) + offset (px) | `UDim::fromScale(0.5f)`, `UDim::fromOffset(8.0f)`, `{0.5f, 8.0f}` |
| `UDim2` | 2D `UDim` (position/size) | `UDim2::fromOffset(x,y)`, `UDim2::fromScale(sx,sy)`, `UDim2(sx,ox,sy,oy)` |
| `UDim4` | top/right/bottom/left `UDim` (padding/margin) | `{{0,t},{0,r},{0,b},{0,l}}` |
| `Color3` | RGB (sRGB in, linear stored) | `Color3(r,g,b)` floats 0..1, `Color3::fromHex(0x282828)`, `Color3::fromRgb(40,40,40)` |
| `Color4` | RGBA | `Color4(r,g,b,a)`, `Color4::fromHex(0x47..,true)`, `Color4::fromRgb(71,114,179,255)` |
| `am_bool` | tri-state bool (`int8_t`) | assign `true`/`false`; unset sentinel is internal |
| `Degrees` | `float` rotation | |
| `AmTextureId` | opaque backend texture handle | from `backend.registerTexture(...)` |

Enums you'll touch often (all have a leading `NONE` = "unset"): `BorderMode{OUTLINE,MIDDLE,INSET}`,
`TextXAlignment{LEFT,CENTER,RIGHT}`, `TextYAlignment{TOP,CENTER,BOTTOM}`, `AutomaticSize{OFF,X,Y,XY}`,
`ZIndexBehavior{GLOBAL,SIBLING}`, `EventResult{CONSUMED,PROPAGATE}`.

## Context + backend lifecycle (engine integration)

`AmethystContext` owns font/atlas/draw state; `AmethystBackend` is the GPU interface a backend
implements (the Vulkan 1.3 backend ships in `backends/`). Typical wiring:

```cpp
AmethystContext amCtx;
amCtx.loadFont(".../font.ttf");      // load fonts before init
VkBackend backend;                    // your AmethystBackend impl
backend.init(initInfo, glfwInfo);
amCtx.init(backend);                  // creates atlas textures via the backend

Style::load(".../theme.ams");         // install the theme (global)

Window window;                        // root UI container
window.absoluteSize = {w, h};

// per frame:
amCtx.sync(cmdBuffer);                // upload dirty atlas data (cmdBuffer is backend-native, as void*)
amCtx.draw(window);                   // walk + record the tree
backend.record(cmdBuffer);
```

## Building UI: declarative scopes (preferred)

`UIScope` wraps a parent and exposes one builder method per component. Each takes a single
brace-initialized **DTO** and an optional lambda receiving a typed child scope. Migrate against this.

```cpp
UIScope(window)
    .frame({
        .classes = {"card"},                              // theme classes (see Theming)
        .base  = { .position = UDim2::fromOffset(20, 20),
                   .size     = UDim2::fromOffset(200, 120) },
        .style = { .backgroundColor = {0.2f, 0.2f, 0.2f},  // instance override (wins over theme)
                   .cornerRadius = 6.0f },
    },
    [&](FrameScope &f) {
        f.component.name = "panel";                        // .component is the live UIObject&
        f.textButton({
            .base  = { .size = UDim2::fromScale(1, 1) },
            .text  = { .textXAlignment = TextXAlignment::CENTER },
            .label = "Click me",
        },
        [&](TextButtonScope &b) {
            b.component.onMouseButton1ClickCb = []{ return EventResult::CONSUMED; };
        });
    });
```

`scope.component` is the constructed node (`T&`); use it for callbacks, names, extensions, and
runtime mutation. Nested builders parent into the enclosing scope automatically.

### Builder methods (`components/ui_scope.h`)

`canvas`, `frame`, `scrollingFrame`, `textLabel`, `textButton`, `imageLabel`, `imageButton`,
`invisibleButton`, `checkbox`, `collapsibleHeader`, `dropdown`, `menuBar`, `tabBar`, `table`,
`textInput`, `treeView`, `sliderFloat`, `sliderInt`, `color3Picker`, `color4Picker`.

Container scopes add helpers: `TabBarScope::tab(...)`, `TableScope::column/row`, `TreeViewScope::column/row`,
`DropdownScope::action/toggle/separator/submenu/items`, `MenuBarScope::menuItem`,
`CollapsibleHeaderScope::header/indicator`, and `DockScope::split/panel` (via `DockScope(dockingLayer)`).

### Color pickers

`color3Picker` / `color4Picker` edit a bound `Color3*` / `Color4*` in place (the `3`/`4` is the color,
not the picker). They are an inline styled core: a saturation/value square plus a hue bar (and an alpha
bar for `Color4Picker`), each draggable. Set `value` to the bound color; the picker pulls it into HSV on
build, drags write back through the pointer and fire `onValueChanged(const Color3&/Color4&)`. Plain
members tune it: `model` (`ColorModel::HSV`; `HSL` reserved), `shape` (`ColorPickerShape::SQUARE`;
`TRIANGLE`/`WHEEL` reserved), and `fieldThumbRadius`. The picker draws its own panel from `style`
(`BaseStyleProperties`) and insets its contents by `base.padding`. If the bound color is changed
externally, call `syncFromValue()` to re-sync and redraw.

## DTO model (the migration-sensitive part)

Every builder takes one `XProperties` struct. They share a consistent shape, **in this field order**:

```
classes   std::vector<std::string>   // theme classes (always field #0)
base      BaseProperties             // layout: position/size/anchor/padding/visible/zIndex/...
style     BaseStyleProperties        // generic visual surface: bg/border/cornerRadius (most components)
<config>  <Component>StyleProperties // component-specific block, named by role:
                                     //   scroll, slider, tabBar, table, treeView, checkbox,
                                     //   header (collapsible), dropdown, menuBar, textInput
text      TextStyleProperties        // text components only
image     ImageStyleProperties       // image components only
button    ButtonProperties           // button components only
<content> std::string / AmTextureId  // label / title / placeholder / valueSuffix / texture / svg
```

Exact composition per component is in `properties.h` (`FrameProperties`, `TextButtonProperties`, ...).
`SliderFloat`/`SliderInt` take typed `SliderFloatProperties`/`SliderIntProperties` (each carries a typed
`value` pointer). `Color3Picker`/`Color4Picker` take `Color3PickerProperties`/`Color4PickerProperties`,
which bind a `Color3*`/`Color4*` and carry `model`/`shape` instead of a component style block.

### Sentinel semantics (important and convenient)

Every property field defaults to an "unset" sentinel (`NaN` for floats/UDim/Color, `NONE` for enums,
`-1` for `am_bool`, `INT32_MIN`/`UINT32_MAX` for ints, empty `optional` for strings). `apply()` copies
**only set fields**. Practical consequences:

- You only fill the fields you want to override; everything else falls through to the theme.
- A partial DTO is correct and idiomatic. `{}` means "fully themed".
- For `am_bool` assign `true`/`false` (never `1`/`0`).
- Setting a field to its sentinel == "don't override".

### Shared block fields (cheat sheet)

`BaseProperties`: `active, anchorPoint, automaticSize, clipsDescendants, guiState, interactable,
layoutOrder, padding(UDim4), margin(UDim4), position(UDim2), size(UDim2), rotation, visible, zIndex,
zindexBehavior`.

`BaseStyleProperties`: `backgroundColor(Color3), backgroundTransparency, borderMode, borderPixelSize,
borderColor(Color3), borderTransparency, cornerRadius`.

`TextStyleProperties`: `fontSize, textColor(Color4), textXAlignment, textYAlignment, textTruncate,
richText, textWrapped, textScaled, lineHeight, strokeThickness, strokeColor(Color4),
fontFamily(optional<string>)`.

`ImageStyleProperties`: `imageColor(Color4), imageTransparency, scaleType, tileSize`.
`ButtonProperties`: `autoButtonColor, modal`.

Component style structs (`CheckboxStyleProperties`, `SliderStyleProperties`, `TableStyleProperties`,
`TreeViewStyleProperties`, `TabBarStyleProperties`, `ScrollingFrameStyleProperties`,
`CollapsibleHeaderStyleProperties`, `DropdownStyleProperties`, `MenuBarStyleProperties`,
`TextInputStyleProperties`) — see `properties.h`.

## Building UI: imperative (when scopes don't fit)

```cpp
auto *tv = window.add<TreeView>();                 // construct + parent + own, returns T*
tv->setBaseProperties({ .size = UDim2::fromScale(1,1) });
tv->setTreeViewProperties({ .rowHeight = 24.0f });

auto lbl = std::make_unique<TextLabel>();          // or build then reparent / move into a slot
lbl->parent = someNode;                             // raw parent ref
lbl->setBaseStyleProperties({ .backgroundTransparency = 1.0f });
lbl->setText("hello");
```

`UIObject` accessors: `setBaseProperties / getBaseProperties`, `setBaseStyleProperties /
getBaseStyleProperties`, plus a `set<Component>Properties` / `get<Component>Properties` per component.
Extensions: `addExtension<T>()` / `removeExtension<T>()` (e.g. `UIDragDetector`, `UIListLayout`,
`UIGridLayout`, `UISizeConstraint`, `UIAspectRatioConstraint`).

## Classes API (runtime)

```cpp
node.addClass("danger");
node.removeClass("danger");
node.hasClass("danger");
node.setClasses({"card", "primary"});   // initializer_list or span<const std::string>
node.getClasses();                       // span<const StyleKey> (interned hashes)
```

Declaratively, set `.classes = {"danger"}` in the DTO. Classes are interned (FNV-1a); the string lives
in a diagnostics registry. A class that matches no rule is inert (no warning).

## Theming: `.ams` stylesheets

CSS-like. A rule is a comma-separated selector list and a `{ ... }` declaration block. Load with
`Style::load(path)` (replaces the global theme).

### Selectors

| Form | Meaning | Example |
|------|---------|---------|
| `type` | component type (kebab) | `text-button { ... }` |
| `.class` | standalone class | `.danger { ... }` |
| `type.class` | type-qualified class | `text-button.primary { ... }` |
| `type#part` | built-in sub-element part | `collapsible-header#header { ... }` |
| `...:pseudo` | pseudo-state (parsed, **not yet applied**) | `.danger:hover { ... }` |
| `a, b` | group (shared block) | `.danger, frame { ... }` |

Type names (kebab, singular): `ui-object` (the global/base), `ui-button`, `ui-label`, `frame`,
`scrolling-frame`, `table`, `tree-view`, `text-button`, `image-button`, `text-label`, `image-label`,
`canvas`, `checkbox`, `dropdown`, `tab-bar`, `slider`, `radio-button`, `collapsible-header`.

### Values

- **Lengths** require `px`: `corner-radius: 4px; font-size: 14px;`
- **Ratios** are unitless `0..1` (or `%`): `background-transparency: 0.5;`
- **Dimensions** (UDim) take `px` / `%` / both: `padding: 8px;` `width: 50%;` `padding: 50% + 8px;`
- **Spacing shorthand** (whitespace-separated): `padding: 10px;` | `10px 20px;` (v,h) | `10px 20px 30px 40px;` (t,r,b,l)
- **Colors**: `#rgb`, `#rrggbb`, `#rrggbbaa`, `rgb(r,g,b)`, `rgba(r,g,b,a)` (channels 0..255)
- **Enums/fonts**: bare keywords (`outline`, `center`) / `font-family: "default";`

### Property names (kebab; full list in `style_properties.def`)

`background-color, background-transparency, border-color, border-transparency, border-pixel-size,
border-mode, corner-radius, padding (+ -top/-right/-bottom/-left), font-family, font-size, text-color,
text-x-alignment, text-y-alignment, line-height, stroke-thickness, stroke-color, scrollbar-color,
scrollbar-transparency, scrollbar-thickness, scrollbar-thumb-color, scrollbar-thumb-transparency,
row-height, cell-padding (+ sides), column-separator-width, column-separator-color,
row-background-color, row-alternate-color, row-hover-color, row-selected-color, indent-per-level,
disclosure-triangle-size, disclosure-triangle-padding, disclosure-triangle-color, slider-color,
slider-transparency, thumb-color, thumb-transparency, track-corner-radius, thumb-corner-radius,
check-color, check-transparency, checkbox-size, label-color, label-padding, value-color,
highlight-color, highlight-transparency, tab-width, tab-spacing, bar-thickness, tab-color,
tab-active-color, tab-hovered-color, tab-pressed-color, header-color, header-transparency, header-height`.

### Precedence (low to high)

1. pure type (type + its ancestors; more-derived wins)
2. standalone `.class`
3. `type.class` (more-derived qualifier wins)
4. instance overrides on the DTO/node

Within equal specificity, theme source order breaks ties. Example: a node with `{danger, primary}`
where the theme has `.danger` and `text-button.primary` → `text-button.primary` wins (tier 3 > tier 2).

### Built-in parts

A component attaches a reserved class to an internal sub-element, themable via `type#part`. Shipped:
`collapsible-header#header` (the header bar). The `#`-form can't collide with user `.class` names.

## Known gaps / caveats (read before migrating wide)

- **Theme coverage is incomplete for image components.** `image-label` and `image-button` resolve
  `BaseStyleProperties` from the theme but have no image-specific themeable properties (`imageColor`,
  `scaleType`, etc. are instance-only). All other components (`frame, text-label, text-button,
  scrolling-frame, slider, tab-bar, table, tree-view, checkbox, collapsible-header` and `ui-object`
  base) fully resolve their themed style.
- **Dynamic class change drops instance overrides.** `addClass`/`removeClass`/`setClasses` after you've
  set instance overrides re-applies the theme and loses the overrides (the `diff`-subtraction recovery
  isn't wired). Declarative construction (classes in the DTO) is correct. Prefer setting classes at
  build time; if you must toggle at runtime, re-apply overrides after.
- **DTO field order matters.** DTOs are aggregates used with designated initializers (C++20 requires
  declaration order). `classes` is field #0. Treat the layout as: new fields append at the end. A
  mid-struct insertion is a (loud, compile-time) break at call sites.
- **`:hover`/`:active`** are tokenized but skipped — no per-state theming yet.

## Stability summary

`.ams` grammar and the builder/DTO surface are structurally stable; the big refactors that churned them
are done. Expect the property *list* to grow (additive, safe) and a few components' theme wiring to be
filled in (no API change, visual change). The one genuine break risk is DTO field-order churn.
