[CmdletBinding()]
param(
    [string[]]$ServerArgs = @(),
    [switch]$SetupIfMissing,
    [string]$RepoRoot
)

$ErrorActionPreference = "Stop"

if (-not $RepoRoot) {
    $scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } elseif ($PSCommandPath) { Split-Path -Parent $PSCommandPath } else { Get-Location }
    $RepoRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
}

$toolsDir = Join-Path $RepoRoot "tools"
$envPath = Join-Path $RepoRoot ".micromamba"
$micromambaExe = Join-Path $toolsDir "Library/bin/micromamba.exe"
$arduinoCliExe = Join-Path $toolsDir "arduino-cli.exe"
$configPath = Join-Path $RepoRoot "arduino-cli.yaml"
$setupScript = Join-Path $RepoRoot "scripts/setup-arduino-mcp.ps1"

function Assert-File {
    param([string]$Path, [string]$Description)
    if (-not (Test-Path $Path)) {
        throw "Missing $Description ($Path). Run scripts/setup-arduino-mcp.ps1 first or pass -SetupIfMissing."
    }
}

if ($SetupIfMissing.IsPresent) {
    & $setupScript
}

Assert-File -Path $micromambaExe -Description "micromamba executable"
Assert-File -Path $envPath -Description "Python environment"
Assert-File -Path $arduinoCliExe -Description "Arduino CLI"
Assert-File -Path $configPath -Description "Arduino CLI config"

$env:PATH = "$toolsDir;$env:PATH"
$env:WORKSPACE = $RepoRoot
$env:ARDUINO_CLI_CONFIG_FILE = $configPath

Write-Host "Launching Arduino MCP server from $RepoRoot" -ForegroundColor Green
& $micromambaExe run -p $envPath arduino-mcp-tool @ServerArgs
