[CmdletBinding()]
param([int]$Jobs = 1, [switch]$Clean)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$cleanArgs = @()
if ($Clean) { $cleanArgs += '--clean' }
& arduino-cli compile @cleanArgs --jobs $Jobs --fqbn 'm5stack:esp32:m5stack_atoms3r:PartitionScheme=default_8MB' --build-path (Join-Path $root 'build/atoms3r-cam-cache') --output-dir (Join-Path $root 'build/atoms3r-cam') (Join-Path $root 'firmware/m5stack/atoms3r-cam')
if ($LASTEXITCODE -ne 0) { throw "AtomS3R-CAM compilation failed (exit $LASTEXITCODE)." }
