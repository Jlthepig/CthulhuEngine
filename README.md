
# Cthulhu

**Cthulhu** is a custom 3D game engine written in **C++** for building **first-person shooter games**.

The long-term goal is to create a serious, editor-driven FPS engine with a focused scope rather than a general-purpose engine.  
Early visual targets are inspired by games like **Return to Castle Wolfenstein** and **Serious Sam**, with room to grow into more advanced rendering over time.

## Goals

- Build a dedicated **FPS-focused** engine
- Use **OpenGL** for rendering
- Create a full **editor workflow**
- Support **glTF** assets
- Add **Lua scripting** later
- Integrate **Jolt Physics** later
- Keep the engine narrow in scope and purpose-built

## Current Status

Cthulhu is in early development.

✅ Implemented:

Window/context setup (GLFW + GLAD)
OpenGL 3.3 core profile
Shader loading and compilation
Shader uniform setters (int, mat4)
Texture loading (stb_image, RGB/RGBA, mipmaps)
Flexible mesh system (configurable vertex attributes)
Indexed rendering (VAO/VBO/EBO)
Fly camera (6DOF, mouse look, keyboard movement)
Delta time
File reader utility
Logging system (KalaLog)
glTF/GLB model loading (fastgltf)
Model class (multi-mesh container)
Basic unlit shader
Depth testing

## Current Tech Stack

- **C++20**
- **OpenGL**
- **GLFW**
- **GLAD**
- **GLM**
- **stb_image**
- **Kalamake** build system

## Project Philosophy

Cthulhu is being developed as a long-term engine project with a focus on:
- learning by building
- visible progress through milestones
- clean foundations without premature overengineering
- practical FPS-specific design decisions

## Planned Features

- Scene loading
- Material system
- FPS controller
- Level editor
- Entity system
- Lua scripting
- Physics integration with Jolt
- Asset pipeline improvements
- Lighting and atmosphere systems

## Project Structure

```txt
Cthulhu/
├─ src/
├─ include/
├─ assets/
├─ shaders/
├─ libraries/
└─ docs/
```

## Acknowledgements

- **[KalaMake](https://github.com/KalaKit/KalaMake)** — build system used for Cthulhu, developed by **Lost Empire Entertainment**.  
  Big thanks for making a clean and capable build system that made this project easier to set up.

- **[Lost Empire Entertainment](https://github.com/greeenlaser)** — if you want to explore their broader ecosystem of tools and engines, check out their GitHub.  
  Note: their **Elypso Engine** is currently being reworked.
