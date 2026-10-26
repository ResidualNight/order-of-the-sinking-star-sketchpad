# StarSketch project guide

StarSketch is a Windows x64 / DirectX 12 in-game sketchpad for Order of the Sinking Star.

## Build and test

- Run `./setup.ps1` once to fetch the pinned, checksum-verified LLVM-MinGW toolchain.
- Run `./build.ps1` to compile the DLL, core/camera-stack/update-notice/UI lifecycle tests and image diagnostic, then execute all four test suites.
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
- Check the executable hash as an advisory compatibility signal. Unknown/newer builds show gray SS-GRID-001 but continue localization and level monitoring; guarded reads, runtime layouts and stable level identity must still validate.
- Keep explicit user-confirmed camera recovery; invalid data must not silently enable old automatic snapping. The user replaced point calibration with temporary camera selection.
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

The in-game entry is now “键位与设置”. Keep infrequent controls there, except grid fine tuning stays on the main panel and appears only while grid snapping is enabled. Auto-clear on a confirmed level transition starts OFF, and the user choice persists with other preferences. Level monitoring requires validated Settings fields and a stable identity; the executable hash is advisory, and invalid/loading samples preserve marks. Clearing on transition is undoable.

The 2026-10-26 local 0.2.0 camera fix accepts native or half-resolution scene viewports and scans committed readable stack regions in bounded chunks, preserving cross-chunk frames and guard pages. Camera freshness, orientation, executable fingerprint and manual fallback checks remain required. GRID-006/010/011 logs include per-request scan counters. Remote multi-user SS-GRID-010 reports still require affected-machine verification; synthetic regressions do not establish their common root cause. Do not overwrite the published 0.2.0 release assets with this local build.

The user named this fix 0.2.1. CurrentVersion in src/version.hpp, package defaults and player instructions use 0.2.1; the release tag is v0.2.1-test. In-game update checks run on a module-referenced background worker after UI initialization and every 30 minutes. Read only the official GitHub Atom feed, compare numeric versions including -test releases, and confirm official installer ZIP/receipt availability before showing green text at the very top of the main panel. Network/metadata failures and same/older versions show no update UI. Never download/install runtime updates inside the game. Preserve the published 0.2.0 and prior local camera-fix DLL hashes when packaging replacements.

The later 0.2.1 render-modes revision replaces the incomplete native/half-only filter: r67271 also exposes fixed 720p/1080p/1440p/4K modes. Validate internal viewport mode, size bounds and aspect separately from the output canvas; supersampled 4K can exceed the window size. Keep freshness, inverse/orientation and executable guards. Successful grid logs now include both render= and output= dimensions; startup identifies `render modes fix 20261026`. Keep the first 0.2.1 DLL hash allowlisted as well. User-supplied logs currently examined still run 0.1.5; do not call them a new-DLL live verification.

The 0.2.1 diagnostic revision identifies `camera diagnostics 20261026`. Failure groups retain stable error IDs on every line and share pid/grid_request plus a UTC header. Keep scan/cache/poll/fresh rejection counters, detailed thread/read failures, threshold values and at most six scalar evidence samples per request. Viewport probes are diagnostic-only, capped at 64 deep checks per scan, and must never admit a rejected camera or alter existing validation/freshness rules. Do not log full matrices, raw memory, marks or calibration geometry. Preserve the preceding render-modes DLL hash when packaging this revision.

The later 2026-10-26 user-authorized compatibility revision supersedes the earlier executable hard gates: a hash mismatch or verification failure is an advisory gray SS-GRID-001 warning, separate from localization status. Continue localization and validated level monitoring on unverified/newer builds; retain guarded reads, runtime layout validation, camera checks and stable level confirmation. Success logs distinguish layout=r67271 from verified_build. This does not establish live compatibility with a newer executable.

The user then authorized direction-only fallback in local 0.2.1. Keep the three-state validator and all other camera/layout/freshness/projection checks. Include orientation-only candidates but prefer a normal fresh candidate in the same poll; only usable geometry can reach UI preview. First use of a direction category pauses snapping/tint until explicit confirmation in 网格精调. Reflect the complete screen homography, never just negate grid axes. Persist only confirmed flip choices by raw orientation category in settings.ini; never persist camera/grid geometry, and never apply a reflected category's correction to a normal category. Startup includes `manual grid direction 20261026`; success logs distinguish direction category, choice and confirmation. Three/four-point calibration remains available pending the replacement assessment in docs/手动校准工具评估.md.

The user subsequently required three-point calibration removal and no repeated grid-dimension input. The three-point UI, mode flag and implementation are now removed. Shift+G opens 单格四点校准: select one cell's four corners, drag numbered handles, undo/reselect/cancel, and explicitly adopt the projective preview. No row/column inputs remain in the runtime UI; never enable snapping/tint merely on the fourth click. Keep calibrated geometry session-only. Candidate-camera lists and cross-level feature preferences are assessed in docs/相机列表与跨关卡匹配评估.md but are not implemented; do not claim the current direction preferences identify a camera source.

Four-point selection now opens a modeless, closable 四点校准说明 window below the main panel by default. Closing only hides instructions; 操作说明 reopens them without changing selected points. Constrain/scroll within small viewports and use the side if no usable room remains below. Applying/cancelling, refreshing, level transitions and leaving drawing mode hide it; never persist tip position or calibration geometry.

On 2026-10-26 the user explicitly authorized installing this build locally. The stopped game's managed 0.1.5 installation at E:\Games\steamapps\common\Order of the Sinking Star was transactionally updated to 0.2.1; all 15 owned files were hash-verified and the game EXE stayed unchanged. Installed DLL SHA256 is b4c38ace8aa35a6a0bb018b0406ffb3144ce157dde8cec6114d2345af40e44aa. Final bundle is dist/local-0.2.1-calibration-help-final/. This is installation verification, not live gameplay acceptance; the game was left closed for user testing. Preserve this runtime's ownership hash in future bundles.

The user subsequently replaced four-point calibration with 临时选择相机. Point selection, its draft/drag state and its help window are removed. Keep the existing calibrate preference/action name for key-binding compatibility; Shift+G now starts bounded candidate collection and a modeless picker. Deduplicate request-local views, preview without snapping/tint/mark input, allow temporary flips and height, then revalidate level/manager/output/view plus a fresh frame before 本次采用. Reacquire matching temporary copies if an address was overwritten; never adopt stale snapshots. SS-GRID-012 covers expired choices. Do not persist camera frames/tokens/identity, temporary height/flips, or overwrite existing automatic direction choices. Cross-level learning remains unimplemented. See docs/临时相机选择说明.md for bounds and lifetime.

The temporary-picker revision was installed for the continuing local test after stopped-game and ownership checks. Current installed 0.2.1 DLL SHA256 is f6e36facd9a7c6a886e2fa98c20ee45b6062c035e023bdd3ceaaa5080670c0fe; bundle is dist/local-0.2.1-camera-picker-final/. All 15 owned files and unchanged EXE were verified. Preserve this hash for future owned upgrades. No live gameplay acceptance or GitHub publication is implied.

The user then explicitly changed 本次采用 to manual snapshot confirmation after reporting SS-GRID-012. This supersedes the prior adoption revalidation requirement: copy exactly the usable displayed preview, close the picker and enable its grid/snapping immediately. The backend adoption/new-frame/source-reacquisition path is removed; do not reintroduce it for manual confirmation or block it with 012. Initial candidate capture still validates data/freshness. Keep the confirmed grid pinned in session memory across drawing-panel close/reopen; explicit relocalization/new selection, confirmed level change or resize clears it. An unavailable level sample pauses snapping while retaining the pin; confirmation of the same level restores it. Do not serialize grid geometry or change saved direction/automatic height preferences.

Current local installation is the direct-preview adoption revision: 0.2.1 DLL SHA256 4a9d1d5234c37530f9711bd742de91f885525e824399d2a3da3a305abf8d88c3, bundle dist/local-0.2.1-preview-adopt/. All 15 owned files and unchanged game EXE verified after transactional update; game left closed for testing. Preserve this hash in future owned upgrades.

The subsequent self-check preserves a previously adopted grid while reopening/refreshing the picker; cancel, Esc, close or leaving drawing restores it. Only an explicit new adoption replaces it; relocalization, resize and confirmed level change discard the backup. Level-unavailable samples pause snapping, tint and fine tuning, while G/T preserve the pin. Zero-size frames finish gestures without losing geometry/history. Guard extreme projective coordinates before integer conversion and discard invalid drawing vertices. Viewpoint_Params byte 860 / float slot 215 is an opaque int32, not a float to finite-check; other camera validations remain. Cancel/restore must not log a new adoption. All geometry, backup flags and adoption counters remain session-only.

Current local installation after self-check is 0.2.1 DLL SHA256 f71886e7d4ebc2df4b9ec0bedb047d384d8e1d166fa3621870a6be6b2b96f255, final bundle dist/local-0.2.1-selfcheck-final/. Four C++ suites and the final ZIP's 11 original 0.1.2 compatibility groups passed. Transactional installation verified all 15 owned files and unchanged EXE, with game left closed. The pre-self-check 4a9d1d52 runtime/receipt backup is in work/install-selfcheck-a75b52a59e114134af08ff9d0ee83e3f/previous-install/. Preserve this runtime hash for future owned upgrades; no live gameplay acceptance or GitHub publication is implied.

The user explicitly requested GitHub publication on 2026-10-26. v0.2.1-test is now a public prerelease targeting fbbbc2dd00230219e13cb7e5052300cfe808dc6b, with both player ZIPs, both update receipts and SHA256SUMS.txt in dist/release-0.2.1/. Keep its five assets immutable. Runtime DLL SHA256 is f71886e7d4ebc2df4b9ec0bedb047d384d8e1d166fa3621870a6be6b2b96f255. Source and Chinese-default/English README are pushed. Four C++ suites, 16 installer groups, 15 mocked update groups and 11 original 0.1.2 compatibility groups passed. Real public 0.2.0-to-0.2.1 download/upgrade, all 15 ownership hashes, same-version rejection and standalone uninstall passed in work/ fixtures. Native in-game update-check code also verified real official Atom/assets return 0.2.1 for 0.2.0 and no offer for 0.2.1. No broader live gameplay coverage is implied; see docs/验证记录.md for final evidence.
