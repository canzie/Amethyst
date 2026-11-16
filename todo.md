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

    - check if both .abc and .abc#tab work, or if 1 is overwritten for the last one, --> DOES NOT, FIX IT SO I CAN SINCE ITS GIGA USEFULL FOR PROPEGATING STUFF 


### Core / input
    - focus manager: one focused node per window, Tab/Shift-Tab traversal, focus scopes, key/char events routed to the focused node instead of the global ring buffer
    - keyboard navigation in the lib widgets on top of the focus manager (menus, dropdown, table, treeview, tab bar): arrows, Enter, Esc

### Lifecycle
    - deferred destruction (destroyLater / queue-free) so callbacks can delete their own node or its owner
    - mouse-left-window, window-focus-lost and capture-lost events; actually send InputState::CANCEL
    - horizontal scroll (xoffset is dropped today)

### Platform
    - content scale / UI scale: lay out in logical pixels, rasterize glyphs and svgs at physical size (4k testapp renders tiny, the engine compensates itself)
