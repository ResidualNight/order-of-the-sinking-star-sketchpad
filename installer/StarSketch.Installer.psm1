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
function Format-StarSketchError([string]$Message){
 if($Message -match '^\[SS-(INSTALL|UPDATE)-\d{3}\] '){return $Message}
 return "[SS-INSTALL-099] $Message"
}
function Get-StarSketchHash([string]$Path){ (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Assert-StarSketchPlainPath([string]$Path){
 $cursor=[IO.Path]::GetFullPath($Path)
 while($cursor){
  if(Test-Path -LiteralPath $cursor){
   if((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw "[SS-INSTALL-001] 路径含有链接或重定向目录，已停止操作：$cursor"}
  }
  $parent=[IO.Directory]::GetParent($cursor)
  if(!$parent){break};$cursor=$parent.FullName
 }
}
function Get-StarSketchGameDirectory([string]$Path){
 if([string]::IsNullOrWhiteSpace($Path)){throw '[SS-INSTALL-002] 请先选择沉星之序的游戏安装文件夹。'}
 $value=$Path.Trim().Trim('"')
 if(![IO.Path]::IsPathRooted($value)){throw '[SS-INSTALL-003] 请选择完整的游戏文件夹路径。'}
 $root=[IO.Path]::GetFullPath($value)
 if($root.Length -gt [IO.Path]::GetPathRoot($root).Length){$root=$root.TrimEnd([IO.Path]::DirectorySeparatorChar)}
 if(!(Test-Path -LiteralPath $root -PathType Container)){throw '[SS-INSTALL-004] 选择的文件夹不存在。'}
 Assert-StarSketchPlainPath $root
 $exe=Join-Path $root 'sinking_star.exe'
 if(!(Test-Path -LiteralPath $exe -PathType Leaf)){throw '[SS-INSTALL-005] 该文件夹没有 sinking_star.exe。请选择游戏程序所在的文件夹。'}
 Assert-StarSketchPlainPath $exe
 return $root
}
function Get-StarSketchTarget([string]$Root,[string]$Relative){
 $allowed=@($script:PayloadMap.Values)+@($script:ManagerFiles | ForEach-Object{"$script:ManagerName/$_"})+@($script:StateName)
 if($Relative -notin $allowed){throw "[SS-INSTALL-006] 安装记录包含不属于草稿纸的文件，已停止操作：$Relative"}
 $full=[IO.Path]::GetFullPath((Join-Path $Root $Relative))
 if(!$full.StartsWith($Root.TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw '[SS-INSTALL-007] 文件路径超出所选游戏目录。'}
 Assert-StarSketchPlainPath $full
 if((Test-Path -LiteralPath $full) -and !(Test-Path -LiteralPath $full -PathType Leaf)){throw "[SS-INSTALL-008] 目标位置不是普通文件：$Relative"}
 return $full
}
function Get-StarSketchPackage([string]$Path){
 if(!(Test-Path -LiteralPath $Path -PathType Leaf)){throw '[SS-INSTALL-009] 缺少安装包清单。请完整解压安装包后重试。'}
 Assert-StarSketchPlainPath $Path
 $info=Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
 if($info.product -ne $script:Product -or $info.schema -ne 1 -or $info.version -notmatch '^\d+\.\d+\.\d+$'){throw '[SS-INSTALL-010] 安装包清单格式无效。'}
 $files=@($info.files);$names=@($files | ForEach-Object{$_.path})
 if($files.Count -ne $script:PayloadMap.Count -or @($names | Sort-Object -Unique).Count -ne $files.Count){throw '[SS-INSTALL-011] 安装包文件清单不完整。'}
 foreach($item in $files){if($item.path -notin @($script:PayloadMap.Keys) -or $item.sha256 -notmatch '^[0-9a-f]{64}$'){throw '[SS-INSTALL-012] 安装包包含未知文件或无效校验值。'}}
 $dll=@($files | Where-Object path -eq 'dxgi.dll')[0]
 if($dll.sha256 -notin @($info.known_dll_sha256)){throw '[SS-INSTALL-013] 安装包 DLL 身份校验失败。'}
 foreach($hash in @($info.known_dll_sha256)){if($hash -notmatch '^[0-9a-f]{64}$'){throw '[SS-INSTALL-014] 安装包版本识别清单无效。'}}
 return $info
}
function Read-StarSketchState([string]$Root,$Package){
 $statePath=Get-StarSketchTarget $Root $script:StateName
 if(!(Test-Path -LiteralPath $statePath)){return $null}
 $state=Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
 if($state.product -ne $script:Product -or $state.schema -ne 1 -or $state.version -notmatch '^\d+\.\d+\.\d+$'){throw '[SS-INSTALL-015] 现有安装记录无效，未改动任何文件。'}
 $files=@($state.files)
 if(!$files.Count -or $files.Count -gt 32){throw '[SS-INSTALL-016] 安装记录文件数量无效。'}
 $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
 foreach($item in $files){
  if(!$seen.Add([string]$item.path) -or $item.sha256 -notmatch '^[0-9a-f]{64}$' -or $item.path -eq $script:StateName){throw '[SS-INSTALL-017] 安装记录包含重复或无效条目。'}
  $null=Get-StarSketchTarget $Root $item.path
 }
 $dll=@($files | Where-Object path -eq 'dxgi.dll')
 if($dll.Count -ne 1 -or $dll[0].sha256 -notin @($Package.known_dll_sha256)){throw '[SS-INSTALL-018] 无法确认现有 DLL 属于本工具，请使用对应版本的管理工具。'}
 return $state
}
function Assert-StarSketchStopped{
 if(& $script:RunningGameProbe){throw '[SS-INSTALL-019] 请先保存进度并退出沉星之序，再安装、更新或卸载。工具不会关闭游戏。'}
}
function Get-StarSketchLegacyState([string]$Root,$Package){
 $dll=Get-StarSketchTarget $Root 'dxgi.dll'
 if(!(Test-Path -LiteralPath $dll)){return $null}
 $hash=Get-StarSketchHash $dll
 if($hash -notin @($Package.known_dll_sha256)){throw '[SS-INSTALL-020] 检测到其他模组或未知来源的 dxgi.dll，已停止操作。请先自行处理兼容性。'}
 $files=@([pscustomobject]@{path='dxgi.dll';sha256=$hash})
 $font=Get-StarSketchTarget $Root 'StarSketch-assets/NotoSerifSC.ttf'
 if(Test-Path -LiteralPath $font){
  $fontHash=Get-StarSketchHash $font
  $expected=@($Package.files | Where-Object path -eq 'StarSketch-assets/NotoSerifSC.ttf')[0].sha256
  if($fontHash -ne $expected){throw '[SS-INSTALL-021] 现有字体文件已被修改，未改动任何文件。请先备份并检查 StarSketch-assets。'}
  $files+=[pscustomobject]@{path='StarSketch-assets/NotoSerifSC.ttf';sha256=$fontHash}
 }
 return [pscustomobject]@{product=$script:Product;schema=1;version='manual';files=$files}
}
function Assert-StarSketchOwnedFiles([string]$Root,$State){
 if(!$State){return}
 foreach($item in @($State.files)){
  $path=Get-StarSketchTarget $Root $item.path
  if((Test-Path -LiteralPath $path) -and (Get-StarSketchHash $path) -ne $item.sha256){throw "[SS-INSTALL-022] 已安装的文件被修改，未改动任何文件：$($item.path)"}
 }
}
function Get-StarSketchStatus([string]$GameDirectory,$Package){
 $root=Get-StarSketchGameDirectory $GameDirectory
 $state=Read-StarSketchState $root $Package
 if($state){Assert-StarSketchOwnedFiles $root $state;return [pscustomobject]@{root=$root;installed=$true;version=$state.version;message="已安装 StarSketch $($state.version)，可更新或卸载。"}}
 $legacy=Get-StarSketchLegacyState $root $Package
 if($legacy){
  $dllHash=@($legacy.files | Where-Object path -eq 'dxgi.dll')[0].sha256
  $bundledHash=@($Package.files | Where-Object path -eq 'dxgi.dll')[0].sha256
  $updateVersion='0.0.0'
  if($dllHash -eq $bundledHash){$updateVersion=$Package.version}
  elseif($dllHash -eq 'fb767808a0389e543af41d3f9496e849ddeda0a1b8b7c8dbc3fb50fa16828617'){$updateVersion='0.1.1'}
  elseif($dllHash -in @('d8b4cd185bcb01f37bc9477bedcff5370928d1ad95430df33b279086199c8c29','9f7bab48c931d01152a328cea80e3f12537750708c4aae5e1f222066505ad575','4a05fdb377c3c1428431e35cf6e1edec3f55b20e799e49ff4c8ccc185aa292d2')){$updateVersion='0.1.2'}
  return [pscustomobject]@{root=$root;installed=$true;version='manual';update_version=$updateVersion;message='检测到手动安装的 StarSketch，可更新或卸载。'}
 }
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
 if([IO.Path]::GetDirectoryName($full) -ne $Root -or [IO.Path]::GetFileName($full) -notmatch '^\.StarSketch-transaction-[0-9a-f]{32}$'){throw '[SS-INSTALL-023] 拒绝清理未验证的临时路径。'}
 Assert-StarSketchPlainPath $full
 foreach($entry in Get-ChildItem -LiteralPath $full -Force -Recurse){if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint){throw '[SS-INSTALL-024] 临时目录出现链接，已保留现场。'}}
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
  try{Assert-StarSketchPlainPath $item.target;if((Test-Path -LiteralPath $item.target) -and (Get-StarSketchHash $item.target) -notin @($item.acceptedHashes)){throw '[SS-INSTALL-025] File changed during rollback'};if($item.backup){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($item.target)) | Out-Null;[IO.File]::Copy($item.backup,$item.target,$true)}elseif(Test-Path -LiteralPath $item.target){[IO.File]::Delete($item.target)}}catch{$errors.Add($item.target)}
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
  if(!(Test-Path -LiteralPath $source -PathType Leaf) -or (Get-StarSketchHash $source) -ne $item.sha256){throw "[SS-INSTALL-026] 安装包文件缺失或损坏，请重新完整解压：$($item.path)"}
  $plan.Add([pscustomobject]@{path=$script:PayloadMap[$item.path];source=$source;sha256=$item.sha256})
 }
 foreach($name in $script:ManagerFiles){
  $source=if($name -eq 'StarSketch-package.json'){Join-Path $PayloadDirectory $name}else{Join-Path $ManagerDirectory $name}
  Assert-StarSketchPlainPath $source
  if(!(Test-Path -LiteralPath $source -PathType Leaf)){throw "[SS-INSTALL-027] 缺少安装管理器文件：$name"}
  $plan.Add([pscustomobject]@{path="$script:ManagerName/$name";source=$source;sha256=(Get-StarSketchHash $source)})
 }
 foreach($item in $plan){
  $target=Get-StarSketchTarget $root $item.path
  $item | Add-Member -NotePropertyName previousHash -NotePropertyValue $null
  if(Test-Path -LiteralPath $target){
   $current=Get-StarSketchHash $target
   $item.previousHash=$current
   $registered=$state -and @($state.files | Where-Object { $_.path -eq $item.path -and $_.sha256 -eq $current }).Count -gt 0
   if(!$registered -and $current -ne $item.sha256){throw "[SS-INSTALL-028] 目标位置已有其他内容，未覆盖：$($item.path)"}
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
   if((Get-StarSketchHash $staged) -ne $item.sha256){throw '[SS-INSTALL-029] 暂存文件校验失败。'}
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
   if($currentHash -ne $job.previousHash){throw "[SS-INSTALL-030] 目标文件在操作期间发生变化，已停止：$($job.path)"}
   $backup=$null
   if(Test-Path -LiteralPath $target){$backup=Join-Path $txn ('old-'+$changes.Count);[IO.File]::Copy($target,$backup)}
   $changes.Add([pscustomobject]@{target=$target;backup=$backup;acceptedHashes=@($job.sha256,$job.previousHash)})
   Commit-StarSketchFile $job.staged $target
  }
 }catch{
  $failure=$_.Exception.Message;$restore=@(Restore-StarSketchChanges $changes)
  for($i=$created.Count-1;$i -ge 0;$i--){if((Test-Path -LiteralPath $created[$i]) -and @(Get-ChildItem -LiteralPath $created[$i] -Force).Count -eq 0){[IO.Directory]::Delete($created[$i])}}
  if($restore.Count){$keep=$true;throw "[SS-INSTALL-031] 安装未完成，部分文件无法还原。备份已保留在 $txn。请退出游戏后检查。原错误：$failure"}
  throw "[SS-INSTALL-032] 安装未完成，已还原本次改动。原因：$failure"
 }finally{if(!$keep){try{Remove-StarSketchTransaction $root $txn}catch{Write-Warning "[SS-INSTALL-036] 临时备份保留在 $txn。原因：$($_.Exception.Message)"}}}
 return [pscustomobject]@{version=$package.version;root=$root;message="StarSketch $($package.version) 安装完成。照常启动游戏，按 E 打开草稿纸。`r`n以后可从游戏目录的 StarSketch-Manager 文件夹打开管理工具。"}
}
function Uninstall-StarSketch([string]$GameDirectory,$Package){
 $root=Get-StarSketchGameDirectory $GameDirectory
 Assert-StarSketchStopped
 $statePath=Get-StarSketchTarget $root $script:StateName
 $originalStateHash=if(Test-Path -LiteralPath $statePath){Get-StarSketchHash $statePath}else{$null}
 $state=Read-StarSketchState $root $Package
 if(!$state){$state=Get-StarSketchLegacyState $root $Package}
 if(!$state){throw '[SS-INSTALL-033] 该目录没有可确认的 StarSketch 安装。'}
 Assert-StarSketchOwnedFiles $root $state
 $txn=New-StarSketchTransaction $root;$changes=[Collections.Generic.List[object]]::new();$keep=$false
 try{
  Assert-StarSketchStopped
  $paths=@($state.files)
  if($originalStateHash){$paths+= [pscustomobject]@{path=$script:StateName;sha256=$originalStateHash}}
  foreach($item in $paths){
   $target=Get-StarSketchTarget $root $item.path
   if(!(Test-Path -LiteralPath $target)){continue}
   if($item.sha256 -and (Get-StarSketchHash $target) -ne $item.sha256){throw "[SS-INSTALL-030] 文件在操作前发生变化：$($item.path)"}
   $backup=Join-Path $txn ('removed-'+$changes.Count)
   Move-StarSketchFile $target $backup
   $changes.Add([pscustomobject]@{target=$target;backup=$backup;acceptedHashes=@($item.sha256)})
  }
 }catch{
  $failure=$_.Exception.Message;$restore=@(Restore-StarSketchChanges $changes)
  if($restore.Count){$keep=$true;throw "[SS-INSTALL-034] 卸载未完成，部分文件无法还原。备份已保留在 $txn。原错误：$failure"}
  throw "[SS-INSTALL-035] 卸载未完成，已还原本次改动。原因：$failure"
 }finally{if(!$keep){try{Remove-StarSketchTransaction $root $txn}catch{Write-Warning "[SS-INSTALL-036] 临时备份保留在 $txn。原因：$($_.Exception.Message)"}}}
 foreach($relative in @('StarSketch-assets/licenses','StarSketch-assets',$script:ManagerName)){
  $path=[IO.Path]::GetFullPath((Join-Path $root $relative));Assert-StarSketchPlainPath $path
  if((Test-Path -LiteralPath $path -PathType Container) -and @(Get-ChildItem -LiteralPath $path -Force).Count -eq 0){[IO.Directory]::Delete($path)}
 }
 return [pscustomobject]@{root=$root;message='草稿纸已卸载。游戏文件、存档及其他文件保留。手动复制的旧说明或许可证可能仍在目录中。'}
}
# Online updates use the same owned-file transaction as local installs. Downloaded
# scripts are installed as data; this running manager never executes their code.
$script:ReleaseApi='https://api.github.com/repos/ResidualNight/order-of-the-sinking-star-sketchpad/releases?per_page=100'
$script:ReleaseRoot='https://github.com/ResidualNight/order-of-the-sinking-star-sketchpad/releases'
function Assert-StarSketchUpdateUri([string]$Address,[switch]$Redirect){
 $uri=$null
 if(![Uri]::TryCreate($Address,[UriKind]::Absolute,[ref]$uri) -or $uri.Scheme -ne 'https' -or $uri.Port -ne 443 -or $uri.UserInfo){throw '[SS-UPDATE-004] 更新地址不属于项目官方 HTTPS 下载源。'}
 $official=$Address -ceq $script:ReleaseApi -or $Address -cmatch '^https://github\.com/ResidualNight/order-of-the-sinking-star-sketchpad/releases/download/v[0-9]+\.[0-9]+\.[0-9]+(-test)?/StarSketch-[0-9]+\.[0-9]+\.[0-9]+-installer(\.zip|-manifest\.json)$'
 if(!$official -and !($Redirect -and $uri.Host -in @('release-assets.githubusercontent.com','objects.githubusercontent.com'))){throw '[SS-UPDATE-004] 更新地址不属于项目官方 HTTPS 下载源。'}
}
function Read-StarSketchHttp([string]$Address,[long]$Limit,[string]$Destination){
 Assert-StarSketchUpdateUri $Address
 $response=$null;$input=$null;$output=$null
 try{
  [Net.ServicePointManager]::SecurityProtocol=[Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
  $request=[Net.HttpWebRequest]::Create($Address);$request.UserAgent='StarSketch-Updater/1.0';$request.Accept='application/vnd.github+json';$request.Timeout=15000;$request.ReadWriteTimeout=15000;$request.MaximumAutomaticRedirections=5
  $response=$request.GetResponse();Assert-StarSketchUpdateUri $response.ResponseUri.AbsoluteUri -Redirect
  if($response.ContentLength -gt $Limit){throw '[SS-UPDATE-011] 更新下载大小超过允许范围，已停止。'}
  $input=$response.GetResponseStream()
  if($Destination){Assert-StarSketchPlainPath $Destination;$output=[IO.File]::Open($Destination,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)}else{$output=[IO.MemoryStream]::new()}
  $buffer=New-Object byte[] 65536;$total=0L;$deadline=[DateTime]::UtcNow.AddMinutes(2)
  while(($count=$input.Read($buffer,0,$buffer.Length)) -gt 0){
   $total+=$count;if($total -gt $Limit){throw '[SS-UPDATE-011] 更新下载大小超过允许范围，已停止。'}
   if([DateTime]::UtcNow -gt $deadline){throw '[SS-UPDATE-001] GitHub 下载超时，请稍后重试。'}
   $output.Write($buffer,0,$count)
  }
  if(!$Destination){return [Text.Encoding]::UTF8.GetString($output.ToArray())}
 }catch{
  if($_.Exception.Message -match '^\[SS-'){throw}
  throw "[SS-UPDATE-001] 无法连接 GitHub 或下载更新，请检查网络、稍后重试。原因：$($_.Exception.Message)"
 }finally{if($output){$output.Dispose()};if($input){$input.Dispose()};if($response){$response.Dispose()}}
}
function Get-StarSketchReleaseAsset($Release,[string]$Name,[long]$Limit){
 if(!$Release -or !$Release.PSObject.Properties['assets']){throw '[SS-UPDATE-002] GitHub 发布文件信息无效。'}
 $matches=@($Release.assets | Where-Object { $_.name -ceq $Name })
 if($matches.Count -ne 1){throw "[SS-UPDATE-003] 新版缺少唯一的更新文件：$Name。请稍后重试。"}
 $asset=$matches[0];$expected="$script:ReleaseRoot/download/$($Release.tag_name)/$Name"
 foreach($field in @('browser_download_url','state','size')){if(!$asset.PSObject.Properties[$field]){throw '[SS-UPDATE-002] GitHub 发布文件信息不完整。'}}
 if($asset.browser_download_url -cne $expected){throw '[SS-UPDATE-004] 更新文件下载地址与项目发布记录不一致。'}
 Assert-StarSketchUpdateUri $asset.browser_download_url
 if($asset.state -ne 'uploaded' -or $asset.size -lt 1 -or $asset.size -gt $Limit){throw '[SS-UPDATE-011] 更新文件大小或上传状态无效。'}
 $digest=$asset.PSObject.Properties['digest']
 if($digest -and $digest.Value -and $digest.Value -cnotmatch '^sha256:[0-9a-f]{64}$'){throw '[SS-UPDATE-002] GitHub 更新文件校验信息无效。'}
 return $asset
}
function Get-StarSketchUpdate([string]$CurrentVersion){
 if($CurrentVersion -notmatch '^\d+\.\d+\.\d+$'){throw '[SS-UPDATE-002] 当前版本号无效，无法检查更新。'}
 try{$current=[version]$CurrentVersion}catch{throw '[SS-UPDATE-002] 当前版本号无效，无法检查更新。'}
 try{$decoded=(Read-StarSketchHttp $script:ReleaseApi 4MB) | ConvertFrom-Json;$releases=@($decoded)}catch{if($_.Exception.Message -match '^\[SS-'){throw};throw '[SS-UPDATE-002] GitHub 发布信息无法解析，请稍后重试。'}
 $choices=@(foreach($release in $releases){
  foreach($field in @('draft','tag_name','prerelease')){if(!$release -or !$release.PSObject.Properties[$field]){throw '[SS-UPDATE-002] GitHub 发布信息不完整，请稍后重试。'}}
  if($release.draft -or $release.tag_name -cnotmatch '^v(\d+\.\d+\.\d+)(-test)?$'){continue}
  $version=$Matches[1]
  try{$number=[version]$version}catch{continue}
  [pscustomobject]@{number=$number;release=$release;version=$version;stable=!$release.prerelease}
 })
 if(!$choices.Count){throw '[SS-UPDATE-002] GitHub 未返回可识别的发布版本。'}
 $latest=$choices | Sort-Object -Property @{Expression='number';Descending=$true},@{Expression='stable';Descending=$true} | Select-Object -First 1
 if($latest.number -le $current){return [pscustomobject]@{available=$false;current_version=$CurrentVersion;version=$latest.version;message="当前已是最新版本（$CurrentVersion），无需更新。"}}
 $archive=Get-StarSketchReleaseAsset $latest.release ("StarSketch-$($latest.version)-installer.zip") 128MB
 $receipt=Get-StarSketchReleaseAsset $latest.release ("StarSketch-$($latest.version)-installer-manifest.json") 64KB
 return [pscustomobject]@{available=$true;current_version=$CurrentVersion;version=$latest.version;tag=$latest.release.tag_name;release_url="$script:ReleaseRoot/tag/$($latest.release.tag_name)";archive=$archive;receipt=$receipt;message="发现新版 $($latest.version)（当前 $CurrentVersion）。"}
}
function Expand-StarSketchUpdate([string]$Archive,[string]$Destination){
 Add-Type -AssemblyName System.IO.Compression
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $expected=@('StarSketch-Setup.ps1','StarSketch.Installer.psm1','StarSketch-安装卸载.cmd','StarSketch-安装卸载.exe','安装说明.txt')+@($script:PayloadMap.Keys | ForEach-Object{"payload/$_"})+@('payload/StarSketch-package.json')
 $zip=[IO.Compression.ZipFile]::OpenRead($Archive)
 try{
  if($zip.Entries.Count -ne $expected.Count){throw '[SS-UPDATE-007] 更新包文件布局无效，未安装。'}
  $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase);$total=0L
  foreach($entry in $zip.Entries){
   $total+=$entry.Length
   if($entry.FullName -cnotin $expected -or !$seen.Add($entry.FullName) -or $entry.Length -gt 50MB -or $total -gt 128MB -or ($entry.ExternalAttributes -band 1024) -or (($entry.ExternalAttributes -shr 16) -band 0xf000) -eq 0xa000){throw '[SS-UPDATE-007] 更新包含未知路径、重复文件、链接或过大文件，未安装。'}
  }
  foreach($entry in $zip.Entries){
   $target=Join-Path $Destination $entry.FullName;Assert-StarSketchPlainPath $target
   [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
   [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$false)
  }
 }finally{$zip.Dispose()}
}
function Install-StarSketchUpdate([string]$GameDirectory,$Update){
 $root=Get-StarSketchGameDirectory $GameDirectory;Assert-StarSketchStopped
 if(!$Update.available -or $Update.version -notmatch '^\d+\.\d+\.\d+$' -or $Update.tag -cnotmatch ('^v'+[regex]::Escape($Update.version)+'(-test)?$')){throw '[SS-UPDATE-002] 更新版本信息无效，请重新检查更新。'}
 $release=[pscustomobject]@{tag_name=$Update.tag;assets=@($Update.archive,$Update.receipt)}
 $asset=Get-StarSketchReleaseAsset $release ("StarSketch-$($Update.version)-installer.zip") 128MB
 $receiptAsset=Get-StarSketchReleaseAsset $release ("StarSketch-$($Update.version)-installer-manifest.json") 64KB
 $stage=New-StarSketchTransaction $root
 try{
  Write-Progress -Activity '更新草稿纸' -Status '下载并校验更新清单'
  $receiptPath=Join-Path $stage 'receipt.json';Read-StarSketchHttp $receiptAsset.browser_download_url 64KB $receiptPath
  $digest=$receiptAsset.PSObject.Properties['digest'];if($digest -and $digest.Value -and ('sha256:'+(Get-StarSketchHash $receiptPath)) -cne $digest.Value){throw '[SS-UPDATE-005] 更新清单与 GitHub 校验值不一致。'}
  try{$receipt=Get-Content -LiteralPath $receiptPath -Raw -Encoding UTF8 | ConvertFrom-Json}catch{throw '[SS-UPDATE-005] 更新清单无法解析。'}
  foreach($field in @('version','archive','archive_sha256','dll_sha256')){if(!$receipt -or !$receipt.PSObject.Properties[$field]){throw '[SS-UPDATE-005] 更新清单缺少必要信息。'}}
  if($receipt.version -cne $Update.version -or $receipt.archive -cne $asset.name -or $receipt.archive_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $receipt.dll_sha256 -cnotmatch '^[0-9a-f]{64}$'){throw '[SS-UPDATE-005] 更新清单的版本、文件名或校验值无效。'}
  $digest=$asset.PSObject.Properties['digest'];if($digest -and $digest.Value -and $digest.Value -cne ('sha256:'+$receipt.archive_sha256)){throw '[SS-UPDATE-005] 更新清单与 GitHub 安装包校验值不一致。'}
  Write-Progress -Activity '更新草稿纸' -Status '下载安装包并校验完整性'
  $archive=Join-Path $stage 'update.zip';Read-StarSketchHttp $asset.browser_download_url 128MB $archive
  if((Get-StarSketchHash $archive) -cne $receipt.archive_sha256){throw '[SS-UPDATE-006] 更新包 SHA256 校验失败，未改动原安装。'}
  $bundle=Join-Path $stage 'bundle';Expand-StarSketchUpdate $archive $bundle
  $payload=Join-Path $bundle 'payload';$package=Get-StarSketchPackage (Join-Path $payload 'StarSketch-package.json')
  $dll=@($package.files | Where-Object path -eq 'dxgi.dll')[0]
  if($package.version -cne $Update.version -or $dll.sha256 -cne $receipt.dll_sha256){throw '[SS-UPDATE-008] 更新包版本或 DLL 身份与清单不一致。'}
  Write-Progress -Activity '更新草稿纸' -Status '校验安装归属并更新，失败时自动还原'
  # Recheck the selected installation immediately before the transaction. The
  # game might have started, or another manager might already have updated it.
  Assert-StarSketchStopped
  $state=Read-StarSketchState $root $package
  if($state -and [version]$state.version -ge [version]$package.version){Assert-StarSketchOwnedFiles $root $state;return [pscustomobject]@{version=$state.version;root=$root;message="当前已安装 $($state.version)，无需覆盖。"}}
  $result=Install-StarSketch $root $payload $bundle
  return [pscustomobject]@{version=$result.version;root=$root;message="已更新到 StarSketch $($result.version)。照常启动游戏，按 E 打开草稿纸。`r`n管理入口已同步更新，下次可从游戏目录 StarSketch-Manager 打开。"}
 }catch{if($_.Exception.Message -match '^\[SS-'){throw};throw "[SS-UPDATE-009] 更新未完成。原因：$($_.Exception.Message)"}
 finally{try{Remove-StarSketchTransaction $root $stage}catch{Write-Warning "[SS-UPDATE-012] 下载临时目录保留在 $stage。原因：$($_.Exception.Message)"}}
}
Export-ModuleMember -Function Get-StarSketchPackage,Get-StarSketchStatus,Install-StarSketch,Uninstall-StarSketch,Format-StarSketchError,Get-StarSketchUpdate,Install-StarSketchUpdate
