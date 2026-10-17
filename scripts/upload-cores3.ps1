[CmdletBinding()]
param([string]$Port = 'COM5')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'build-cores3.ps1')
& arduino-cli upload -p $Port --fqbn 'm5stack:esp32:m5stack_cores3:PSRAM=enabled,PartitionScheme=default' --input-dir (Join-Path $root 'build/cores3') (Join-Path $root 'firmware/m5stack/cores3')
if ($LASTEXITCODE -ne 0) { throw "CoreS3 upload failed (exit $LASTEXITCODE)." }
