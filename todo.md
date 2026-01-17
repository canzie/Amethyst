- add text/fonts
- add text editing
- add more font options and weights etc
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



### Text

current plan:
- CharacterInstance per visible character: position, size, glyphIndex, color, fontIndex
- FontData SSBO: points, contours, glyphs (from ttf parser)
- full buffer rewrite on dirty, stream buffer so no flush
- culling done on cpu, culled chars just dont get written
- 1 draw call for all text

where does styling go:
- bold/italic/weight = different font file, fontIndex selects which loaded font
  - Roboto-Regular.ttf = fontIndex 0
  - Roboto-Bold.ttf = fontIndex 1
  - could do synthetic italic with shear transform in shader later
- letter spacing = cpu side, affects position calc when building char buffer
- line height = cpu side, same
- font size = cpu side, scales the glyph bbox to get char size
- color = per character, allows rich text coloring

gpu buffers:
- binding 0: CharacterBuffer (stream, rebuilt on dirty)
- binding 1: GlyphBuffer (points array)
- binding 2: ContourBuffer (contour start/count)
- binding 3: GlyphMetaBuffer (per glyph: contour range, bbox, advance)
- could pack 1-3 into one buffer with offsets

shader:
- vertex: emit quad per char, pass uv + glyph index
- fragment: lookup glyph contours/points, eval bezier sdf, render outline first then fill with scanline

todo now:
1. finish text.vs.glsl - done
2. text.fs.glsl - bezier eval, just outline for now
3. backend: add stream buffer, text pipeline, font data upload
4. textlabel: build char buffer on dirty
5. test with simple text
