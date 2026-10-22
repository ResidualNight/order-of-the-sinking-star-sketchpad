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
 $hasPayload=Test-Path -LiteralPath $packagePath
 if(!$hasPayload){$packagePath=Join-Path $PSScriptRoot 'StarSketch-package.json'}
 $package=Get-StarSketchPackage $packagePath
 if(!$GameDirectory -and !$hasPayload){$GameDirectory=Split-Path -Parent $PSScriptRoot}
 $bg=[Drawing.Color]::FromArgb(16,24,37);$panelColor=[Drawing.Color]::FromArgb(24,35,51);$gold=[Drawing.Color]::FromArgb(230,195,115);$textColor=[Drawing.Color]::FromArgb(234,232,222)
 $form=[Windows.Forms.Form]::new();$form.Text='沉星草稿 · 安装与卸载';$form.StartPosition='CenterScreen';$form.ClientSize=[Drawing.Size]::new(840,540);$form.MinimumSize=[Drawing.Size]::new(760,520);$form.BackColor=$bg;$form.ForeColor=$textColor;$form.Font=[Drawing.Font]::new('Microsoft YaHei UI',10);$form.AutoScaleMode='Dpi'
 $layout=[Windows.Forms.TableLayoutPanel]::new();$layout.Dock='Fill';$layout.Padding=[Windows.Forms.Padding]::new(28);$layout.ColumnCount=1;$layout.RowCount=8
 foreach($h in @(54,40,32,46,55,70,0,40)){$row=[Windows.Forms.RowStyle]::new();if($h){$row.SizeType='Absolute';$row.Height=$h}else{$row.SizeType='Percent';$row.Height=100};$layout.RowStyles.Add($row) | Out-Null}
 $form.Controls.Add($layout)
 function Add-Label([string]$Value,[int]$Row){$label=[Windows.Forms.Label]::new();$label.Text=$Value;$label.Dock='Fill';$label.TextAlign='MiddleLeft';$layout.Controls.Add($label,0,$Row);return $label}
 $title=Add-Label '沉星草稿  StarSketch' 0;$title.Font=[Drawing.Font]::new('Microsoft YaHei UI',22,[Drawing.FontStyle]::Bold);$title.ForeColor=$gold
 $subtitle=Add-Label ("《沉星之序》游戏内草稿纸 · $($package.version) 测试版") 1
 $hint=Add-Label '选择包含 sinking_star.exe 的游戏安装文件夹' 2
 $picker=[Windows.Forms.TableLayoutPanel]::new();$picker.Dock='Fill';$picker.ColumnCount=2;$picker.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::Percent,100)) | Out-Null;$picker.ColumnStyles.Add([Windows.Forms.ColumnStyle]::new([Windows.Forms.SizeType]::Absolute,110)) | Out-Null
 $pathBox=[Windows.Forms.TextBox]::new();$pathBox.Dock='Fill';$pathBox.Margin=[Windows.Forms.Padding]::new(0,6,12,6);$pathBox.AccessibleName='游戏安装文件夹';$pathBox.BackColor=$panelColor;$pathBox.ForeColor=$textColor;$pathBox.BorderStyle='FixedSingle';$pathBox.Text=$GameDirectory
 $browse=[Windows.Forms.Button]::new();$browse.Text='浏览…';$browse.Dock='Fill';$browse.Margin=[Windows.Forms.Padding]::new(0,3,0,3);$picker.Controls.Add($pathBox,0,0);$picker.Controls.Add($browse,1,0);$layout.Controls.Add($picker,0,3)
 $buttons=[Windows.Forms.FlowLayoutPanel]::new();$buttons.Dock='Fill';$buttons.WrapContents=$false;$buttons.Padding=[Windows.Forms.Padding]::new(0,8,0,0)
 function New-Action([string]$Caption,[int]$Width){$b=[Windows.Forms.Button]::new();$b.Text=$Caption;$b.Width=$Width;$b.Height=38;$b.FlatStyle='Flat';$b.FlatAppearance.BorderColor=$gold;$b.ForeColor=$gold;$b.BackColor=$panelColor;$b.Margin=[Windows.Forms.Padding]::new(0,0,12,0);return $b}
 $install=New-Action '安装 / 更新' 180;$uninstall=New-Action '卸载草稿纸' 180;$inspect=New-Action '检查目录' 140
 $buttons.Controls.AddRange(@($install,$uninstall,$inspect));$layout.Controls.Add($buttons,0,4)
 $description=Add-Label '请先保存进度并退出游戏。安装会保留其他文件，卸载只处理可确认属于草稿纸且未被修改的文件。' 5;$description.ForeColor=[Drawing.Color]::FromArgb(180,187,194)
 $status=[Windows.Forms.TextBox]::new();$status.Multiline=$true;$status.ReadOnly=$true;$status.ScrollBars='Vertical';$status.Dock='Fill';$status.BackColor=$panelColor;$status.ForeColor=$textColor;$status.BorderStyle='FixedSingle';$status.AccessibleName='操作结果';$status.Text='请选择游戏文件夹，然后点击“安装 / 更新”或“卸载草稿纸”。';$layout.Controls.Add($status,0,6)
 $footer=Add-Label '安装包请先完整解压。安装后可从游戏目录中的 StarSketch-Manager 再次打开管理工具。' 7;$footer.Font=[Drawing.Font]::new('Microsoft YaHei UI',9);$footer.ForeColor=[Drawing.Color]::FromArgb(153,164,178)
 foreach($b in @($browse)){$b.FlatStyle='Flat';$b.FlatAppearance.BorderColor=$gold;$b.ForeColor=$gold;$b.BackColor=$panelColor}
 if(!$hasPayload){$install.Enabled=$false;$subtitle.Text+=' · 卸载管理';$status.Text='这是已安装版本的管理入口。可卸载草稿纸；安装或更新请打开完整安装包。'}
 $script:busy=$false
 function Run-Action([scriptblock]$Action){
  if($script:busy){return};$script:busy=$true;$form.UseWaitCursor=$true
  foreach($b in @($install,$uninstall,$browse,$inspect)){$b.Enabled=$false};$pathBox.Enabled=$false
  $status.Text='正在检查和处理，请稍候…';$status.ForeColor=$textColor;[Windows.Forms.Application]::DoEvents()
  try{$result=& $Action;$status.Text=$result.message;$status.ForeColor=[Drawing.Color]::FromArgb(150,222,172)}catch{$status.Text=$_.Exception.Message;$status.ForeColor=[Drawing.Color]::FromArgb(255,173,160)}
  finally{$form.UseWaitCursor=$false;$script:busy=$false;$pathBox.Enabled=$true;foreach($b in @($uninstall,$browse,$inspect)){$b.Enabled=$true};$install.Enabled=$hasPayload}
 }
 $browse.Add_Click({
  $dialog=[Windows.Forms.FolderBrowserDialog]::new();$dialog.Description='选择沉星之序的安装文件夹（包含 sinking_star.exe）';$dialog.ShowNewFolderButton=$false
  if(![string]::IsNullOrWhiteSpace($pathBox.Text) -and (Test-Path -LiteralPath $pathBox.Text -PathType Container)){$dialog.SelectedPath=$pathBox.Text}
  try{if($dialog.ShowDialog($form) -eq [Windows.Forms.DialogResult]::OK){$pathBox.Text=$dialog.SelectedPath;Run-Action {Get-StarSketchStatus $pathBox.Text $package}}}finally{$dialog.Dispose()}
 })
 $inspect.Add_Click({Run-Action {Get-StarSketchStatus $pathBox.Text $package}})
 $install.Add_Click({Run-Action {Install-StarSketch -GameDirectory $pathBox.Text -PayloadDirectory $payload -ManagerDirectory $PSScriptRoot}})
 $uninstall.Add_Click({Run-Action {Uninstall-StarSketch -GameDirectory $pathBox.Text -Package $package}})
 $form.Add_FormClosing({param($sender,$eventArgs)if($script:busy){$eventArgs.Cancel=$true}})
 $form.Add_Shown({if($GameDirectory){Run-Action {Get-StarSketchStatus $pathBox.Text $package}}})
 [void]$form.ShowDialog();$form.Dispose()
}catch{[void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'StarSketch 安装工具',[Windows.Forms.MessageBoxButtons]::OK,[Windows.Forms.MessageBoxIcon]::Error);exit 1}
