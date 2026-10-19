# DarkFire Engine — Code Style & Conventions

## 1. Toolchain & Platform
* **Platform:** Windows 10/11 x64 only.
* **Dialect:** C++20.
* **Compiler:** Visual Studio 2026 (`vs2026`, toolset `v145`) or Visual Studio 2022 (`vs2022`, toolset `v143`).
* **Warnings as Errors:** `/W4 /WX` on all first-party code. Third-party code compiles with `/W0`.

## 2. Formatting & Layout
* **Braces:** Allman style (opening brace on its own newline).
* **Indentation:** Tabs (width 4).
* **Pointer Alignment:** Left aligned (`T* p`, not `T *p` or `T*p`).
* **Line Length:** 120-column maximum.
* **Line Endings:** CRLF.
* **Encoding:** UTF-8 without BOM.

## 3. Naming Conventions
* **Member Variables:** Prefixed with `m_` (e.g. `m_name`, `m_capacity`).
* **Classes & Structs:** PascalCase (e.g. `FrameArena`, `VirtualFileSystem`).
* **Functions & Methods:** camelCase (e.g. `getElapsedSeconds()`, `isOk()`).
* **Constants & Enums:** UPPER_SNAKE_CASE or PascalCase for strongly-typed enum values.
* **Types:** Row vectors, row-major matrices, left-handed coordinate system (+Y up, +Z forward, +X right).

## 4. Architecture & Error Handling
* **No C++ Exceptions:** Use `Core::Result<T, E>` for fallible operations.
* **Assertions:**
  * `DF_ASSERT(expr, msg)` for invariant checking in Debug builds.
  * `DF_VERIFY(expr, msg)` for recoverable checks (logs error in Release, breaks in Debug).
* **Memory & Ownership:**
  * Default ownership is `std::unique_ptr` or value types.
  * No owning raw pointers.
  * No `std::shared_ptr` for entities or gameplay assets.
  * Per-frame scratch memory uses `Core::FrameArena` (16 MB monotonic buffer).
* **Strings & Paths:**
  * UTF-8 `std::string` and `std::string_view` everywhere internally.
  * `std::filesystem::path` for filesystem operations. Convert to `std::wstring` only at the immediate Win32 / DirectX API boundary.
