[简体中文](README.md) | **English**

# StarSketch · In-game Sketchpad for Order of the Sinking Star

![StarSketch in-game sketchpad](docs/images/starsketch-cover.png)

An in-game sketchpad for **[Order of the Sinking Star](https://store.steampowered.com/app/499170/Order_of_the_Sinking_Star/)** on Windows x64 / DirectX 12.

Draw freehand lines and arrows, place stars and numbered markers, and overlay a translucent checkerboard on the game screen. The grid uses game data and the camera's perspective, with temporary camera selection available in 0.2.4. This is an independent project, not an official game component.

**Current version: 0.2.4 test build.** Supports Steam build 26249670 from October 30, 2026, while retaining r67271 support. A usable candidate grid is now adopted automatically and snapping starts immediately; checkerboard tint follows the T toggle. Temporary camera selection and common aspect mappings remain available. This also fixes stale-grid re-adoption after refresh and lost mapping controls after canceling camera selection. The user tested the corresponding feature build successfully; broader machine and level coverage remains pending. See the [0.2.4 release notes (Chinese)](https://github.com/ResidualNight/order-of-the-sinking-star-sketchpad/releases/tag/v0.2.4-test).

This page provides English instructions; the current in-game interface and installer use Chinese. Chinese button labels are included below to help you navigate them.

## Download and installation

**The graphical installer is recommended.** Both ZIP packages contain the same 0.2.4 test-build DLL and game features. No development tools are required. Download one of the player packages below from **Assets** on the [0.2.4 release page](https://github.com/ResidualNight/order-of-the-sinking-star-sketchpad/releases/tag/v0.2.4-test). GitHub's automatically generated **Source code** archives are not player packages.

| Method | Download | What it does |
| --- | --- | --- |
| Graphical installation / update / removal (recommended) | [StarSketch-0.2.4-installer.zip](https://github.com/ResidualNight/order-of-the-sinking-star-sketchpad/releases/download/v0.2.4-test/StarSketch-0.2.4-installer.zip) | Select the game folder, install, check for updates, or uninstall |
| Manual file copy | [StarSketch-0.2.4-r67271-test.zip](https://github.com/ResidualNight/order-of-the-sinking-star-sketchpad/releases/download/v0.2.4-test/StarSketch-0.2.4-r67271-test.zip) | Copy the DLL and assets yourself |

### Option 1: Graphical installer / uninstaller

1. Save your progress and exit the game. **Extract the entire `StarSketch-0.2.4-installer.zip` into its own folder.** Do not run the EXE from inside the ZIP or extract only the EXE.
2. Run `StarSketch-安装卸载.exe`, or the `StarSketch-安装卸载.cmd` file in the same folder.
3. Browse to or paste the game installation folder that directly contains `sinking_star.exe`. In Steam, right-click the game in your Library, then choose **Manage → Browse local files**.
4. Click **检查目录** (Check folder), confirm that the folder is correct, then click **安装 / 更新** (Install / Update). Wait for the success message.
5. Launch the game normally through Steam. Press **E** to open the sketchpad, or use your saved binding if you have changed it.

The installer automatically checks this project's GitHub releases when opened. You can also click **检查插件更新** (Check for plugin updates). When a newer version is found, it asks whether to update. Choosing Yes downloads, verifies, and installs the update; choosing No leaves the installation unchanged. Test releases are included. The online updater does not install the same version or downgrade.

If online updating fails, use the link at the bottom of the installer to open the GitHub releases page, download the latest complete installer ZIP, and follow the steps above. Network failures do not prevent installing or uninstalling with an already downloaded package.

The game folder keeps a management entry at `StarSketch-Manager/StarSketch-安装卸载.cmd`. It can check for updates or uninstall even after the original downloaded bundle has been deleted. The original 0.1.2 manager must first be updated using a newer complete installer before it can update online. The tool uses installation records and file hashes to manage its own files, preserving the game, saves, and unrelated files. It refuses to replace files while the game is running. Known manual installations of 0.1.1 and 0.1.2 can also be updated or removed. See the [installer guide (Chinese)](docs/安装工具说明.md).

**Existing 0.1.2 users:** Select your existing game folder in the newer complete installer and click **检查目录** (Check folder). Both manual and installer-based installations can be upgraded directly with **安装 / 更新** (Install / Update), without uninstalling first. You can also choose **卸载草稿纸** (Uninstall sketchpad). The original 0.1.2 manager has no online-update feature and needs this full installer update once. The game and saves are preserved; unknown or modified files are not forcibly overwritten or deleted.

### Option 2: Manual file copy

1. Save your progress, exit the game, and extract `StarSketch-0.2.4-r67271-test.zip`.
2. Copy **`dxgi.dll` and the entire `StarSketch-assets/` folder** into the folder containing `sinking_star.exe`. Do not put them inside an extra nested extraction folder.
3. Keep the included instructions and `licenses/` folder. Launch the game through Steam and press **E** to open the sketchpad.

The files should be arranged like this:

```text
Game installation folder/
├─ sinking_star.exe
├─ dxgi.dll
└─ StarSketch-assets/
   └─ NotoSerifSC.ttf
```

For manual updates, exit the game before replacing StarSketch's DLL and assets. To uninstall manually, exit the game and remove StarSketch's `dxgi.dll` and the original font inside `StarSketch-assets/`. Preserve any files you added yourself; other instructions, licenses, and game files do not need to be deleted. This method does not require running an installation script.

Always exit the game before installing, updating, or uninstalling with either method. If another mod already provides a `dxgi.dll` in the game folder, resolve compatibility before copying files over it. The graphical tool stops when it encounters an unknown DXGI proxy, modified installed files, or linked paths. It does not close the game for you.

## Controls

Usable candidate grids are adopted automatically. Use **网格精调** (Grid fine tuning) to choose stretch, centered letterboxing or centered cropping; changes apply immediately. Scene cameras also let you select the actual game-image aspect, while the original method uses the internal aspect it reads. For a centered 2560×1440 image inside a 2560×1600 window, select 16:9 and **居中黑边** (Centered letterboxing). Black bars are not detected automatically. Changing aspect or fit rebuilds the grid and resets lower-level fine tuning, so select the mapping before adjusting spacing. Mapping and geometry remain session-only.

These are the default bindings for version 0.2.4. The former numbered-star tool is now split into unnumbered stars (**P**) and numbered markers (**N**). Except for entering and leaving drawing mode, tool shortcuts work only while drawing mode is active.

| Key | Action |
| --- | --- |
| E | Enter / leave drawing mode |
| B | Freehand pen; drag with the left mouse button |
| A | Arrow; drag with the left mouse button to set its endpoints |
| P | Unnumbered star; left-click to place |
| N | Numbered marker with bold white serif digits and a black outline; drag to place consecutive numbers when snapping is confirmed |
| N+C | Restart subsequent numbering at 1 while preserving existing marks; undoable |
| Hold right mouse button | Partially erase strokes; erase arrows, stars, and numbered markers as whole marks |
| G | Show the grid and snap arrows, stars, and numbered markers to cell centers |
| T | Toggle the full-screen translucent checkerboard |
| Shift+G | Temporary camera picker: preview candidates, adjust flips/height, then apply for this localization |
| C | Clear marks while keeping the grid and checkerboard; undoable |
| Ctrl+Z / Ctrl+Y | Undo / redo |
| Esc | Cancel the current stroke or camera selection, or return to the game |

The main panel contains the tools, shared color swatches, grid detection, and temporary camera selection. Sizes, widths, opacity, and custom colors are under **键位与设置** (Key bindings & settings). The **网格精调** (Fine-tune grid) section appears on the main panel only while **网格与吸附** (Grid & snapping) is enabled. No F1–F12 keys are assigned by default. Key bindings & settings lets you rebind entering/leaving drawing mode, tools, camera selection, clear, undo, redo, and hold-to-erase. Single keys, Ctrl/Alt/Shift combinations, F1–F12, and two ordinary-key chords are supported. A single-key binding is confirmed when the key is released. Esc remains reserved for cancellation and closing; conflicting bindings do not overwrite existing ones. Erase can use a keyboard key or the right/middle mouse button.

The pen, arrows, and stars share three base colors—red, white, and blue—and two custom swatches. Edit the custom swatches in Key bindings & settings using RGB, HEX, or the color picker. Numbers increase from 1; deleting a number does not renumber the others. Clearing resets the sequence, and undo/redo restores it. With grid snapping enabled and confirmed, hold the left mouse button and drag to place numbers continuously. Fast movement fills crossed cells, and each drag is one undo step. Hold N+C together to restart subsequent numbering at 1 without changing existing marks; either press order works. The action supports rebinding, undo, and redo. To prevent accidental clearing, the default single-key N and C actions run on release. Color changes affect only later marks. Key bindings, colors, sizes, widths, opacity, and other preferences survive game restarts.

Key bindings & settings also provides **切换关卡自动清除标注** (Automatically clear marks when changing levels). It is off by default, and your choice is saved. When enabled, a reliably confirmed level transition clears marks and restarts numbering at 1; this is undoable. Loading, failed reads, an unchanged level identity with the same dimensions, toggling the sketchpad, and resizing do not trigger a clear.

Existing marks remain fixed to the screen and stay visible after leaving drawing mode. Drawings are not saved after the game exits. The Chinese input method is disabled only for the game window; system keyboard layouts and other applications are unchanged.

## Grid detection and supported scope

Version 0.2.4 shows a gray [SS-GRID-001] compatibility warning and continues localization and level monitoring on unverified or newer builds; runtime layouts and camera checks must still pass. It reads the logical grid and available character height, using either a scene pose or a fresh camera frame from the original method to build a perspective grid. The scene source does not require stack samples; usable candidates delivered by automatic localization are adopted immediately. Opening the sketchpad with E refreshes localization, even with the grid and checkerboard hidden; an automatically or manually adopted session grid is retained. A render/output size change schedules localization after the size has been stable for 250 ms. Repeated resizing cancels the previous pending request and uses the latest size. While localization is pending or fails, snapping and checkerboard rendering from the old automatic grid are suspended. The grid otherwise stays frozen during ordinary drawing.

Shift+G opens the temporary camera picker. Preview candidates, adjust flips or height, and click **本次采用** (Apply for this session) to use the displayed grid immediately. Closing and reopening drawing mode keeps it; canceling a later selection restores it. Explicit relocalization, a confirmed level change, or a render/output resize clears the old grid. Unconfirmed level data pauses snapping until the same level is confirmed again. Three-point and four-point calibration have been removed; cross-level camera learning is not implemented. See the [camera-picker guide (Chinese)](docs/临时相机选择说明.md).

The original data-driven grid was verified in the `torc_blocked` level on r67271 / Steam Build 26006400 at 3840×2160. Refreshing on opening and after stable resizing was verified in the normal overworld view while switching between 3840×2160 and 3200×1800. Cross-level automatic clearing in 0.2.0 needs broader in-game coverage. Other levels, characters on multiple height layers, resolutions, and graphics-mod combinations have not been comprehensively verified. Camera-orientation checks pass numerical regressions, but the previously reported live flip has not been reproduced in-game. See the [verification record (Chinese)](docs/验证记录.md).

When characters occupy different height layers, the default is the layer of the character projected closest to the screen center. You can override the height in Fine-tune grid or restore the character layer. Invalid data suspends snapping from the previous automatic result; use temporary camera selection when valid candidates are available. The overworld Tab/Q map view is outside the supported scope.

## Building from source

Since 0.1.3, errors carry stable identifiers. For example, `[SS-GRID-006]` means waiting for a fresh camera frame timed out, and `[SS-GRID-011]` indicates that an abnormal camera-orientation candidate was found. Version 0.2.4 can preview a fresh frame that passes every other check: use **上下翻转** (Flip vertically), **左右翻转** (Flip horizontally), then **确认并记住** (Confirm and remember) under Fine-tune grid. Snapping and tinting stay paused until confirmed. Direction choices persist with preferences; existing marks stay in place. When reporting a problem, include the complete identifier, triggering action, level, and resolution. See the [error-code reference (Chinese)](docs/错误代码.md).

You need Windows x64, PowerShell 5.1+, and a network connection for the initial compiler download. Dependency sources and the font are included in the repository; the toolchain is excluded from Git history.

```powershell
.\setup.ps1
.\build.ps1
.\package.ps1
# Optional: wrap the player ZIP from the previous step in the graphical installer
.\package-installer.ps1
# Installer regressions: operate only on isolated fixtures under work/
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\installer_tests.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\update_tests.ps1
```

To use an existing LLVM-MinGW 20251021 toolchain:

```powershell
.\build.ps1 -ToolchainPath 'C:\path\to\llvm-mingw'
```

`build.ps1` builds the DLL, core tests, camera-stack tests, update-notice tests, UI lifecycle tests, and the legacy image diagnostic, then runs all four test suites. Outputs go into `build/`. `package.ps1` creates a ZIP in `dist/` containing only player runtime files, instructions, and licenses. The default build does not launch the game or replace an installed DLL.

## Project layout

| Directory | Contents |
| --- | --- |
| `src/` | DXGI/DX12 integration, marks, UI, and read-only grid localization from game data |
| `tests/` | Core, camera-stack, update-notice, UI lifecycle, and installer regressions; numerical camera samples and diagnostic sources |
| `installer/` | Graphical window, install/uninstall module, and native launcher sources |
| `docs/` | [Feature specification](docs/功能规格.md), [grid implementation](docs/网格数据方案评估.md), [verification record](docs/验证记录.md), and [change history](docs/更新记录.md) (Chinese) |
| `assets/` | Noto Serif SC font and license |
| `third_party/` | Required ImGui sources and provenance notes |

Local authentication files, toolchains, build outputs, game screenshots, extracted game data, and research logs are excluded from the repository. Player binaries are distributed through Releases rather than source history.

## License

Project code is licensed under [MIT](LICENSE). Third-party components retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for sources and scope. The repository does not include game executables or level packages. The README cover was edited from official promotional artwork; the game artwork belongs to its respective rights holders and is not covered by this project's MIT license.
