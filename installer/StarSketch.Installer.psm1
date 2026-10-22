Set-StrictMode -Version 2
$ErrorActionPreference='Stop'
$script:Product='StarSketch'
$script:StateName='StarSketch-install.json'
$script:ManagerName='StarSketch-Manager'
$script:RunningGameProbe={ @(Get-Process -Name sinking_star -ErrorAction SilentlyContinue).Count -gt 0 }
$script:PayloadMap=[ordered]@{
 'dxgi.dll'='dxgi.dll'
 'StarSketch-assets/NotoSerifSC.ttf'='StarSketch-assets/NotoSerifSC.ttf'
 '使用说明.txt'='StarSketch-assets/使用说明.txt'
 'licenses/Dear-ImGui-MIT.txt'='StarSketch-assets/licenses/Dear-ImGui-MIT.txt'
 'licenses/LLVM.txt'='StarSketch-assets/licenses/LLVM.txt'
 'licenses/MinGW-COPYING'='StarSketch-assets/licenses/MinGW-COPYING'
 'licenses/MinGW-COPYING.MinGW-w64.txt'='StarSketch-assets/licenses/MinGW-COPYING.MinGW-w64.txt'
 'licenses/MinGW-COPYING.MinGW-w64-runtime.txt'='StarSketch-assets/licenses/MinGW-COPYING.MinGW-w64-runtime.txt'
 'licenses/MinGW-COPYING.winpthreads.txt'='StarSketch-assets/licenses/MinGW-COPYING.winpthreads.txt'
 'licenses/OFL-NotoSerifSC.txt'='StarSketch-assets/licenses/OFL-NotoSerifSC.txt'
 'licenses/StarSketch-MIT.txt'='StarSketch-assets/licenses/StarSketch-MIT.txt'
}
$script:ManagerFiles=@('StarSketch-Setup.ps1','StarSketch.Installer.psm1','StarSketch-安装卸载.cmd','StarSketch-package.json')
function Get-StarSketchHash([string]$Path){ (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Assert-StarSketchPlainPath([string]$Path){
 $cursor=[IO.Path]::GetFullPath($Path)
 while($cursor){
  if(Test-Path -LiteralPath $cursor){
   if((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw "路径含有链接或重定向目录，已停止操作：$cursor"}
  }
  $parent=[IO.Directory]::GetParent($cursor)
  if(!$parent){break};$cursor=$parent.FullName
 }
}
function Get-StarSketchGameDirectory([string]$Path){
 if([string]::IsNullOrWhiteSpace($Path)){throw '请先选择沉星之序的游戏安装文件夹。'}
 $value=$Path.Trim().Trim('"')
 if(![IO.Path]::IsPathRooted($value)){throw '请选择完整的游戏文件夹路径。'}
 $root=[IO.Path]::GetFullPath($value)
 if($root.Length -gt [IO.Path]::GetPathRoot($root).Length){$root=$root.TrimEnd([IO.Path]::DirectorySeparatorChar)}
 if(!(Test-Path -LiteralPath $root -PathType Container)){throw '选择的文件夹不存在。'}
 Assert-StarSketchPlainPath $root
 $exe=Join-Path $root 'sinking_star.exe'
 if(!(Test-Path -LiteralPath $exe -PathType Leaf)){throw '该文件夹没有 sinking_star.exe。请选择游戏程序所在的文件夹。'}
 Assert-StarSketchPlainPath $exe
 return $root
}
function Get-StarSketchTarget([string]$Root,[string]$Relative){
 $allowed=@($script:PayloadMap.Values)+@($script:ManagerFiles | ForEach-Object{"$script:ManagerName/$_"})+@($script:StateName)
 if($Relative -notin $allowed){throw "安装记录包含不属于草稿纸的文件，已停止操作：$Relative"}
 $full=[IO.Path]::GetFullPath((Join-Path $Root $Relative))
 if(!$full.StartsWith($Root.TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw '文件路径超出所选游戏目录。'}
 Assert-StarSketchPlainPath $full
 if((Test-Path -LiteralPath $full) -and !(Test-Path -LiteralPath $full -PathType Leaf)){throw "目标位置不是普通文件：$Relative"}
 return $full
}
function Get-StarSketchPackage([string]$Path){
 if(!(Test-Path -LiteralPath $Path -PathType Leaf)){throw '缺少安装包清单。请完整解压安装包后重试。'}
 Assert-StarSketchPlainPath $Path
 $info=Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
 if($info.product -ne $script:Product -or $info.schema -ne 1 -or $info.version -notmatch '^\d+\.\d+\.\d+$'){throw '安装包清单格式无效。'}
 $files=@($info.files);$names=@($files | ForEach-Object{$_.path})
 if($files.Count -ne $script:PayloadMap.Count -or @($names | Sort-Object -Unique).Count -ne $files.Count){throw '安装包文件清单不完整。'}
 foreach($item in $files){if($item.path -notin @($script:PayloadMap.Keys) -or $item.sha256 -notmatch '^[0-9a-f]{64}$'){throw '安装包包含未知文件或无效校验值。'}}
 $dll=@($files | Where-Object path -eq 'dxgi.dll')[0]
 if($dll.sha256 -notin @($info.known_dll_sha256)){throw '安装包 DLL 身份校验失败。'}
 foreach($hash in @($info.known_dll_sha256)){if($hash -notmatch '^[0-9a-f]{64}$'){throw '安装包版本识别清单无效。'}}
 return $info
}
function Read-StarSketchState([string]$Root,$Package){
 $statePath=Get-StarSketchTarget $Root $script:StateName
 if(!(Test-Path -LiteralPath $statePath)){return $null}
 $state=Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
 if($state.product -ne $script:Product -or $state.schema -ne 1 -or $state.version -notmatch '^\d+\.\d+\.\d+$'){throw '现有安装记录无效，未改动任何文件。'}
 $files=@($state.files)
 if(!$files.Count -or $files.Count -gt 32){throw '安装记录文件数量无效。'}
 $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
 foreach($item in $files){
  if(!$seen.Add([string]$item.path) -or $item.sha256 -notmatch '^[0-9a-f]{64}$' -or $item.path -eq $script:StateName){throw '安装记录包含重复或无效条目。'}
  $null=Get-StarSketchTarget $Root $item.path
 }
 $dll=@($files | Where-Object path -eq 'dxgi.dll')
 if($dll.Count -ne 1 -or $dll[0].sha256 -notin @($Package.known_dll_sha256)){throw '无法确认现有 DLL 属于本工具，请使用对应版本的管理工具。'}
 return $state
}
function Assert-StarSketchStopped{
 if(& $script:RunningGameProbe){throw '请先保存进度并退出沉星之序，再安装、更新或卸载。工具不会关闭游戏。'}
}
function Get-StarSketchLegacyState([string]$Root,$Package){
 $dll=Get-StarSketchTarget $Root 'dxgi.dll'
 if(!(Test-Path -LiteralPath $dll)){return $null}
 $hash=Get-StarSketchHash $dll
 if($hash -notin @($Package.known_dll_sha256)){throw '检测到其他模组或未知来源的 dxgi.dll，已停止操作。请先自行处理兼容性。'}
 $files=@([pscustomobject]@{path='dxgi.dll';sha256=$hash})
 $font=Get-StarSketchTarget $Root 'StarSketch-assets/NotoSerifSC.ttf'
 if(Test-Path -LiteralPath $font){
  $fontHash=Get-StarSketchHash $font
  $expected=@($Package.files | Where-Object path -eq 'StarSketch-assets/NotoSerifSC.ttf')[0].sha256
  if($fontHash -ne $expected){throw '现有字体文件已被修改，未改动任何文件。请先备份并检查 StarSketch-assets。'}
  $files+=[pscustomobject]@{path='StarSketch-assets/NotoSerifSC.ttf';sha256=$fontHash}
 }
 return [pscustomobject]@{product=$script:Product;schema=1;version='manual';files=$files}
}
function Assert-StarSketchOwnedFiles([string]$Root,$State){
 if(!$State){return}
 foreach($item in @($State.files)){
  $path=Get-StarSketchTarget $Root $item.path
  if((Test-Path -LiteralPath $path) -and (Get-StarSketchHash $path) -ne $item.sha256){throw "已安装的文件被修改，未改动任何文件：$($item.path)"}
 }
}
function Get-StarSketchStatus([string]$GameDirectory,$Package){
 $root=Get-StarSketchGameDirectory $GameDirectory
 $state=Read-StarSketchState $root $Package
 if($state){Assert-StarSketchOwnedFiles $root $state;return [pscustomobject]@{root=$root;installed=$true;version=$state.version;message="已安装 StarSketch $($state.version)，可更新或卸载。"}}
 $legacy=Get-StarSketchLegacyState $root $Package
 if($legacy){return [pscustomobject]@{root=$root;installed=$true;version='manual';message='检测到手动安装的 StarSketch，可更新或卸载。'}}
 return [pscustomobject]@{root=$root;installed=$false;version='';message='游戏目录正确，可以安装草稿纸。'}
}
function New-StarSketchTransaction([string]$Root){
 $path=Join-Path $Root ('.StarSketch-transaction-'+[guid]::NewGuid().ToString('N'))
 Assert-StarSketchPlainPath $path
 [IO.Directory]::CreateDirectory($path) | Out-Null
 return $path
}
function Remove-StarSketchTransaction([string]$Root,[string]$Path){
 $full=[IO.Path]::GetFullPath($Path)
 if([IO.Path]::GetDirectoryName($full) -ne $Root -or [IO.Path]::GetFileName($full) -notmatch '^\.StarSketch-transaction-[0-9a-f]{32}$'){throw '拒绝清理未验证的临时路径。'}
 Assert-StarSketchPlainPath $full
 foreach($entry in Get-ChildItem -LiteralPath $full -Force -Recurse){if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint){throw '临时目录出现链接，已保留现场。'}}
 Remove-Item -LiteralPath $full -Recurse -Force
}
function Ensure-StarSketchParent([string]$Root,[string]$Relative,[Collections.Generic.List[string]]$Created){
 $target=Get-StarSketchTarget $Root $Relative
 $parent=[IO.Path]::GetDirectoryName($target)
 $missing=[Collections.Generic.List[string]]::new()
 for($cursor=$parent;$cursor -ne $Root -and !(Test-Path -LiteralPath $cursor);$cursor=[IO.Path]::GetDirectoryName($cursor)){$missing.Add($cursor)}
 for($i=$missing.Count-1;$i -ge 0;$i--){[IO.Directory]::CreateDirectory($missing[$i]) | Out-Null;$Created.Add($missing[$i])}
 return $target
}
function Commit-StarSketchFile([string]$Source,[string]$Target){
 if(Test-Path -LiteralPath $Target){[IO.File]::Replace($Source,$Target,($Source+'.replaced'))}else{[IO.File]::Move($Source,$Target)}
}
function Move-StarSketchFile([string]$Source,[string]$Target){[IO.File]::Move($Source,$Target)}
function Restore-StarSketchChanges($Changes){
 $errors=[Collections.Generic.List[string]]::new()
 for($i=$Changes.Count-1;$i -ge 0;$i--){
  $item=$Changes[$i]
  try{Assert-StarSketchPlainPath $item.target;if((Test-Path -LiteralPath $item.target) -and (Get-StarSketchHash $item.target) -notin @($item.acceptedHashes)){throw 'File changed during rollback'};if($item.backup){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($item.target)) | Out-Null;[IO.File]::Copy($item.backup,$item.target,$true)}elseif(Test-Path -LiteralPath $item.target){[IO.File]::Delete($item.target)}}catch{$errors.Add($item.target)}
 }
 return @($errors.ToArray())
}
function Install-StarSketch([string]$GameDirectory,[string]$PayloadDirectory,[string]$ManagerDirectory){
 $root=Get-StarSketchGameDirectory $GameDirectory
 Assert-StarSketchStopped
 $statePath=Get-StarSketchTarget $root $script:StateName
 $originalStateHash=if(Test-Path -LiteralPath $statePath){Get-StarSketchHash $statePath}else{$null}
 $package=Get-StarSketchPackage (Join-Path $PayloadDirectory 'StarSketch-package.json')
 $state=Read-StarSketchState $root $package
 if(!$state){$state=Get-StarSketchLegacyState $root $package}
 Assert-StarSketchOwnedFiles $root $state
 $plan=[Collections.Generic.List[object]]::new()
 foreach($item in @($package.files)){
  $source=Join-Path $PayloadDirectory $item.path
  Assert-StarSketchPlainPath $source
  if(!(Test-Path -LiteralPath $source -PathType Leaf) -or (Get-StarSketchHash $source) -ne $item.sha256){throw "安装包文件缺失或损坏，请重新完整解压：$($item.path)"}
  $plan.Add([pscustomobject]@{path=$script:PayloadMap[$item.path];source=$source;sha256=$item.sha256})
 }
 foreach($name in $script:ManagerFiles){
  $source=if($name -eq 'StarSketch-package.json'){Join-Path $PayloadDirectory $name}else{Join-Path $ManagerDirectory $name}
  Assert-StarSketchPlainPath $source
  if(!(Test-Path -LiteralPath $source -PathType Leaf)){throw "缺少安装管理器文件：$name"}
  $plan.Add([pscustomobject]@{path="$script:ManagerName/$name";source=$source;sha256=(Get-StarSketchHash $source)})
 }
 foreach($item in $plan){
  $target=Get-StarSketchTarget $root $item.path
  $item | Add-Member -NotePropertyName previousHash -NotePropertyValue $null
  if(Test-Path -LiteralPath $target){
   $current=Get-StarSketchHash $target
   $item.previousHash=$current
   $registered=$state -and @($state.files | Where-Object { $_.path -eq $item.path -and $_.sha256 -eq $current }).Count -gt 0
   if(!$registered -and $current -ne $item.sha256){throw "目标位置已有其他内容，未覆盖：$($item.path)"}
  }
 }
 $txn=New-StarSketchTransaction $root
 $changes=[Collections.Generic.List[object]]::new();$created=[Collections.Generic.List[string]]::new();$keep=$false
 try{
  # Stage every input and backup before modifying installed files.
  $jobs=[Collections.Generic.List[object]]::new();$number=0
  foreach($item in $plan){
   $staged=Join-Path $txn ('new-'+$number)
   [IO.File]::Copy($item.source,$staged)
   if((Get-StarSketchHash $staged) -ne $item.sha256){throw '暂存文件校验失败。'}
   $jobs.Add([pscustomobject]@{path=$item.path;staged=$staged;sha256=$item.sha256;previousHash=$item.previousHash});$number++
  }
  $record=[ordered]@{product=$script:Product;schema=1;version=$package.version;files=@($plan | ForEach-Object{[ordered]@{path=$_.path;sha256=$_.sha256}})}
  $stateStage=Join-Path $txn 'new-state'
  [IO.File]::WriteAllText($stateStage,($record | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
  $jobs.Add([pscustomobject]@{path=$script:StateName;staged=$stateStage;sha256=(Get-StarSketchHash $stateStage);previousHash=$originalStateHash})
  Assert-StarSketchStopped
  foreach($job in $jobs){
   $target=Ensure-StarSketchParent $root $job.path $created
   $currentHash=if(Test-Path -LiteralPath $target){Get-StarSketchHash $target}else{$null}
   if($currentHash -ne $job.previousHash){throw "目标文件在操作期间发生变化，已停止：$($job.path)"}
   $backup=$null
   if(Test-Path -LiteralPath $target){$backup=Join-Path $txn ('old-'+$changes.Count);[IO.File]::Copy($target,$backup)}
   $changes.Add([pscustomobject]@{target=$target;backup=$backup;acceptedHashes=@($job.sha256,$job.previousHash)})
   Commit-StarSketchFile $job.staged $target
  }
 }catch{
  $failure=$_.Exception.Message;$restore=@(Restore-StarSketchChanges $changes)
  for($i=$created.Count-1;$i -ge 0;$i--){if((Test-Path -LiteralPath $created[$i]) -and @(Get-ChildItem -LiteralPath $created[$i] -Force).Count -eq 0){[IO.Directory]::Delete($created[$i])}}
  if($restore.Count){$keep=$true;throw "安装未完成，部分文件无法还原。备份已保留在 $txn。请退出游戏后检查。原错误：$failure"}
  throw "安装未完成，已还原本次改动。原因：$failure"
 }finally{if(!$keep){try{Remove-StarSketchTransaction $root $txn}catch{Write-Warning "临时备份保留在 $txn"}}}
 return [pscustomobject]@{version=$package.version;root=$root;message="StarSketch $($package.version) 安装完成。照常启动游戏，按 E 打开草稿纸。`r`n以后可从游戏目录的 StarSketch-Manager 文件夹打开管理工具。"}
}
function Uninstall-StarSketch([string]$GameDirectory,$Package){
 $root=Get-StarSketchGameDirectory $GameDirectory
 Assert-StarSketchStopped
 $statePath=Get-StarSketchTarget $root $script:StateName
 $originalStateHash=if(Test-Path -LiteralPath $statePath){Get-StarSketchHash $statePath}else{$null}
 $state=Read-StarSketchState $root $Package
 if(!$state){$state=Get-StarSketchLegacyState $root $Package}
 if(!$state){throw '该目录没有可确认的 StarSketch 安装。'}
 Assert-StarSketchOwnedFiles $root $state
 $txn=New-StarSketchTransaction $root;$changes=[Collections.Generic.List[object]]::new();$keep=$false
 try{
  Assert-StarSketchStopped
  $paths=@($state.files)
  if($originalStateHash){$paths+= [pscustomobject]@{path=$script:StateName;sha256=$originalStateHash}}
  foreach($item in $paths){
   $target=Get-StarSketchTarget $root $item.path
   if(!(Test-Path -LiteralPath $target)){continue}
   if($item.sha256 -and (Get-StarSketchHash $target) -ne $item.sha256){throw "文件在操作前发生变化：$($item.path)"}
   $backup=Join-Path $txn ('removed-'+$changes.Count)
   Move-StarSketchFile $target $backup
   $changes.Add([pscustomobject]@{target=$target;backup=$backup;acceptedHashes=@($item.sha256)})
  }
 }catch{
  $failure=$_.Exception.Message;$restore=@(Restore-StarSketchChanges $changes)
  if($restore.Count){$keep=$true;throw "卸载未完成，部分文件无法还原。备份已保留在 $txn。原错误：$failure"}
  throw "卸载未完成，已还原本次改动。原因：$failure"
 }finally{if(!$keep){try{Remove-StarSketchTransaction $root $txn}catch{Write-Warning "临时备份保留在 $txn"}}}
 foreach($relative in @('StarSketch-assets/licenses','StarSketch-assets',$script:ManagerName)){
  $path=[IO.Path]::GetFullPath((Join-Path $root $relative));Assert-StarSketchPlainPath $path
  if((Test-Path -LiteralPath $path -PathType Container) -and @(Get-ChildItem -LiteralPath $path -Force).Count -eq 0){[IO.Directory]::Delete($path)}
 }
 return [pscustomobject]@{root=$root;message='草稿纸已卸载。游戏文件、存档及其他文件保留。手动复制的旧说明或许可证可能仍在目录中。'}
}
Export-ModuleMember -Function Get-StarSketchPackage,Get-StarSketchStatus,Install-StarSketch,Uninstall-StarSketch
