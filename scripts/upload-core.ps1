[CmdletBinding()]
param(
    [string]$Port = 'COM5',
    [string]$Fqbn = 'm5stack:esp32:m5stack_core'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
# Always compile the current source before uploading.
& (Join-Path $PSScriptRoot 'build-core.ps1') -Fqbn $Fqbn
& arduino-cli upload -p $Port --fqbn $Fqbn --input-dir (Join-Path $root 'build/core') (Join-Path $root 'firmware/m5stack/core')
if ($LASTEXITCODE -ne 0) { throw "Core upload failed (exit $LASTEXITCODE)." }
