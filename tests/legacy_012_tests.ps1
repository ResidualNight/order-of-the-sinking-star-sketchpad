[CmdletBinding()]
param([string]$InstallerArchive,[string]$LegacyInstallerArchive,[string]$LegacyManualArchive)
$ErrorActionPreference='Stop';Set-StrictMode -Version 2
$root=Split-Path -Parent $PSScriptRoot
if(!$InstallerArchive){$InstallerArchive=Join-Path $root 'dist\StarSketch-0.2.0-installer.zip'}
if(!$LegacyInstallerArchive){$LegacyInstallerArchive=Join-Path $root 'dist\StarSketch-0.1.2-installer.zip'}
if(!$LegacyManualArchive){$LegacyManualArchive=Join-Path $root 'dist\StarSketch-0.1.2-r67271-test.zip'}
function Assert([bool]$Condition,[string]$Message){if(!$Condition){throw "FAIL: $Message"}}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Put([string]$Path,[string]$Text){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path))|Out-Null;[IO.File]::WriteAllText($Path,$Text,[Text.UTF8Encoding]::new($false))}
# Pin the original published 0.1.2 artifacts; never obtain fixtures from a real game.
Assert ((Hash $LegacyInstallerArchive) -ceq '3700402ed8bdb47e7a9146957720f2d175a5c090052857f99d3169b86cb0561d') 'original 0.1.2 installer checksum'
Assert ((Hash $LegacyManualArchive) -ceq '041755405382f6aefdd03afcbbcd036fc30e7495be8a6774b3efb2361467e749') 'original 0.1.2 manual checksum'
$receiptPath=[IO.Path]::ChangeExtension($InstallerArchive,$null).TrimEnd('.')+'-manifest.json'
$receipt=Get-Content -LiteralPath $receiptPath -Raw -Encoding UTF8|ConvertFrom-Json
Assert ((Hash $InstallerArchive) -ceq $receipt.archive_sha256) 'new installer checksum'
$run=Join-Path $root ('work\legacy-012-tests\'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($run)|Out-Null
$old=Join-Path $run 'old-installer';$manual=Join-Path $run 'old-manual';$new=Join-Path $run 'new-installer'
Expand-Archive -LiteralPath $LegacyInstallerArchive -DestinationPath $old
Expand-Archive -LiteralPath $LegacyManualArchive -DestinationPath $manual
Expand-Archive -LiteralPath $InstallerArchive -DestinationPath $new
$script:activeModule=$null
function Use-Module([string]$Directory){
 if($script:activeModule){Remove-Module $script:activeModule}
 $script:activeModule=Import-Module (Join-Path $Directory 'StarSketch.Installer.psm1') -Global -Force -PassThru -DisableNameChecking
 & $script:activeModule {$script:RunningGameProbe={$false}}
}
Use-Module $new
$package=Get-StarSketchPackage (Join-Path $new 'payload\StarSketch-package.json')
Assert ($package.version -eq $receipt.version -and [version]$package.version -gt [version]'0.1.2') 'target is newer than 0.1.2'
Assert ('d8b4cd185bcb01f37bc9477bedcff5370928d1ad95430df33b279086199c8c29' -in $package.known_dll_sha256) 'published 0.1.2 DLL remains allowlisted'
function Game([string]$Name,[switch]$Managed,[switch]$MissingFont){
 $game=Join-Path $run $Name
 Put (Join-Path $game 'sinking_star.exe') 'fixture only, not a game executable'
 Put (Join-Path $game 'saves\keep.txt') 'preserve-save'
 Put (Join-Path $game 'custom-mod.txt') 'preserve-mod'
 if($Managed){
  Use-Module $old
  Install-StarSketch $game (Join-Path $old 'payload') $old|Out-Null
  Use-Module $new
 }else{
  foreach($file in Get-ChildItem -LiteralPath $manual){
   if($MissingFont -and $file.Name -eq 'StarSketch-assets'){continue}
   Copy-Item -LiteralPath $file.FullName -Destination $game -Recurse
  }
 }
 Put (Join-Path $game 'StarSketch-assets\user-note.txt') 'preserve-note'
 return $game
}
function Preserved([string]$Game){
 Assert ([IO.File]::ReadAllText((Join-Path $Game 'sinking_star.exe')) -eq 'fixture only, not a game executable') 'game executable preserved'
 Assert ([IO.File]::ReadAllText((Join-Path $Game 'saves\keep.txt')) -eq 'preserve-save') 'save preserved'
 Assert ([IO.File]::ReadAllText((Join-Path $Game 'custom-mod.txt')) -eq 'preserve-mod') 'unrelated mod preserved'
 Assert ([IO.File]::ReadAllText((Join-Path $Game 'StarSketch-assets\user-note.txt')) -eq 'preserve-note') 'unknown asset preserved'
}
function Snapshot([string]$Game){$files=@{};foreach($file in Get-ChildItem -LiteralPath $Game -File -Recurse){$files[$file.FullName.Substring($Game.Length)]=Hash $file.FullName};return ($files.GetEnumerator()|Sort-Object Key|ConvertTo-Json -Compress)}
function Failure([scriptblock]$Action,[string]$Code){$message=$null;try{& $Action|Out-Null}catch{$message=$_.Exception.Message};Assert ($message -and $message.StartsWith("[$Code] ")) ("expected $Code, got $message")}
function Verify-Upgraded([string]$Game){
 $state=Get-Content -LiteralPath (Join-Path $Game 'StarSketch-install.json') -Raw -Encoding UTF8|ConvertFrom-Json
 Assert ($state.version -eq $package.version -and @($state.files).Count -eq 15) 'new version and 15 ownership hashes'
 foreach($file in $state.files){Assert ((Hash (Join-Path $Game $file.path)) -eq $file.sha256) ('owned hash: '+$file.path)}
 Assert ((Hash (Join-Path $Game 'dxgi.dll')) -eq $receipt.dll_sha256) 'final DLL matches receipt'
 Preserved $Game
}
$passed=[Collections.Generic.List[string]]::new()
foreach($managed in @($false,$true)){
 $kind=if($managed){'managed'}else{'manual'}
 $game=Game ($kind+'-uninstall-中文 空格') -Managed:$managed
 $status=Get-StarSketchStatus $game $package
 Assert ($status.display_version -eq '0.1.2' -and $status.update_version -eq '0.1.2') ($kind+' old version correctly identified')
 Assert ($status.installation -eq $kind -and $status.message.Contains('无需先卸载') -and $status.message.Contains('0.1.2 旧管理入口没有在线更新功能')) 'version-specific local upgrade guidance'
 $result=Uninstall-StarSketch $game $package
 Assert ($result.version -eq '0.1.2' -and $result.message.Contains('0.1.2')) 'uninstall reports removed version'
 Assert (!(Test-Path -LiteralPath (Join-Path $game 'dxgi.dll')) -and !(Test-Path -LiteralPath (Join-Path $game 'StarSketch-install.json'))) 'old runtime and receipt removed'
 if(!$managed){Assert (Test-Path -LiteralPath (Join-Path $game '使用说明.txt')) 'unowned manual readme retained';Assert (Test-Path -LiteralPath (Join-Path $game 'licenses\OFL-NotoSerifSC.txt')) 'unowned manual license retained'}
 Preserved $game;$passed.Add($kind+' 0.1.2 identified and directly uninstalled by new installer')
}
$upgraded=@()
foreach($managed in @($false,$true)){
 $kind=if($managed){'managed'}else{'manual'};$game=Game ($kind+'-upgrade') -Managed:$managed
 Install-StarSketch $game (Join-Path $new 'payload') $new|Out-Null
 Verify-Upgraded $game;$upgraded+=$game
 $passed.Add($kind+' 0.1.2 directly upgraded without prior uninstall')
}
foreach($managed in @($false,$true)){
 $kind=if($managed){'managed'}else{'manual'};$game=Game ($kind+'-rollback') -Managed:$managed;$before=Snapshot $game
 $original=& $script:activeModule {(Get-Item Function:Commit-StarSketchFile).ScriptBlock}
 try{
  & $script:activeModule {param($body)$script:OriginalCommit=$body;$script:CommitCount=0;function script:Commit-StarSketchFile([string]$Source,[string]$Target){$script:CommitCount++;if($script:CommitCount -eq 3){throw '0.1.2 upgrade fixture failure'};& $script:OriginalCommit $Source $Target}} $original
  Failure {Install-StarSketch $game (Join-Path $new 'payload') $new} 'SS-INSTALL-032'
 }finally{& $script:activeModule {param($body)Set-Item Function:script:Commit-StarSketchFile -Value $body} $original}
 Assert ((Snapshot $game) -eq $before) 'failed upgrade restores exact original 0.1.2 files and state'
 $original=& $script:activeModule {(Get-Item Function:Move-StarSketchFile).ScriptBlock}
 try{
  & $script:activeModule {param($body)$script:OriginalMove=$body;$script:MoveCount=0;function script:Move-StarSketchFile([string]$Source,[string]$Target){$script:MoveCount++;if($script:MoveCount -eq 2){throw '0.1.2 uninstall fixture failure'};& $script:OriginalMove $Source $Target}} $original
  Failure {Uninstall-StarSketch $game $package} 'SS-INSTALL-035'
 }finally{& $script:activeModule {param($body)Set-Item Function:script:Move-StarSketchFile -Value $body} $original}
 Assert ((Snapshot $game) -eq $before) 'failed uninstall restores exact original 0.1.2 files and state'
 $passed.Add($kind+' 0.1.2 upgrade/uninstall failures roll back exactly')
}
foreach($managed in @($false,$true)){
 $game=Game ('modified-font-'+$managed) -Managed:$managed;Put (Join-Path $game 'StarSketch-assets\NotoSerifSC.ttf') 'modified by user';$before=Snapshot $game
 $code=if($managed){'SS-INSTALL-022'}else{'SS-INSTALL-021'}
 Failure {Install-StarSketch $game (Join-Path $new 'payload') $new} $code
 Failure {Uninstall-StarSketch $game $package} $code
 Assert ((Snapshot $game) -eq $before) 'modified old font and other files unchanged'
}
$passed.Add('modified 0.1.2 fonts block both update and uninstall without writes')
foreach($relative in @('dxgi.dll','StarSketch-Manager\StarSketch-Setup.ps1')){
 $game=Game ('modified-'+($relative.Replace('\','-'))) -Managed
 Put (Join-Path $game $relative) 'modified by user';$before=Snapshot $game
 Failure {Install-StarSketch $game (Join-Path $new 'payload') $new} 'SS-INSTALL-022'
 Failure {Uninstall-StarSketch $game $package} 'SS-INSTALL-022'
 Assert ((Snapshot $game) -eq $before) 'modified old DLL/manager preserved'
}
$passed.Add('modified 0.1.2 DLL and manager block both actions without writes')
foreach($managed in @($false,$true)){
 $game=Game ('running-'+$managed) -Managed:$managed;$before=Snapshot $game
 try{
  & $script:activeModule {$script:RunningGameProbe={$true}}
  Failure {Install-StarSketch $game (Join-Path $new 'payload') $new} 'SS-INSTALL-019'
  Failure {Uninstall-StarSketch $game $package} 'SS-INSTALL-019'
 }finally{& $script:activeModule {$script:RunningGameProbe={$false}}}
 Assert ((Snapshot $game) -eq $before) 'running old game never mutated'
}
$passed.Add('running 0.1.2 installation blocks both actions for manual/managed installs')
$missing=Game 'missing-font-upgrade' -MissingFont
Install-StarSketch $missing (Join-Path $new 'payload') $new|Out-Null;Verify-Upgraded $missing
$missing=Game 'missing-font-uninstall' -MissingFont
Uninstall-StarSketch $missing $package|Out-Null;Preserved $missing
Assert (!(Test-Path -LiteralPath (Join-Path $missing 'dxgi.dll'))) 'DLL-only 0.1.2 uninstalled'
$passed.Add('manual 0.1.2 with missing font can be upgraded or uninstalled')
$uiManual=Game 'ui-manual-0.1.2';$uiManaged=Game 'ui-managed-0.1.2' -Managed
# Hide the downloaded bundles using checked fixture-local renames. Then use only installed managers.
foreach($directory in @($old,$manual,$new)){
 $full=[IO.Path]::GetFullPath($directory)
 Assert ($full.StartsWith(($run+'\'),[StringComparison]::OrdinalIgnoreCase)) 'bundle rename stays in fixture'
 Rename-Item -LiteralPath $full -NewName ((Split-Path -Leaf $full)+'-hidden')
}
foreach($game in $upgraded){
 Use-Module (Join-Path $game 'StarSketch-Manager')
 $owned=Get-StarSketchPackage (Join-Path $game 'StarSketch-Manager\StarSketch-package.json')
 Uninstall-StarSketch $game $owned|Out-Null;Preserved $game
 Assert (!(Test-Path -LiteralPath (Join-Path $game 'StarSketch-Manager')) -and !(Test-Path -LiteralPath (Join-Path $game 'dxgi.dll'))) 'standalone upgraded manager self-uninstalls'
}
$passed.Add('both upgraded installs self-uninstall after original bundles disappear')
Remove-Module $script:activeModule
$report=[ordered]@{passed=$passed.ToArray();version=$package.version;run=$run;new_bundle=($new+'-hidden');ui_manual=$uiManual;ui_managed=$uiManaged}
$report|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding UTF8
Write-Output ('PASS: '+$passed.Count+' original 0.1.2 compatibility groups')
foreach($item in $passed){Write-Output ('  '+$item)}
Write-Output ('Fixtures and result: '+$run)
