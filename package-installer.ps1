[CmdletBinding()]
param([ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')][string]$Version='0.2.4',[string]$RuntimeArchive,[string]$OutputDirectory)
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
# Retain released runtimes and the local diagnostics so future packages can update their owned installs.
$known=@($runtime.dll_sha256,'4a9d1d5234c37530f9711bd742de91f885525e824399d2a3da3a305abf8d88c3','f6e36facd9a7c6a886e2fa98c20ee45b6062c035e023bdd3ceaaa5080670c0fe','b4c38ace8aa35a6a0bb018b0406ffb3144ce157dde8cec6114d2345af40e44aa','7eda306e0d09619e1287c1f620819ab24cace8121e29b2ddebec724362e967bd','746801158d52be7b83fa7b7f6a82de56af0c5edffc3cec1b54ca0793a0d789bd','e074bbc046d69c9dd5b91dc6ca824790f483204a5c06b8bbbcf7c1d8f8e22cfa','00f9ca07e0f43c349e20e3d8b5546916d2ad00e9945f1a1a897554d6f04e07da','599bd87bd167b0a1c3f3c1b8b1ec7b04c605915e5a7a3891e573a240324e0b37','5651b7b95db5e5e5bf1f9e98affc4fc02bf4fa6d0eec3a02901b52e0dd73397d','7364192d535919c3f2bcefbb7647eed305efd32bb7b8925f7cc6461d4ef59e69','3cc03bb6b8ca22134e6f6e46c28e0bce4acd92a089e728d1adbf9c3024be5b92','fb767808a0389e543af41d3f9496e849ddeda0a1b8b7c8dbc3fb50fa16828617','d8b4cd185bcb01f37bc9477bedcff5370928d1ad95430df33b279086199c8c29','9f7bab48c931d01152a328cea80e3f12537750708c4aae5e1f222066505ad575','4a05fdb377c3c1428431e35cf6e1edec3f55b20e799e49ff4c8ccc185aa292d2','436d902bf0a40873d48ed2baf6c1d0704bf5a7610eb8bfc301b253243b610d7e','7dcdf15b2a6c981e3a692de7d996553ebdfd7c3274afdd1564a6237ec750a278','652291e3b9f2aaf7702a329bf09ed2b37a48f4082511e392e013d1e5ac462d05','6cbe3465f9f84c57b610ce3593f229b8a153fd4baa0cfec7f5095c0b54734711','04a754086178d8521abad4733744e370fc7309bd86846d555a3a7e0c91e82640','cb953493ed4d4c5d8f71f488d5e77f79cb302accdc3a1baa6a9d8cbcfea065da') | Sort-Object -Unique
# Preserve immutable public and earlier local 0.2.1 runtimes for owned upgrades.
$known=@(($known+'f71886e7d4ebc2df4b9ec0bedb047d384d8e1d166fa3621870a6be6b2b96f255'+'c3e3c38ca037fc876a6b2d2a8150207d038a2759a4c251ff3babf30359a19030'+'394766fee125dea644bf4c7de61b7f7a7d7a51bd36ef8b09078205c4e7161c48'+'557a0df9a05ba7bd971889fc55f265110408e986c9a58ae76b90f237e2b571c2') | Sort-Object -Unique)
# Retain the preceding local background-sampling package for owned upgrades.
$known=@(($known+'0890a59fb74379716cb6f7688f152d35b62f61427f0c733f6afbba63db5948b0') | Sort-Object -Unique)
$known=@(($known+'a3b7b0cbac5c51327c1605e9380763d116b7111c6224e898e21eecd5a8cce4be') | Sort-Object -Unique)
$known=@(($known+'82590a876da45f1b574a2870cffd2ee975ccbd8f50397046940136937636fbbf') | Sort-Object -Unique)
$known=@(($known+'aa97a8327860696941f94b6b94429d0d46f0fb34ba64774a6fd762828329a779') | Sort-Object -Unique)
$known=@(($known+'994e464f5a3c47019f20d18d4d6ec58d8a0a3c700f49e8cec0e0ea653c167ccb'+'88b7f6f3b7d3a70e32fdb632297e281ff379fd05ab702a5d2ebbdae03cc2b6a9'+'71526c053d733cea4a91551764dca80b931abe135f204ca229442a486e145828'+'e281f321105650efca5776f11619b3b9adb0a8283d8207048a1682d3b9cc767b') | Sort-Object -Unique)
$known=@(($known+'c6e1c0e9a5d2326a166a5e22c3992c73eab16acc7e6152d37b2f59b690e03d4c'+'8d27d19df9324d34cf1a3646115cfbe58dff7c09d08403b6bf8b146d673c575f') | Sort-Object -Unique)
$known=@(($known+'83281a635a7c53058949f615d72d7c60200da12c99d977d27d2005659adc3b1a') | Sort-Object -Unique)
$known=@(($known+'82d322336c110b1cc8492f45ddd139d35060d3f726735c222c5d15a6deb9448c') | Sort-Object -Unique)
$known=@(($known+'bc6d6fb6a8d3b2a560971aaf683647d3d765683d3fce25a2aa01195e2f716430') | Sort-Object -Unique)
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
