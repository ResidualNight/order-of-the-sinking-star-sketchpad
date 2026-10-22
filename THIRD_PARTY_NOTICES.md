# Third-party notices

The project license does not replace these upstream licenses. Game executables, packages, extracted art and raw screenshots are not distributed with this repository.

| Component | Version / source | License and included files |
| --- | --- | --- |
| Dear ImGui | [v1.91.9b](https://github.com/ocornut/imgui/tree/v1.91.9b), commit `f5befd2d29e66809cd1110a152e375a7f1981f06` | [MIT](third_party/imgui-1.91.9b/LICENSE.txt); runtime headers, core and Win32/DX12 backend sources are vendored. The experimental DX12 canvas is adapted from the upstream example. |
| Noto Serif SC | [Google Fonts](https://github.com/google/fonts/blob/main/ofl/notoserifsc/NotoSerifSC%5Bwght%5D.ttf), upstream Git blob `eab063faf229160a52d3760f5555150e4eb9e5bf` | [SIL OFL 1.1](assets/OFL-NotoSerifSC.txt); unchanged font bytes are stored as `assets/NotoSerifSC.ttf`. |
| LLVM-MinGW | [20251021 / LLVM 21.1.4](https://github.com/mstorsjo/llvm-mingw/releases/tag/20251021) | Downloaded build toolchain, not vendored into Git. Player packages retain LLVM and applicable MinGW runtime notices from the toolchain. |

The old offline format research used [joric/ootss simp_tool.py](https://github.com/joric/ootss/blob/main/scripts/simp_tool.py) under its [Unlicense](https://github.com/joric/ootss/blob/main/LICENSE). That research tool and extracted game data are local-only and are not linked into the runtime.

The runtime UI and star marker are drawn in code. No other sketchpad mod's source or assets are included.

The README cover (`docs/images/starsketch-cover.png`) is an AI-assisted edit of official [Order of the Sinking Star promotional artwork](https://store.steampowered.com/app/499170/Order_of_the_Sinking_Star/), used to illustrate this independent tool. Rights in the underlying game artwork remain with their respective owners; the project MIT license does not apply to that artwork.
