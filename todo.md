### Docking layer 
    - corner resizing for docking layer
    - more options in the docking layer, not only a 50-50 split, but also a 75-25 split

### Animations
    - tweening/animations

### Style System
    - Add more stuff to ams

### Components
    - The popup should do its overlay layer stuff automatically
    - Scroling frame click and drag the scrollbar instead of only mouse wheel.
    - the 2 other color picker levels
    - hint thing. When hovering over something i want some sort of hint text, can be text, can be just text
    - a nice way to do options menus, most likely use the popup logic, can make it an extension? since its something any compoennt can have
        - we'd just do frame1->addExtensions<HintPopup>("text"); and frame1->addExtensions<ContextMenu>(std::function<void>(DropdownScope&));
        !!!! VER VERY IMPORTANT: the click handlers can be set up immediatly, the allocations and actual components only lazily. could even have an internal caching mechanism
        like a fifo buffer of maybe 8-16 elements, since there is 0 chance that you could even have all of these open at once. we keep the blueprints but not the actual allocations
        i think there is a high chance someone clicks this once on a component maybe opens something then never opens the ContextMenu/hint again.
