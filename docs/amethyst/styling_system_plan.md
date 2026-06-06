# Styling System Plan (classes + theme cascade)

Status: NOT STARTED. Planning doc for a fresh session. Builds on the completed
properties split (`properties_split_plan.md`): themeable visuals already live in
`*StyleProperties` structs with sentinel-based `apply()` merging, and `BaseStyleProperties`
is a plain member on `UIObject` (`m_baseStyle`).

## Goal

Add a CSS-like **class** selector layer on top of the existing `Style`/`StyleProperty`/
`ComponentType` cascade in `modules/style.h`, so a theme file can target groups of nodes
by class (e.g. `.danger`, `.sidebar-item`) instead of only by component type. Includes
**built-in (default) classes** on internal sub-elements so themes can style parts like a
collapsible header's header bar.

Out of scope for the first pass (but design must not preclude them):
- **IDs** (`#foo`). Deferred. Model classes only for now.
- **Pseudo-states** (`:hover`, `:active`, `:focus`). Deferred, but reserve syntax + data
  model. Maps naturally onto the existing `GuiState` field.
- Selector combinators (`.a .b` descendant, `>` child), attribute selectors. Not planned.

## CSS recap (decisions baked in)

- An element has **many classes** (space-separated list); **one id** (unique). We do
  classes only for now.
- The HTML attribute is `class`; the JS/DOM + React property is `className`. Tailwind is
  just utility classes in the normal `class` -- not a separate mechanism.

## Naming (term for "class")

`class` (singular) is a C++ keyword, but `classes`, `addClass`, `hasClass`, `removeClass`
are all legal identifiers. 

**Decision: use CSS vocabulary** -- member `classes`, methods `addClass`/`removeClass`/
`hasClass`/`setClasses`. Rationale: we are deliberately mirroring CSS, and the future
`:hover`/`:active` work reads naturally as "class + state". Runner-up if we want zero
keyword ambiguity: `styleClass`/`styleClasses`/`addStyleClass`.

## Data model

- Class tokens are interned to a small integer/hash (reuse the `StyleKey = uint32_t` hashing
  already in `style.h`) so per-node storage and lookup are cheap. Store a hash, keep the
  string only in the theme/registry side for diagnostics.
- Each node carries an ordered, small set of class tokens. Likely a `small_vector`-style
  inline buffer (most nodes have 0-2 classes); avoid a heap alloc for the common case.
- **Where it lives:** on `UIObject` (styling is visual; `Container`/`InvisibleButton` carry
  it transparently like `m_baseStyle` does). Revisit if layout-only nodes ever need it.

## Cascade / precedence

Lowest to highest (CSS specificity minus id, minus `!important`):
1. **Component-type defaults** -- existing `ComponentType` hierarchy walk
   (`TEXT_BUTTON -> UI_BUTTON -> UI_OBJECT`).
2. **Class rules** -- a node's classes, resolved against theme class blocks.
3. **Inline / instance overrides** -- whatever the caller passed via the scope DTO
   (`setBaseStyleProperties`, `setTextStyleProperties`, ...). These already win because
   `apply()` only overwrites set (non-sentinel) fields.

Multiple classes setting the same property: **theme source order wins** for equal
specificity (matches CSS). Document and test this explicitly. (Alternative considered:
node's class-attachment order -- rejected, CSS uses rule order, keep parity.)

## Built-in / default classes (the key new idea)

Internal sub-elements created by a component get an implicit, documented class so themes can
target them without the component exposing every sub-part as a property. Examples:
- `CollapsibleHeader`'s header bar -> built-in class on `m_headerBackground`.
- Slider track/thumb, table header row, dropdown popup, tab buttons, etc.

Naming convention TBD -- pick ONE:
- BEM-ish: `collapsible-header__header`, `slider__thumb`.
- dotted/scoped: `collapsible-header.header` (the user's phrasing; note `.` collides with
  CSS class-selector syntax in a theme file, so if we keep `.` it's only a *display* name,
  the selector would still be a single token).
- short prefix: `ch-header`, `slider-thumb`.

Recommendation: **BEM `block__element`** -- unambiguous, no selector-syntax collision, and
the `block` half can mirror the component's theme `ComponentType` name. User can override.

These defaults are applied by the component itself at construction (addClass on the
sub-element), so they always resolve through the same cascade and a theme can restyle them.

## Theme file (theme.toml) extensions

Current: type-keyed tables feeding `Style::instance().get<T>(prop, ComponentType)`.
Add: class-keyed blocks. Sketch (exact TOML shape TBD):
```
[class.danger]
background_color = "#cc3333"
text_color = "#ffffff"

[class."collapsible-header__header"]
background_color = "#222831"
```
Keep the `StyleProperty` field names as the single source of truth for keys (they already
mirror the struct fields). Reuse the existing `StyleValue` variant + parser.

## Resolution timing

`applyStyle()` currently runs once in each component ctor. With classes:
- Re-resolve when a node's class set changes (`addClass`/`setClasses` -> mark dirty +
  re-pull) and on theme reload.
- Resolved values feed the SAME `apply()` path: theme produces a `*StyleProperties` with the
  matched fields set, component does `m_x.apply(themeResult)`, then instance overrides
  `apply()` on top. Net: one merge pipeline, classes are just another source layer.
- Decide: resolve eagerly on change, or lazily at draw when `FLAG_DIRTY`. Lean lazy/at-draw
  to batch (a node may get several classes added in a row).

## Pseudo-states (future, reserve only)

- Syntax: `.foo:hover`, `text-button:active`.
- Data model: a class rule can hold per-state property sets. At resolve time, pick the set
  matching the node's current `GuiState` (the field already exists on `BaseProperties`),
  falling back to the base set.
- No event wiring needed beyond what already drives `GuiState`; resolution just keys off it.

## API sketch

```cpp
// on UIObject
void addClass(std::string_view name);
void removeClass(std::string_view name);
bool hasClass(std::string_view name) const;
void setClasses(std::initializer_list<std::string_view> names);
// scope DTOs gain an optional: std::vector<...> classes; (or small inline set)
// e.g. .frame({.classes = {"card", "danger"}, .base = {...}})
```

## Open questions

1. Token storage type + interning location (global registry in `Style`?).
2. Exact TOML schema for class blocks and (later) `:state` blocks.
3. Built-in class naming convention (recommend BEM `block__element`).
4. Do scope DTOs carry `classes`, or only the imperative `addClass` API, or both?
5. Multi-class same-property tie-break confirmed as theme source order.
6. Eager vs lazy (at-draw) re-resolution.

## Suggested first-pass order

1. Class storage + `addClass`/`hasClass` on `UIObject` (no theme hookup yet).
2. Theme parser: `[class.X]` blocks -> a class -> `*StyleProperties` map in `Style`.
3. Resolution: fold class results into the existing per-component `applyStyle()` /
   `apply()` pipeline, with the cascade order above.
4. Add built-in classes to a couple of components (start with `CollapsibleHeader` header
   bar and `Slider` thumb) to validate sub-element theming.
5. Only then design `:hover`/`:active`.
