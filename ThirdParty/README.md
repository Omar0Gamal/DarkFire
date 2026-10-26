# DarkFire Engine — Third-Party Dependencies

All third-party libraries are either tracked as Git submodules pinned to release tags or vendored with `ORIGIN.txt` metadata.
Licenses are preserved in `Docs/Licenses/`.

| Library | Version / Tag | Type | License | Build / Integration |
|---|---|---|---|---|
| **DirectXTK** | `oct2024` | Git Submodule | MIT | Compiled via `ThirdParty/Build/DirectXTK.lua` |
| **DirectXTex** | `oct2024` | Git Submodule | MIT | Compiled via `ThirdParty/Build/DirectXTex.lua` |
| **EnTT** | `v3.13.2` | Git Submodule | MIT | Header-only (`ENTT_API_EXPORT` in `DarkFire.dll`) |
| **nlohmann/json** | `v3.11.3` | Git Submodule | MIT | Header-only (confined to `.cpp` files) |
| **Dear ImGui** | `docking` | Git Submodule | MIT | Compiled inside `DarkFire.dll` under `DF_EDITOR` |
| **ImGuizmo** | `master` | Git Submodule | MIT | Compiled inside `DarkFire.dll` under `DF_EDITOR` |
| **doctest** | `v2.4.11` | Git Submodule | MIT | Header-only unit testing framework |
| **Tracy** | `v0.11.1` | Git Submodule | BSD-3-Clause | Compiled into `DarkFire.dll` under `TRACY_ENABLE` |
| **cgltf** | `v1.14` | Git Submodule | MIT | Header-only (`CGLTF_IMPLEMENTATION`) |
| **tinyobjloader** | `v2.0.0-rc.10` | Vendored | MIT | Header-only (`TINYOBJLOADER_IMPLEMENTATION`) |
| **backward-cpp** | `v1.6` | Vendored | MIT | Header-only stack trace utility |
| **miniaudio** | `0.11.21` | Vendored | MIT / Public Domain | Single-file audio playback & spatialization |
| **ufbx** | `v0.14.3` | Vendored | MIT / Unlicense | Single-file fast FBX asset importer |
| **Premake5** | `5.0.0` | Vendored Tool | BSD-3-Clause | `ThirdParty/Premake/Windows/premake5.exe` |
