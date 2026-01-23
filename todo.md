- add text editing
- add addons like gizmo that can be enabled at compile time
- tables
    - ???
- resizing docks and save the layouts
    - make sure the resize handles are updated during a resize
    - use proper resize cursors 
        - add some kind of map going from glfw to the enum in input interface, then poll which cursors shape should be used in the current frame
        - can even have a observer pattern that the backend subscribes to???

- add a concpet of themes using a toml file
- drag - drop with payloads, not just mindless dragging.
    - this will not drag the object itself but instead some sort of payload, mostly data but can be have some sortof configurable view (minified version of thing being dragged, or just some text etc.)
- animations, either custom solution or look at simple global tweens using some sort of global upload list to add an item to be tweened to.



- implement the gradient extension
