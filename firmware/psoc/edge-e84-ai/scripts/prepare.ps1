# Run once in Windows PowerShell; git must be available.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$board = Join-Path $root 'bsps/TARGET_KIT_PSE84_AI'
$commit = '4894dd6a1ac2cee8be0d028476da4c7e9e640431'
if (Test-Path $board) { throw "BSP already exists: $board (left untouched)" }
New-Item -ItemType Directory -Force (Split-Path $board -Parent) | Out-Null
& git clone https://github.com/Infineon/TARGET_KIT_PSE84_AI $board
if ($LASTEXITCODE -ne 0) { throw 'BSP clone failed' }
& git -C $board checkout --detach $commit
if ($LASTEXITCODE -ne 0) { throw 'BSP checkout failed' }
Write-Host 'BSP prepared. Next run make getlibs in modus-shell from the application root.'
Write-Host 'Keep generated assetlocks.json for the dependency versions used in a hardware build.'
