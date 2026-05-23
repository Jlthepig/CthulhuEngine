# What is Cthulhu?

Cthulhu is a custom 3D game engine written in **C++** for the sole purpose of building **first-person shooter games**.

The long-term goal is to create a serious, editor-driven FPS engine with a focused scope rather than a general-purpose engine.  
This will allow Cthulhu to excel at its job.

I plan to work on this until my last breath.

I'm currently 16 years old.

it's the first year of making Cthulhu so let's see if I can work on this for 60 more years!
---

## Current Status

Cthulhu is still in early development.
Contributing is not possible yet and there are no docs.

- OpenGL 4.0 rendering with Cook-Torrance PBR and normal mapping
- glTF model loading with FastGltf (PBR materials, embedded textures)
- Jolt Physics integration with an FPS character controller
- Directional and point light shadow mapping
- HDR skybox with tonemapping and gamma correction
- Editor/Game mode toggle and ImGui debug panel

---

## Project Philosophy

Cthulhu is being developed as a long-term engine project with a focus on:

- Learning by building  
- Visible progress through milestones  
- Clean foundations without premature overengineering  
- Practical FPS-specific design decisions  

---

## Project Structure

```text
Cthulhu/
├─ src/             # Engine source files
├─ include/         # Engine headers
├─ SandboxSample/   # Standalone game sample using the engine library
├─ assets/          # Models, textures, scenes, etc.
├─ shaders/         # GLSL shader files
├─ Libraries/       # Third party dependencies
└─ build/           # Build output
```

## Acknowledgements

```text
KalaMake — build system used for Cthulhu, developed by Lost Empire Entertainment.
A very big thanks for making a great alternative to CMake which is really saved me time and loads of headaches.

Lost Empire Entertainment — if you want to explore their broader ecosystem of tools and engines, check out their GitHub.
Note: their Elypso Engine is currently being reworked.
```

## Ai Usage 
I will be honest and transparent with you that Ai has been used in this project to assist me I will admit sometimes I am reliant on it some people don't like Ai some people don't care I hope you still find interest in Cthulhu regardless!
