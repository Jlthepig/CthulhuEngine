# Architecture

## Rendering Pipeline
1. Shadow pass — renders depth from light's POV
2. Main pass — forward rendering with lighting
3. Grid — blended debug grid
4. Skybox — HDR cubemap with LEQUAL depth
5. ImGui — debug overlay

## Scene System
- `Scene` owns a list of `Entity`
- `Entity` has a `Transform` and points to a `Model`
- `Model` owns `Mesh`es, `Material`s, and `Texture`s
- `Mesh` references a `Material` by index

## Key Decisions
- Transform matrices are cached with dirty flags
- Frustum culling happens on CPU per entity
- Materials are separate from meshes to support sharing