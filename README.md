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

- Window class with GLFW + GLAD context setup
- OpenGL 3.3 core profile
- Shader loading and compilation with double-load protection
- Shader uniform setters (int, float, vec3, mat4)
- Texture loading from disk (stb_image, RGB/RGBA, mipmaps) with double-load protection
- Texture loading from memory (embedded glTF textures)
- Texture extraction from glTF models
- Flexible mesh system (configurable vertex attributes)
- Indexed rendering (VAO/VBO/EBO)
- Fly camera (6DOF, mouse look, keyboard movement)
- Input system (key down, pressed, released detection, mouse tracking)
- Delta time
- File reader utility
- Logging system (KalaLog)
- glTF/GLB model loading (fastgltf)
- Model class (multi-mesh container with per-mesh textures)
- Debug grid rendering (GL_LINES) with distance-based alpha fade
- Blinn-Phong lighting (ambient, diffuse, specular)
- Directional light support
- Point light with attenuation (constant, linear, quadratic)
- PCF shadow mapping with bias
- HDR skybox (equirectangular to cubemap conversion)
- HDR tonemapping and gamma correction
- Frustum culling (AABB-based)
- Entity system (transform, model pointer, AABB, ID, name, active flag)
- Scene system (entity container with auto-incrementing IDs)
- Cached transform matrices (dirty flag system)
- Transform with getters/setters and matrix caching
- ImGui debug panel (FPS counter, entity count)
- Depth testing
- Back face culling
- Alpha blending

---

## 🛠️ Tech Stack

| Category | Technology |
|----------|------------|
| Language | C++20 |
| Graphics | OpenGL 3.3 |
| Windowing | GLFW |
| GL Loader | GLAD |
| Math | GLM |
| Image Loading | stb_image |
| Model Loading | fastgltf |
| Build System | KalaMake |
| Logging | KalaLog |
| Debug UI | ImGui |

---

## 🧭 Project Philosophy

Cthulhu is being developed as a long-term engine project with a focus on:

- Learning by building
- Visible progress through milestones
- Clean foundations without premature overengineering
- Practical FPS-specific design decisions

---

## 🔮 Planned Features

- Additional light types (spot lights)
- Omnidirectional shadow mapping (point light shadows)
- Cascaded shadow maps
- Material system / PBR
- Scene loading
- FPS controller
- Level editor
- Lua scripting
- Physics integration with Jolt
- Audio integration with miniaudio
- Asset pipeline improvements
- Expanded ImGui editor interface
- Instanced rendering
- Spatial partitioning (octree / BVH)

---

## 📁 Project Structure
```
Cthulhu/
├─ src/           # Engine source files
├─ include/       # Engine headers
├─ assets/        # Models, textures, etc.
├─ shaders/       # GLSL shader files
├─ Libraries/     # Third party dependencies
└─ build/         # Build output
```
---
🙏 Acknowledgements
KalaMake — build system used for Cthulhu, developed by Lost Empire Entertainment. Big thanks for making a clean and capable build system that made this project easier to set up.

Lost Empire Entertainment — if you want to explore their broader ecosystem of tools and engines, check out their GitHub. Note: their Elypso Engine is currently being reworked.
