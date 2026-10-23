[CmdletBinding()]
param([string]$WorkRoot)
$ErrorActionPreference='Stop';Set-StrictMode -Version 2
$root=Split-Path -Parent $PSScriptRoot
if(!$WorkRoot){$WorkRoot=Join-Path $root 'work\update-tests'}
$run=Join-Path $WorkRoot ([guid]::NewGuid().ToString('N'));[IO.Directory]::CreateDirectory($run) | Out-Null
Add-Type -AssemblyName System.IO.Compression;Add-Type -AssemblyName System.IO.Compression.FileSystem
$module=Import-Module (Join-Path $root 'installer\StarSketch.Installer.psm1') -Force -PassThru -DisableNameChecking
& $module {$script:RunningGameProbe={$false}}
function Assert([bool]$Condition,[string]$Message){if(!$Condition){throw "FAIL: $Message"}}
function Failure([scriptblock]$Action,[string]$Code){$message=$null;try{& $Action | Out-Null}catch{$message=$_.Exception.Message};Assert ($message -and $message.StartsWith("[$Code] ")) ("expected $Code, got $message")}
function Put([string]$Path,[string]$Text){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null;[IO.File]::WriteAllText($Path,$Text,[Text.UTF8Encoding]::new($false))}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path).Hash.ToLowerInvariant()}
function Snapshot([string]$Path){$items=@{};foreach($file in Get-ChildItem -LiteralPath $Path -Recurse -File){$items[$file.FullName.Substring($Path.Length)]=Hash $file.FullName};return ($items.GetEnumerator() | Sort-Object Key | ConvertTo-Json -Compress)}
$map=& $module {$script:PayloadMap};$known=[Collections.Generic.List[string]]::new()
function Bundle([string]$Version){
 $directory=Join-Path $run ('bundle-'+$Version);$payload=Join-Path $directory 'payload';$files=@()
 foreach($name in $map.Keys){$file=Join-Path $payload $name;Put $file ("fixture-$Version-$name");$files+=[ordered]@{path=$name;sha256=(Hash $file)}}
 $known.Add((Hash (Join-Path $payload 'dxgi.dll')))
 $info=[ordered]@{product='StarSketch';schema=1;version=$Version;known_dll_sha256=$known.ToArray();files=$files}
 Put (Join-Path $payload 'StarSketch-package.json') ($info | ConvertTo-Json -Depth 6)
 foreach($name in @('StarSketch-Setup.ps1','StarSketch.Installer.psm1','StarSketch-安装卸载.cmd')){Copy-Item -LiteralPath (Join-Path $root ('installer\'+$name)) -Destination (Join-Path $directory $name)}
 Put (Join-Path $directory 'StarSketch-安装卸载.exe') 'fixture launcher, never executed';Put (Join-Path $directory '安装说明.txt') 'fixture notices'
 return $directory
}
$oldBundle=Bundle '0.1.2';$newBundle=Bundle '0.1.3'
function Archive([string]$Directory,[string]$Variant='normal'){
 $path=Join-Path $run ([guid]::NewGuid().ToString('N')+'.zip');$zip=[IO.Compression.ZipFile]::Open($path,[IO.Compression.ZipArchiveMode]::Create)
 try{
  foreach($file in Get-ChildItem -LiteralPath $Directory -File -Recurse){
   $name=$file.FullName.Substring($Directory.Length+1).Replace('\','/')
   if($Variant -eq 'duplicate' -and $name -eq '安装说明.txt'){$name='StarSketch-Setup.ps1'}
   $entry=[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,$name,[IO.Compression.CompressionLevel]::Optimal)
   if($Variant -eq 'link' -and $name -eq '安装说明.txt'){$entry.ExternalAttributes=-1577123840}
  }
  if($Variant -eq 'traversal'){[void]$zip.CreateEntry('../outside.txt')}
 }finally{$zip.Dispose()}
 return $path
}
function Release([string]$Archive,[string]$Version='0.1.3'){
 $receiptPath=Join-Path $run ([guid]::NewGuid().ToString('N')+'.json')
 $receipt=[ordered]@{version=$Version;archive="StarSketch-$Version-installer.zip";archive_sha256=(Hash $Archive);dll_sha256=(Hash (Join-Path $newBundle 'payload\dxgi.dll'))}
 Put $receiptPath ($receipt | ConvertTo-Json)
 $base="https://github.com/ResidualNight/order-of-the-sinking-star-sketchpad/releases/download/v$Version-test/"
 $assets=@();$downloads=@{}
 foreach($item in @(@{name="StarSketch-$Version-installer.zip";path=$Archive},@{name="StarSketch-$Version-installer-manifest.json";path=$receiptPath})){
  $url=$base+$item.name;$downloads[$url]=$item.path
  $assets+=[pscustomobject]@{name=$item.name;browser_download_url=$url;size=(Get-Item -LiteralPath $item.path).Length;state='uploaded';digest=('sha256:'+(Hash $item.path))}
 }
 return [pscustomobject]@{release=[pscustomobject]@{draft=$false;prerelease=$true;tag_name="v$Version-test";assets=$assets};downloads=$downloads}
}
function Configure($Data){
 $json=ConvertTo-Json -InputObject @($Data.release) -Depth 9 -Compress
 & $module {param($json,$downloads)
  $script:MockJson=$json;$script:MockDownloads=$downloads;$script:DownloadCount=0;$script:NetworkFailure=$false;$script:StartGameOnDownload=$false
  function script:Read-StarSketchHttp([string]$Address,[long]$Limit,[string]$Destination){
   if($script:NetworkFailure){throw '[SS-UPDATE-001] injected network failure'}
   Assert-StarSketchUpdateUri $Address
   if(!$Destination){return $script:MockJson}
   $script:DownloadCount++;[IO.File]::Copy($script:MockDownloads[$Address],$Destination,$false)
   if($script:StartGameOnDownload -and $Address.EndsWith('.zip')){$script:RunningGameProbe={$true}}
  }
 } $json $Data.downloads
}
$good=Release (Archive $newBundle);Configure $good
$passed=[Collections.Generic.List[string]]::new()
$offer=Get-StarSketchUpdate '0.1.2';Assert ($offer.available -and $offer.version -eq '0.1.3') 'test prerelease detected'
Assert (!(Get-StarSketchUpdate '0.1.3').available) 'same version not offered';Assert (!(Get-StarSketchUpdate '1.0.0').available) 'older release not offered'
$ten=Release (Archive $newBundle) '0.1.10';Configure $ten;Assert ((Get-StarSketchUpdate '0.1.9').available) 'numeric version ordering'
Configure $good
& $module {$parsed=$script:MockJson | ConvertFrom-Json;$draft=$script:MockJson | ConvertFrom-Json;$draft[0].draft=$true;$draft[0].tag_name='v99.0.0-test';$script:MockJson=ConvertTo-Json -InputObject @($draft[0],$parsed[0]) -Depth 9 -Compress}
Assert ((Get-StarSketchUpdate '0.1.2').version -eq '0.1.3') 'draft skipped'
$passed.Add('test releases, numeric versions, no downgrade and draft filtering')
Configure $good;& $module {$script:NetworkFailure=$true};Failure {Get-StarSketchUpdate '0.1.2'} 'SS-UPDATE-001'
Configure $good;& $module {$script:MockJson='not json'};Failure {Get-StarSketchUpdate '0.1.2'} 'SS-UPDATE-002'
Configure $good;& $module {$script:MockJson='[{"unexpected":true}]'};Failure {Get-StarSketchUpdate '0.1.2'} 'SS-UPDATE-002'
Configure $good;& $module {$parsed=$script:MockJson | ConvertFrom-Json;$parsed[0].assets=@($parsed[0].assets[0]);$script:MockJson=ConvertTo-Json -InputObject @($parsed[0]) -Depth 9 -Compress};Failure {Get-StarSketchUpdate '0.1.2'} 'SS-UPDATE-003'
Configure $good;& $module {$parsed=$script:MockJson | ConvertFrom-Json;$parsed[0].assets[0].browser_download_url='https://example.com/file.zip';$script:MockJson=ConvertTo-Json -InputObject @($parsed[0]) -Depth 9 -Compress};Failure {Get-StarSketchUpdate '0.1.2'} 'SS-UPDATE-004'
Configure $good;& $module {$parsed=$script:MockJson | ConvertFrom-Json;$parsed[0].assets[0].size=129MB;$script:MockJson=ConvertTo-Json -InputObject @($parsed[0]) -Depth 9 -Compress};Failure {Get-StarSketchUpdate '0.1.2'} 'SS-UPDATE-011'
Assert ((Format-StarSketchError '[SS-UPDATE-006] fixture') -eq '[SS-UPDATE-006] fixture') 'update identifier retained'
$passed.Add('offline, malformed JSON, missing assets, foreign URL, size bounds and error identifiers')
$game=Join-Path $run '模拟 游戏';Put (Join-Path $game 'sinking_star.exe') 'MZ-fixture';Put (Join-Path $game 'saves\keep.txt') 'keep'
Install-StarSketch $game (Join-Path $oldBundle 'payload') $oldBundle | Out-Null
$before=Snapshot $game
Configure $good;$offer=Get-StarSketchUpdate '0.1.2'
& $module {$script:RunningGameProbe={$true}};Failure {Install-StarSketchUpdate $game $offer} 'SS-INSTALL-019';Assert ((& $module {$script:DownloadCount}) -eq 0) 'running game rejected before downloads';& $module {$script:RunningGameProbe={$false}}
Configure $good;& $module {$script:StartGameOnDownload=$true};Failure {Install-StarSketchUpdate $game $offer} 'SS-INSTALL-019';& $module {$script:RunningGameProbe={$false}}
Assert ((Snapshot $game) -eq $before) 'game-start race preserves installation'
$passed.Add('running game and game-start race block updates without mutations')
Configure $good;$corrupt=Join-Path $run 'corrupt.zip';Put $corrupt 'corrupt'
& $module {param($url,$path)$script:MockDownloads[$url]=$path} $offer.archive.browser_download_url $corrupt
Failure {Install-StarSketchUpdate $game $offer} 'SS-UPDATE-006';Assert ((Snapshot $game) -eq $before) 'bad checksum preserves installation'
# Configure owns a shared downloads table: reconstruct it after the corruption injection.
$good=Release (Archive $newBundle);Configure $good;$offer=Get-StarSketchUpdate '0.1.2'
$passed.Add('archive checksum failure leaves old version intact')
foreach($variant in @('traversal','duplicate','link')){
 $bad=Release (Archive $newBundle $variant);Configure $bad;$badOffer=Get-StarSketchUpdate '0.1.2'
 Failure {Install-StarSketchUpdate $game $badOffer} 'SS-UPDATE-007';Assert ((Snapshot $game) -eq $before) ('unsafe '+$variant+' preserves installation')
}
Assert (!(Test-Path -LiteralPath (Join-Path $game 'outside.txt'))) 'no ZIP traversal output'
$passed.Add('traversal, duplicate ZIP entries and symbolic-link entries rejected')
$packagePath=Join-Path $newBundle 'payload\StarSketch-package.json';$savedPackage=[IO.File]::ReadAllText($packagePath)
$badPackage=$savedPackage | ConvertFrom-Json;$badPackage.version='9.9.9';Put $packagePath ($badPackage | ConvertTo-Json -Depth 6)
$bad=Release (Archive $newBundle);Configure $bad;$badOffer=Get-StarSketchUpdate '0.1.2';Failure {Install-StarSketchUpdate $game $badOffer} 'SS-UPDATE-008';Put $packagePath $savedPackage
Assert ((Snapshot $game) -eq $before) 'package identity mismatch preserves installation'
$dllPath=Join-Path $newBundle 'payload\dxgi.dll';$savedDll=[IO.File]::ReadAllText($dllPath);Put $dllPath 'modified payload'
$bad=Release (Archive $newBundle);Configure $bad;$badOffer=Get-StarSketchUpdate '0.1.2';Failure {Install-StarSketchUpdate $game $badOffer} 'SS-UPDATE-008';Put $dllPath $savedDll
$passed.Add('package version and DLL identity mismatch rejected')
$good=Release (Archive $newBundle);Configure $good;$offer=Get-StarSketchUpdate '0.1.2'
$original=& $module {(Get-Item Function:Commit-StarSketchFile).ScriptBlock}
& $module {param($body)$script:OriginalCommit=$body;$script:CommitCount=0;function script:Commit-StarSketchFile([string]$Source,[string]$Target){$script:CommitCount++;if($script:CommitCount -eq 3){throw 'injected update commit failure'};& $script:OriginalCommit $Source $Target}} $original
Failure {Install-StarSketchUpdate $game $offer} 'SS-INSTALL-032'
& $module {param($body)Set-Item Function:script:Commit-StarSketchFile -Value $body} $original
Assert ((Snapshot $game) -eq $before) 'partial online update rolls back exactly'
$passed.Add('online update commit failure restores old files and manifest')
Install-StarSketchUpdate $game $offer | Out-Null
Assert ((Hash (Join-Path $game 'dxgi.dll')) -eq (Hash (Join-Path $newBundle 'payload\dxgi.dll'))) 'new DLL installed'
$state=Get-Content -LiteralPath (Join-Path $game 'StarSketch-install.json') -Raw -Encoding UTF8 | ConvertFrom-Json
Assert ($state.version -eq '0.1.3' -and @($state.files).Count -eq 15) 'new version and ownership installed'
foreach($item in $state.files){Assert ((Hash (Join-Path $game $item.path)) -eq $item.sha256) ('ownership hash '+$item.path)}
Assert ([IO.File]::ReadAllText((Join-Path $game 'saves\keep.txt')) -eq 'keep') 'save preserved'
Assert (@(Get-ChildItem -LiteralPath $game -Force -Filter '.StarSketch-transaction-*').Count -eq 0) 'download and transaction staging cleaned'
Install-StarSketchUpdate $game $offer | Out-Null
$passed.Add('one-click update installs new version and manager, preserves saves and cleans staging')
# Match the graphical manager's background runspace and replace its own module.
$selfGame=Join-Path $run 'self-update-worker';Put (Join-Path $selfGame 'sinking_star.exe') 'MZ-fixture'
Install-StarSketch $selfGame (Join-Path $oldBundle 'payload') $oldBundle | Out-Null
$worker=[PowerShell]::Create()
[void]$worker.AddScript({
 param($modulePath,$game,$offer,$downloads)
 $ErrorActionPreference='Stop';$loaded=Import-Module $modulePath -Force -PassThru -DisableNameChecking
 & $loaded {param($downloads)
  $script:Downloads=$downloads;$script:RunningGameProbe={$false}
  function script:Read-StarSketchHttp([string]$Address,[long]$Limit,[string]$Destination){[IO.File]::Copy($script:Downloads[$Address],$Destination,$false)}
 } $downloads
 Install-StarSketchUpdate $game $offer
}).AddArgument((Join-Path $selfGame 'StarSketch-Manager\StarSketch.Installer.psm1')).AddArgument($selfGame).AddArgument($offer).AddArgument($good.downloads)
try{
 $pending=$worker.BeginInvoke();$items=@($worker.EndInvoke($pending))
 Assert (!$worker.HadErrors -and $items[-1].version -eq '0.1.3') 'background self-update completes'
 Assert ((Hash (Join-Path $selfGame 'dxgi.dll')) -eq (Hash (Join-Path $newBundle 'payload\dxgi.dll'))) 'background self-update installs new DLL'
 $selfState=Get-Content -LiteralPath (Join-Path $selfGame 'StarSketch-install.json') -Raw -Encoding UTF8 | ConvertFrom-Json
 foreach($item in $selfState.files){Assert ((Hash (Join-Path $selfGame $item.path)) -eq $item.sha256) ('background ownership '+$item.path)}
 Assert (@(Get-ChildItem -LiteralPath $selfGame -Force -Filter '.StarSketch-transaction-*').Count -eq 0) 'background self-update cleans transactions'
}finally{$worker.Dispose()}
$passed.Add('background runspace passes update offer and safely replaces its own installed manager')
$worker=[PowerShell]::Create();[void]$worker.AddScript({Write-Warning '[SS-INSTALL-036] fixture retained staging';throw '[SS-UPDATE-001] fixture terminal download failure'})
$failure=$null
try{$pending=$worker.BeginInvoke();[void]$worker.EndInvoke($pending)}catch{$failure=Format-StarSketchError $_.Exception}finally{
 Assert (@($worker.Streams.Warning).Count -eq 1) 'background terminal failure retains warning stream';$worker.Dispose()
}
Assert ($failure -and $failure.StartsWith('[SS-UPDATE-001] ')) 'terminal background exception retains actionable error code without wrapper'
$passed.Add('background terminal exception and cleanup warning retain original identifiers')
$installedModulePath=Join-Path $game 'StarSketch-Manager\StarSketch.Installer.psm1';Remove-Module $module
$installed=Import-Module $installedModulePath -Force -PassThru -DisableNameChecking;& $installed {$script:RunningGameProbe={$false}}
Assert ([bool](Get-Command Get-StarSketchUpdate -ErrorAction SilentlyContinue)) 'installed manager retains online updater'
$installedPackage=Get-StarSketchPackage (Join-Path $game 'StarSketch-Manager\StarSketch-package.json');Uninstall-StarSketch $game $installedPackage | Out-Null
Assert (!(Test-Path -LiteralPath (Join-Path $game 'StarSketch-Manager'))) 'self manager removed';Assert (Test-Path -LiteralPath (Join-Path $game 'saves\keep.txt')) 'uninstall preserves save';Remove-Module $installed
$passed.Add('updated installed manager works without downloaded bundle and self-uninstalls')
[ordered]@{date='2026-10-24';tests=$passed.ToArray();result='pass';fixtures=$run} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'results.json') -Encoding UTF8
Write-Output ('PASS: '+$passed.Count+' online update regression groups');$passed | ForEach-Object{Write-Output ('  '+$_)}
