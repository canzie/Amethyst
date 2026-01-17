// parse am themes from toml files using toml++
// i guess i need to define some categories like
// [general], [buttons], [labels], [containers] (like frames), [metadata]
// assuming when someting like border thickness is defned in general but not buttons, and also in labels, the buttons will use the
// general setting but the labels their own, and ofcourse you can still overwrite any specific object via code, this would just be a
// general theme,
//
// the main cpu datatype will be a palette, not a "theme".
//
