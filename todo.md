- add text editing
- implement the ui layout extensions
- implement the gradient extension
- add support for images (we will use some sort of texture id, the backend then needs to handle this, with some mappings, probably will do a bindless setup, where given a texture index it creates its own descroptor and maps that index to the core texture id)
- add more specific types like dropdowns and menus(like the main header with chaining menu items), tables
- add addons like gizmo that can be enabled at compile time
- add support for docking
- add a concpet of themes using a toml file
- drag - drop with payloads, not just mindless dragging.
    - this will not drag the object itself but instead some sort of payload, mostly data but can be have some sortof configurable view (minified version of thing being dragged, or just some text etc.)
- animations, either custom solution or look at simple global tweens using some sort of global upload list to add an item to be tweened to.



