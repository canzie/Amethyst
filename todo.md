- implement the ui layout extensions
- add text editing
- add support for docking
    - do i make tabs a seperate thing? or only for docked windows?
    - do i make the docking layer or docking frame responsible for the tab?
        - there is the main window (some kind of frame), the top part, which is some kind of draggable container? but limited to its axis
          it can contain a label and a button (like a close button?)
        - it needs to display its siblings.
- add more specific types like dropdowns and menus(like the main header with chaining menu items), tables
- add addons like gizmo that can be enabled at compile time
- add a concpet of themes using a toml file
- drag - drop with payloads, not just mindless dragging.
    - this will not drag the object itself but instead some sort of payload, mostly data but can be have some sortof configurable view (minified version of thing being dragged, or just some text etc.)
- animations, either custom solution or look at simple global tweens using some sort of global upload list to add an item to be tweened to.



- implement the gradient extension
