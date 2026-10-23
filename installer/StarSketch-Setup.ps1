[CmdletBinding()]
param([string]$GameDirectory)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
try{
 Import-Module (Join-Path $PSScriptRoot 'StarSketch.Installer.psm1') -Force -DisableNameChecking
 $payload=Join-Path $PSScriptRoot 'payload'
 $packagePath=Join-Path $payload 'StarSketch-package.json'
 $script:hasPayload=Test-Path -LiteralPath $packagePath
 if(!$script:hasPayload){$packagePath=Join-Path $PSScriptRoot 'StarSketch-package.json'}
 $script:package=Get-StarSketchPackage $packagePath
 if(!$GameDirectory -and !$script:hasPayload){$GameDirectory=Split-Path -Parent $PSScriptRoot}
 $bg=[Drawing.Color]::FromArgb(16,24,37);$panelColor=[Drawing.Color]::FromArgb(24,35,51);$gold=[Drawing.Color]::FromArgb(230,195,115);$textColor=[Drawing.Color]::FromArgb(234,232,222)
 $form=[Windows.Forms.Form]::new();$form.Text='沉星草稿 · 安装、更新与卸载';$form.StartPosition='CenterScreen';$form.ClientSize=[Drawing.Size]::new(900,590);$form.MinimumSize=[Drawing.Size]::new(840,560);$form.BackColor=$bg;$form.ForeColor=$textColor;$form.Font=[Drawing.Font]::new('Microsoft YaHei UI',10);$form.AutoScaleMode='Dpi'
 $layout=[Windows.Forms.TableLayoutPanel]::new();$layout.Dock='Fill';$layout.Padding=[Windows.Forms.Padding]::new(28);$layout.ColumnCount=1;$layout.RowCount=8
 foreach($h in @(54,40,32,46,55,70,0,40)){$row=[Windows.Forms.RowStyle]::new();if($h){$row.SizeType='Absolute';$row.Height=$h}else{$row.SizeType='Percent';$row.Height=100};$layout.RowStyles.Add($row) | Out-Null}
 $form.Controls.Add($layout)
 function Add-Label([string]$Value,[int]$Row){$label=[Windows.Forms.Label]::new();$label.Text=$Value;$label.Dock='Fill';$label.TextAlign='MiddleLeft';$layout.Controls.Add($label,0,$Row);return $label}
 $title=Add-Label '沉星草稿  StarSketch' 0;$title.Font=[Drawing.Font]::new('Microsoft YaHei UI',22,[Drawing.FontStyle]::Bold);$title.ForeColor=$gold
 $subtitle=Add-Label ("《沉星之序》游戏内草稿纸 · $($script:package.version) 测试版") 1
 $hint=Add-Label '选择包含 sinking_star.exe 的游戏安装文件夹' 2
 $picker=[Windows.Forms.TableLayoutPanel]::new();$picker.Dock='Fill';$picker.ColumnCount=2;$picker.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::Percent,100)) | Out-Null;$picker.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::Absolute,110)) | Out-Null
 $pathBox=[Windows.Forms.TextBox]::new();$pathBox.Dock='Fill';$pathBox.Margin=[Windows.Forms.Padding]::new(0,6,12,6);$pathBox.AccessibleName='游戏安装文件夹';$pathBox.BackColor=$panelColor;$pathBox.ForeColor=$textColor;$pathBox.BorderStyle='FixedSingle';$pathBox.Text=$GameDirectory
 $browse=[Windows.Forms.Button]::new();$browse.Text='浏览…';$browse.Dock='Fill';$browse.Margin=[Windows.Forms.Padding]::new(0,3,0,3);$picker.Controls.Add($pathBox,0,0);$picker.Controls.Add($browse,1,0);$layout.Controls.Add($picker,0,3)
 $buttons=[Windows.Forms.FlowLayoutPanel]::new();$buttons.Dock='Fill';$buttons.WrapContents=$false;$buttons.Padding=[Windows.Forms.Padding]::new(0,8,0,0)
 function New-Action([string]$Caption,[int]$Width){$b=[Windows.Forms.Button]::new();$b.Text=$Caption;$b.Width=$Width;$b.Height=38;$b.FlatStyle='Flat';$b.FlatAppearance.BorderColor=$gold;$b.ForeColor=$gold;$b.BackColor=$panelColor;$b.Margin=[Windows.Forms.Padding]::new(0,0,12,0);return $b}
 $install=New-Action '安装 / 更新' 165;$uninstall=New-Action '卸载草稿纸' 150;$inspect=New-Action '检查目录' 115;$update=New-Action '检查插件更新' 195
 $buttons.Controls.AddRange(@($install,$uninstall,$inspect,$update));$layout.Controls.Add($buttons,0,4)
 $description=Add-Label '打开时自动检查 GitHub 更新，发现新版后由你确认。更新前请保存进度并退出游戏；下载与校验通过后才替换安装文件。' 5;$description.ForeColor=[Drawing.Color]::FromArgb(180,187,194)
 $status=[Windows.Forms.TextBox]::new();$status.Multiline=$true;$status.ReadOnly=$true;$status.ScrollBars='Vertical';$status.Dock='Fill';$status.BackColor=$panelColor;$status.ForeColor=$textColor;$status.BorderStyle='FixedSingle';$status.AccessibleName='操作结果';$status.Text='请选择游戏文件夹。可安装本包、在线更新或卸载草稿纸。';$layout.Controls.Add($status,0,6)
 $footer=Add-Label '安装后可从游戏目录 StarSketch-Manager 再次打开；删除原下载包后仍可在线更新和卸载。' 7;$footer.Font=[Drawing.Font]::new('Microsoft YaHei UI',9);$footer.ForeColor=[Drawing.Color]::FromArgb(153,164,178)
 $browse.FlatStyle='Flat';$browse.FlatAppearance.BorderColor=$gold;$browse.ForeColor=$gold;$browse.BackColor=$panelColor
 if(!$script:hasPayload){$install.Enabled=$false;$subtitle.Text+=' · 已安装管理';$status.Text='这是已安装管理入口，可检查插件更新或卸载，无需原安装包。'}
 $script:busy=$false;$script:worker=$null
 $timer=[Windows.Forms.Timer]::new();$timer.Interval=150
 function Set-Busy([bool]$Busy){
  $script:busy=$Busy;$form.UseWaitCursor=$Busy;$pathBox.Enabled=!$Busy
  foreach($b in @($uninstall,$browse,$inspect,$update)){$b.Enabled=!$Busy};$install.Enabled=!$Busy -and $script:hasPayload
 }
 function Show-Failure([string]$Message){$status.Text=Format-StarSketchError $Message;$status.ForeColor=[Drawing.Color]::FromArgb(255,173,160)}
 function Run-Action([scriptblock]$Action){
  if($script:busy){return};Set-Busy $true
  $status.Text='正在检查和处理，请稍候…';$status.ForeColor=$textColor;[Windows.Forms.Application]::DoEvents()
  try{$result=& $Action;$status.Text=$result.message;$status.ForeColor=[Drawing.Color]::FromArgb(150,222,172)}catch{Show-Failure $_.Exception.Message}
  finally{Set-Busy $false}
 }
 function Pick-GameDirectory{
  $dialog=[Windows.Forms.FolderBrowserDialog]::new();$dialog.Description='选择沉星之序的安装文件夹（包含 sinking_star.exe）';$dialog.ShowNewFolderButton=$false
  if(![string]::IsNullOrWhiteSpace($pathBox.Text) -and (Test-Path -LiteralPath $pathBox.Text -PathType Container)){$dialog.SelectedPath=$pathBox.Text}
  try{if($dialog.ShowDialog($form) -eq [Windows.Forms.DialogResult]::OK){$pathBox.Text=$dialog.SelectedPath;return $true};return $false}finally{$dialog.Dispose()}
 }
 function Start-OnlineWork([string]$Operation,[string]$CurrentVersion,$Offer){
  if($script:busy){return};Set-Busy $true;$status.ForeColor=$textColor
  $status.Text=if($Operation -eq 'check'){'正在连接 GitHub 检查插件更新…'}else{'正在下载、校验并更新草稿纸，请稍候…'}
  $script:workerKind=$Operation;$script:workerDirectory=$pathBox.Text
  $script:worker=[PowerShell]::Create()
  [void]$script:worker.AddScript({
   param($ModulePath,$Operation,$Directory,$CurrentVersion,$Offer)
   $ErrorActionPreference='Stop';Import-Module $ModulePath -Force -DisableNameChecking
   if($Operation -eq 'check'){Get-StarSketchUpdate $CurrentVersion}else{Install-StarSketchUpdate $Directory $Offer}
  }).AddArgument((Join-Path $PSScriptRoot 'StarSketch.Installer.psm1')).AddArgument($Operation).AddArgument($script:workerDirectory).AddArgument($CurrentVersion).AddArgument($Offer)
  try{$script:pending=$script:worker.BeginInvoke();$timer.Start()}catch{$script:worker.Dispose();$script:worker=$null;Set-Busy $false;Show-Failure $_.Exception.Message}
 }
 function Check-Updates{
  if($script:busy){return}
  try{
   $current=$script:package.version
   if(![string]::IsNullOrWhiteSpace($pathBox.Text)){$local=Get-StarSketchStatus $pathBox.Text $script:package;if($local.installed){if($local.version -eq 'manual'){$current=$local.update_version}else{$current=$local.version}}}
   Start-OnlineWork 'check' $current $null
  }catch{Show-Failure $_.Exception.Message}
 }
 function Confirm-Update($Offer){
  $status.Text=$Offer.message;$status.ForeColor=[Drawing.Color]::FromArgb(150,222,172)
  if(!$Offer.available){return}
  $currentLabel=if($Offer.current_version -eq '0.0.0'){'手动安装（版本未登记）'}else{$Offer.current_version}
  $question="发现 StarSketch $($Offer.version)，当前版本 $currentLabel。`r`n`r`n是否从项目官方 GitHub 下载并更新？请先保存进度并退出游戏。"
  $answer=[Windows.Forms.MessageBox]::Show($form,$question,'发现插件更新',[Windows.Forms.MessageBoxButtons]::YesNo,[Windows.Forms.MessageBoxIcon]::Question,[Windows.Forms.MessageBoxDefaultButton]::Button2)
  if($answer -ne [Windows.Forms.DialogResult]::Yes){$status.Text+=' 已取消更新，安装文件未改动。';return}
  if([string]::IsNullOrWhiteSpace($pathBox.Text) -and !(Pick-GameDirectory)){$status.Text='已取消选择目录，安装文件未改动。';return}
  Start-OnlineWork 'update' $Offer.current_version $Offer
 }
 $timer.Add_Tick({
  if(!$script:worker){$timer.Stop();return}
  if(!$script:pending.IsCompleted){if($script:worker.Streams.Progress.Count){$status.Text=$script:worker.Streams.Progress[$script:worker.Streams.Progress.Count-1].StatusDescription};return}
  $timer.Stop();$kind=$script:workerKind;$directory=$script:workerDirectory;$failure=$null;$result=$null
  try{$items=@($script:worker.EndInvoke($script:pending));if($script:worker.HadErrors){throw $script:worker.Streams.Error[0].Exception.Message};$result=$items[-1]}
  catch{$failure=if($script:worker.Streams.Error.Count){$script:worker.Streams.Error[0].Exception.Message}else{$_.Exception.Message}}
  finally{$script:worker.Dispose();$script:worker=$null;Set-Busy $false}
  if($failure){Show-Failure $failure;return}
  if($kind -eq 'check'){Confirm-Update $result;return}
  try{
   $script:package=Get-StarSketchPackage (Join-Path $directory 'StarSketch-Manager\StarSketch-package.json');$script:hasPayload=$false;$install.Enabled=$false
   $subtitle.Text="《沉星之序》游戏内草稿纸 · $($script:package.version) 测试版 · 已安装管理"
   $status.Text=$result.message;$status.ForeColor=[Drawing.Color]::FromArgb(150,222,172)
  }catch{Show-Failure $_.Exception.Message}
 })
 $browse.Add_Click({if(Pick-GameDirectory){Run-Action {Get-StarSketchStatus $pathBox.Text $script:package}}})
 $inspect.Add_Click({Run-Action {Get-StarSketchStatus $pathBox.Text $script:package}})
 $install.Add_Click({Run-Action {Install-StarSketch -GameDirectory $pathBox.Text -PayloadDirectory $payload -ManagerDirectory $PSScriptRoot}})
 $uninstall.Add_Click({Run-Action {Uninstall-StarSketch -GameDirectory $pathBox.Text -Package $script:package}})
 $update.Add_Click({Check-Updates})
 $form.Add_FormClosing({param($sender,$eventArgs)if($script:busy){$eventArgs.Cancel=$true}})
 $form.Add_Shown({Check-Updates})
 [void]$form.ShowDialog();$timer.Dispose();$form.Dispose()
}catch{$message=if(Get-Command Format-StarSketchError -ErrorAction SilentlyContinue){Format-StarSketchError $_.Exception.Message}else{"[SS-INSTALL-098] $($_.Exception.Message)"};[void][Windows.Forms.MessageBox]::Show($message,'StarSketch 安装工具',[Windows.Forms.MessageBoxButtons]::OK,[Windows.Forms.MessageBoxIcon]::Error);exit 1}
