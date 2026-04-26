# 🐙 Cthulhu

Cthulhu is a custom 3D game engine written in **C++** for building **first-person shooter games**.

The long-term goal is to create a serious, editor-driven FPS engine with a focused scope rather than a general-purpose engine.  
Early visual targets are inspired by *Return to Castle Wolfenstein* and *Serious Sam*, with room to grow into more advanced rendering over time.

---

## 🎯 Goals

- Build a dedicated FPS-focused engine  
- Use OpenGL for rendering  
- Create a full editor workflow  
- Support glTF assets  
- Add Lua scripting later  
- Integrate Jolt Physics for world simulation  
- Keep the engine narrow in scope and purpose-built  

---

## 📦 Current Status

Cthulhu is in early development.

### ✅ Implemented

  -- Engine static library with SandboxSample game project
  -- Engine application wrapper with update callbacks
  -- Window & input system (GLFW + GLAD, OpenGL 3.3)
  -- glTF/GLB model loading with materials and embedded textures
  -- Renderer pipeline (shadows → main pass → grid → skybox → ImGui)
  -- Blinn-Phong lighting with directional and point lights
  -- Shadow mapping (2048x2048, PCF)
  -- Frustum culling (AABB-based)
  -- HDR skybox with equirectangular-to-cubemap conversion
  -- HDR tonemapping and gamma correction
  -- Scene system with JSON scene files
  -- Jolt Physics integration (static & dynamic bodies, ground plane)
  -- FPS Character Controller (capsule collision, gravity, jumping)
  -- Editor/Game mode toggle
  -- Fly camera (6DOF)
  -- ImGui debug panel

---

## 🛠️ Tech Stack

| Category        | Technology        |
|----------------|------------------|
| Language       | C++20            |
| Graphics       | OpenGL 3.3       |
| Windowing      | GLFW             |
| GL Loader      | GLAD             |
| Math           | GLM              |
| Image Loading  | stb_image        |
| Model Loading  | fastgltf         |
| JSON Parsing   | simdjson         |
| Build System   | KalaMake         |
| Logging        | KalaLog          |
| Debug UI       | ImGui            |
| Physics        | Jolt Physics 5.5.0 |
| Audio (vendored) | miniaudio     |

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
- Level editor  
- Lua scripting  
- miniaudio integration (library vendored)  
- Asset pipeline improvements  
- Expanded ImGui editor interface  
- Instanced rendering  
- Spatial partitioning (octree / BVH)  

---

## 📁 Project Structure

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

## 🙏 Acknowledgements

```text
KalaMake — build system used for Cthulhu, developed by Lost Empire Entertainment.
Big thanks for making a clean and capable build system that made this project easier to set up.

Lost Empire Entertainment — if you want to explore their broader ecosystem of tools and engines, check out their GitHub.
Note: their Elypso Engine is currently being reworked.
```