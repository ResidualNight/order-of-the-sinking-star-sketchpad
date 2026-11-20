param([string]$ToolchainPath,[string]$GameExecutable)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if(!$ToolchainPath){$ToolchainPath=Join-Path $root 'tools\llvm-mingw-20251021-ucrt-x86_64'}
$compiler=Join-Path $ToolchainPath 'bin\clang++.exe'
if(!(Test-Path -LiteralPath $compiler)){throw 'Compiler missing. Supply -ToolchainPath.'}
$out=Join-Path $root 'build\controller-probe'
$fixture=Join-Path $out 'fixtures'
[IO.Directory]::CreateDirectory($fixture)|Out-Null
# FARPROC erases the Win32 export signature; wrappers always invoke the exact typed pointer.
$common=@('-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-cast-function-type-mismatch','-static','-DNOMINMAX',('-I'+(Join-Path $root 'src')))
& $compiler @common (Join-Path $root 'tests\gamepad_capture_tests.cpp') -o (Join-Path $out 'gamepad_capture_tests.exe')
if($LASTEXITCODE){throw 'Capture tests compilation failed'}
& $compiler @common -shared (Join-Path $root 'tests\gamepad_fixture.cpp') -o (Join-Path $fixture 'xinput1_3.dll') ('-Wl,--out-implib,'+(Join-Path $fixture 'libfixture_xinput.a'))
if($LASTEXITCODE){throw 'Fixture compilation failed'}
$sources=@((Join-Path $root 'tests\gamepad_bridge_tests.cpp'),(Join-Path $root 'src\gamepad_bridge.cpp'))
& $compiler @common @sources -o (Join-Path $out 'gamepad_bridge_tests.exe')
if($LASTEXITCODE){throw 'Dynamic bridge tests compilation failed'}
& $compiler @common -DPROBE_DIRECT_IMPORT @sources ('-L'+$fixture) -lfixture_xinput -o (Join-Path $fixture 'gamepad_direct_tests.exe')
if($LASTEXITCODE){throw 'Direct bridge tests compilation failed'}
& $compiler @common -municode (Join-Path $root 'tests\gamepad_probe.cpp') (Join-Path $root 'src\gamepad_bridge.cpp') -o (Join-Path $out 'StarSketch-controller-probe.exe') -luser32 -lgdi32
if($LASTEXITCODE){throw 'Probe compilation failed'}
& $compiler @common -municode (Join-Path $root 'tests\gamepad_cache_tests.cpp') (Join-Path $root 'src\gamepad_bridge.cpp') -o (Join-Path $out 'gamepad_cache_tests.exe')
if($LASTEXITCODE){throw 'Cached bridge tests compilation failed'}
$cacheGame=if($GameExecutable){$GameExecutable}else{'-'}
foreach($mode in @('--old-profile','--clean-remove','--new-profile','--new-profile-clean','--steam-26787118','--steam-26787118-clean','--adaptive','--adaptive-changed')){
 & (Join-Path $out 'gamepad_cache_tests.exe') $cacheGame $mode
 if($LASTEXITCODE){throw "Cached bridge tests failed: $mode"}
 $cacheGame='-'
}
& $compiler @common (Join-Path $root 'tests\gamepad_entry_tests.cpp') -o (Join-Path $out 'gamepad_entry_tests.exe')
if($LASTEXITCODE){throw 'Trigger entry compile failed'}
& (Join-Path $out 'gamepad_entry_tests.exe')
if($LASTEXITCODE){throw 'Trigger entry tests failed'}
& $compiler @common (Join-Path $root 'tests\gamepad_entry_bridge_tests.cpp') (Join-Path $root 'src\gamepad_bridge.cpp') -o (Join-Path $out 'gamepad_entry_bridge_tests.exe')
if($LASTEXITCODE){throw 'Trigger bridge compile failed'}
& (Join-Path $out 'gamepad_entry_bridge_tests.exe') (Join-Path $fixture 'xinput1_3.dll')
if($LASTEXITCODE){throw 'Trigger bridge tests failed'}
$imgui=Join-Path $root 'third_party\imgui-1.91.9b'
$uiSources=@('tests\controller_ui_tests.cpp','src\controller_ui.cpp','src\ui.cpp','src\preferences.cpp','src\feedback.cpp','src\scene_camera.cpp','third_party\imgui-1.91.9b\imgui.cpp','third_party\imgui-1.91.9b\imgui_draw.cpp','third_party\imgui-1.91.9b\imgui_tables.cpp','third_party\imgui-1.91.9b\imgui_widgets.cpp') | ForEach-Object {Join-Path $root $_}
& $compiler -std=c++17 -O2 -static -DNOMINMAX ('-I'+(Join-Path $root 'src')) ('-I'+$imgui) @uiSources -o (Join-Path $out 'controller_ui_tests.exe') -luser32 -limm32 -lshell32 -lole32
if($LASTEXITCODE){throw 'Controller UI compile failed'}
& (Join-Path $out 'controller_ui_tests.exe')
if($LASTEXITCODE){throw 'Controller UI tests failed'}
& (Join-Path $out 'gamepad_capture_tests.exe')
if($LASTEXITCODE){throw 'Capture tests failed'}
& (Join-Path $out 'gamepad_bridge_tests.exe') (Join-Path $fixture 'xinput1_3.dll')
if($LASTEXITCODE){throw 'Dynamic bridge tests failed'}
& (Join-Path $fixture 'gamepad_direct_tests.exe') (Join-Path $fixture 'xinput1_3.dll')
if($LASTEXITCODE){throw 'Direct bridge tests failed'}
& (Join-Path $out 'gamepad_bridge_tests.exe') (Join-Path $fixture 'xinput1_3.dll') --foreign-hook
if($LASTEXITCODE){throw 'Hook ownership tests failed'}
& (Join-Path $out 'gamepad_bridge_tests.exe') (Join-Path $fixture 'xinput1_3.dll') --capacity
if($LASTEXITCODE){throw 'Endpoint capacity tests failed'}
& (Join-Path $out 'StarSketch-controller-probe.exe') --ui-selftest
if($LASTEXITCODE){throw 'Probe UI tests failed'}
$sessionSources=@('tests\controller_session_tests.cpp','src\controller_runtime.cpp','src\controller_probe.cpp','src\gamepad_bridge.cpp','src\controller_ui.cpp','src\ui.cpp','src\feedback.cpp','src\scene_camera.cpp','third_party\imgui-1.91.9b\imgui.cpp','third_party\imgui-1.91.9b\imgui_draw.cpp','third_party\imgui-1.91.9b\imgui_tables.cpp','third_party\imgui-1.91.9b\imgui_widgets.cpp') | ForEach-Object {Join-Path $root $_}
& $compiler -std=c++17 -O2 -static -DNOMINMAX ('-I'+(Join-Path $root 'src')) ('-I'+$imgui) @sessionSources -o (Join-Path $out 'controller_session_tests.exe') -luser32 -limm32 -lshell32 -lole32
if($LASTEXITCODE){throw 'Controller session compile failed'}
& (Join-Path $out 'controller_session_tests.exe') (Join-Path $fixture 'xinput1_3.dll')
if($LASTEXITCODE){throw 'Controller session tests failed'}
Write-Host 'Controller bridge, entry, UI and session suites passed. No game DLL was built or installed by this script.'
