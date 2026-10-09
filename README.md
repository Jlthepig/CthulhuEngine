<img width="1000" height="500" alt="Cthulhu Engine" src="https://github.com/user-attachments/assets/6eb22c1a-5b1a-4fed-8f5c-548693e3a9f9" />

![GitHub top language](https://img.shields.io/github/languages/top/Jlthepig/CthulhuEngine?color=green) ![GitHub License](https://img.shields.io/github/license/Jlthepig/CthulhuEngine?color=dark%20green) ![GitHub Repo stars](https://img.shields.io/github/stars/Jlthepig/CthulhuEngine?style=flat&color=%2339FF14)






# What is Cthulhu?

Cthulhu is a custom 3D game engine written in **C++** for the sole purpose of building **first-person shooter games**.

The long-term goal is to create a serious, editor-driven FPS engine with a focused scope rather than a general-purpose engine.  
This will allow Cthulhu to excel at its job.

I plan to work on this until my last breath.

I'm currently 16 years old.

## It's the first year of making Cthulhu, so let's see if I can work on this for 60 more years!

---

## Current Status

Cthulhu is still in early development.

The foundations for the editor finally are now in place, and I will begin moving on to work on the editor itself which will use OctoGUI a custom ui library. Contributing is not possible yet and there are currently no docs.

Current features and systems include:

- OpenGL 4.3 rendering with Cook-Torrance PBR and normal mapping
- glTF model loading with FastGLTF, including PBR materials and embedded textures
- Jolt Physics integration with an FPS character controller
- Flecs ECS integration
- Directional and point light shadow mapping
- HDR skybox with irradiance and prefiltered environment maps, tonemapping, and gamma correction
- Miniaudio integration
- Debug rendering
- Project system with `project.cthulhu`, `res://` paths, and projects independent of the engine itself
- Generic `CthulhuRuntime` for running Cthulhu projects
- Scene loading, serialization, and unsaved-change tracking
- Stable entity and asset IDs, with a project-wide asset registry
- Component descriptors that drive the inspector, undo, and copying for built-in and user components
- Scribe, the editor command API: undo/redo, merged edits, and change events
- Asset operations: import, move/rename with references following, Recycle Bin delete, and missing-asset reporting
- Edit and Play modes, with Play/Stop restoring the edited scene exactly
- Extension points for game systems, and a runtime API safe to use inside systems

The editor UI is not built yet, but the architecture underneath the hood which powers the whole thing eg. Scribe, component descriptors, asset operations and Play/Stop is complete. The editor is now my priority and is next

---

## Project Philosophy

Cthulhu is being developed as a long-term engine project with a focus on:

- Learning by building
- Visible progress through milestones
- Clean foundations without premature overengineering
- Practical FPS-specific design decisions
- Building engine systems before building UI around them
- Keeping the engine focused specifically on FPS development

---

## Cthulhu Projects

Cthulhu takes the godot approach.

A minimal Cthulhu project looks like:

```text
MyGame/
├── project.cthulhu
└── .cthulhu/
```

The entire project directory acts as the project's resource root.

For example:

```text
res://assets/models/weapon.glb
res://maps/level01.scene
```

A project can define settings such as its name, window resolution, and optional startup scene inside `project.cthulhu`.

Example:

```text
name = "My Game"

windowWidth = 1920
windowHeight = 1080

mainScene = "res://maps/level01.scene"
```

Cthulhu projects can currently be launched through the runtime:

```text
CthulhuRuntime.exe path/to/project.cthulhu
```

---

## Project Structure

```text
Cthulhu/
├── src/                 # Engine source files
├── include/             # Engine headers
├── EditorApp/           # Cthulhu editor application
├── RuntimeApp/          # Generic Cthulhu project runtime
├── EngineResources/     # Engine-owned shaders and internal resources
├── Libraries/           # Third-party dependencies
└── build/               # Build output
```

Game projects and sandbox projects are kept separate from the engine repository.

For example:

```text
CthulhuSandbox/
├── project.cthulhu
├── .cthulhu/
└── assets/
    ├── audio/
    ├── models/
    └── scenes/
```

---

## Development Roadmap

Cthulhu is currently being developed in 4 major architectural phases.

```text
Phase 1 — Project Foundation                    COMPLETE
Phase 2 — Scene, Asset & Editor Architecture    COMPLETE
Phase 3 — Editor powered by OctoGUI             IN PROGRESS
Phase 4 — FPS / Game Systems Expansion
```

Phase 1 established the project format, resource paths, runtime, project creation, and engine lifecycle.

Phase 2 made scenes, assets, entities, components, and engine APIs robust enough for the editor, and added Play/Stop and a runtime gameplay API.

---

## Acknowledgements

**[KalaMake](https://github.com/KalaKit/KalaMake)** — the build system used for Cthulhu, developed by Lost Empire Entertainment.

A very big thanks for making a great alternative to CMake. It has saved me a lot of time and headaches.

**[Lost Empire Entertainment](https://github.com/Lost-Empire-Entertainment)** — if you want to explore their broader ecosystem of tools and engines, check out their GitHub.

Note: their Elypso Engine is currently being reworked.

---

## AI Usage

I want to be honest and transparent that AI has been used in this project to assist me.

I will admit that sometimes I am reliant on it. Some people don't like AI and some people don't care. I hope you still find Cthulhu interesting regardless!

AI has not been used as an agent. Any code generated by AI has been read by me and typed by hand if I don't understand it, I don't use it Period.
