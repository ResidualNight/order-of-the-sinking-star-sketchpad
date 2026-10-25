# StarSketch project guide

StarSketch is a Windows x64 / DirectX 12 in-game sketchpad for Order of the Sinking Star.

## Build and test

- Run `./setup.ps1` once to fetch the pinned, checksum-verified LLVM-MinGW toolchain.
- Run `./build.ps1` to compile the DLL, core/UI lifecycle tests and image diagnostic, then execute both test suites.
- An existing toolchain may be supplied with `./build.ps1 -ToolchainPath <directory>`.
- Run `./package.ps1` after building to create a player ZIP in ignored `dist/`.
- Run `./package-installer.ps1` to wrap a checksum-verified player ZIP in the graphical installer; requires Windows PowerShell 5.1 / .NET Framework.
- Run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./tests/installer_tests.ps1` for isolated installer regressions. Fixtures stay in ignored `work/`; never test uninstall against a real game directory.
- Run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./tests/update_tests.ps1` for online-update regressions with mocked downloads and isolated game fixtures.

## Stack and layout

- C++17, independent DXGI proxy, DX12 renderer, Dear ImGui 1.91.9b, Noto Serif SC.
- `src/` runtime; `tests/` numeric and interaction models; `docs/` current behavior and verification.
- `assets/` and selected `third_party/` files are licensed, versioned build inputs.
- `.local/`, `work/`, `tools/`, `build/`, `dist/` and `references/` stay local and ignored.

## Boundaries

- Preserve screen-fixed marks and session-only drawing/history/calibration. User-authorized preferences and key bindings persist in %LOCALAPPDATA%/StarSketch/settings.ini; never serialize marks or calibrated grid geometry.
- Read game data only after the exact executable hash and runtime layouts validate.
- Keep manual calibration; invalid data must not silently enable old automatic snapping.
- Never force-stop a running game. Update an installed DLL only after a graceful exit.
- Never commit SSH keys, auth config, tokens, game executables/packages, extracted data or raw logs.
- Do not overwrite another mod's DXGI proxy. Verify our installation manifest before replacing ours.
- Keep third-party notices and font licenses when packaging.
- Prefix player-facing errors with a stable `[SS-<area>-<number>]` identifier and use the same identifier in related logs. Register new codes in `docs/错误代码.md`; never renumber or reuse existing codes for a different cause. Keep error identifiers in installer failure/rollback messages too.
- Installer mutations must stay inside the explicitly selected game folder and the exact owned-file allowlist. Preserve unknown/modified files; reject reparse points. Never recursively delete the game folder.
- Keep transactional rollback and ownership hashes; installed-manager uninstall must work after the original downloaded bundle is removed.

## Current state

0.1.2 test build: refresh on every opening and after a 250ms stable resize; original data grid was verified on r67271, torc_blocked, 3840x2160.
Publish GitHub updates only when requested by the user.
0.1.2 opening/resize triggers verified in the normal overworld view at 3840x2160 <-> 3200x1800; see `docs/验证记录.md` for limits.
Broader level, resolution and multi-layer coverage is still pending.

0.1.3 adds stable error identifiers, rejects reflected/upside-down automatic camera candidates (SS-GRID-011), and adds opt-in one-click online updates after background GitHub checks in both installer and installed manager. Keep release ZIP/receipt names and ownership hashes compatible with the updater. Regression evidence is in `docs/验证记录.md`; the reported live flip has not been reproduced.

0.1.4 fixes backup retention when rollback and directory cleanup both fail, surfaces cleanup warnings in the GUI, and unwraps background exceptions to retain their original error identifiers. Game behavior is unchanged from 0.1.3; only the runtime version marker changes. Installer regressions include secondary cleanup failure and background self-update.

The graphical installer and manual-copy player ZIP are both distributed with the v0.2.0-test release and contain the same 0.2.0 runtime. Earlier releases remain available. Online updates require the 0.1.3 manager or later; old 0.1.2 managers first need a full installer update.

0.2.0 includes in-game persistent key rebinding, three base/two custom shared swatches, unnumbered colored stars with antique gold rims and a separate default-N numbered tool with bold outlined serif digits. GitHub publication was explicitly authorized by the user on 2026-10-25.

The 0.2.0 release tag is `v0.2.0-test`; final reviewed ZIPs, both update-compatible JSON receipts, checksums and local release notes are in ignored `dist/release-0.2.0/`. Keep release names compatible with the updater, and never announce availability before the assets are uploaded. See docs/验证记录.md for final hashes and check results.

For original 0.1.2 migration regressions, run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File ./tests/legacy_012_tests.ps1 -InstallerArchive <new installer ZIP>` after packaging. It checksum-pins the original 0.1.2 manual/installer ZIPs in dist/ and operates only on work/ fixtures; never run compatibility uninstall tests against a real game.

0.2.0 also supports grouped left-drag numbering on confirmed snapping grids and a remappable default N+C restart that preserves existing marks. Two ordinary-key chords consume their component actions; participating single-key actions fire on release. Keep previously installed local 0.1.5 DLL hashes allowlisted when packaging the 0.2.0 replacement.

The in-game entry is now “键位与设置”. Keep infrequent controls there, except grid fine tuning stays on the main panel and appears only while grid snapping is enabled. Auto-clear on a confirmed level transition starts OFF, and the user choice persists with other preferences. Level monitoring requires the exact executable hash, validated Settings fields and a stable identity; invalid/loading samples preserve marks. Clearing on transition is undoable.
