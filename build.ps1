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
$sources=@('src\native.cpp','src\feedback.cpp','src\ui.cpp','src\game_grid.cpp','src\scene_camera.cpp','src\game_grid_worker.cpp','src\update_check.cpp','src\preferences.cpp','third_party\imgui-1.91.9b\imgui.cpp','third_party\imgui-1.91.9b\imgui_draw.cpp','third_party\imgui-1.91.9b\imgui_tables.cpp','third_party\imgui-1.91.9b\imgui_widgets.cpp','third_party\imgui-1.91.9b\backends\imgui_impl_dx12.cpp','third_party\imgui-1.91.9b\backends\imgui_impl_win32.cpp')|ForEach-Object{Join-Path $root $_}
& $compiler @common -DNDEBUG -shared @sources (Join-Path $root 'src\dxgi.def') -o (Join-Path $out 'dxgi.dll') -ld3d12 -ld3dcompiler -ldxguid -luser32 -lgdi32 -ldwmapi -limm32 -lshcore -lbcrypt -lwinhttp -lxmllite -lshlwapi -lole32 -lshell32
if($LASTEXITCODE -ne 0){throw 'Plugin compilation failed'}
& $compiler @common (Join-Path $root 'tests\core_tests.cpp') (Join-Path $root 'src\scene_camera.cpp') -o (Join-Path $out 'core_tests.exe')
if($LASTEXITCODE -ne 0){throw 'Core tests compilation failed'}
& $compiler @common (Join-Path $root 'tests\camera_scan_tests.cpp') (Join-Path $root 'tests\camera_worker_tests.cpp') (Join-Path $root 'tests\scene_camera_tests.cpp') (Join-Path $root 'src\scene_camera.cpp') (Join-Path $root 'src\game_grid.cpp') (Join-Path $root 'src\game_grid_worker.cpp') -o (Join-Path $out 'camera_scan_tests.exe') -lbcrypt
if($LASTEXITCODE -ne 0){throw 'Camera scan tests compilation failed'}
& $compiler @common (Join-Path $root 'tests\update_notice_tests.cpp') (Join-Path $root 'src\update_check.cpp') -o (Join-Path $out 'update_notice_tests.exe') -lwinhttp -lxmllite -lshlwapi -lole32 -luuid
if($LASTEXITCODE -ne 0){throw 'Update notice tests compilation failed'}
$uiTests=@('tests\ui_lifecycle_tests.cpp','tests\preferences_tests.cpp','src\feedback.cpp','src\ui.cpp','src\scene_camera.cpp','src\preferences.cpp','third_party\imgui-1.91.9b\imgui.cpp','third_party\imgui-1.91.9b\imgui_draw.cpp','third_party\imgui-1.91.9b\imgui_tables.cpp','third_party\imgui-1.91.9b\imgui_widgets.cpp')|ForEach-Object{Join-Path $root $_}
& $compiler @common @uiTests -o (Join-Path $out 'ui_lifecycle_tests.exe') -luser32 -limm32 -lshell32 -lole32
if($LASTEXITCODE -ne 0){throw 'UI lifecycle tests compilation failed'}
& $compiler @common -municode (Join-Path $root 'tests\grid_diagnostic.cpp') -o (Join-Path $out 'grid_diagnostic.exe') -lole32 -lwindowscodecs -luuid
if($LASTEXITCODE -ne 0){throw 'Grid diagnostic compilation failed'}
& (Join-Path $out 'core_tests.exe')
if($LASTEXITCODE -ne 0){throw 'Core tests failed'}
& (Join-Path $out 'camera_scan_tests.exe')
if($LASTEXITCODE -ne 0){throw 'Camera scan tests failed'}
& (Join-Path $out 'update_notice_tests.exe')
if($LASTEXITCODE -ne 0){throw 'Update notice tests failed'}
& (Join-Path $out 'ui_lifecycle_tests.exe')
if($LASTEXITCODE -ne 0){throw 'UI lifecycle tests failed'}
$runtimeAssets=Join-Path $out 'StarSketch-assets'
[IO.Directory]::CreateDirectory($runtimeAssets)|Out-Null
Copy-Item -LiteralPath (Join-Path $root 'assets\NotoSerifSC.ttf') -Destination (Join-Path $runtimeAssets 'NotoSerifSC.ttf')
