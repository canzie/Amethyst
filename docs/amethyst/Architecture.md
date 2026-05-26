# Amethyst Architecture: Properties & Builder

## Properties System

Every component exposes a properties struct where all fields are `std::optional<T>`. Calling `setProperties()` only applies the fields that have a value, and automatically marks the component dirty. Internal storage uses plain (non-optional) member variables initialized to sensible defaults.

Shared property blocks live in `properties.h` and are composed into per-component structs:

- `TextProperties` - font, size, color, alignment, text content
- `ImageProperties` - image handle, tint, scaling mode
- `ButtonProperties` - autoButtonColor, modal

Per-component structs compose these blocks alongside a `UIObjectProperties` base:

```cpp
struct TextLabelProperties {
    UIObjectProperties base;
    TextProperties     text;
};

struct TextButtonProperties {
    UIObjectProperties base;
    TextProperties     text;
    ButtonProperties   button;
};
```

Callbacks (`onToggled`, `onValueChanged`, etc.) are **not** part of properties structs because they are not serializable. They are set directly on the component after construction.

## UIBuilder

`UIBuilder` is a lightweight wrapper holding a `Instance*` parent pointer. It provides typed child-creation methods that construct the component, attach it to the parent, apply the supplied properties, and return a reference to the new node.

```cpp
// Defined in ui_builder.h
class UIBuilder {
public:
    explicit UIBuilder(Instance* parent);

    Frame*       frame(FrameProperties props = {});
    TextLabel*   textLabel(TextLabelProperties props = {});
    TextButton*  textButton(TextButtonProperties props = {});
    ImageLabel*  imageLabel(ImageLabelProperties props = {});
    ImageButton* imageButton(ImageButtonProperties props = {});

    CollapsibleHeader* collapsibleHeader(CollapsibleHeaderProperties props,
                                         std::function<void(CollapsibleHeader&)> fn);
    TabBar*            tabBar(TabBarProperties props,
                              std::function<void(TabBar&)> fn);
    // ...
};
```

For composite components, the method accepts a properties struct and a lambda that receives the component directly so callers can call its section methods.

## Composite Components

Composite components (`CollapsibleHeader`, `TabBar`, `Table`, `TreeView`) expose section methods directly on themselves rather than going through `UIBuilder`. This keeps the API surface intentional: given a `CollapsibleHeader&` you only see `.header()`, `.body()`, and `.setProperties()`, not unrelated methods like `.textLabel()`.

Section methods accept a lambda that receives a `UIBuilder&` wrapping the appropriate internal container, giving access to the full child-creation API inside the section.

```cpp
collapsibleHeader.header([&](UIBuilder& h) { /* add children to header row */ });
collapsibleHeader.body([&](UIBuilder& b)   { /* add children to body panel */ });
```

## Usage Example

```cpp
UIBuilder ui(&window);

ui.collapsibleHeader({.title = "Settings", .headerHeight = 32.0f},
    [&](auto& section) {
        section.header([&](auto& h) {
            h.imageLabel({.image = arrowSvg});
            h.textButton({.text = {.text = "Options"}});
        });
        section.body([&](auto& b) {
            b.textLabel({.text = {.text = "Resolution: 1920x1080"}});
            b.textLabel({.text = {.text = "Fullscreen: Off"}});
        });
    }
);
```

## Style Integration

`applyStyle()` functions return fully-populated properties structs. Component constructors call `setProperties(defaultStyle())` during initialization so every component starts from the active theme without extra boilerplate.

Properties structs serve as the single source of truth for three concerns:

- **Serialization** - serialize/deserialize the struct, not component internals
- **Theming** - `applyStyle()` returns a struct; swap it to retheme at runtime
- **Runtime updates** - set only the fields you want to change, leave others unset
