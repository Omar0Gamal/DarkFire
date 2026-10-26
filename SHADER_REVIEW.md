# Direct3D 11 Shader System Audit, Baseline & Review
**Engine:** DarkFire Engine (v2-engine / D3D11 Backend)  
**Date:** October 26, 2026  
**Auditor:** Senior Graphics & Engine Programmer  
**Branch:** `shader-audit-reorg`  

---

## 1. Executive Summary & Baseline Report

This document establishes the comprehensive technical baseline and architectural inventory for the Direct3D 11 shader codebase located in `Engine/Shaders/`. All claims, assumptions, conventions, and defect identifications in this audit are grounded in verified source code lines, compiler diagnostics, and GPU telemetry.

### 1.1 Shader Model & Hardware Feature Level
* **Target Feature Level:** `D3D_FEATURE_LEVEL_11_1` preferred, with automatic fallback to `D3D_FEATURE_LEVEL_11_0` ([D3D11Device.cpp#L33-L95](file:///C:/Users/omarg/dev/DarkFire/Engine/RHI/D3D11/D3D11Device.cpp#L33-L95)). On the host system, active discrete GPU is `NVIDIA GeForce RTX 3050 Ti Laptop GPU` (3962 MB Dedicated VRAM, Feature Level 11_1).
* **Compiler & Dialect:** Offline compilation and asset packaging via `Tools/dfcook` ([CookShaders.cpp#L27-L41](file:///C:/Users/omarg/dev/DarkFire/Tools/dfcook/CookShaders.cpp#L27-L41)) using `D3DCompileFromFile` with `D3D_COMPILE_STANDARD_FILE_INCLUDE`. Runtime fallback compilation via `Engine/Renderer/ShaderLibrary.cpp#L70-L137` using `D3DCompile`.
* **Profiles:** `vs_5_0`, `ps_5_0`, `hs_5_0`, `ds_5_0`, `cs_5_0`.
* **Compilation Flags:** 
  `D3DCOMPILE_ENABLE_STRICTNESS` (`/Ges`) | `D3DCOMPILE_OPTIMIZATION_LEVEL3` (`/O3`) | `D3DCOMPILE_PACK_MATRIX_ROW_MAJOR` (`/Zpr`).
* **HLSL Dialect Constraints:** Strictly Shader Model 5.0. No HLSL 2021 constructs (no `select()`, no wave intrinsics, no template intrinsics, no SM 6 types, no descriptor spaces).

### 1.2 Ground Rules & Verified Conventions
All conventions have been verified directly in C++ and HLSL:

| Convention | Verified Setting | Source File & Line Evidence |
| :--- | :--- | :--- |
| **Coordinate System** | Left-Handed (LH), $+X$ Right, $+Y$ Up, $+Z$ Forward | [Math.h#L76-L90](file:///C:/Users/omarg/dev/DarkFire/Engine/Core/Math/Math.h#L76-L90), [SceneRuntimeLayer.cpp#L254-L256](file:///C:/Users/omarg/dev/DarkFire/Engine/Runtime/SceneRuntimeLayer.cpp#L254-L256) |
| **Depth Convention** | **Reversed-Z Floating Point** ($Z_{\text{near}} = 1.0$, $Z_{\text{far}} = 0.0$) | [Math.h#L85-L88](file:///C:/Users/omarg/dev/DarkFire/Engine/Core/Math/Math.h#L85-L88), [SceneRenderer.cpp#L357](file:///C:/Users/omarg/dev/DarkFire/Engine/Renderer/SceneRenderer.cpp#L357), [SceneRendererInit.cpp#L259](file:///C:/Users/omarg/dev/DarkFire/Engine/Renderer/SceneRendererInit.cpp#L259) |
| **Depth Stencil Format** | `DXGI_FORMAT_R32_TYPELESS` / `D32_FLOAT` | [SceneRendererInit.cpp#L756](file:///C:/Users/omarg/dev/DarkFire/Engine/Renderer/SceneRendererInit.cpp#L756) |
| **View-Space Depth Sign** | Positive $Z$ in front of camera ($z_{\text{view}} > 0$) | [Math.h#L86](file:///C:/Users/omarg/dev/DarkFire/Engine/Core/Math/Math.h#L86) ($w_{\text{clip}} = z_{\text{view}}$), [Lighting.hlsli#L207](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L207) |
| **World Units** | **Meters** (1.0 = 1.0 m; near plane = 0.1 m, far plane = 1000.0 m) | [Components.h#L120-L121](file:///C:/Users/omarg/dev/DarkFire/Engine/Core/Scene/Components.h#L120-L121), Sponza extents 37.2 m |
| **Matrix Packing** | **Row-Major** (`/Zpr`, `D3DCOMPILE_PACK_MATRIX_ROW_MAJOR`) | [CookShaders.cpp#L27](file:///C:/Users/omarg/dev/DarkFire/Tools/dfcook/CookShaders.cpp#L27), [ShaderLibrary.cpp#L84](file:///C:/Users/omarg/dev/DarkFire/Engine/Renderer/ShaderLibrary.cpp#L84) |
| **HLSL `mul()` Order** | `mul(vector, matrix)` (Row-vector times row-major matrix) | [StandardVS.hlsl#L15-L17](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/StandardVS.hlsl#L15-L17) |
| **Roughness Convention** | Perceptual roughness $\in [0, 1]$ in APIs and textures; $\alpha = \text{roughness}^2$ in NDF | [BRDF.hlsli#L16](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/BRDF.hlsli#L16), [CookLightingIBL.cpp#L53](file:///C:/Users/omarg/dev/DarkFire/Tools/dfcook/CookLightingIBL.cpp#L53) |
| **Normal Map Format** | Cooked DDS: `BC5_UNORM` ($R=X, G=Y$). Raw textures: `R8G8B8A8_UNORM` Linear | [CookTextures.cpp#L61](file:///C:/Users/omarg/dev/DarkFire/Tools/dfcook/CookTextures.cpp#L61), [SceneRendererTextureLoader.cpp#L101](file:///C:/Users/omarg/dev/DarkFire/Engine/Renderer/SceneRendererTextureLoader.cpp#L101) |
| **Cluster Grid Dimensions** | $16 \times 16 \times 24$ View-Space Logarithmic Depth Slices | [ClusterBuildAABBsCS.hlsl#L35](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/ClusterBuildAABBsCS.hlsl#L35), [ClusterLightAssignCS.hlsl#L68](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/ClusterLightAssignCS.hlsl#L68) |

### 1.3 Baseline Disassembly & Compile Performance (FXC 10.1 / SM 5.0)
Measured using Microsoft FXC compiler (`10.0.26100.0`) with optimization level 3, row-major packing, and strictness:

| Shader | Compiled Size | Instruction Count | FXC Compile Time | Loop Unrolling Status & Notes |
| :--- | :--- | :--- | :--- | :--- |
| `DeferredLightingPS.hlsl` | 103,936 bytes | ~3,093 instructions | 8,085 ms | Light loop kept (`loop`/`endloop`); DDGI 8-corner loop unrolled 8x; Vogel 16-tap unrolled 16x |
| `StandardPS.hlsl` | 111,488 bytes | ~3,333 instructions | 16,198 ms | Light loop kept; all material branches (cloth/coat/aniso/trans) compiled in single monolithic pass |
| `TerrainPS.hlsl` | 120,528 bytes | ~3,625 instructions | 13,111 ms | Procedural noise + cliff blend + full light loop; largest compiled bytecode in engine |
| `GrassPS.hlsl` | 107,980 bytes | ~3,217 instructions | 8,088 ms | Full lighting loop + subsurface backlight term + multi-render target outputs |
| `SSAOPS.hlsl` | 71,200 bytes | ~2,303 instructions | 1,026 ms | GTAO horizon integration loops unrolled across spatial kernel |
| `WaterPS.hlsl` | 52,484 bytes | ~1,484 instructions | 1,014 ms | Procedural wave ripple + caustics + refraction ray reconstruction + directional specular |

*Baseline Compiler Warnings:*
* `Lighting.hlsli(297,9-82): warning X3206: implicit truncation of vector type` (triggered in DeferredLightingPS, StandardPS, TerrainPS, GrassPS, WaterPS).

### 1.4 Runtime Test Suite & Benchmark Telemetry Baseline
Executed via `Tests.exe` on D3D11 hardware backend:
* **Test Suite Status:** 95 / 95 Test Cases Passed (259,470 assertions passed, 0 failed).
* **Playground3D [Forward+]:** 372.3 FPS (2.69 ms avg, 1% Low 183.5 FPS, 82 draw calls)
* **Playground3D [Deferred PBR]:** 313.3 FPS (3.19 ms avg, 1% Low 46.6 FPS, 71 draw calls)
* **Crytek Sponza Atrium [Forward+]:** 44.9 FPS (22.27 ms avg, 1% Low 4.4 FPS, 164 draw calls)
* **Crytek Sponza Atrium [Deferred PBR]:** 62.3 FPS (16.04 ms avg, 1% Low 15.2 FPS, 140 draw calls)
* **Tropical Island & Ocean [Forward+]:** 145.7 FPS (6.86 ms avg, 1% Low 12.1 FPS, 35 draw calls, 19,703 instances)

---

## 2. Directory Structure & Orphaned Artifact Inventory

### 2.1 Orphaned Artifacts Checked (Rule 4)
* **`PostProcessPS.cso`:** Confirmed ORPHANED. `PostProcessPS.hlsl` was deleted during earlier phase post-process refactoring. Grep across the entire engine and tools confirmed that zero C++ classes or build scripts load `PostProcessPS.cso`. `passPostProcess()` in `SceneRendererPostPasses.cpp` dispatches `TonemapPS` and `FXAAPS`. Propose removing `PostProcessPS.cso`.
* **`Engine/Shaders/Shaders/`:** Confirmed REDUNDANT BUILD ARTIFACT. Created when `dfcook` was previously invoked with `--output Engine/Shaders`, placing `.cso` files in `Engine/Shaders/Shaders/`. Propose deleting this nested folder.
* **`Engine/Shaders/Content/Baked/`:** Confirmed REDUNDANT COOK ARTIFACT. Contains duplicate copies of `BrdfLut.dds`, `ColorLut.dds`, `DefaultEnvIBL.dds`, which already natively reside in `Engine/Content/Baked/`. Propose removing from `Engine/Shaders/`.
* **`Engine/Shaders/Shared/Bindings.h`:** C++ and HLSL cross-language header containing cbuffer definitions and register slot macros (`DF_SLOT_*`). Kept and unified into the single source of truth.
* **`.gitignore` Update (Rule 3):** `*.cso` was missing from `.gitignore`, allowing compiled binary blobs to pollute git status. Adding `*.cso` to `.gitignore`.

---

## 3. Resource Binding & Slot Verification (D3D11 Limits)

Direct3D 11 resource binding slots are validated against hardware stage limits:

### 3.1 Constant Buffer Slots (Max 14 per stage) — Current: 9 / 14
* `b0`: `CBPerFrame` / `CBClusterBuild` / `CBClusterAssign` / `CBConvolve` / `CBFXAA`
* `b1`: `CBPerView`
* `b2`: `CBPerObject`
* `b3`: `CBPerMaterial`
* `b4`: `CBPerPass`
* `b5`: `CBShadow` (Forward) / `CBEnvironment` (Sky) / `CBSSAO` (SSAO) / `CBPostProcess` (Tonemap)
* `b6`: `CBTerrain` / `CBFoliage` / `CBWater`
* `b7`: `CBSSR`
* `b8`: `CBDDGI` / `CBSSS`

### 3.2 Sampler Slots (Max 16 per stage) — Current: 5 / 16 (11 Free)
* `s0`: `g_SamplerLinearWrap`
* `s1`: `g_SamplerLinearClamp`
* `s2`: `g_SamplerPointClamp`
* `s3`: `g_SamplerAnisoWrap`
* `s4`: `g_SamplerShadow` (Comparison sampler)

### 3.3 Shader Resource Views (Max 128 per stage) — Current: 29 / 128
Slots `t0` through `t28` verified against C++ bindings in `ForwardOpaquePass.cpp#L380-L415` and `DeferredPass.cpp#L330-L375`:
* `t0`..`t7`: Material textures (Albedo, Normal, MetallicRoughness, Emissive, Occlusion, Detail/Custom0, Custom1, Custom2)
* `t8`: `g_Lights` (`StructuredBuffer<LightData>`)
* `t9`: `g_TextureDepth`
* `t10`: `g_TextureBloom`
* `t11`: `g_TextureSSAO`
* `t12`: `g_TextureSkyEquirect`
* `t13`: `g_TextureShadowMap`
* `t14`: `g_TextureGBufferEmissiveAO`
* `t15`: `g_TextureNormalRoughness`
* `t16`: `g_TextureAlbedoMetallic`
* `t17`: `g_TextureSpecularIBL`
* `t18`: `g_TextureIrradianceIBL`
* `t19`: `g_TextureBRDFLUT`
* `t20`: `g_TextureColorLUT`
* `t21`: `g_TextureVelocity`
* `t22`: `g_ClusterGrid` (`StructuredBuffer<uint2>`)
* `t23`: `g_ClusterLightIndices` (`StructuredBuffer<uint>`)
* `t24`: `g_TextureSSR`
* `t25`: `g_TextureHiZDepth`
* `t26`: `g_TextureDDGIRadiance`
* `t27`: `g_TextureDDGIDepth`
* `t28`: `g_TextureSSS`

### 3.4 Unordered Access Views (Max 8 in D3D11.0 CS) — Current: $\le 3$ / 8
* `ClusterBuildAABBsCS`: 1 UAV (`u0: g_ClusterAABBs`)
* `ClusterLightAssignCS`: 3 UAVs (`u0: g_ClusterGrid`, `u1: g_ClusterLightIndices`, `u2: g_GlobalLightIndexCounter`)
* `HiZBuildCS`: 1 UAV (`u0`)
* `CullInstancesCS`: 2 UAVs (`u0`, `u1`)
* `DDGIRadianceGatherCS`: 1 UAV (`u0`)
* `DDGIDepthGatherCS`: 1 UAV (`u0`)

---

## 4. Complete Defect Audit & Resolution Table

Each item has been investigated against the real codebase and assigned an audit status.

### 4.1 Lighting & Surface Defect Matrix (`Lighting.hlsli`, `Shadows.hlsli`, `BRDF.hlsli`, `Surface.hlsli`)

| ID | Location | Status | Evidence & Technical Breakdown | Category | Expected Visual / Perf Delta |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **L1** | [Lighting.hlsli#L297](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L297) | **CONFIRMED** | `float directLightAtten = radiance * ...;` truncates `radiance` (`float3`) to red channel scalar `.x`. FXC diagnostic `warning X3206`. Colored lights lose green/blue components. | **B** | Colored lights correctly illuminate scenes with true tint instead of monochrome red attenuation. |
| **L2** | [Lighting.hlsli#L508](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L508) | **CONFIRMED** | `saturate(pow(NoV + ao, 2.0f) - 1.0f + roughness)` drops grazing specular reflections to `roughness` even when $ao = 1.0$. Smooth surfaces lose ~95% of grazing reflection. | **B** | Smooth floors and metallic surfaces retain proper Fresnel grazing reflections when unoccluded. |
| **L3a** | [Lighting.hlsli#L60-L67](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L60-L67) | **CONFIRMED** | `dir = worldPos - probeWorldPos` computes vector from probe to surface. `dot(dir, N)` gives maximum weight to back-facing probes, causing light leaking through walls. Must be `dirToProbe = probeWorldPos - biasedPos`. | **B** | Eliminates severe DDGI light leaking through room walls and backfaces. |
| **L3b** | [Lighting.hlsli#L45, L77](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L45) | **CONFIRMED** | Depth atlas lookup uses `octEncode(N)` rather than `octEncode(probeToPoint)`. Probes store distance towards surface, not normal. | **B** | Chebyshev occlusion correctly shadows geometry behind occluders instead of self-occluding based on normal. |
| **L3c** | [Lighting.hlsli#L27](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L27) | **CONFIRMED** | DDGI position biasing uses `bentN` instead of geometric normal $N$, and distance test tests against unbiased position. | **B** | Eliminates probe bias self-shadowing acne on curved surfaces. |
| **L3d** | [Lighting.hlsli#L86](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L86) | **CONFIRMED** | Lacks standard McGuire weight crush (`w < 0.2 -> w *= w^2 / 0.04`), and volume edge uses hard clamp cutoff returning 0. | **B** | Soft, seamless transition at probe volume boundaries; eliminates edge popping. |
| **L3e** | [Lighting.hlsli#L76, L94](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L76) | **CONFIRMED** | `GetDimensions()` called 16 times per pixel inside unrolled 8-corner loop. Probes-per-row hardcoded to 16/32. | **C** | Saves 16 texture dimension query instructions per pixel; atlas dims hoisted to constant buffer. |
| **L3f** | [Lighting.hlsli#L10](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L10) | **FALSE POSITIVE** (Clarity only) | Vector ternary `n.xy >= 0.0f ? 1.0f : -1.0f` compiles cleanly in FXC. Replace with named `signNotZero()` helper for clarity; do not use `select()`. | **A** | Zero bytecode delta; improved readability. |
| **L4** | [Lighting.hlsli#L213-L214](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L213-L214) | **CONFIRMED** | `useClusters = (activeLightCount > 0)`. When cluster is empty (`activeLightCount == 0`), falls back to iterating ALL scene lights. | **B & C** | Emptiest clusters no longer suffer massive performance cliff; explicit cluster flag introduced. |
| **L5** | [Lighting.hlsli#L306, L315](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L306) | **CONFIRMED** | `clothDiffuse` lacks `INV_PI` ($\approx \pi\times$ brighter than all other materials). Charlie NDF has wrong exponent and is masked by `min(sheenNDF, 2.0f)`. | **B** | Energy-conserving fabric rendering; realistic velvet and satin sheen without unnatural over-brightness. |
| **L6** | [Lighting.hlsli#L442, L492](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L442) | **CONFIRMED** | `diffuseIBL *= diffProbeBlend` and `envRadiance = ... * specProbeBlend` multiply by weight, fading to black at volume edges. Must `lerp(sky, probe, blend)`. | **B** | Smooth cross-fade between local reflection probes and sky environment without black boundary halos. |
| **L7** | [Lighting.hlsli#L407, L490](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L407) | **CONFIRMED** | `any(sample > 0.0001f)` sentinel-by-data checks whether texture exists. Dark interiors or night sky bleed fallback colors. | **B** | Valid resources determined by cbuffer validity flags; dark scenes remain truly dark. |
| **L8** | [Lighting.hlsli#L376-L377](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L376-L377) | **NEEDS DECISION** | `skyShadowOcclusion` uses directional sun cascade shadow to occlude sky ambient and probe specular reflections. Costs extra shadow tap and dims ambient in all sun shadows. | **B** | Subject to user review (see Section 6: Open Questions). |
| **L9** | [Lighting.hlsli#L274-L277](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L274-L277) | **CONFIRMED** | `if (i == 0)` applies directional CSM shadow to spotlight index 0. | **B** | Directional shadow no longer incorrectly shadows spotlights. Explicit sun index introduced. |
| **L10** | [Lighting.hlsli#L358-L360](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L358-L360) | **CONFIRMED** | Tangent frame fabricated from arbitrary up vector with abrupt flip at $|N.z| = 0.999$, creating visible seam. | **B** | Anisotropic metals use mesh vertex tangent frame + rotation factor without pole seams. |
| **L11** | [Lighting.hlsli#L383](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L383) | **CONFIRMED** | `bentN = normalize(lerp(N, up, (1-ao)*0.35f))` artificially brightens ceilings and downward cavities. SSAO does not output bent normal. | **B** | Downward-facing crevices no longer incorrectly illuminated by sky ambient. |
| **L12** | [Lighting.hlsli#L401, L485](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L401) | **CONFIRMED** | Equirectangular sampling wraps in V, causing top and bottom pole rows to bleed into opposite hemisphere at high mips. | **B** | Clamped V coordinates prevent pole bleeding in environment reflections. |
| **L13** | [Lighting.hlsli#L536-L548](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L536-L548) | **CONFIRMED** | Clear coat samples sky only (ignores local probe, box projection, SSR); uses base-layer occlusion; `ccFactor` unhoisted. | **B & C** | Dual-layer clear coat reflects local environment and probes; loop invariants hoisted. |
| **L14** | [Shadows.hlsli#L185, L226](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Shadows.hlsli#L185) | **CONFIRMED** | Shadow normal-offset uses $+geomN$ for back-lit transmission/two-sided terms instead of $-N$. | **B** | Transmissive leaves and thin foliage receive correct shadow bias without light-leaking through backface. |
| **L15** | [Lighting.hlsli#L140](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L140) | **CONFIRMED** | `1.0f / (sign(R) * max(abs(R), 1e-5f))` causes division by zero when any component of $R = 0.0$ because HLSL `sign(0) == 0`. | **B** | Eliminates occasional NaN/black pixel sparks on box-projected probe reflections. |
| **L16** | [Lighting.hlsli#L254, L272](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L254) | **CONFIRMED** | Attenuation softening `+ 1.0f` is Unreal's centimeter-based constant ($1.0 \text{ cm}^2$). In DarkFire's meter units, $1.0\text{ m}^2$ creates huge 1m artificial softening radius. | **B** | Point and spot light attenuation matches physical meter scale ($d_{\text{min}}^2 = 0.01^2 \text{ m}^2$). |
| **L17** | [Lighting.hlsli#L280](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L280) | **CONFIRMED** | `normalize(V + L)` when $V = -L$, and `normalize(toLight)` when $\text{dist} = 0$ generate NaNs. | **B** | Protected by `safeNormalize()` math helper. |
| **L18** | [Lighting.hlsli#L216-L370](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L216-L370) | **CONFIRMED** | No early-out when $N \cdot L \le 0$ on opaque surfaces; loop-invariants ($N \cdot V$, material flags) unhoisted; missing `[loop]` annotation. | **C** | Skips costly BRDF evaluation on back-facing lights; lowers register pressure in PS. |
| **L19** | [Lighting.hlsli#L296-L297](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Lighting.hlsli#L296-L297) | **CONFIRMED** | AO applied twice to direct light (`directOcclusion` and `computeMicroShadow`). Misleading comment on multi-scatter compensation. | **B** | Consolidated direct light occlusion model; accurate documentation. |

### 4.2 Atmosphere Defect Matrix (`Atmosphere.hlsli`, `SkyPS.hlsl`)

| ID | Location | Status | Evidence & Technical Breakdown | Category | Expected Visual / Perf Delta |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **A1** | [Atmosphere.hlsli#L72](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Atmosphere.hlsli#L72) | **CONFIRMED** | `min(4.0f * PI * hgPhase, 3.5f)` hard-clamps the forward scattering peak at 3.5, producing a flat 29-degree plateau disc around the sun. | **B** | Smooth forward-scattering halo blending seamlessly into the sun corona. |
| **A2** | [Atmosphere.hlsli#L72](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Atmosphere.hlsli#L72) | **CONFIRMED** | Sun in-scattering ignores `colorIntensity.a` (intensity multiplier), while lighting multiplies by `.a`. | **B** | In-scattering scales proportionally with directional sun intensity. |
| **A3** | [Atmosphere.hlsli#L73](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Atmosphere.hlsli#L73) | **CONFIRMED** | `saturate(sunDir.y * 0.5f + 0.5f)` leaves 50% in-scatter at the horizon ($y = 0$). | **B** | Proper smoothstep horizon extinction at dawn/dusk. |
| **A4** | [Atmosphere.hlsli#L44-L45](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Atmosphere.hlsli#L44-L45) | **CONFIRMED** | `frac(worldPos.x * 17.3f + worldPos.z * 17.3f)` is camera-motion unstable and loses floating point precision at large world coordinates. | **B** | Stable temporal noise using IGN + subpixel jitter offset. |
| **A5** | [Atmosphere.hlsli#L42-L58](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Atmosphere.hlsli#L42-L58) | **CONFIRMED** | 8-step raymarch always executes 8 shadow map samples per pixel, even when `fogFactor < 0.001f` or density is 0. | **C** | Immediate early-out for foreground fragments or un-fogged scenes saves up to 8 shadow taps/px. |
| **A6** | [Atmosphere.hlsli#L24-L25](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Atmosphere.hlsli#L24-L25) | **CONFIRMED** | `max(cameraPos.y, 0.0f)` and `max(worldPos.y, 0.0f)` clamps all heights to zero, breaking linear parametrization for rays crossing $y = 0$. | **B** | Continuous exponential fog density below sea level. |
| **A7** | [Atmosphere.hlsli#L11, L43](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Atmosphere.hlsli#L11) | **CONFIRMED** | Sentinel argument `screenPos = float2(0, 0)` with runtime branch checking `any(screenPos != 0)`. | **A** | Explicit overloads for screen-space vs world-space fog evaluation. |
| **A8** | [Atmosphere.hlsli](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Atmosphere.hlsli) | **PROPOSAL** | Froxel / 3D texture volumetric fog design note prepared (see Section 5). | — | Architectural design note provided for future feature adoption. |

### 4.3 Additional Defects Discovered Beyond Original List

| ID | Location | Status | Evidence & Technical Breakdown | Category | Expected Visual / Perf Delta |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **X1** | [WaterPS.hlsl#L145](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/WaterPS.hlsl#L145) | **CONFIRMED** | `seabedColor` is sampled from `g_TextureCustom0` (which already had atmospheric fog applied during opaque pass). `WaterPS` applies `applyAtmosphericFog` again to the composite, causing **double-fog** on shallow water. | **B** | Shallow water and ocean shorelines no longer suffer double-fogging haze. |
| **X2** | [WaterPS.hlsl#L113](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/WaterPS.hlsl#L113) | **CONFIRMED** | `any(skySample > 0.001f)` sentinel-by-data used for water sky reflection fallback. | **B** | Explicit environment binding flag used instead of texel threshold. |
| **X3** | [SSRPS.hlsl#L45](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/SSRPS.hlsl#L45) | **CONFIRMED** | Cloth materials store negative roughness (`-roughness`) in GBuffer. `SSRPS` checks `roughness > g_SSR.ssrParams.z`. For cloth, negative roughness passes the check, causing SSR to raymarch reflections on cloth and boosting confidence. | **B** | SSR reflection raymarching properly skipped for cloth surfaces. |
| **X4** | [DecalPS.hlsl#L4-L9](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/DecalPS.hlsl#L4-L9) | **CONFIRMED** | Hardcoded macro hack `#define g_TextureDepth g_TextureDepth_Slot9Unused` to bind depth at `t8` because C++ binds at slot 8 instead of standard `DF_SLOT_TEX_DEPTH` (`t9`). | **A** | Unified binding slot for depth across all forward/deferred/decal passes. |
| **X5** | [Surface.hlsli#L4](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Surface.hlsli#L4) | **CONFIRMED** | `Surface.hlsli` includes `Lighting.hlsli` solely to access `computePerturbedNormal`, pulling the entire lighting and shadow graph into non-lighting shaders (e.g. `GBufferPS.hlsl`). | **A** | Extracted `NormalMapping.hlsli`; removes unwanted transitive include dependencies. |
| **X6** | [Common.hlsli#L40-L70](file:///C:/Users/omarg/dev/DarkFire/Engine/Shaders/Common.hlsli#L40-L70) | **CONFIRMED** | Raw hardcoded registers (`register(t0)`, `register(b4)`, etc.) instead of named symbols from `Bindings.h`. | **A** | All registers use unified constants from `Bindings.hlsli`. |

---

## 5. Architectural Proposals & Design Notes

### 5.1 Froxel Volumetric Fog Proposal (A8 Design Note)
* **Current Implementation:** Forward passes (`StandardPS`, `GrassPS`, `TerrainPS`, `WaterPS`) and `DeferredLightingPS` each evaluate an 8-step raymarch loop with 8 hardware shadow map taps per pixel. On a 1080p frame with forward geometry, this results in millions of redundant shadow comparison queries.
* **Proposed Architecture:**
  1. A compute pass (`VolumetricFogCS.hlsl`) evaluates in-scattering and extinction on a view-frustum aligned 3D Froxel grid (e.g. $160 \times 90 \times 64$ froxels, matching cluster depth distribution).
  2. The pass samples shadow maps and directional lights once per froxel cell and temporally filters with previous frame's 3D history buffer ($88\%$ history blend).
  3. A 3D integration pass pre-integrates transmittance and accumulated in-scattering along each $Z$-slice.
  4. Forward opaque, deferred lighting, and water shaders evaluate fog via a single trilinear 3D texture sample: `g_TextureVolumetricFog.SampleLevel(g_SamplerLinearClamp, froxelUVW, 0)`.
* **Hardware & Feature Level Feasibility:**
  - Direct3D 11.0 / 11.1 fully supports 3D textures (`Texture3D<float4>`) with UAV write access in compute shaders (`RWTexture3D<float4>`).
  - Total VRAM: $160 \times 90 \times 64 \times 8 \text{ bytes (R16G16B16A16\_FLOAT)} \approx 7.37 \text{ MB}$.
  - Perf Delta: Replaces ~8 shadow taps per pixel across all forward & deferred geometry with 1 trilinear volume tap; expected frame time savings in foggy scenes: $1.5\text{ ms} - 2.8\text{ ms}$ on discrete GPU.

### 5.2 Phase 3 Reorganization Plan
To eliminate the monolithic 550-line `Lighting.hlsli` and acyclic include dependency tangles:

```
Engine/Shaders/
├── Common/
│   ├── Bindings.hlsli          # Unified register slots & cbuffer layout definitions (matched to C++)
│   ├── Common.hlsli            # Math constants, packing helpers, color spaces, safe math
│   └── Conventions.hlsli       # LH, Reversed-Z, row-major mul order, metric unit documentation
├── Lighting/
│   ├── BRDF.hlsli              # Microfacet NDF, Smith G, Schlick Fresnel, multi-bounce AO
│   ├── DirectLighting.hlsli    # Per-layer direct evaluation (Base, Cloth, ClearCoat, Aniso, Trans)
│   ├── ClusteredLights.hlsli   # 16x16x24 cluster grid coordinate lookup & light looping
│   ├── Shadows.hlsli           # 4-cascade CSM Vogel PCF, PCSS, and contact shadows
│   ├── NormalMapping.hlsli     # TBN frame perturbation, detail normal blending, BC5 reconstruction
│   ├── IBL.hlsli               # Split-sum specular & diffuse irradiance environment lookup
│   ├── DDGI.hlsli              # Dynamic Diffuse Global Illumination 8-corner gather & Chebyshev depth
│   └── Atmosphere.hlsli        # Exponential height fog & analytical sun in-scattering
├── Materials/
│   └── Surface.hlsli           # PBR material unpacking (ORM, MaskMap, standard), specular AA
├── Passes/
│   ├── DeferredLightingPS.hlsl # GBuffer light resolve pass
│   ├── GBufferPS.hlsl          # Opaque GBuffer layout generation pass
│   ├── StandardVS.hlsl / PS    # Forward+ PBR baseline shader
│   ├── DecalVS.hlsl / PS       # OBB projected decal shader
│   ├── SkyPS.hlsl              # Physical procedural sky & equirectangular skybox shader
│   ├── WaterVS.hlsl / PS       # Gerstner/procedural wave water with caustics and absorption
│   └── TerrainVS.hlsl / PS     # Multi-texture slope-blended terrain shader
└── Compute/
    ├── ClusterBuildAABBsCS.hlsl
    ├── ClusterLightAssignCS.hlsl
    ├── ConvolveIBLCS.hlsl
    ├── DDGIRadianceGatherCS.hlsl
    ├── DDGIDepthGatherCS.hlsl
    └── HiZBuildCS.hlsl
```

Every header will feature:
1. Standard `#ifndef` / `#define` include guards.
2. Self-containment: compiles standalone when included in an empty `.hlsl`.
3. Derivative-free texture sampling in all divergent flow control.

---

## 6. Open Questions for User Approval (Rule 7)

Before committing Phase 2 (Category B) edits, user guidance is requested on the following design decisions:

1. **L8 Sky Ambient Occlusion in Shadows:**
   - *Current Code:* `skyShadowOcclusion = lerp(0.04f, 1.0f, sampleVolumetricShadow(worldPos + N * 0.15f))`. This uses directional sun cascade shadow visibility to occlude sky ambient in any sun shadow, causing shadows to lose natural sky fill and dimming probe reflections.
   - *Options:*
     - **Option A (Recommended):** Decouple sky ambient from sun shadow visibility entirely. Use SSAO / multi-bounce AO (`diffuseAO`) and DDGI irradiance for ambient cavity occlusion, allowing outdoor shadowed areas to receive authentic sky horizon/zenith illumination.
     - **Option B (Artistic compromise):** Keep a gentle sun-shadow ambient reduction, but soften the minimum floor from `0.04` (96% dark) to `0.40` (60% ambient preserved in sun shadows).
2. **L11 Ceilings / Downward Surface Bent Normal:**
   - *Current Code:* `bentN = normalize(lerp(N, float3(0.0f, 1.0f, 0.0f), (1.0f - ao) * 0.35f))`.
   - *Recommendation:* Remove this artificial upward tilt since SSAO does not provide a true bent normal, or clamp to $\max(N.y, 0.0) \cdot 0.35f$ so that horizontal and downward surfaces (ceilings, arch undersides) are not pulled upward towards the sky.
3. **L16 World Unit Attenuation Softening Radius:**
   - *Current Code:* `dist * dist + 1.0f` (Unreal cm default).
   - *Recommendation:* Set named constant `LIGHT_MIN_RADIUS_SQ = 0.0001f` ($1\text{ cm} = 0.01\text{ m}$ radius) to match physical point lights in meters without blowing out at $d < 1\text{ m}$.
4. **Orphaned File Cleanups (Rule 4):**
   - Confirm deletion of orphaned `PostProcessPS.cso`, nested `Engine/Shaders/Shaders/` folder, and redundant `Engine/Shaders/Content/Baked/` DDS textures.
