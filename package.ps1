[CmdletBinding()]
param(
    [ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')][string]$Version='0.2.4',
    [string]$BinaryPath,
    [string]$ToolchainPath,
    [string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if(!$BinaryPath){$BinaryPath=Join-Path $root 'build\dxgi.dll'}
if(!$ToolchainPath){$ToolchainPath=Join-Path $root 'tools\llvm-mingw-20251021-ucrt-x86_64'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'dist'}
if(!(Test-Path -LiteralPath $BinaryPath)){throw 'DLL missing. Run ./build.ps1 first.'}
$runtimeNotices=Join-Path $ToolchainPath 'x86_64-w64-mingw32\share\mingw32'
$noticeNames=@('COPYING','COPYING.MinGW-w64.txt','COPYING.MinGW-w64-runtime.txt','COPYING.winpthreads.txt')
$required=@((Join-Path $root 'assets\NotoSerifSC.ttf'),(Join-Path $root 'assets\OFL-NotoSerifSC.txt'),(Join-Path $ToolchainPath 'LICENSE.TXT'))
foreach($name in $noticeNames){$required+=Join-Path $runtimeNotices $name}
foreach($file in $required){if(!(Test-Path -LiteralPath $file)){throw ('Required packaging input missing: '+$file)}}
$stage=Join-Path $root ('.local\packages\'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory((Join-Path $stage 'StarSketch-assets')) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $stage 'licenses')) | Out-Null
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
Copy-Item -LiteralPath $BinaryPath -Destination (Join-Path $stage 'dxgi.dll')
Copy-Item -LiteralPath (Join-Path $root 'assets\NotoSerifSC.ttf') -Destination (Join-Path $stage 'StarSketch-assets\NotoSerifSC.ttf')
Copy-Item -LiteralPath (Join-Path $root 'assets\OFL-NotoSerifSC.txt') -Destination (Join-Path $stage 'licenses\OFL-NotoSerifSC.txt')
Copy-Item -LiteralPath (Join-Path $root 'third_party\imgui-1.91.9b\LICENSE.txt') -Destination (Join-Path $stage 'licenses\Dear-ImGui-MIT.txt')
Copy-Item -LiteralPath (Join-Path $ToolchainPath 'LICENSE.TXT') -Destination (Join-Path $stage 'licenses\LLVM.txt')
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $stage 'licenses\StarSketch-MIT.txt')
# Keep the owned-file allowlist stable while including the camera reference notice.
$cameraNotice=Get-Content -LiteralPath (Join-Path $root 'third_party\ootss-cmd-LICENSE.txt') -Raw -Encoding UTF8
Add-Content -LiteralPath (Join-Path $stage 'licenses\StarSketch-MIT.txt') -Value ("`r`n"+$cameraNotice) -Encoding UTF8
foreach($name in $noticeNames){Copy-Item -LiteralPath (Join-Path $runtimeNotices $name) -Destination (Join-Path $stage ('licenses\MinGW-'+$name))}
Copy-Item -LiteralPath (Join-Path $root 'docs\使用说明.md') -Destination (Join-Path $stage '使用说明.txt')
$archive=Join-Path $OutputDirectory ('StarSketch-'+$Version+'-r67271-test.zip')
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stream=[IO.File]::Open($archive,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::None)
$zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
try{
    foreach($file in Get-ChildItem -LiteralPath $stage -Recurse -File){
        $entryName=$file.FullName.Substring($stage.Length+1).Replace('\','/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,$entryName,[IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
}finally{$zip.Dispose();$stream.Dispose()}
$manifest=[ordered]@{version=$Version;archive=[IO.Path]::GetFileName($archive);archive_sha256=(Get-FileHash -LiteralPath $archive).Hash.ToLower();dll_sha256=(Get-FileHash -LiteralPath $BinaryPath).Hash.ToLower();game_exe_sha256='d802098b7cb85eadfc733b88d1554042fad950c572e916febea3f669a474a549'}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory ('StarSketch-'+$Version+'-manifest.json')) -Encoding UTF8
Write-Output $archive
