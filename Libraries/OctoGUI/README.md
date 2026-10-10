# OctoGUI v0.1.0

OctoGUI is a little C++ GUI library for games and tools. You build your UI
once, give it your mouse and keyboard input every frame, and it hands you back
a list of things to draw. That's pretty much it.

It's still early days, so expect a few rough edges and the odd API change.

## What's in here

- `include/OctoGui/` - the headers you'll actually use (start with `Context.h`)
- `src/` - the library code itself
- `backends/OpenGL/` - draws the UI with OpenGL
- `backends/GLFW/` - gets mouse and keyboard input from GLFW
- `third_party/` - the stuff OctoGUI needs for text and images (FreeType,
  HarfBuzz, stb_image), each with its license
- `assets/` - a default font (Roboto) and the default icon sheet

GLAD and GLFW aren't included. Your project already has those, so OctoGUI just
uses yours.

## What you need

- A C++20 compiler. A fairly recent one: GCC 11 is too old, but GCC 13,
  Clang 18 and current MSVC/clang-cl are all fine.
- OpenGL 3.3 or newer
- GLFW 3
- GLAD, or whatever OpenGL loader you already use

The text libraries come prebuilt for Windows only.

## Getting it into your project

1. **Build the library.** Compile everything in `src/` as a static library.
   It needs these include folders: `include`, `src`, `third_party/stb`,
   `third_party/freetypeharfbuzz/include` and
   `third_party/freetypeharfbuzz/include/harfbuzz`.
2. **Link the text libraries.** They're already built and sitting in
   `third_party/freetypeharfbuzz/lib/`. Link all six `.lib` files in there.
3. **Add the backends to your app.** Compile the two backend `.cpp` files as
   part of your own app, not the library, because they use your GLAD and GLFW.
   Add `include`, `backends/OpenGL` and `backends/GLFW` to your include folders.
4. **Using a different OpenGL loader?** OctoGUI looks for `glad/gl.h` by
   default. If you use something else, define `OCTOGUI_GL_LOADER_HEADER` as
   your loader's header (e.g. `"my/loader.h"`) when building the OpenGL
   backend. Either way, load OpenGL before you start OctoGUI.
5. **Set up input last.** If your app sets its own GLFW callbacks for keys,
   typing or scrolling, set those up first and call `glfw::initInput` after.
   OctoGUI passes everything on to your callbacks, so nothing gets lost. Do it
   the other way round and your callbacks will replace OctoGUI's.

## Heads up: OpenGL settings

When OctoGUI draws, it switches off depth testing and face culling, turns on
blending, and sets the viewport to the whole window. It doesn't switch them
back afterwards, so set up your own OpenGL state at the start of every frame.
The easy way is to draw your game first and the UI last.

`HOW_TO_USE_OCTOGUI.md` walks through a full setup and frame loop.

## What it doesn't do (yet)

No docking, no tabs, and no pulling windows out into their own OS windows.

## License

OctoGUI is MIT licensed (see `LICENSE`). The libraries in `third_party/` have
their own licenses, which sit right next to them. Portions of this software
are copyright (c) The FreeType Project (www.freetype.org). All rights reserved.
