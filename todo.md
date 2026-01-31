- visibility issue
    - allow allocs to be non owning, this means they wont be freed automatically
    - make certain componments own some allocation pool, then allow its children to use them, depending on who is visible
        - e.g. the treeview will calculate and own any allocation its direct children use, that way there are no wasted allocations
          and no need to do any weird freeing/visibility culling on the gpu, little extra bookkeeping for giga performance
    - for the ones that dont do this, but still need their children culled
        - clipping might have to be used, when visibility is false we can set the clip rect to {0.0f} -> no moves or reallocs, just a simple update for the changed elements
        - for text we can preallocate, either dynamically or by a user provided buffer size, same thing here then, no reallocs, just some possible wasted space and early returns

- components that would use a pool:
    - treeview
    - table
    - 



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

