# Changelog

## v0.1.0 - the first release

This is the first proper release of OctoGUI. Here's what's in it.

### What you get
- A UI tree you build once and update whenever you need to. Widgets are
  referred to by handles, and an old handle to a deleted widget won't crash
  your program.
- Lots of widgets: panels, labels, buttons, icons, checkboxes, radio buttons,
  sliders, progress bars, text boxes, number boxes, dropdowns, separators,
  split views, collapsible sections and tree views. You can also make your own.
- Layouts that stack things vertically or horizontally, with padding, spacing,
  size limits and alignment.
- Themes (dark and light) and per-widget styling, so you can change one button
  or every button at once.
- Two ways to react to the user: ask a widget directly ("was this clicked?")
  or go through a queue of events.
- Keyboard focus, typing, copy and paste, scrolling, and stacking widgets on
  top of each other.
- Good-looking text using FreeType and HarfBuzz, plus an icon sheet.
- Rounded corners, gradients, soft shadows and clipping, with drawing batched
  together so the GPU isn't swamped with tiny jobs.
- An OpenGL 3.3 renderer and GLFW input support out of the box.
- You can use your own OpenGL loader instead of GLAD.

### Fixed
- OctoGUI keeps its copy of stb_image to itself now, so it won't clash with
  your app if you use stb_image too.

### Not in this version
- Docking, tabs and detachable windows.
- The text libraries only come prebuilt for Windows for now.
