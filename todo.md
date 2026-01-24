- add addons like gizmo that can be enabled at compile time
- tables
    - ???
    - cnt be via extension since it makes no sense, a button cannot be a container for a table, just freaky
    - needs to be a component

- add a concpet of themes using a toml file
- drag - drop with payloads, not just mindless dragging.
    - this will not drag the object itself but instead some sort of payload, mostly data but can be have some sortof configurable view (minified version of thing being dragged, or just some text etc.)
- animations, either custom solution or look at simple global tweens using some sort of global upload list to add an item to be tweened to.

- implement the gradient extension



## CLeaning
    - for text allocations. first thing is supporting z indices, then taking a look at if the implementation cant be done in a nicer way, for example force components like text inputs to allocate a buffer, so that we dont have to reorder after every letter typed/removed -> more fake draws which also not desirable
    - culling, right now not really a thing, every allocation goes to the gpu, should just temporarily release? i guess?. for visibility culling for children, that could lead to a bunch of changes at once, so we also need batch submitting/releaeing.
