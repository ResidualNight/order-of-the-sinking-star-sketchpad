[CmdletBinding()]
param([string]$WorkRoot)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$root=Split-Path -Parent $PSScriptRoot
if(!$WorkRoot){$WorkRoot=Join-Path $root 'work\installer-tests'}
$run=Join-Path $WorkRoot ([guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($run) | Out-Null
$module=Import-Module (Join-Path $root 'installer\StarSketch.Installer.psm1') -Force -PassThru -DisableNameChecking
& $module { $script:RunningGameProbe={ $false } }
function Assert([bool]$Condition,[string]$Message){if(!$Condition){throw "FAIL: $Message"}}
function Expect-Failure([scriptblock]$Action,[string]$Message){$failed=$false;try{& $Action | Out-Null}catch{$failed=$true};Assert $failed $Message}
function Put([string]$Path,[string]$Value){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null;[IO.File]::WriteAllText($Path,$Value,[Text.UTF8Encoding]::new($false))}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path).Hash.ToLower()}
$payload=Join-Path $run 'package\payload';$manager=Join-Path $run 'package'
$map=& $module { $script:PayloadMap }
$files=@()
foreach($name in $map.Keys){$path=Join-Path $payload $name;Put $path ('fixture-v1:'+ $name);$files+=[ordered]@{path=$name;sha256=(Hash $path)}}
$known=@(@($files | Where-Object path -eq 'dxgi.dll')[0].sha256)
$packageData=[ordered]@{product='StarSketch';schema=1;version='0.1.2';known_dll_sha256=$known;files=$files}
Put (Join-Path $payload 'StarSketch-package.json') ($packageData | ConvertTo-Json -Depth 6)
foreach($name in @('StarSketch-Setup.ps1','StarSketch.Installer.psm1','StarSketch-安装卸载.cmd')){Copy-Item -LiteralPath (Join-Path $root ('installer\'+$name)) -Destination (Join-Path $manager $name)}
$package=Get-StarSketchPackage (Join-Path $payload 'StarSketch-package.json')
function Game([string]$Name){$path=Join-Path $run $Name;Put (Join-Path $path 'sinking_star.exe') 'MZ-fixture-not-a-real-game';Put (Join-Path $path 'saves\keep.txt') 'keep';return $path}
function Snapshot([string]$Directory){$values=@{};foreach($f in Get-ChildItem -LiteralPath $Directory -Recurse -File){$values[$f.FullName.Substring($Directory.Length)]=Hash $f.FullName};return ($values.GetEnumerator() | Sort-Object Key | ConvertTo-Json -Compress)}
$passed=[Collections.Generic.List[string]]::new()
$bad=Join-Path $run 'not-game';[IO.Directory]::CreateDirectory($bad) | Out-Null
Expect-Failure {Install-StarSketch $bad $payload $manager} 'wrong folder rejected'
$passed.Add('wrong folder rejected')
$game=Game 'fresh install with 空格'
Install-StarSketch $game $payload $manager | Out-Null
Assert ((Hash (Join-Path $game 'dxgi.dll')) -eq $known[0]) 'fresh DLL'
Assert (Test-Path -LiteralPath (Join-Path $game 'StarSketch-Manager\StarSketch-安装卸载.cmd')) 'installed manager entry'
$state=Get-Content -LiteralPath (Join-Path $game 'StarSketch-install.json') -Raw -Encoding UTF8 | ConvertFrom-Json
Assert (@($state.files).Count -eq 15) 'exact ownership list'
Install-StarSketch $game $payload $manager | Out-Null
Assert (Test-Path -LiteralPath (Join-Path $game 'saves\keep.txt')) 'save preserved'
Put (Join-Path $game 'StarSketch-assets\user-note.txt') 'keep-extra'
Uninstall-StarSketch $game $package | Out-Null
Assert (!(Test-Path -LiteralPath (Join-Path $game 'dxgi.dll'))) 'DLL removed'
Assert (!(Test-Path -LiteralPath (Join-Path $game 'StarSketch-install.json'))) 'state removed'
Assert (!(Test-Path -LiteralPath (Join-Path $game 'StarSketch-Manager'))) 'manager removed'
Assert (Test-Path -LiteralPath (Join-Path $game 'StarSketch-assets\user-note.txt')) 'extra asset preserved'
Assert (Test-Path -LiteralPath (Join-Path $game 'sinking_star.exe')) 'game executable preserved'
$passed.Add('fresh install / repeated update / uninstall with unrelated files preserved')
$foreign=Game 'foreign';Put (Join-Path $foreign 'dxgi.dll') 'other-mod'
$before=Snapshot $foreign
Expect-Failure {Install-StarSketch $foreign $payload $manager} 'foreign install refused'
Expect-Failure {Uninstall-StarSketch $foreign $package} 'foreign uninstall refused'
Assert ((Snapshot $foreign) -eq $before) 'foreign mod unchanged'
$passed.Add('foreign DXGI protected')
$modified=Game 'modified';Install-StarSketch $modified $payload $manager | Out-Null
Put (Join-Path $modified 'dxgi.dll') 'changed-after-install';$before=Snapshot $modified
Expect-Failure {Install-StarSketch $modified $payload $manager} 'modified install refused'
Expect-Failure {Uninstall-StarSketch $modified $package} 'modified uninstall refused'
Assert ((Snapshot $modified) -eq $before) 'modified state unchanged'
$passed.Add('modified installed file protected')
$legacy=Game 'manual';Copy-Item -LiteralPath (Join-Path $payload 'dxgi.dll') -Destination (Join-Path $legacy 'dxgi.dll')
Put (Join-Path $legacy 'StarSketch-assets\NotoSerifSC.ttf') 'fixture-v1:StarSketch-assets/NotoSerifSC.ttf'
Assert ((Get-StarSketchStatus $legacy $package).installed) 'manual recognized'
Uninstall-StarSketch $legacy $package | Out-Null
Assert (!(Test-Path -LiteralPath (Join-Path $legacy 'dxgi.dll'))) 'manual removed'
$passed.Add('known manual installation recognized and uninstalled')
$malicious=Game 'bad-state';Install-StarSketch $malicious $payload $manager | Out-Null
$statePath=Join-Path $malicious 'StarSketch-install.json';$saved=[IO.File]::ReadAllText($statePath)
$state=$saved | ConvertFrom-Json;$state.files[1].path='../outside.txt';Put $statePath ($state | ConvertTo-Json -Depth 6)
$before=Snapshot $malicious
Expect-Failure {Uninstall-StarSketch $malicious $package} 'traversal state refused'
Assert ((Snapshot $malicious) -eq $before) 'bad state unchanged'
$passed.Add('manifest path traversal rejected')
$corrupt=Game 'bad-payload';$payloadFile=Join-Path $payload 'dxgi.dll';$savedDll=[IO.File]::ReadAllText($payloadFile);Put $payloadFile 'corrupted'
Expect-Failure {Install-StarSketch $corrupt $payload $manager} 'bad payload rejected'
Assert (!(Test-Path -LiteralPath (Join-Path $corrupt 'dxgi.dll'))) 'bad payload not installed'
Put $payloadFile $savedDll
$passed.Add('corrupted package rejected before changes')
$running=Game 'running';$before=Snapshot $running
& $module {$script:RunningGameProbe={$true}}
Expect-Failure {Install-StarSketch $running $payload $manager} 'running game blocks install'
Expect-Failure {Uninstall-StarSketch $running $package} 'running game blocks uninstall'
& $module {$script:RunningGameProbe={$false}}
Assert ((Snapshot $running) -eq $before) 'running fixture unchanged'
$passed.Add('running game blocks mutation')
# Inject a failure after some atomic writes to exercise real rollback paths.
$rollback=Game 'install-rollback';Install-StarSketch $rollback $payload $manager | Out-Null;$before=Snapshot $rollback
$original=& $module {(Get-Item Function:Commit-StarSketchFile).ScriptBlock}
& $module {param($body)$script:OriginalCommit=$body;$script:CommitCount=0;function script:Commit-StarSketchFile([string]$Source,[string]$Target){$script:CommitCount++;if($script:CommitCount -eq 3){throw 'fixture install failure'};& $script:OriginalCommit $Source $Target}} $original
Expect-Failure {Install-StarSketch $rollback $payload $manager} 'injected install fails'
& $module {param($body)Set-Item Function:script:Commit-StarSketchFile -Value $body} $original
Assert ((Snapshot $rollback) -eq $before) 'install rollback exact'
$passed.Add('partial install failure restores prior files and manifest')
$originalMove=& $module {(Get-Item Function:Move-StarSketchFile).ScriptBlock}
& $module {param($body)$script:OriginalMove=$body;$script:MoveCount=0;function script:Move-StarSketchFile([string]$Source,[string]$Target){$script:MoveCount++;if($script:MoveCount -eq 2){throw 'fixture uninstall failure'};& $script:OriginalMove $Source $Target}} $originalMove
Expect-Failure {Uninstall-StarSketch $rollback $package} 'injected uninstall fails'
& $module {param($body)Set-Item Function:script:Move-StarSketchFile -Value $body} $originalMove
Assert ((Snapshot $rollback) -eq $before) 'uninstall rollback exact'
$passed.Add('partial uninstall failure restores files')
# An actual content/version upgrade preserves ownership and remains uninstallable.
$manager2=Join-Path $run 'package-v2';Copy-Item -LiteralPath $manager -Destination $manager2 -Recurse
$payload2=Join-Path $manager2 'payload';$metadataPath=Join-Path $payload2 'StarSketch-package.json'
$upgradeInfo=Get-Content -Raw -LiteralPath $metadataPath -Encoding UTF8 | ConvertFrom-Json
$upgradeInfo.version='0.1.3';Put (Join-Path $payload2 'dxgi.dll') 'fixture-v2-dll'
$newDllHash=Hash (Join-Path $payload2 'dxgi.dll');$upgradeInfo.known_dll_sha256=@($known[0],$newDllHash)
@($upgradeInfo.files | Where-Object path -eq 'dxgi.dll')[0].sha256=$newDllHash
Put $metadataPath ($upgradeInfo | ConvertTo-Json -Depth 6)
$upgrade=Game 'upgrade';Install-StarSketch $upgrade $payload $manager | Out-Null
Install-StarSketch $upgrade $payload2 $manager2 | Out-Null
$newPackage=Get-StarSketchPackage $metadataPath
Assert ((Get-StarSketchStatus $upgrade $newPackage).version -eq '0.1.3') 'version updated'
Assert ((Hash (Join-Path $upgrade 'dxgi.dll')) -eq $newDllHash) 'new DLL installed'
Uninstall-StarSketch $upgrade $newPackage | Out-Null
Assert (Test-Path -LiteralPath (Join-Path $upgrade 'saves\keep.txt')) 'upgrade save retained'
$passed.Add('changed DLL/version upgrade remains uninstallable')
# A file appearing between staging and its commit must be preserved, not overwritten.
$concurrent=Game 'concurrent';Install-StarSketch $concurrent $payload $manager | Out-Null
$original=& $module {(Get-Item Function:Commit-StarSketchFile).ScriptBlock}
$fontTarget=Join-Path $concurrent 'StarSketch-assets\NotoSerifSC.ttf'
& $module {param($body,$changedPath)$script:OriginalCommit=$body;$script:CommitCount=0;$script:ConcurrentPath=$changedPath;function script:Commit-StarSketchFile([string]$Source,[string]$Target){& $script:OriginalCommit $Source $Target;$script:CommitCount++;if($script:CommitCount -eq 1){[IO.File]::WriteAllText($script:ConcurrentPath,'concurrent-change')}}} $original $fontTarget
Expect-Failure {Install-StarSketch $concurrent $payload $manager} 'concurrent content rejected'
& $module {param($body)Set-Item Function:script:Commit-StarSketchFile -Value $body} $original
Assert ([IO.File]::ReadAllText($fontTarget) -eq 'concurrent-change') 'concurrent file preserved'
Assert ((Hash (Join-Path $concurrent 'dxgi.dll')) -eq $known[0]) 'other installed DLL restored'
$passed.Add('concurrent file modification preserved')
# Refuse an asset directory junction rather than writing outside the selected game.
$linkGame=Game 'junction';$outside=Join-Path $run 'outside-target';Put (Join-Path $outside 'keep.txt') 'outside-keep'
New-Item -ItemType Junction -Path (Join-Path $linkGame 'StarSketch-assets') -Target $outside | Out-Null
Expect-Failure {Install-StarSketch $linkGame $payload $manager} 'junction rejected'
Assert (!(Test-Path -LiteralPath (Join-Path $linkGame 'dxgi.dll'))) 'junction case had no DLL writes'
Assert ([IO.File]::ReadAllText((Join-Path $outside 'keep.txt')) -eq 'outside-keep') 'junction target preserved'
$passed.Add('directory junction rejected without following it')

# A running copy of the installed module must be able to remove its own managed scripts.
$installedModulePath=Join-Path $rollback 'StarSketch-Manager\StarSketch.Installer.psm1'
Remove-Module $module
$installed=Import-Module $installedModulePath -Force -PassThru -DisableNameChecking
& $installed {$script:RunningGameProbe={$false}}
$installedPackage=Get-StarSketchPackage (Join-Path $rollback 'StarSketch-Manager\StarSketch-package.json')
Uninstall-StarSketch $rollback $installedPackage | Out-Null
Assert (!(Test-Path -LiteralPath (Join-Path $rollback 'StarSketch-Manager'))) 'self manager removed'
Remove-Module $installed
$passed.Add('installed manager uninstalls without original package')
[ordered]@{date='2026-10-23';tests=$passed.ToArray();result='pass';fixtures=$run} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'results.json') -Encoding UTF8
Write-Output ('PASS: '+$passed.Count+' installer regression groups')
$passed | ForEach-Object{Write-Output ('  '+$_)}
Write-Output ('Fixtures and result: '+$run)