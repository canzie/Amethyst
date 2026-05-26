- corner resizing for docking layer
- more options in the docking layer, not only a 50-50 split, but also a 75-25 split
- improve text allocations, at the very least allow a buffer with a max size (no dynamic sizing)
- tweening/animations
- implement the gradient extension
- more styling options overall
- fix bug in collapsible header and make it so the content moves when collapsing the header
- color picker
- svg/icon in dropdown (optional, as menu bar doesnt need it)
- use svg for checkbox
- find out how to best do a more declerative layout thing
ui.collapsibleHeader(props, [&](auto& section)
{
    section.header([&](auto& header)
    {
        header.image(...);
        header.button(...);
    });

    section.body([&](auto& body)
    {
        body.label(...);
        body.label(...);
    });
});
- (Rapture thing) allow a table cell or treeview row to be "selected" with a different background color, perhaps an on selected callback?

