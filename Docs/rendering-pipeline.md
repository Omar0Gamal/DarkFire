# DarkFire Shader Overhaul: Correct PBR + Beautiful Look

## Goal
Fix the rendering bugs that make the scene look flat, hazy and "off", then bring the look up to Unreal/Unity-HDRP level with a proper image-based-lighting (IBL) pipeline, real cascaded shadows, a better AO and filmic post-processing. The work is ordered so that **every phase leaves the engine working and visibly better**.

## Decisions (made for you, tell me if you disagree)
| Question | Decision | Why |
|---|---|---|
| Look target | **Unreal-like filmic**: ACES-fit tonemap with a slight desaturation recovery, plus a neutral-gamut option | Matches PBR expectations and looks good out of the box on Sponza |
| Sky | **Procedural physical-style sky is the default**, and an HDRI (`.hdr`/`.exr`/BC6H DDS) is optional and takes over when set | No asset dependency. The sky is also baked into an IBL cubemap, so lighting always matches the visible sky |
| Light units | Directional in **lux-like scene units** (sun ≈ 3.0 to 6.0), exposure 1.0, with an optional auto-exposure | Keeps existing scene values valid |
| Scope | **All phases**, delivered in order. Phase 1 and 2 are the must-haves | |
| Backend | D3D11 only (the current renderer) | |

## Findings (verified in code)
### Bugs
| # | Location | Problem |
|---|---|---|
| A1 ✔ | `SceneRendererPasses.cpp` L646-664 vs L813-855 | The sky texture (t12) is only bound in the sky pass. Opaque, terrain, foliage, transparent and water passes never bind it, so specular IBL is always the 3-colour gradient |
| A2 | `SceneRendererResources.cpp` L816 | The sky is loaded as 8-bit UNORM, so HDR highlights are clipped and wrong |
| A3 | `CompositeSSRPS`, `CompositeGIPS` (additive blend) | SSR and SSGI are added on top of lighting that already has full specular IBL and ambient, so energy is double-counted (haze and over-glossy look) |
| A4 | `Lighting.hlsli` L421 | No specular or horizon occlusion. Raw SSAO is applied to specular |
| A5 | `StandardPS.hlsl` L17-21 | The alpha-test cutoff is clamped to 0.20-0.75 and contains a no-op. Foliage erodes |
| A6 | `TonemapPS.hlsl` | AgX selects ACES silently. The vignette `smoothstep` has reversed edges (undefined in HLSL). Contrast and saturation are applied after the tonemap |
| A7 ✔ | `SceneRendererPasses.cpp` L124-170 | CSM is dead code: 4 identical matrices, `shadowExtra.w=1`, one 2048² map over ≈ 90 m (≈ 22 texels/m), so blurry shadows |
| A8 | `Lighting.hlsli` L333 | Spot shadow only when `i == 0`. No point-light shadows |
| A9 | `PostProcessPS.hlsl` | A legacy duplicate that uses `pow(1/2.2)`, still created at `SceneRenderer.cpp` L309 |
| A10 | `StandardVS.hlsl` L21 | Instanced normals use the world matrix, which is wrong for non-uniform scale |
| A11 | SSAO (`SSAOPS` + `SceneRendererPostPasses.cpp` L50) | `power=2.0` and `intensity=1.2` over-darken. AO is also applied to direct light and specular |

### Non-standard hacks
Forced sheen/transmission/two-sided minimums (`Lighting.hlsli` L356-385), a non-standard detail-normal add, specular AA with no clamp, no multi-scatter compensation, the water sun highlight (`pow(256)*2.5`), procedural colours that look sRGB but are used as linear (terrain, grass), normal-map mips that aren't renormalised, a decoupled sky/ambient intensity, and fog evaluated only at the fragment height.

## Proposed Changes

### Phase 1: Correctness and energy conservation (largest win)
1. **Bind IBL resources in every lit pass** (t12 irradiance/specular cubes, plus a BRDF LUT in a new slot). Fallback: a black texture. (A1)
2. **HDR texture loading**: add a float path in `SceneRendererResources.cpp` using DirectXTex `LoadFromHDRMemory`/`LoadFromDDSMemory` into `R16G16B16A16_FLOAT` with a full mip chain. (A2)
3. **Fix SSR/SSGI energy** (A3):
   - SSR composite blends with `(1 - alpha)` on the already-lit colour: `final = lit * (1 - a) + ssr * a`. Reflection colour uses the Fresnel/env-BRDF term from `envBRDFApprox`.
   - SSGI is multiplied by material AO, clamped, and optionally only enabled when SH probes are off, to avoid double indirect light.
4. **Specular occlusion** (Lagarde `computeSpecularOcclusion`) and **horizon occlusion** in `Lighting.hlsli`. SSAO no longer multiplies specular directly. (A4)
5. **Alpha test**: `clip(a - cutoff)` plus **alpha-to-coverage** with a sharpened mip alpha (scale alpha by coverage per mip on load). (A5)
6. **Instance normals**: add `invTranspose` to the instance buffer, or normalise a scale-compensated matrix. (A10)
7. **Delete `PostProcessPS.hlsl`** and its creation. (A9)

### Phase 2: Lighting quality, where "beautiful" comes from
8. **Image-based lighting pipeline** (new `IBLBaker`, built at startup and whenever the sky changes):
   - Render the procedural sky (or HDRI) to a 256² cubemap.
   - **Diffuse**: convolve to L2 SH (9 coefficients), reusing the existing SH evaluation path. This replaces the 3-colour hemisphere ambient.
   - **Specular**: GGX-prefilter into a mip chain (6 mips, importance sampled). The shader samples `SampleLevel(cube, R, roughness * maxMip)`.
   - **BRDF LUT**: generate the 2D split-sum LUT once (replaces the `envBRDFApprox` fit, or keep it as the fallback).
9. **Real 4-cascade CSM** (A7): practical splits (λ = 0.75), a 4096² `Texture2DArray` (or 2×2 atlas), sphere-fit cascades, per-cascade texel snapping, cascade selection plus a 10% blend band in `evaluateDirectionalShadow`, slope-scaled receiver-plane depth bias, and PCF/PCSS-lite contact softening. Shadows for the first N spot lights (atlas slots). (A8)
10. **Multi-scatter energy compensation** (Fdez-Agüera) so rough metals aren't dark, and **Kaplanyan specular AA** (variance 0.25, clamp 0.18) instead of the current formula.
11. **Standard detail normals** (RNM blend) and **normal-map mip renormalisation** at load time.
12. **Unreal-style attenuation**: `saturate(1 - (d/r)^4)^2 / (d² + 1)`, and a smoothstep² spot cone.
13. **Cloth/transmission/two-sided cleanup**: remove the forced minimums (authored values are respected, with sane defaults), use the Charlie NDF plus Neubelt visibility for sheen, and a thickness-based transmission term.

### Phase 3: Atmosphere and post, the visual polish
14. **Procedural sky**: Preetham/Hosek-style (or a simple Rayleigh+Mie) with a time-of-day sun. The sky disk and corona are physically scaled, and the sun colour and ambient follow sun elevation (warm at low angles).
15. **Fog**: analytic integrated exponential height fog (closed-form integral along the view ray, not point-sampled), with sun in-scattering using a Henyey-Greenstein phase.
16. **GTAO** replaces the current SSAO: horizon-based with a multi-bounce approximation (Jimenez) so albedo-dependent bounce light brightens corners naturally. Half-res with a bilateral upsample. Defaults: radius 0.5, intensity 1.0, power 1.0. (A11)
17. **Tonemap/grade** (A6): a real AgX implementation plus the ACES fit with a gamut-compression step. Grading is done in a log or ACEScct-like space before the tonemap, with lift/gamma/gain and white balance (temperature/tint). The vignette is fixed. Both a **dither** (±0.5 LSB triangular noise) to kill banding in the sky gradient, and optional auto-exposure from a luminance histogram.
18. **Bloom**: keep the dual-filter pyramid, but with a Karis average on the first downsample only and an energy-conserving upsample (`lerp`, not additive). Add lens dirt as a stretch goal.
19. **Optional**: contact-shadow ray march (screen-space, 16 steps) for fine detail near the sun, and TAA with a jitter and velocity buffer (this needs a motion vector target) as a stretch item.

### Phase 4: Materials and procedural content
20. Treat the terrain/grass/water authored colours as sRGB (`sRGBToLinear` exact), add a slope/height-blended **triplanar** albedo with normal maps in terrain, and make the water a proper GGX + foam + depth-colour model. The sun highlight comes from the shared BRDF.
21. Add **parallax-corrected** reflection probes (box-projected) as a stretch item for Sponza's interior, with a per-room probe.

## Risk notes
> [!WARNING]
> - The CSM change touches the C++ pass order (`SceneRendererPasses.cpp`) and shader bindings. I'll keep the old single-map path behind a flag until the cascades are verified.
> - The IBL baker adds a startup cost (about 20-60 ms). It is cached and only rebuilt when the sky changes.
> - Phase 3 items 17 and 19 change the default look. Presets stay selectable via the post-process component, and the old ACES look is kept as an option.

> [!IMPORTANT]
> New constant-buffer fields must be added in **both** `Shaders/Shared/Bindings.h` and the C++ structs, and kept 16-byte aligned. This is the most likely source of build-time breakage.

## Verification Plan
### Automated
- Build: `msbuild DarkFire.slnx /p:Configuration=Debug /p:Platform=x64` (exact invocation to be confirmed from the project files).
- Compile every `.hlsl` through the engine's shader library and require zero warnings and errors.
- Run the tests in `Tests/`, and add C++ unit tests for the CSM split computation and the SH projection (a constant-colour sky must produce a constant irradiance).
- Add an offline BRDF-LUT sanity test (values in [0, 1], and `LUT(roughness=0, NoV=1) ≈ (1, 0)`).

### Manual (I'll capture screenshots at each phase and compare against `bug.png`)
- **Furnace test**: a rough white sphere under a uniform white sky renders near-white (≤ 2% energy loss), with SSR and SSGI both on.
- **Material ladder**: metal and dielectric spheres with roughness 0 to 1 against a reference.
- **Sponza**: check the debug views 0-8, shadow stability while the camera moves (no swimming), the foliage alpha edges, and fine AO in the arches.
- **Day to dusk**: orbit the sun with `[T]` and check that the sky, fog, ambient and shadows stay consistent.

---

## Additions (second review)

### Extra bugs and gaps found
| # | Location | Problem / gap |
|---|---|---|
| B1 | `StandardPS.hlsl` L13, L40, L54, L81 and `Lighting.hlsli` L25 | Every material texture is sampled with `g_SamplerLinearWrap` (s0), while `g_SamplerAnisoWrap` (s3) exists but isn't used. Textures turn to mush at grazing angles (Sponza floors, as in `patch_floor.png`). **Fix:** use the aniso sampler (16x, `MipLODBias` about -0.25 once TAA/upscaling is in) for albedo, normal, MR, AO and emissive. This is a one-line change per sample with a large visible payoff. |
| B2 | `PostProcessPS`/`FXAAPS` | FXAA works on the LDR image, so it blurs texture detail (the screenshot patches look soft). **Fix:** keep FXAA as a low-end option, make **TAA** (jitter, velocity, history clamp, sharpening) the default, and add optional **MSAA 4x** for the forward opaque pass (the engine is forward, so MSAA is affordable). |
| B3 | `getOrCreateTexture` | Textures are always converted to uncompressed `R8G8B8A8`, so memory is 4x to 6x higher than BC-compressed and bandwidth suffers. **Fix:** use BC7 (albedo), BC5 (normals, with Z reconstructed in the shader), and BC4/BC7 (ORM), compressed at cook time (`Cooked/`, `Tools/`) via DirectXTex. |
| B4 | `StandardPS.hlsl` L111-114 | Emissive isn't clamped or exposure-aware, and it writes a hot HDR value that is also read by SSR and SSGI, which causes fireflies. **Fix:** feed emissive into bloom only, and clamp what SSR and SSGI read. |
| B5 | `SSRPS.hlsl` | There is no hierarchical-Z trace, no temporal accumulation, and the hit colour is clamped at 4.0. Rays are linear in view space (32 steps). **Fix:** switch to a HiZ DDA trace with a temporal resolve, and fall back to the IBL specular on a miss. |
| B6 | `evaluateDirectLighting` | The `for` loop over `g_Pass.lightCount` is per pixel with no culling, so many point lights cost a lot. **Fix:** add a clustered or tiled light list (compute shader) once the light count passes about 8. |
| B7 | `SkyPS.hlsl` | The sky is drawn at `depth == 0` with no dithering, and there are no clouds or stars. **Fix:** add dither, plus a cheap volumetric or noise cloud layer and a night-time star field as an optional extra. |
| B8 | Whole renderer | There is no screen-space contact-shadow term, no volumetric light shafts, and no depth of field. **Fix:** contact shadows (Phase 3, item 19), optional **volumetric fog / light shafts** (froxel or raymarched), and an optional bokeh DOF. |

### Extra process items
- **Quality presets** (Low / Medium / High / Ultra) in the post-process and environment components. They set the shadow resolution, cascade count, GTAO and SSR step counts, the TAA/MSAA mode and the IBL resolution, so the look scales across hardware. The current frame time is 25 ms (about 40 FPS) with 666 draw calls, so the perf budget below matters.
- **Performance budget**: target at most 16.6 ms total on the dev machine at 1080p High. Reserve about 1.5 ms for shadows, 1 ms for GTAO, 1 ms for SSR, 0.7 ms for bloom and 0.5 ms for post. Capture the **Tracy** profile (already in `ThirdParty/tracy`) before and after each phase.
- **Draw-call reduction**: Sponza has 666 draws in 127 batches. Add GPU instancing and static batching where the material is shared, plus frustum and occlusion culling for the shadow cascades (each cascade culls to its own bounds).
- **Shader tooling**: a hot-reload key for shaders, a shader-warning-as-error build flag, `#pragma` based debug views (add the new views: IBL diffuse, IBL specular, cascade colours, GTAO, specular occlusion, overdraw), and a **shader permutation cache** so compile times don't grow.
- **Regression harness**: a small screenshot-capture mode (fixed camera, fixed time) that writes PNGs per phase, plus a diff tool with a tolerance, so later changes can't silently regress the look.
- **Git hygiene**: do the work on a feature branch (`feature/pbr-overhaul`) with one commit per numbered item, so any step can be reverted independently.
- **Docs**: update `Docs/` with the new light units, the sky and IBL workflow, and the material authoring guide (texture colour spaces, ORM packing, normal map convention).

### Extra decisions (defaults if you don't object)
| Question | Default |
|---|---|
| Anti-aliasing | TAA by default, FXAA fallback, MSAA optional |
| Texture compression | BC7 / BC5 at cook time. Runtime fallback stays RGBA8 |
| Auto-exposure | Off by default, enabled per-scene via the post-process component |
| Volumetrics, clouds, DOF | Off by default, delivered after Phase 3 as optional extras |
| Branch | `feature/pbr-overhaul` |

### Revised order of work
1. **Phase 0 (quick wins, about half a day):** B1 (anisotropic sampler), A5 (alpha cutoff), A6 (vignette and AgX selection), A9 (delete the legacy shader), B4 (emissive clamp).
2. **Phase 1** (energy fixes), **Phase 2** (IBL, CSM, BRDF), **Phase 3** (atmosphere and post, now including B2 TAA/MSAA).
3. **Phase 4** (materials, texture compression B3, SSR upgrade B5).
4. **Phase 5 (optional):** B6 clustered lights, B7 clouds, B8 volumetrics and DOF.

> [!TIP]
> Phase 0 is cheap and low risk and will already change the look noticeably (sharper textures, correct alpha, no vignette artefacts). I'd suggest starting there as soon as you approve.
