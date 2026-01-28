- Add a batch release and submit to the geometry allocation, so text can use it nicer.
- move zindex away from the instance data


- some way to draw primitives, like let the user give 3 points on a triangle, or 2 for a line etc.
    - dont think this is possible with sdf formulas, so would need another pipeline...
    - technically it would only be needed for a 3d gizmo for now so if it can be done differently then maybe.
    - could also create some sort of canvas, and limit any of the primitives to only be allowed inside there? then what about text?
        - i guess dragging and text wouldnt work huh....

- add a concpet of themes using a toml file
- drag - drop with payloads, not just mindless dragging.
    - this will not drag the object itself but instead some sort of payload, mostly data but can be have some sortof configurable view (minified version of thing being dragged, or just some text etc.)
- animations, either custom solution or look at simple global tweens using some sort of global upload list to add an item to be tweened to.

- implement the gradient extension

