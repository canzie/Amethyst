# Styling System Plan (struct getters + classes, cached)

Status: **IMPLEMENTED** on branch `style_system_refactor` (builds clean, demo runs, theme parses with
no warnings). Ground-up **replacement** of the legacy `Style` system (the `get<T>` flat-variant layer),
full feature set in one go (standalone classes + type-qualified classes), performance-first.

### As built (deltas from the design below)
- **Value is `std::variant<Color3, Color4, float, UDim, BorderMode, TextXAlignment, TextYAlignment,
  FontHandle>`**, not a hand-rolled tagged union. A variant of trivially-copyable alternatives is itself
  trivially copyable, so the dense-set goal holds; `FONT_FAMILY` still interns to a `FontHandle`. The
  getters use `std::get<T>` rather than a custom `.as<T>()`.
- **X-macro tables live in `modules/style_properties.def`** (included by `style.cpp` + the parser), not
  inline in `style.h`.
- **Class-change re-resolution is construction-order, not subtraction.** `addClass`/`setClasses` call the
  virtual `UIObject::resolveStyle()` which re-applies the full themed structs via the per-struct getters;
  the scope sets classes before the instance style DTO, so the declarative path is correct. The
  `diff()`-based subtraction (to preserve inline overrides on a *dynamic* `addClass` after instance
  overrides) is NOT wired yet -- `diff()` exists on every struct, ready to use when needed. Known
  limitation: a dynamic `addClass` after instance overrides re-applies theme and drops those overrides.
- **Built-in sub-element class** landed only on `CollapsibleHeader`'s header bar (now `collapsible-header#header`,
  with `headerColor` left sentinel so the class drives it). `Slider` thumb was skipped (it is drawn from
  `SliderStyleProperties`, not a child `UIObject`, so it has no class target).
- **TOML is gone; the custom CSS parser shipped** (`.ams` files). See the updated
  "Custom CSS parser (DONE)" section. toml++ is NOT removed from the build -- `parsers/config/layout_config.cpp`
  still depends on it, so only the theme front-end was swapped.

## Premise: replace, don't extend

The current `modules/style.h` (`get<T>`/`set`, `getDefault`, `getPropertyNames`, the per-component
`applyStyle` blocks copying values field-by-field) predates the `*StyleProperties` structs. The
value path is scattered across every component as per-property `get<T>` calls. That is removed.

The new system keeps a property enum + a value map as the storage core (uniform, cache-friendly),
but the consumer side is one generated getter per `*StyleProperties` struct that builds a fresh,
fully-typed struct from a resolved value set. No per-property lookups in components.

## Vocabulary

- **Property** (`StyleProperty` enum, kept): one themeable key, e.g. `BACKGROUND_COLOR`. The enum is
  contiguous `0..NUM_STYLE_PROPS`, used as a dense array index.
- **Value**: a small **POD** tagged union (`Color4 | float | enum | UDim | fontHandle`, ~20 bytes),
  trivially copyable. `FONT_FAMILY` (the only string property) interns to a `uint` font handle so
  `Value` stays POD. (Replaces the old `std::string`-bearing `StyleValue` variant.)
- **Set**: a collection of `Property -> Value`. Two physical forms (below).
- **Scope**: what a stored set belongs to -- a `ComponentType`, a class, or a `(ComponentType, class)`
  pair.

## Two set representations (deliberate, for performance)

- **Sparse stored sets** -- raw theme input, authored with 1-3 properties. Small flat list of
  `(StyleProperty, Value)`. Used for: raw per-type overrides, standalone class sets, qualified
  `(type, class)` sets.
- **Dense resolved sets** -- `std::array<Value, NUM_STYLE_PROPS>` indexed by `StyleProperty`. Used
  for: the baked per-type sets and the cached merged results. Built from full defaults, so every
  slot is populated -> a getter read is a plain array index, no hashing, no presence test. Trivially
  copyable, so a copy is a `memcpy`.

## Storage

```cpp
std::unordered_map<ComponentType, SparseSet>              m_rawType;       // per-type theme overrides
std::unordered_map<StyleKey, SparseSet>                   m_classSets;     // standalone class, key = FNV-1a hash
std::unordered_map<TypeClassKey, SparseSet>               m_typeClassSets; // (type, class) qualified rules
std::unordered_map<StyleKey, uint32_t>                    m_classOrder;    // class hash -> theme source order
std::unordered_map<TypeClassKey, uint32_t>               m_typeClassOrder; // (type,class) -> theme source order
// TypeClassKey packs (ComponentType, classHash); StyleKey is now ONLY a class-name hash.
```

## Baked type sets (static, at load)

The type hierarchy never changes at runtime, so bake one dense full set per component type after
parsing (replaces `getDefault()` + the type walk in `get<T>`):

```cpp
std::unordered_map<ComponentType, DenseSet> m_typeResolved;
// for each ComponentType t:
//   DenseSet d = s_defaults;                                   // full: every prop -> default value
//   for (ComponentType h : getTypeHierarchy(t) /* general -> specific */) applySparse(d, m_rawType[h]);
//   m_typeResolved[t] = d;
```

A node with **no classes** uses `m_typeResolved[type]` directly -- zero runtime work.

## Precedence: a sort key per contributing rule

Every rule that matches a node gets a sort key; sort ascending, apply low->high (later overwrites):

```
tier 0  pure type        T          -> (0, depth(T))                 [all baked into m_typeResolved]
tier 1  standalone class .C         -> (1, sourceOrder(C))
tier 2  type + class     T.C        -> (2, depth(T), sourceOrder(T.C))
tier 3  instance overrides          -> always last (applied by the component after resolve)
```

`depth(T)` = index in the node's hierarchy (general low, derived high). Consequences (all CSS-aligned,
with the type hierarchy as an Amethyst-specific refinement of tier 2):
- A class always beats the whole pure-type chain (tier 1,2 > tier 0).
- A type-qualified class beats a standalone class (tier 2 > tier 1).
- Among qualified rules, the one qualified by the more-derived type wins (`text_button.C` over
  `ui_button.C`), independent of class list order or theme order.
- Source order is only the **tiebreak** within equal specificity (two standalone classes; or two
  qualified rules sharing the same qualifying type).

## Matching (which rules apply to a node)

A rule contributes iff its type part is the node's type or an ancestor (or absent = global) AND its
class part is a class the node carries. For a `Frame` with `classA`, candidates are: `m_typeResolved[FRAME]`
(tiers 0), `m_classSets[classA]` (tier 1), and `m_typeClassSets[(FRAME,classA)]` /
`m_typeClassSets[(UI_OBJECT,classA)]` (tier 2). A `text_button.classA` rule cannot match (a Frame is
not a TEXT_BUTTON). A class that matches no rule is **inert** -- applied as nothing, no warning.

## Runtime resolution + cache

```cpp
const DenseSet &Style::resolveSet(ComponentType type, std::span<const StyleKey> classes)
{
    if (classes.empty()) { return m_typeResolved[type]; }            // baked, no work

    CacheKey key{type, sorted(classes)};                            // order-independent
    if (auto *hit = m_cache.get(key)) { return *hit; }              // one hash lookup

    DenseSet d = m_typeResolved[type];                              // POD memcpy of the baked set
    // collect matching tier-1 + tier-2 sparse sets, sort by the tier key (tiny: ~k * hierarchyDepth)
    for (const SparseSet *s : sortedContributors(type, classes)) { applySparse(d, *s); }
    return m_cache.put(key, std::move(d));                          // LRU, capped
}
```

- **Cache key** = `(type, sorted class hashes)`. Attach order never affects the result (precedence is
  tier/depth/source-order), so sorting the hashes makes equivalent nodes share one entry. Key hash =
  fold `type` with the sorted class hashes.
- **LRU cache**, capped (default 256, configurable; cleared on theme reload). ~1.4 KB/entry, so 256 is
  ~360 KB. Eviction O(1) (intrusive list + map).
- The only repeated cost is a single hash lookup (cache hit) per distinct combination; the merge runs
  once per combination, never per frame.

## Per-struct getters (consumer side)

One generated getter per `*StyleProperties` struct reads the resolved dense set and builds the struct:

```cpp
BaseStyleProperties Style::getBaseStyle(ComponentType type, std::span<const StyleKey> classes)
{
    const DenseSet &d = resolveSet(type, classes);
    BaseStyleProperties r;
#define X(e, field, t, tag) r.field = d[static_cast<size_t>(StyleProperty::e)].as<t>();
    BASE_STYLE_FIELDS(X)
#undef X
    return r;
}
```

Component style step -- no scattered `get<T>`, ~20 array reads at construction:

```cpp
void TextButton::resolveStyle()
{
    auto cls = getClasses();
    m_baseStyle = Style::instance().getBaseStyle(ComponentType::TEXT_BUTTON, cls);
    m_textStyle = Style::instance().getTextStyle(ComponentType::TEXT_BUTTON, cls);
    m_button    = Style::instance().getButtonProps(ComponentType::TEXT_BUTTON, cls);
    // scope DTO instance overrides apply() on top afterward (tier 3)
}
```

(`fontFamily` getter resolves the font handle back to a string via the intern registry; one of the
two special cases below.)

## X-macro field lists (single source of truth)

Each struct's fields are declared once as `(enum, field, cpp type, parse tag)`; everything else is
generated, so the duplication cannot drift:

```c
#define BASE_STYLE_FIELDS(X)                                          \
    X(BACKGROUND_COLOR,        backgroundColor,        Color3, COLOR3)\
    X(BACKGROUND_TRANSPARENCY, backgroundTransparency, float,  FLOAT) \
    X(BORDER_MODE,             borderMode,             BorderMode, BMODE) \
    X(BORDER_PIXEL_SIZE,       borderPixelSize,        float,  FLOAT) \
    X(BORDER_COLOR,            borderColor,            Color3, COLOR3) \
    X(BORDER_TRANSPARENCY,     borderTransparency,     float,  FLOAT) \
    X(CORNER_RADIUS,           cornerRadius,           float,  FLOAT)
```

Generated from the one list: the struct fields (`t field = propUnset<t>();`), `apply()` + `diff()`
(replaces the `AM_APPLY` blocks), the parser entry (`"backgroundColor" -> BACKGROUND_COLOR` + parse
fn by tag, replaces `getPropertyNames` + the parse `switch`), the per-struct getter body, and the
`s_defaults` contribution. `propUnset<T>()` wraps the existing `PROP_UNSET_*` constants. Two special
cases get one-off rows outside `X`: `TextStyleProperties::fontFamily` (`optional<string>` + font
handle), and nested sub-structs (`TableStyleProperties::header` is a `TextStyleProperties` -> an
`X_SUB` row that recurses).

## Classes

- A class name interns to a `StyleKey` via FNV-1a 32-bit; the token is both node-side identity and
  store key. The string lives only in a `Style`-side `hash -> name` registry (diagnostics + collision
  assert).
- `UIObject` carries `std::vector<StyleKey> m_classes` (empty = no alloc; 4 bytes/class).
- API, declarative + imperative:
  ```cpp
  void addClass(std::string_view name);
  void removeClass(std::string_view name);
  bool hasClass(std::string_view name) const;
  void setClasses(std::initializer_list<std::string_view> names);
  // scope DTOs gain: std::vector<std::string> classes;  ui_scope.cpp applies classes BEFORE the
  // style DTO so instance overrides (tier 3) win.
  ```
- First pass scope of *selectors*: standalone `.class` and type-qualified `type.class`. Future
  punctuation (see the custom-parser section): `#` = built-in sub-component part, `@` = id, `:` =
  pseudo-state. All deferred. No descendant/child combinators.

### Built-in / default classes

Internal sub-elements get an implicit documented class at construction (`addClass` on the sub-element)
so themes can target parts (`CollapsibleHeader` header bar, `Slider` thumb/track). Interim spelling
(TOML era) is BEM `block__element` as a plain class name (`collapsible-header__header`) -- bare-legal,
no quoting. Under the custom parser this becomes the `type#part` form (`collapsible_header#header`); the
component attaches it the same way internally either way.

## Class changes / re-resolution

`setClasses`/`addClass`/`removeClass` re-run `resolveStyle` (usually a cache hit). Inline overrides are
recovered by subtraction (no stored override layer), since the resolved set is a pure recomputable
function:
```
inst         = diff(node structs, getXStyle(type, oldClasses))   // recover inline overrides
m_classes    = newClasses
node structs = getXStyle(type, newClasses), then .apply(inst)    // rebuild + re-layer
```
`diff()` is generated from the same X-macro list (inverse of `apply()`). Field `==` is safe (values
assigned verbatim). Harmless edge: an inline override equal to the resolved value reads as no override.
Run eagerly in the setter; mark `FLAG_DIRTY`. Drop the node's cache entry only if its class-set became
otherwise-unreferenced (LRU handles it; no manual refcounting needed).

## Parsing

TOML is **interim** (custom format planned) -- keep the parser thin; only the value parsers
(`parseColor3/4`, `parseUDim`, spacing shorthand, reused) and the section walk are format-specific. A
section header maps to a Scope:
- `[textButtons]` -> `ComponentType` -> `m_rawType[type]`.
- `[class.danger]` / `.danger` -> class hash -> `m_classSets`, stamp `m_classOrder`.
- `[textButtons.danger]` / `text_button.danger` -> `(type, hash)` -> `m_typeClassSets`, stamp
  `m_typeClassOrder`.

For each `key = value`, the generated parser entry parses `value` by tag and inserts into the current
scope's sparse set. All three scope kinds share the same entries -- zero new property mapping.

### Custom CSS parser (DONE)

Shipped as built. Deltas from the design sketch below:
- **File extension is `.ams`** (`libamethyst/assets/theme.ams`). All demos load it.
- **Property + type names are kebab-case.** The `AM_STYLE_PROPS` X-macro gained a column: rows are now
  `(ENUM, "kebab-key", CppType, ParseTag, default)`. `ParseTag` (`COLOR3/COLOR4/LENGTH/RATIO/UDIM/BMODE/
  XALIGN/YALIGN/FONT`) selects value syntax independently of the C++ type, so `LENGTH` can require `px`
  while `RATIO` stays unitless. Type selectors use a kebab map (`ui-object`, `text-button`, ... replacing
  `general`, `textButtons`).
- **Units:** lengths require `px` (warn if missing), ratios are bare `0..1` (or `%`), UDim takes
  `px`/`%`/`50% + 8px`. Spacing shorthand is whitespace-separated (`padding: 10px 20px;`).
- **Colors:** `#rgb` / `#rrggbb` / `#rrggbbaa`, plus `rgb()` / `rgba()` (alpha 0-255). The TOML `[r,g,b,a]`
  array form is gone.
- **Comma groups** (`.danger, frame { ... }`): the declaration block is parsed once and routed to each
  selector independently; each class/type-class selector gets its own source-order stamp.
- **`:pseudo` is tokenized but skipped** (debug log), reserved for the future `:hover`/`:active` work.
- **`#` parts reuse the class store:** `collapsible-header#header` interns the whole selector string as a
  standalone class token; the component attaches the identical string via `addClass`. No new scope kind,
  no `style.h` change. The `#` in the canonical name structurally cannot collide with a `.class` selector
  (which tokenizes a bare identifier), so parts are self-namespacing.

### Original sketch: drop TOML, write our own CSS parser

The TOML shape is a poor fit -- classes have to hide under `[class.x]` / `[type.class]` tables because
TOML has no selector sigils, so `.danger` becomes `[class.danger]` and `button.primary` becomes
`[textButtons.primary]`, and built-in sub-element classes need the interim BEM `block__element` spelling
(`collapsible-header__header`). Replace it with a small hand-written CSS-ish parser using our own
selector punctuation:

| sigil | meaning | example |
|-------|---------|---------|
| (bare) | component type | `text-button { ... }` |
| `.` | class | `.danger { ... }` |
| `#` | **built-in sub-component part** (replaces BEM `__`) | `collapsible_header#header { ... }` |
| `@` | id, only if we ever allow ids in the stylesheet | `@my-id { ... }` |
| `:` | pseudo-state (later) | `.danger:hover { ... }` |

Note the deliberate reassignment: `#` is NOT id here -- it selects a component's named internal part
(so `collapsible_header#header` targets the header bar), and `@` is the id sigil if ids land. The BEM
`__` convention goes away entirely; a built-in part is authored as `type#part` and the component still
attaches it the same way internally.

```css
text-button              { background-color: #545454; corner-radius: 4; }
.danger                  { background-color: #cc3333; color: #fff; }
text-button.primary      { background-color: #4772b3; }
collapsible_header#header { background-color: #404047; }
```
Parsing this is trivial and removes the toml++ dependency for themes: tokenize a selector up to `{`
(a type name, `.class`, `type.class`, or `type#part`), then `prop: value;` declarations up to `}`. The
selector parse maps onto the existing scopes (`addTypeValue` / `addClassValue` / `addTypeClassValue`,
plus a part scope for `#`), and the declaration side reuses the SAME generated property table + value
parsers -- only the surrounding syntax changes. Keep the `StyleValue`/scope storage and the getters
exactly as-is; this is a front-end-only swap. (The CURRENT TOML parser + BEM `__` class names stay until
this lands -- they work as is.)

### Deleted vs kept

Deleted: `get<T>`/`set`/`hasValue`, `getDefault()` switch, `getPropertyNames()`, `StyleKey` packing,
the `std::string` `StyleValue` variant, the per-component `applyStyle` field-by-field blocks.
Kept: `StyleProperty` enum (now the dense index + sparse-set key), `ComponentType` +
`getTypeHierarchy`, `getComponentTypeNames`, the value parsers, the `*StyleProperties` structs.

## Pseudo-states (future, reserve only)

**Superseded by `style_layering_plan.md`.** Pseudo-states were implemented on top of this system, but
doing so exposed that the `*StyleProperties` structs conflate three roles (live value, theme-resolved
return, setter-argument DTO) into one type, which made "does an instance override survive a state/class
change" fundamentally unreliable without a proper fix. See that doc for the from-scratch design
(live/args/override-mask struct triplet per style type) before touching pseudo-state code further.

- Syntax `.foo:hover`, `text_button:active`. `:` reserved now, not wired.
- Data model: a scope can hold per-state sets; at resolve time pick the set matching the node's
  current `GuiState` (already on `BaseProperties`), else the base set. No new event wiring. Adds a
  tier dimension to the sort key; the `diff`-based re-resolution makes per-state re-layering tractable.

## Suggested build order

1. POD `Value` + font-handle interning + `propUnset<T>()`. X-macro field lists for each
   `*StyleProperties`; generate struct fields + `apply()` + `diff()` + `s_defaults` + parser entries +
   getter bodies. Handle the `fontFamily` and nested-struct special cases.
2. Rewrite `modules/style.h/.cpp`: sparse/dense set types, `m_rawType` / `m_classSets` /
   `m_typeClassSets` (+ order maps), `s_defaults`, `m_typeResolved` baking, the tier sort +
   `resolveSet`, the LRU cache, FNV-1a `classToken` + name registry, and the generated per-struct
   getters. Keep `ComponentType` + `getTypeHierarchy` + `getComponentTypeNames`. Drop
   `get<T>`/`getDefault`/`getPropertyNames`.
3. Rewrite the theme parser: three scope-section kinds, generated parser entries, value parsers ported.
4. Replace each component's `applyStyle` with `resolveStyle` (call the per-struct getters).
5. Class storage + API on `UIObject` with subtraction re-resolution; scope DTOs gain `classes`;
   `ui_scope.cpp` applies them before the style DTO.
6. Built-in BEM classes on `CollapsibleHeader` header bar + `Slider` thumb to validate sub-element
   theming.
7. Only then design `:hover`/`:active`.
```
