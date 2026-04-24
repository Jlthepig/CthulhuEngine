# 🐙 Cthulhu

**Cthulhu** is a custom 3D game engine written in **C++** for building **first-person shooter games**.

The long-term goal is to create a serious, editor-driven FPS engine with a focused scope rather than a general-purpose engine.  
Early visual targets are inspired by **Return to Castle Wolfenstein** and **Serious Sam**, with room to grow into more advanced rendering over time.

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

- Window class with GLFW + GLAD context setup (OpenGL 3.3 core, vsync off)  
- Input system (key down, pressed, released detection, mouse delta, left-click locks cursor, ESC releases)  
- Delta time  
- File reader utility  
- Logging system (KalaLog)  
- Shader loading and compilation with double-load protection  
- Shader uniform setters (int, float, vec3, vec4, mat4)  
- Texture loading from disk (stb_image, RGB/RGBA, mipmaps, sRGB internal formats) with double-load protection  
- Texture loading from memory (embedded glTF bufferView textures)  
- Flexible mesh system (configurable vertex attributes, interleaved layout)  
- Indexed rendering (VAO/VBO/EBO)  
- Model class (multi-mesh container with per-mesh materials & textures)  
- glTF/GLB model loading (fastgltf) — positions, normals, TEXCOORD_0; embedded textures; image deduplication  
- Material system (base color factor, base color texture index per mesh)  
- Renderer pipeline (shadow pass → main pass → grid → skybox → ImGui)  
- Blinn-Phong lighting (ambient, diffuse, specular)  
- Directional light support  
- Directional light shadow mapping (orthographic, 2048x2048, PCF 3x3, bias)  
- Point light with attenuation (constant, linear, quadratic, up to 8 in shader)  
- Frustum culling (AABB-based, world-space transformed corners)  
- Debug grid rendering (GL_LINES) with distance-based alpha fade  
- HDR skybox (equirectangular to cubemap conversion, 512x512 faces)  
- HDR tonemapping (Reinhard) and gamma correction  
- Runtime triangle counting & draw call tracking  
- Depth testing  
- Back face culling  
- Alpha blending  
- Entity system (transform, model pointer, AABB, ID, name, active flag)  
- Scene system (entity container with auto-incrementing IDs, model cache)  
- JSON scene format (.scene files) — entities, transforms, bounds, directional & point lights  
- SceneLoader & JsonParser (simdjson) — deserializes scenes into engine objects  
- Cached transform matrices (dirty flag system)  
- Transform with getters/setters and matrix caching  
- Fly camera (6DOF, mouse look, WASD + Q/E vertical, 90° FOV)  
- ImGui debug panel (FPS counter, entity count, draw calls, triangle count, shadow map resolution)  

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
| JSON Parsing | simdjson |
| Build System | KalaMake |
| Logging | KalaLog |
| Debug UI | ImGui |
| Physics (vendored) | Jolt Physics 5.5.0 |
| Audio (vendored) | miniaudio |

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
- Full PBR material system  
- FPS controller  
- Level editor  
- Lua scripting  
- Jolt Physics integration (library vendored)  
- miniaudio integration (library vendored)  
- Asset pipeline improvements  
- Expanded ImGui editor interface  
- Instanced rendering  
- Spatial partitioning (octree / BVH)  

---

## 📁 Project Structure

```
  Cthulhu/
  ├─ src/ # Engine source files
  ├─ include/ # Engine headers
  ├─ assets/ # Models, textures, etc.
  ├─ shaders/ # GLSL shader files
  ├─ Libraries/ # Third party dependencies
  └─ build/ # Build output
```

---

## 🙏 Acknowledgements

**KalaMake** — build system used for Cthulhu, developed by Lost Empire Entertainment.  
Big thanks for making a clean and capable build system that made this project easier to set up.

**Lost Empire Entertainment** — if you want to explore their broader ecosystem of tools and engines, check out their GitHub.  
Note: their Elypso Engine is currently being reworked.
