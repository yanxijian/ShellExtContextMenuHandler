param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("register", "unregister")]
    [string]$Action,

    [string]$DllPath = ""
)

$ErrorActionPreference = "Stop"

function Get-DefaultDllPath {
    $repoRoot = Split-Path -Parent $PSScriptRoot
    $candidates = @(
        (Join-Path $repoRoot "build\bin\Release\CppShellExtContextMenuHandler.dll"),
        (Join-Path $repoRoot "build\Release\CppShellExtContextMenuHandler.dll"),
        (Join-Path $repoRoot "build\bin\Debug\CppShellExtContextMenuHandler.dll"),
        (Join-Path $repoRoot "build\Debug\CppShellExtContextMenuHandler.dll"),
        (Join-Path $repoRoot "x64\Release\CppShellExtContextMenuHandler.dll"),
        (Join-Path $repoRoot "Release\CppShellExtContextMenuHandler.dll")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "Cannot find CppShellExtContextMenuHandler.dll. Build the project or pass -DllPath."
}

function Test-IsAdministrator {
    $current = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    return $current.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-IsAdministrator)) {
    throw "Administrator privileges are required. Run this script from an elevated PowerShell session."
}

if ([string]::IsNullOrWhiteSpace($DllPath)) {
    $DllPath = Get-DefaultDllPath
}

$DllPath = (Resolve-Path $DllPath).Path
$dllDir = Split-Path -Parent $DllPath
$configSource = Join-Path (Split-Path -Parent $PSScriptRoot) "config\menu.json"
$configTarget = Join-Path $dllDir "menu.json"

if (Test-Path $configSource) {
    Copy-Item -Path $configSource -Destination $configTarget -Force
    Write-Host "Copied config to $configTarget"
}

$regsvr32 = Join-Path $env:Windir "System32\regsvr32.exe"
$arguments = if ($Action -eq "unregister") { "/u `"$DllPath`"" } else { "`"$DllPath`"" }

Write-Host "Running: $regsvr32 $arguments"
& $regsvr32 $arguments

if ($LASTEXITCODE -ne 0) {
    throw "regsvr32 failed with exit code $LASTEXITCODE"
}

Write-Host "Shell extension $Action completed."
