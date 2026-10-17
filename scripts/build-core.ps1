[CmdletBinding()]
param([string]$Fqbn = 'm5stack:esp32:m5stack_core')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$sketch = Join-Path $root 'firmware/m5stack/core'
$output = Join-Path $root 'build/core'
& arduino-cli compile --fqbn $Fqbn --output-dir $output $sketch
if ($LASTEXITCODE -ne 0) { throw "Core compilation failed (exit $LASTEXITCODE)." }
