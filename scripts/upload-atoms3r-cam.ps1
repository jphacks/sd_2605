[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$Port)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'build-atoms3r-cam.ps1')
& arduino-cli upload -p $Port --fqbn 'm5stack:esp32:m5stack_atoms3r:PartitionScheme=default_8MB' --input-dir (Join-Path $root 'build/atoms3r-cam') (Join-Path $root 'firmware/m5stack/atoms3r-cam')
if ($LASTEXITCODE -ne 0) { throw "AtomS3R-CAM upload failed (exit $LASTEXITCODE)." }
