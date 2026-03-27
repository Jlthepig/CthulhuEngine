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

Current implemented work includes:
- OpenGL window/context setup
- Shader loading/usage
- Texture loading
- Textured 3D cube rendering
- Depth testing
- Basic fly camera
- Mouse look
- Delta time movement

## Tech Stack

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
