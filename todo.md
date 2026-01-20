- implement proper visibility culling, zindex
    - eother use a middle man buffer
    - or just rorder on the main data buffer
    - for zIndex the objects have to be sorted, so need a fast sort algo, prob radix sort in this case, or dont sort at all? i guess we can try some fancy pants stuff
    - 

- add text editing
- add addons like gizmo that can be enabled at compile time
- add a concpet of themes using a toml file
- drag - drop with payloads, not just mindless dragging.
    - this will not drag the object itself but instead some sort of payload, mostly data but can be have some sortof configurable view (minified version of thing being dragged, or just some text etc.)
- animations, either custom solution or look at simple global tweens using some sort of global upload list to add an item to be tweened to.



- implement the gradient extension
