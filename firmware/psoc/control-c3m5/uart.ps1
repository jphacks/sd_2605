[CmdletBinding()]
param(
    [ValidateSet('build', 'program', 'getlibs', 'device-configurator')]
    [string]$Action = 'build',
    [string]$ToolsPath = "$env:USERPROFILE/ModusToolbox/tools_3.9"
)
$ErrorActionPreference = 'Stop'
$bash = Join-Path $ToolsPath 'modus-shell/bin/bash.exe'
if (!(Test-Path -LiteralPath $bash)) { throw "ModusToolbox bash not found: $bash" }
Push-Location (Join-Path $PSScriptRoot 'uart-hello')
try {
    # bash supplies the supported ModusToolbox runtime environment (including PATH).
    & $bash -c ('export PATH="/usr/bin:$PATH"; make {1} CONFIG=Debug CY_TOOLS_PATHS="{0}"' -f $ToolsPath.Replace('\', '/'), $Action)
    if ($LASTEXITCODE -ne 0) { throw "ModusToolbox $Action failed (exit $LASTEXITCODE)." }
} finally { Pop-Location }


