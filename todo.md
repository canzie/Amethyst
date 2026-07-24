### Docking layer 
    - corner resizing for docking layer
    - more options in the docking layer, not only a 50-50 split, but also a 75-25 split

### Animations
    - tweening/animations

### Components
    - Scroling frame click and drag the scrollbar instead of only mouse wheel.
    
    - the 2 other color picker levels
    
    - make it so tables and treeviews have their own proper header, this then needs to be hooked up to the style system as parts (header)
      it should also provide a better way to individually position header labels(could be some kind of arg in the column), and decide color etc. also a way to add separators to the header labels
      and the ability to resize the columns
    - table and treeview should get their own scrollbar functionality, i'm kind of against not reusing the scrolling frame but one big issue is not being able
      to lock the header in place when i just want to scroll the content itself, it should also then show this scrollbar under the header, not next to it.
      thinking about it more its probably then best to add scrolling frame as a core part of the components, then provide some scroll options, including a component part
      for ui scope it would be weird to directly provide scrolling frame options (outside of styling), so perhaps we just hide it, make it all automatic. dont think
      anyone would not want their content to be visible? i guess wether to show the scrollbar or have it be hidden but still allow scrolling should be a bool flag, or we can propegate the auto thingy
      , yeah can then also be in the style file, under the table/treeview itself? or no in the style ams file it can be under the part maybe? idk i guess we could allow either.
    
    - tab bars custom close button ???
    - context menu: right now context menus are amazing for menu bars and casual dropdowns. but for the asset picker dropdown thing the context menu needs
      to be like 2/3x as tall as a "normal" row/entry, it also needs to display quite a lot of custom stuff, so i guess we need a new type or something that allows customising the content of a menu item
      one question that comes up here, do these need their own custom scrolling? since pressing a dropdown on 100/1000 items shouldnt render all of them at once, that is shit
      another one is the viewport thing, there it wont really be a dropdown, more like a popup i guess, so maybe its not another one? i mean it will have rows, it will have text on one side, with maybe an icon and a shortcut on the right, and some will allow submenus, separators, radio buttons, bools etc
      i guess radio buttons need to be implemented, then allowing text in separators, and allowing a submenu AND a radiobutton in the same row?, can maybe separate these if its easier(radio and radio-sub-menu).
      
      so tldr: find out how to best allow custom shit like adding an svg/icon to an entry, full custom content in an entry (most likely exact same solution as the first point -> move conextmenu items to just that, decouple them from their visual representation)
    
    - sliders should use 'Shape' for their thumbs, not a frame -> color picker swaps to circle
