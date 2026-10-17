[CmdletBinding()]
param([int]$Jobs = 1, [switch]$Clean)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$cleanArgs = @()
if ($Clean) { $cleanArgs += '--clean' }
& arduino-cli compile @cleanArgs --jobs $Jobs --fqbn 'm5stack:esp32:m5stack_cores3:PSRAM=enabled,PartitionScheme=default' --build-path (Join-Path $root 'build/cores3-cache') --output-dir (Join-Path $root 'build/cores3') (Join-Path $root 'firmware/m5stack/cores3')
if ($LASTEXITCODE -ne 0) { throw "CoreS3 compilation failed (exit $LASTEXITCODE)." }
