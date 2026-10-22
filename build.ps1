param([switch]$Debug,[string]$ToolchainPath)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if(!$ToolchainPath){$ToolchainPath=Join-Path $root 'tools\llvm-mingw-20251021-ucrt-x86_64'}
$compiler=Join-Path $ToolchainPath 'bin\clang++.exe'
if(!(Test-Path -LiteralPath $compiler)){throw 'Compiler missing. Run ./setup.ps1 first, or pass -ToolchainPath.'}
$imgui=Join-Path $root 'third_party\imgui-1.91.9b'
$out=Join-Path $root 'build'
[IO.Directory]::CreateDirectory($out)|Out-Null
$common=@('-std=c++17','-O2','-static','-fms-extensions','-DUNICODE','-D_UNICODE','-DNOMINMAX',('-I'+(Join-Path $root 'src')),('-I'+$imgui))
$sources=@('src\native.cpp','src\ui.cpp','src\game_grid.cpp','third_party\imgui-1.91.9b\imgui.cpp','third_party\imgui-1.91.9b\imgui_draw.cpp','third_party\imgui-1.91.9b\imgui_tables.cpp','third_party\imgui-1.91.9b\imgui_widgets.cpp','third_party\imgui-1.91.9b\backends\imgui_impl_dx12.cpp','third_party\imgui-1.91.9b\backends\imgui_impl_win32.cpp')|ForEach-Object{Join-Path $root $_}
& $compiler @common -DNDEBUG -shared @sources (Join-Path $root 'src\dxgi.def') -o (Join-Path $out 'dxgi.dll') -ld3d12 -ld3dcompiler -ldxguid -luser32 -lgdi32 -ldwmapi -limm32 -lshcore -lbcrypt
if($LASTEXITCODE -ne 0){throw 'Plugin compilation failed'}
& $compiler @common (Join-Path $root 'tests\core_tests.cpp') -o (Join-Path $out 'core_tests.exe')
if($LASTEXITCODE -ne 0){throw 'Core tests compilation failed'}
& $compiler @common -municode (Join-Path $root 'tests\grid_diagnostic.cpp') -o (Join-Path $out 'grid_diagnostic.exe') -lole32 -lwindowscodecs -luuid
if($LASTEXITCODE -ne 0){throw 'Grid diagnostic compilation failed'}
& (Join-Path $out 'core_tests.exe')
if($LASTEXITCODE -ne 0){throw 'Core tests failed'}
$runtimeAssets=Join-Path $out 'StarSketch-assets'
[IO.Directory]::CreateDirectory($runtimeAssets)|Out-Null
Copy-Item -LiteralPath (Join-Path $root 'assets\NotoSerifSC.ttf') -Destination (Join-Path $runtimeAssets 'NotoSerifSC.ttf')
