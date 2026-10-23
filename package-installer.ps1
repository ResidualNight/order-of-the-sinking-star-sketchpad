[CmdletBinding()]
param([ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')][string]$Version='0.1.3',[string]$RuntimeArchive,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'dist'}
if(!$RuntimeArchive){$RuntimeArchive=Join-Path $root ('dist\StarSketch-'+$Version+'-r67271-test.zip')}
$manifestPath=Join-Path ([IO.Path]::GetDirectoryName($RuntimeArchive)) ('StarSketch-'+$Version+'-manifest.json')
$runtime=Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
if($runtime.version -ne $Version -or (Get-FileHash -LiteralPath $RuntimeArchive).Hash.ToLower() -ne $runtime.archive_sha256){throw 'Runtime package version / SHA256 mismatch.'}
$stage=Join-Path $root ('.local\installer-packages\'+[guid]::NewGuid().ToString('N'))
$payload=Join-Path $stage 'payload'
[IO.Directory]::CreateDirectory($payload) | Out-Null
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$expected=@('dxgi.dll','StarSketch-assets/NotoSerifSC.ttf','使用说明.txt','licenses/Dear-ImGui-MIT.txt','licenses/LLVM.txt','licenses/MinGW-COPYING','licenses/MinGW-COPYING.MinGW-w64.txt','licenses/MinGW-COPYING.MinGW-w64-runtime.txt','licenses/MinGW-COPYING.winpthreads.txt','licenses/OFL-NotoSerifSC.txt','licenses/StarSketch-MIT.txt')
$source=[IO.Compression.ZipFile]::OpenRead($RuntimeArchive)
$files=@()
try{
 if($source.Entries.Count -ne $expected.Count){throw 'Unexpected runtime ZIP contents.'}
 $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
 foreach($entry in $source.Entries){
  if($entry.FullName -notin $expected -or !$seen.Add($entry.FullName) -or $entry.Length -gt 50MB){throw 'Unsafe or duplicated runtime ZIP entry.'}
  $path=Join-Path $payload $entry.FullName
  [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path)) | Out-Null
  [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$path,$false)
  $files+=[ordered]@{path=$entry.FullName;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLower()}
 }
}finally{$source.Dispose()}
if((Get-FileHash -LiteralPath (Join-Path $payload 'dxgi.dll')).Hash.ToLower() -ne $runtime.dll_sha256){throw 'Runtime DLL hash mismatch.'}
# Include the preceding local error-code diagnostic so its owned installation can be updated too.
$known=@($runtime.dll_sha256,'fb767808a0389e543af41d3f9496e849ddeda0a1b8b7c8dbc3fb50fa16828617','d8b4cd185bcb01f37bc9477bedcff5370928d1ad95430df33b279086199c8c29','9f7bab48c931d01152a328cea80e3f12537750708c4aae5e1f222066505ad575','4a05fdb377c3c1428431e35cf6e1edec3f55b20e799e49ff4c8ccc185aa292d2') | Sort-Object -Unique
$package=[ordered]@{product='StarSketch';schema=1;version=$Version;known_dll_sha256=@($known);game_exe_sha256=$runtime.game_exe_sha256;files=$files}
[IO.File]::WriteAllText((Join-Path $payload 'StarSketch-package.json'),($package | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
foreach($name in @('StarSketch-Setup.ps1','StarSketch.Installer.psm1','StarSketch-安装卸载.cmd')){Copy-Item -LiteralPath (Join-Path $root ('installer\'+$name)) -Destination (Join-Path $stage $name)}
Copy-Item -LiteralPath (Join-Path $root 'docs\安装工具说明.md') -Destination (Join-Path $stage '安装说明.txt')
# Build a small windowed launcher using the .NET compiler already included with Windows.
$windowsPS=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$automation=& $windowsPS -NoProfile -Command '[psobject].Assembly.Location'
if($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $automation)){throw 'Windows PowerShell 5.1 assembly missing.'}
$csc=Join-Path $env:SystemRoot 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if(!(Test-Path -LiteralPath $csc)){throw '.NET Framework C# compiler missing.'}
& $csc /nologo /target:winexe /platform:x64 /optimize+ ('/out:'+(Join-Path $stage 'StarSketch-安装卸载.exe')) ('/reference:'+$automation) /reference:System.Windows.Forms.dll (Join-Path $root 'installer\SetupHost.cs')
if($LASTEXITCODE -ne 0){throw 'Graphical launcher compilation failed.'}
$archive=Join-Path $OutputDirectory ('StarSketch-'+$Version+'-installer.zip')
if(Test-Path -LiteralPath $archive){throw 'Installer archive already exists; choose another output directory to preserve it.'}
$stream=[IO.File]::Open($archive,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
$zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create,$false,[Text.Encoding]::UTF8)
try{
 foreach($file in Get-ChildItem -LiteralPath $stage -Recurse -File){
  $entryName=$file.FullName.Substring($stage.Length+1).Replace('\','/')
  [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,$entryName,[IO.Compression.CompressionLevel]::Optimal) | Out-Null
 }
}finally{$zip.Dispose();$stream.Dispose()}
$receipt=[ordered]@{version=$Version;archive=[IO.Path]::GetFileName($archive);archive_sha256=(Get-FileHash -LiteralPath $archive).Hash.ToLower();dll_sha256=$runtime.dll_sha256;runtime_archive_sha256=$runtime.archive_sha256}
$receipt | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory ('StarSketch-'+$Version+'-installer-manifest.json')) -Encoding UTF8
Write-Output $archive
