# Library Review 2

Second audit, 2026-11-16, on top of `df86a89`. Five read-only reviewers covered the core
tree and input, rendering and the Vulkan backend, text, the composite widgets, and the
styling system with the value widgets. The items in `library_review.md` were skipped.
The fixes are merged into `main` (`40847e0`..`a8866cf`, see git history) and RaptureVK is
ported. All 294 tests pass under ASan and UBSan on `cdb0caf`. Nothing was run on a GPU, so
the shader and Vulkan changes are only known to compile.

Testapp issues (frame loop, swapchain, upload order) are out of scope by decision.

## Still open

- **Widget callbacks are still single-slot `std::function`s**: Table, TreeView, TabBar,
  ContextMenu, Dropdown, Popup, Checkbox, Slider, Drag, ColorPicker, CollapsibleHeader,
  UITextField and UIInput. Only UIObject and UIButton use `EventSignal`. Naming is mixed too
  (`onClosedCb` on ContextMenu/Dropdown, `onClosed` on Popup). Undecided whether to move them.
- Horizontal wheel scroll: `Window::onMouseScroll` drops `xoffset`.
- Instance buffers are single-buffered: writing an existing block can show one mixed frame.
  Known and accepted; double buffering may come later as an option, since low-memory or
  single-buffered apps don't need it.
- `image-scale-type` and `image-tile-size` are parsed and applied to the properties, but no
  renderer reads them; kept until needed. RaptureVK's `ViewportPanel` sets `STRETCH`, which
  is what it gets anyway.
- A disabled TabBar doesn't pass `:disabled` to its tabs.
- See `todo.md`: focus manager, deferred destruction and window-leave/focus-lost events,
  keyboard navigation in widgets, wheel scroll chaining and gesture
  latching, column resizing.
