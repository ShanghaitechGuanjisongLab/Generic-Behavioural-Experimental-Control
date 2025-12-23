[CmdletBinding()]
param(
    [string]$PythonVersion = "3.11",
    [string]$RepoRoot
)

$ErrorActionPreference = "Stop"

if (-not $RepoRoot) {
    $scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } elseif ($PSCommandPath) { Split-Path -Parent $PSCommandPath } else { Get-Location }
    $RepoRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
}

function Write-Step {
    param([string]$Message)
    Write-Host "[setup] $Message" -ForegroundColor Cyan
}

function Set-DirectoryIfMissing {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Get-FileDownload {
    param(
        [string]$Url,
        [string]$Destination,
        [switch]$Force
    )
    if ((-not $Force) -and (Test-Path $Destination)) {
        return
    }
    Write-Step "Downloading $Url"
    Invoke-WebRequest -Uri $Url -OutFile $Destination -UseBasicParsing
}

function Add-RepoToolsToPath {
    param([string[]]$CandidatePaths)

    $envKey = 'HKCU:\Environment'
    $currentValue = (Get-ItemProperty -Path $envKey -Name Path -ErrorAction SilentlyContinue).Path
    $currentEntries = if ($currentValue) { $currentValue -split ';' } else { @() }
    $normalized = $currentEntries | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' }
    $added = $false
    foreach ($candidate in $CandidatePaths) {
        if (-not $candidate) { continue }
        if (-not (Test-Path $candidate)) { continue }
        $resolved = (Resolve-Path $candidate).Path.TrimEnd('\')
        $alreadyPresent = $false
        foreach ($entry in $normalized) {
            if ($entry.TrimEnd('\') -ieq $resolved) {
                $alreadyPresent = $true
                break
            }
        }
        if (-not $alreadyPresent) {
            $normalized += $resolved
            $added = $true
        }
    }

    if ($added) {
        $newValue = ($normalized | Select-Object -Unique) -join ';'
        Set-ItemProperty -Path $envKey -Name Path -Value $newValue
        [System.Environment]::SetEnvironmentVariable('Path', $newValue, 'User')
        Write-Step "Added repo tooling directories to current user PATH. Restart terminals to pick up changes."
    }
    else {
        Write-Step "Required directories already exist in current user PATH"
    }
}

$toolsDir = Join-Path $RepoRoot "tools"
$envPath = Join-Path $RepoRoot ".micromamba"
$micromambaExe = Join-Path $toolsDir "Library/bin/micromamba.exe"
$micromambaArchive = Join-Path $toolsDir "micromamba.tar.bz2"
$arduinoCliZip = Join-Path $toolsDir "arduino-cli.zip"
$arduinoCliExe = Join-Path $toolsDir "arduino-cli.exe"
$configPath = Join-Path $RepoRoot "arduino-cli.yaml"
$arduinoDataDir = Join-Path $RepoRoot ".arduino-data"
$arduinoDownloadsDir = Join-Path $RepoRoot ".arduino-downloads"

Set-DirectoryIfMissing $toolsDir
Set-DirectoryIfMissing $arduinoDataDir
Set-DirectoryIfMissing $arduinoDownloadsDir

if (-not (Test-Path $micromambaExe)) {
    Write-Step "Installing micromamba"
    Get-FileDownload -Url "https://micro.mamba.pm/api/micromamba/win-64/latest" -Destination $micromambaArchive -Force
    if (Test-Path $micromambaExe) {
        Remove-Item $micromambaExe -Force
    }
    tar -xjf $micromambaArchive -C $toolsDir
    Remove-Item $micromambaArchive -Force
}

if (-not (Test-Path $envPath)) {
    Write-Step "Creating Python $PythonVersion environment"
    & $micromambaExe create -y -p $envPath "python=$PythonVersion" pip | Out-Host
}
else {
    Write-Step "Python environment already exists at $envPath"
}

Write-Step "Upgrading pip"
& $micromambaExe run -p $envPath python -m pip install --upgrade pip | Out-Host

Write-Step "Installing Arduino MCP server"
& $micromambaExe run -p $envPath python -m pip install --upgrade "git+https://github.com/amahpour/arduino-mcp-server-simple.git" | Out-Host

Write-Step "Pinning pydantic 2.11.7 for FastMCP compatibility"
& $micromambaExe run -p $envPath python -m pip install --upgrade "pydantic[email]==2.11.7" | Out-Host

if (-not (Test-Path $arduinoCliExe)) {
    Write-Step "Downloading Arduino CLI"
    Get-FileDownload -Url "https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip" -Destination $arduinoCliZip -Force
    tar -xf $arduinoCliZip -C $toolsDir
    Remove-Item $arduinoCliZip -Force
}
else {
    Write-Step "Arduino CLI already present"
}

if (-not (Test-Path $configPath)) {
    Write-Step "Generating arduino-cli.yaml"
    & $arduinoCliExe config init --dest-file $configPath | Out-Host
}

Write-Step "Pointing Arduino CLI to repository-local data directories"
& $arduinoCliExe config set directories.data $arduinoDataDir --config-file $configPath | Out-Host
& $arduinoCliExe config set directories.downloads $arduinoDownloadsDir --config-file $configPath | Out-Host

$pathTargets = @(
    $toolsDir,
    (Join-Path $toolsDir "Library/bin"),
    (Join-Path $envPath "Scripts")
)
Add-RepoToolsToPath -CandidatePaths $pathTargets

Write-Step "Setup complete. Use scripts/run-arduino-mcp.ps1 to launch the server."
