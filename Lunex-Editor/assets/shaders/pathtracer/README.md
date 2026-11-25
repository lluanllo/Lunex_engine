# Lunex Path Tracer shaders (planned)

Shader set for the `PathTracer` render engine (see `Lunex/src/Renderer/Pipeline/RenderPipeline.h`).
Selecting "Lunex Path Tracer" in Settings > Render > Render Engine is already stored per project;
until these shaders exist the rasterizer keeps rendering.

Planned layout:

| File                     | Stage   | Purpose                                                        |
|--------------------------|---------|----------------------------------------------------------------|
| `PT_BuildBVH.glsl`       | compute | Flatten scene meshes into a BVH (SAH) stored in SSBOs           |
| `PT_Trace.glsl`          | compute | One path per pixel per frame: BSDF sampling + NEE to lights/sky |
| `PT_Accumulate.glsl`     | compute | Progressive accumulation, reset when camera/scene changes       |
| `PT_Denoise.glsl`        | compute | Optional A-trous / SVGF style denoiser using G-Buffer guides    |
| `PT_Resolve.glsl`        | frag    | Exposure + tone mapping into the viewport framebuffer           |

Materials: the same `MaterialAsset` PBR parameters (and later the material node graph
compiled to GLSL) so both engines render the same assets.
