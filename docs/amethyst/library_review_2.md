# Library Review 2

Second audit, 2026-11-16, on top of `df86a89`. Five read-only reviewers covered the core
tree and input, rendering and the Vulkan backend, text, the composite widgets, and the
styling system with the value widgets. The items in `library_review.md` were skipped.
Fixes landed on the `review-fixes` branch in passes, each with GoogleTest coverage.
Everything below was built and run under ASan and UBSan. Nothing was run on a GPU, so the
shader and Vulkan changes are only known to compile.

Testapp issues (frame loop, swapchain, upload order) are out of scope by decision.

---

## Tests

- GoogleTest, fetched by `libamethyst/tests/CMakeLists.txt`.
- One executable per directory: `tests/<suite>/*_test.cpp` builds `amethyst_<suite>_tests`.
- Shared helpers live in `tests/common/`:
  - `ui_test_helpers.h`: `TestWindow` with press/release/click/move/scroll, `place()`,
    `TestButton`, `TestNode`.
  - `fake_backend.h`: a GPU-free `AmethystBackend`.
- `AMETHYST_SANITIZE=ON` builds everything with ASan and UBSan.

| Suite | Tests | Covers |
|---|---|---|
| config | 6 | layout config round trips (ported from the old hand-rolled test) |
| core | 37 | EventSignal, Instance tree, Window dispatch/hover/press, input modes, buttons |
| rendering | 53 | GpuArena, free list, packers, glyph buffer, resource hub, colour packing |
| text | 53 | utf8, text processor, TextArea, UITextField, NumberInput, labels |
| widgets | 39 | Table, TreeView, TabBar, ContextMenu/Dropdown, header, tooltip, scroll bars |
| layout | 9 | list/grid layout, anchor |
| style | 54 | parser robustness, cascade, specificity, parts, class propagation, reload |
| math | 7 | half packing (exhaustive), colour conversions |
| widgets_value | 14 | number formats, slider/drag ranges, log drag, colour picker hue |

## What was fixed

### Core: lifetime and input (`40847e0`, `74687b2`, `c46b831`)

- `EventSignal::fire` is re-entrant. It keeps each slot alive while the slot runs, holds
  slots connected mid-fire until the next fire, and stops if the signal is destroyed.
  It returns `false` in that case.
- `Instance::aliveToken()` / `isAlive()`. Dispatch, `UIObject` and `UIButton` check it after
  calling user code, so a callback may destroy its own node, its row, or its table.
- The hover stack holds weak tokens. It used to add one `onDestroy` slot per hover-enter,
  which leaked without bound, and that slot captured the Window.
- The node that consumed a press is told (`onPressCancelled`) when the release lands
  elsewhere. Buttons no longer stay pressed or click from a stale press. Button 2 needs
  its own press too.
- `~Window` destroys the overlay before the tick list and tooltip stack.
- `reparent` refuses cycles and a missing parent.
- Scope items captured the stack-allocated scope.
- Hover refreshes after a wheel scroll once layout has run.
- **Input modes**, per interaction category:
  - `consume` is the default and stops the event.
  - `propagate` receives the event and lets it continue.
  - `ignore` is new: the node is skipped by hit-testing for that category, but its
    children are still tested.
  - Labels propagate by default, so text over a row, a tree row, or the header title no
    longer swallows clicks.
  - Hover falls through propagating nodes the same way clicks do.
- **UIButton callbacks are `EventSignal`s** with `void` listeners, matching UIObject: the
  virtual `onX` is the internal path, the signal is for outside listeners. Whether a click
  carries on is the button's input mode, never a listener's return value.
  - `onMouseEnterCb`, `onMouseLeaveCb` and `onMouseMovedCb` are removed. Use
    `onHoverChanged` and `onInputChangedCb` instead.

### Rendering (`ca5ba4e`)

- GPU arena frees are deferred by the number of frames in flight (`setFramesInFlight`,
  default 2), so a freed block can't be handed to another registry while an in-flight
  frame still reads it.
- The texture upload barrier waits on fragment reads and keeps the shader-read layout.
  There is one staging buffer per frame in flight.
- Every bindless texture lookup uses `nonuniformEXT`.
- A failed text allocation or glyph-block growth draws no text instead of another
  registry's glyphs, and keeps its updates dirty for a retry. Destroyed slices are cleared.
- Clip rect: only all-zero means "no clip", and the max edge is exclusive.
- `RGBA8` textures are sRGB, and the image tint multiplies alpha, so transparent PNG
  regions stay transparent.
- The SVG atlas has a 1px gutter.
- The flush range is aligned to `nonCoherentAtomSize`.
- `packColor` clamps and rounds.
- Glyph-block relocation now updates its slice entry.
- `GpuArena`: free-list merging fixed, and growing from zero capacity no longer loops
  forever.
- Shelf and skyline packer edge cases fixed.

### Text (`e108930`)

- TextArea measured the caret against the row layout from before an edit.
- Key and char events are delivered in arrival order through `pollTextInputEvent`.
- Only one input is focused at a time (`UIInput::getFocusedInput`).
- UITextField:
  - An empty selection no longer swallows the first Backspace.
  - Caret positions come from the same layout as the drawn glyphs (tabs, line breaks).
  - Line breaks are normalised, and a single-line field turns them into spaces.
  - The cursor snaps to a UTF-8 boundary on `setText`.
  - `readOnly` still allows navigation, selection and copy.
  - `maxLength` counts codepoints.
- Labels release their glyphs on `setText("")`.
- The font is part of the layout cache key.
- Measured height includes the line-height multiplier.
- UTF-8 stray continuation bytes no longer trap the cursor.
- FontLoader leaked its face on move-assignment and kept a stale pixel size.
- NumberInput parses with `std::from_chars` (locale free) and clamps overflow.

### Widgets and layout (`1734487`..`367dcca`)

- Use-after-free fixed in Table and TreeView row callbacks, ContextMenu actions and toggles,
  and TabBar close.
- Table:
  - Selection is a logical row. `onRowSelected` and `onRowActivated` receive logical rows,
    and `displayToRow` / `rowToDisplay` convert.
  - The wheel scrolls over cells.
  - Rows can still be selected when both row colours are transparent.
- TreeView:
  - `clear()` drops selection and hover.
  - Hover follows rows while scrolling.
  - Disclosure buttons connect once per pool slot.
- ContextMenu:
  - Old submenus are destroyed on rebuild instead of leaking into the overlay.
  - Popups flip to the other side before clamping.
  - `Dropdown::close` fires once.
- Tabs:
  - Reorder uses rotate.
  - Tear-off and drop keep the active state correct.
  - Closing the active tab fires `onSelectionChanged`.
  - Connections are per tab and replaced when a tab moves to another bar.
- Controls on a CollapsibleHeader header are hit-testable. The header background propagates.
- Tooltip surfaces are emptied between owners.
- The anchor uses the constrained size.
- List layout FILL shrinks as well as grows, and the sort is stable.
- Grid alignment positions the whole block.
- Column `minWidth` / `maxWidth` are honoured. Weights are always proportional.
  `FIXED` = static, `RESIZABLE` (was `STRETCH`) = user may resize; resizing itself is not
  implemented yet.

### Styling and value widgets (`f00daec`)

- Cascade: a later block wins at equal specificity, even when a selector repeats.
- `type#part` matches the part's owner type. Bare `#part` and `type.class#part` are new.
- Class propagation:
  - Inherited classes are kept separately from a node's own classes, so removing a class
    from the bar works and own classes survive.
  - Inherited classes match part rules only.
  - Parts don't count as `:disabled` just for being non-interactable.
  - This fixes the `.abc` vs `.abc#tab` todo item.
- `padding` from themes applies.
- Pseudo-states re-resolve in every `arrange()` override (`refreshStyleIfStale`).
- `Style::load` restyles live widgets through a generation counter. `loadFromString` is
  available.
- Parser: value errors are dropped with a warning; hex and `rgba()` parse correctly;
  unknown at-rules are skipped; a fuzz test covers malformed input.
- Pseudo-states on bare types parse (`radio-button:active`), and the default theme has a
  selected-radio look.
- Slider/Drag:
  - Ranges are safe (inverted, NaN, int64).
  - Format strings are validated.
  - Theme text style reaches Drag's field.
  - Log drag works through zero and with negatives.
  - DragInt speed is a double.
- Half packing handles subnormals correctly.
- SVG images re-rasterise on resize, and a gradient tint uses the gradient's first stop.
- The colour picker keeps its hue on gray.

## Decisions made along the way

- UITextField is single-line only; `multiline` was removed (TextArea is the multi-line
  editor). Line breaks in a field become spaces.
- CollapsibleHeader passes its classes to its header pieces as inherited classes, the same way
  TabBar does, so `.abc#header` styles them and classes set on the pieces survive.
- Testapp issues are ignored; it only has to compile.
- Column sizing: `FIXED` = static, `RESIZABLE` = user resizable, weight is always a share.
- `ScrollBarVisibility`:
  - `AUTO`: a bar only on an overflowing axis.
  - `ALWAYS`: a bar even when the content fits.
  - `NEVER`: no bar.
  - `NONE` was removed, and `AUTO` is first so a value-initialised field defaults to it.
- Radio groups start at value 0.
- Input: three modes per category, `consume` default. Bare Frames keep blocking (menu
  backgrounds). Labels propagate. A button in a row takes its click; if the row should
  select too, the owner wires it (`table.setSelectedRow`).
- Signals are listen-only; propagation belongs to the node.
- The click-through helper was removed in favour of the label default.

## Still open

- **RaptureVK is not migrated**: about 45 button-callback assignments in 17 files need
  `x->track(x->onMouseButton1ClickCb.connect(...))`. Enter/leave becomes
  `onHoverChanged`.
- Instance buffers are single-buffered: writing an existing block can show one mixed frame.
  Known and accepted; double buffering may come later as an option, since low-memory or
  single-buffered apps don't need it.
- `image-scale-type` and `image-tile-size` are parsed but not used; kept until needed.
- A disabled TabBar doesn't pass `:disabled` to its tabs.
- Hit-testing order vs draw order (GLOBAL z) is a design question.
- Removed subtrees keep their registry slots and stay drawn until destroyed.
- See `todo.md`: focus manager, deferred destruction and window-leave/focus-lost events,
  content scale / DPI, keyboard navigation in widgets.
