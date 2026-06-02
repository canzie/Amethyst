# Properties Split Migration Plan

Staged plan to separate **style** (themeable visuals) from **layout/behavior** (instance
state), extract **content** out of the property structs, and collapse the multi-arg scope
signatures into per-scope input structs.

## Reconciliation: the partition already exists

The themeable/non-themeable line is already encoded by the `StyleProperty` enum in
`modules/style.h`. Every entry there is a themeable visual; position/size/anchor/rotation/
visible/active/zIndex are deliberately absent. So:

- `BaseStyleProperties` is **not a new taxonomy**. It is the struct-shaped mirror of the
  surface-related `StyleProperty` entries.
- `applyStyle()` (pull theme defaults via `Style::instance().get<T>(prop, type)`, then
  merge instance overrides) is the resolution step, already the working pattern in
  `slider.cpp::s_applyStyle`.
- Cost: themeable fields now have two representations (the `StyleProperty` variant cascade
  and the `*StyleProperties` structs). The field lists must stay in sync.

## Naming convention

`XProperties` = structural/layout/content data. `XStyleProperties` = the themeable visual
style that pairs with it.

- `BaseProperties` (layout + behavior, universal) + `BaseStyleProperties` (its surface
  visuals)
- `TextStyleProperties` (font/color/align/stroke) + the `text` string as content
- `ImageStyleProperties` (color/scale/tile) + `image`/`svg` as content

## Field partition

### BaseProperties (universal — every UIObject, including Container/InvisibleButton)
Layout: `anchorPoint`, `automaticSize`, `position`, `size`, `rotation`, `layoutOrder`,
`padding`, `margin`
Behavior: `active`, `visible`, `interactable`, `clipsDescendants`, `guiState`, `zIndex`,
`zindexBehavior`

`padding`/`margin` stay here even though themeable: a container has spacing. The split line
is "purely visual", not "themeable". Only fields a non-rendering node genuinely lacks leave.

### BaseStyleProperties (drawables that render a surface only) — 1:1 with StyleProperty
`backgroundColor`, `backgroundTransparency`, `borderMode`, `borderPixelSize`,
`borderColor`, `borderTransparency`, `cornerRadius`

### TextProperties -> TextStyleProperties + content
TextStyleProperties: `fontFamily`, `fontSize`, `textColor`, `textXAlignment`,
`textYAlignment`, `textTruncate`, `richText`, `textWrapped`, `textScaled`, `lineHeight`,
`strokeThickness`, `strokeColor`
Content (plain `std::string`, not in the cascade): `text`

### ImageProperties -> ImageStyleProperties + content
ImageStyleProperties: `imageColor`, `imageTransparency`, `scaleType`, `tileSize`
Content: `image` (`AmTextureId`), `svg` (`std::string`)

## UIObject owns no styling

`UIObject` stores `BaseProperties` only. The surface visuals move down to the concrete
drawables that render a rect: `Frame`, `TextLabel`, `ImageLabel`, `TextButton`,
`ImageButton`. `Container`/`InvisibleButton` hold no `BaseStyleProperties` at all.

`UIObject::createInstanceData()` takes the style as a pointer:
`createInstanceData(const BaseStyleProperties *style)`. When non-null, the 5 style lines
(120,121,123,124,126) read from it instead of `m_uiObjProps`; when null, they are skipped
and no surface is baked. Surface drawables pass `&m_baseStyle`; non-surface nodes
(`Container`/`InvisibleButton`) pass `nullptr`. `UIObject` stores no style and reads none
from itself, baking stays in one place.

`BaseStyleProperties` is stored on the leaf drawables (`Frame`, `TextLabel`, `ImageLabel`,
`TextButton`, `ImageButton`), not `UIButton`, so `InvisibleButton` (a `UIButton`) carries
no surface.

## Scope-input structs

One DTO per scope type, pure construction ergonomics. The scope unpacks it and calls the
component's granular setters (`setBaseProperties`, `setBaseStyleProperties`, ...). Example:

```cpp
struct TextButtonProperties {
    BaseProperties base;
    BaseStyleProperties style;
    TextStyleProperties text;
    std::string content;
    ButtonProperties button;
};
// .textButton({.base = {...}, .style = {...}, .text = {...}, .content = "Save"})
```

## Landings

Each landing is atomic: the tree breaks mid-landing, compiles at the boundary. Ordered by
dependency. Landing 3 is independent ergonomics and can be deferred.

### Landing 1 — BaseStyleProperties extraction
- `properties.h`: add `BaseStyleProperties`; remove the 7 visual fields from
  `BaseProperties`.
- `ui_object.h/.cpp`: `createInstanceData(const BaseStyleProperties *style)`; the 5 style
  lines read from it when non-null, skipped when null.
- Surface drawables (`Frame`, `TextLabel`, `ImageLabel`, `TextButton`, `ImageButton`):
  store `BaseStyleProperties`, add `setBaseStyleProperties`/getter, pass `&m_baseStyle` to
  `createInstanceData` in `draw`. `frame.cpp::applyStyle` pulls the struct from `Style`
  for `FRAME`.
- `invisible_button.h`: drop `backgroundTransparency = 1`, hold no style.
- Scope plumbing: add `BaseStyleProperties style = {}` to the surface-bearing `UIScope`
  methods in `ui_scope.h/.cpp`.
- Consumers reading/writing visuals through `BaseProperties` -> `BaseStyleProperties`:
  `scrolling_frame.cpp`, `slider.cpp`, `menu_bar.cpp`, `collapsible_header.cpp`,
  `tab_bar.cpp`, `table.cpp`, `tree_view.cpp`, `dropdown.cpp`, `text_label.cpp`,
  `text_button.cpp`, `docking_layer.cpp`, `widgets/gizmo.cpp`.
- Demos: `main.cpp`, `demo_treeview.cpp`, `demo_svg.cpp`, `demo_menus.cpp`,
  `demo_collapsible_header.cpp`.

### Landing 2 — content extraction (TextStyleProperties / ImageStyleProperties)
- `properties.h`: split `TextProperties` -> `TextStyleProperties` + `text` string;
  `ImageProperties` -> `ImageStyleProperties` + `image`/`svg`; update embeds
  (`TextInputProperties`, `CollapsibleHeaderProperties.title`, `TableProperties.headerText`)
  to the style structs; `applyTextProperties` -> `applyTextStyleProperties` (drops `text`).
- Component members: `m_textProps` -> `TextStyleProperties m_textStyle` + `std::string
  m_text`; same shape for image/checkbox/slider/text_input content.
- Readers: `text_label.cpp`, `text_button.cpp`, `text_input.cpp`, `collapsible_header.cpp`,
  `table.cpp`, `slider.cpp`, `dropdown.cpp`, `tab_bar.cpp`, `checkbox.cpp`,
  `image_button.cpp`, `image_label.cpp` + demos.
- Decision: `textTruncate`/`richText`/`textWrapped`/`textScaled` have no `StyleProperty`
  entry. Keep in `TextStyleProperties` as non-theme-backed fields, or add enum entries.

### Landing 3 — scope-input structs (ergonomics, independent)
Replace the multi-positional scope signatures with one `XProperties` DTO per scope type.
Scope methods become single-arg. Touches `ui_scope.h/.cpp` + all demos. No semantic change.

## Resolved decisions
1. Styling moves out of `UIObject` into the surface drawables (stored on leaves, not
   `UIButton`); `createInstanceData(const BaseStyleProperties *style)` takes it as a pointer
   (`nullptr` = no surface).
2. `padding`/`margin` stay in `BaseProperties`.
3. Content (`text`, `image`, `svg`) is content, not style; plain `std::string`, only
   `fontFamily` is `optional<string>`.

## Sequencing
Order is 1 -> 2 -> 3. Landing 3 runs last. No point interleaving it: the tree stays broken
through 1 and 2 regardless, so the ergonomic DTO pass goes on top once the split is done.
