

# 🐙 Cthulhu

**Cthulhu** is a custom 3D game engine written in **C++** for building **first-person shooter games**.

The long-term goal is to create a serious, editor-driven FPS engine with a focused scope rather than a general-purpose engine.  
Early visual targets are inspired by games like **Return to Castle Wolfenstein** and **Serious Sam**, with room to grow into more advanced rendering over time.

---

## 🎯 Goals

- Build a dedicated **FPS-focused** engine
- Use **OpenGL** for rendering
- Create a full **editor workflow**
- Support **glTF** assets
- Add **Lua scripting** later
- Integrate **Jolt Physics** later
- Keep the engine narrow in scope and purpose-built

---

## 📦 Current Status

Cthulhu is in **early development**.

### ✅ Implemented

- Window/context setup (GLFW + GLAD)
- OpenGL 3.3 core profile
- Shader loading and compilation
- Shader uniform setters (int, mat4)
- Texture loading (stb\_image, RGB/RGBA, mipmaps)
- Texture extraction from glTF models
- Flexible mesh system (configurable vertex attributes)
- Indexed rendering (VAO/VBO/EBO)
- Fly camera (6DOF, mouse look, keyboard movement)
- Delta time
- File reader utility
- Logging system (KalaLog)
- glTF/GLB model loading (fastgltf)
- Model class (multi-mesh container)
- Basic unlit shader
- Depth testing
- Back face culling

---

## 🛠️ Tech Stack

| Category | Technology |
|----------|------------|
| Language | C++20 |
| Graphics | OpenGL 3.3 |
| Windowing | GLFW |
| GL Loader | GLAD |
| Math | GLM |
| Image Loading | stb\_image |
| Model Loading | fastgltf |
| Build System | KalaMake |
| Logging | KalaLog |

---

## 🧭 Project Philosophy

Cthulhu is being developed as a long-term engine project with a focus on:

- Learning by building
- Visible progress through milestones
- Clean foundations without premature overengineering
- Practical FPS-specific design decisions

---

## 🔮 Planned Features

- Lighting and atmosphere systems
- Material system
- Scene loading
- FPS controller
- Level editor
- Entity system
- Lua scripting
- Physics integration with Jolt
- Audio integration with miniaudio
- Asset pipeline improvements
- ImGui editor interface

---

## 📁 Project Structure

```txt
Cthulhu/
├─ src/           # Engine source files
├─ include/       # Engine headers
├─ assets/        # Models, textures, etc.
├─ shaders/       # GLSL shader files
├─ Libraries/     # Third party dependencies
└─ build/         # Build output
```

---

## 🙏 Acknowledgements

- [KalaMake](https://github.com/KalaKit/KalaMake) — build system used for Cthulhu, developed by Lost Empire Entertainment. Big thanks for making a clean and capable build system that made this project easier to set up.

- [Lost Empire Entertainment](https://github.com/greeenlaser) — if you want to explore their broader ecosystem of tools and engines, check out their GitHub. Note: their Elypso Engine is currently being reworked.
