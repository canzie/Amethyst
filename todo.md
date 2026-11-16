### Docking layer 
    - corner resizing for docking layer
    - more options in the docking layer, not only a 50-50 split, but also a 75-25 split

### Animations
    - tweening/animations

### Components
    - Scroling frame click and drag the scrollbar instead of only mouse wheel.
    
    - the 2 other color picker levels
    
    - (treeview/table) and the ability to resize the columns
    
    - tab bars custom close button ???

    - CollapsibleHeader still assigns its own classes onto its header pieces (setClasses(getClasses())), wiping classes set through header(fn); move it to setInheritedStyle like TabBar
    - TabBar does not hand :disabled down to its tabs when the bar itself is disabled


### Core / input
    - focus manager: one focused node per window, Tab/Shift-Tab traversal, focus scopes, key/char events routed to the focused node instead of the global ring buffer
    - keyboard navigation in the lib widgets on top of the focus manager (menus, dropdown, table, treeview, tab bar): arrows, Enter, Esc
    - wheel scroll chaining: a scrollable consumes the wheel only while its offset moves, and at its end in that direction it propagates to what holds it; plus gesture latching, so a wheel gesture stays on the node that first took it until the wheel idles (~100-200ms) and only the next gesture chains outward. Today a scrollable only propagates when it has no scroll range at all

### Lifecycle
    - deferred destruction (destroyLater / queue-free) so callbacks can delete their own node or its owner
    - mouse-left-window, window-focus-lost and capture-lost events; actually send InputState::CANCEL
    - horizontal scroll (xoffset is dropped today)

### Platform
    - content scale / UI scale: lay out in logical pixels, rasterize glyphs and svgs at physical size (4k testapp renders tiny, the engine compensates itself)
