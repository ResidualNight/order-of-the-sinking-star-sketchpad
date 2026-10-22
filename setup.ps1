[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$archiveName='llvm-mingw-20251021-ucrt-x86_64.zip'
$expected='0A0CE4733B54DE8C4858ECFC2A08447DE57728DCCF9FBF7FCBA3CE7E0D0C6AF8'
$tools=Join-Path $root 'tools'
$toolchain=Join-Path $tools 'llvm-mingw-20251021-ucrt-x86_64'
$archive=Join-Path $tools $archiveName
$legacyArchive=Join-Path $tools 'llvm-mingw.zip'
if(Test-Path -LiteralPath (Join-Path $toolchain 'bin\clang++.exe')){
    & (Join-Path $toolchain 'bin\clang++.exe') --version
    Write-Output 'Using the existing project toolchain.'
    exit 0
}
[IO.Directory]::CreateDirectory($tools) | Out-Null
if(!(Test-Path -LiteralPath $archive) -and (Test-Path -LiteralPath $legacyArchive)){$archive=$legacyArchive}
if(!(Test-Path -LiteralPath $archive)){
    [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri ('https://github.com/mstorsjo/llvm-mingw/releases/download/20251021/'+$archiveName) -OutFile $archive -UseBasicParsing
}
if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected){throw 'Toolchain SHA256 mismatch. The archive was left in place for inspection.'}
if(Test-Path -LiteralPath $toolchain){throw 'Toolchain directory is incomplete. Inspect it before retrying; setup does not overwrite it.'}
Expand-Archive -LiteralPath $archive -DestinationPath $tools
$compiler=Join-Path $toolchain 'bin\clang++.exe'
if(!(Test-Path -LiteralPath $compiler)){throw 'Compiler missing after extraction'}
& $compiler --version
if($LASTEXITCODE -ne 0){throw 'Compiler verification failed'}
