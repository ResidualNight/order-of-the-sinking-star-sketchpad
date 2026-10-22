# StarSketch project guide

StarSketch is a Windows x64 / DirectX 12 in-game sketchpad for Order of the Sinking Star.

## Build and test

- Run `./setup.ps1` once to fetch the pinned, checksum-verified LLVM-MinGW toolchain.
- Run `./build.ps1` to compile the DLL, core/UI lifecycle tests and image diagnostic, then execute both test suites.
- An existing toolchain may be supplied with `./build.ps1 -ToolchainPath <directory>`.
- Run `./package.ps1` after building to create a player ZIP in ignored `dist/`.
- Run `./package-installer.ps1` to wrap a checksum-verified player ZIP in the graphical installer; requires Windows PowerShell 5.1 / .NET Framework.
- Run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./tests/installer_tests.ps1` for isolated installer regressions. Fixtures stay in ignored `work/`; never test uninstall against a real game directory.

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
- Installer mutations must stay inside the explicitly selected game folder and the exact owned-file allowlist. Preserve unknown/modified files; reject reparse points. Never recursively delete the game folder.
- Keep transactional rollback and ownership hashes; installed-manager uninstall must work after the original downloaded bundle is removed.

## Current state

0.1.2 test build: refresh on every opening and after a 250ms stable resize; original data grid was verified on r67271, torc_blocked, 3840x2160.
Publish GitHub updates only when requested by the user.
0.1.2 opening/resize triggers verified in the normal overworld view at 3840x2160 <-> 3200x1800; see `docs/验证记录.md` for limits.
Broader level, resolution and multi-layer coverage is still pending.

The graphical installer and manual-copy player ZIP are both distributed with the v0.1.2-test release; both wrap the unchanged 0.1.2 runtime. Installer evidence is recorded in `docs/验证记录.md`.
