# StarSketch project guide

StarSketch is a Windows x64 / DirectX 12 in-game sketchpad for Order of the Sinking Star.

## Build and test

- Run `./setup.ps1` once to fetch the pinned, checksum-verified LLVM-MinGW toolchain.
- Run `./build.ps1` to compile the DLL, core tests and image diagnostic, then execute core tests.
- An existing toolchain may be supplied with `./build.ps1 -ToolchainPath <directory>`.
- Run `./package.ps1` after building to create a player ZIP in ignored `dist/`.

## Stack and layout

- C++17, independent DXGI proxy, DX12 renderer, Dear ImGui 1.91.9b, Noto Serif SC.
- `src/` runtime; `tests/` numeric and interaction models; `docs/` current behavior and verification.
- `assets/` and selected `third_party/` files are licensed, versioned build inputs.
- `.local/`, `work/`, `tools/`, `build/`, `dist/` and `references/` stay local and ignored.

## Boundaries

- Preserve screen-fixed marks and session-only state; do not add persistence by accident.
- Read game data only after the exact executable hash and runtime layouts validate.
- Keep manual calibration; invalid data must not silently enable old automatic snapping.
- Never force-stop a running game. Update an installed DLL only after a graceful exit.
- Never commit SSH keys, auth config, tokens, game executables/packages, extracted data or raw logs.
- Do not overwrite another mod's DXGI proxy. Verify our installation manifest before replacing ours.
- Keep third-party notices and font licenses when packaging.

## Current state

0.1.1 test build: data-driven perspective grid verified on r67271, torc_blocked, 3840x2160.
Broader level, resolution and multi-layer coverage is still pending; see `docs/验证记录.md`.
