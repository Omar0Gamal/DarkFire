# DarkFire Engine (v2)

DarkFire is a modern, high-performance, game-agnostic C++20 3D game engine and standalone editor built for Windows x64 and DirectX 11.

## Architecture Highlights
- **Data-Driven Scene System:** Built upon EnTT entity-component storage with zero runtime inheritance overhead.
- **RHI & DirectX 11 Pipeline:** Reversed-Z depth buffering, linear HDR rendering, ACES tonemapping, bloom, and multi-pass instanced batching.
- **Modular Native Scripting:** Dynamic C-linkage game modules loaded across DLL boundaries.
- **Standalone Editor (`Editor.exe`):** Integrated Dear ImGui Docking workspace with 3D viewport, `ImGuizmo` transform manipulators, asset browser, and non-destructive scene simulation.

## Building from Source

### Prerequisites
- Windows 10/11 (x64)
- Visual Studio 2026 or 2022 with C++20 toolset and Desktop development with C++ workload.
- Git with Git LFS.

### Quick Start
1. Clone the repository with submodules:
   ```cmd
   git clone --recursive https://github.com/Omar0Gamal/DarkFire.git
   cd DarkFire
   ```
2. Generate project files:
   ```cmd
   Scripts\Setup.bat
   ```
3. Compile all projects:
   ```cmd
   Scripts\Build.bat Debug
   ```
4. Run unit tests:
   ```cmd
   Scripts\Test.bat Debug
   ```

## Repository Structure
- `Engine/` — Core engine shared library (`DarkFire.dll`).
- `Player/` — Generic runtime host (`Player.exe`).
- `Editor/` — Standalone editor desktop application (`Editor.exe`).
- `Templates/` — Project templates (including `Playground3D`).
- `Tests/` — Doctest test suite (`Tests.exe`).
- `ThirdParty/` — Pinned Git submodules and vendor libraries.
- `Scripts/` — Automation scripts (`Setup.bat`, `Build.bat`, `Test.bat`, `Package.bat`).

## License
DarkFire is licensed under the MIT License. See [LICENSE](LICENSE) for details. Third-party licenses are documented in [Docs/Licenses/](Docs/Licenses/).
