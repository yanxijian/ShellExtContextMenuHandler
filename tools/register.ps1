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

function Test-JsonFile([string]$Path) {
    try {
        Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json | Out-Null
        return $true
    }
    catch {
        return $false
    }
}

function Test-RegistrationConfig([string]$Path) {
    try {
        $document = Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    catch {
        return $false
    }

    if ($document.schemaVersion -ne 1 -or $null -eq $document.shellRegistrations) {
        return $false
    }

    $targets = @($document.shellRegistrations)
    if ($targets.Count -eq 0) {
        return $false
    }

    $known = @("file", "directory", "directoryBackground", "drive", "fileSystemObject")
    foreach ($target in $targets) {
        if ($known -notcontains $target) {
            return $false
        }
    }

    return (($targets | Select-Object -Unique).Count -eq $targets.Count)
}

function Get-DllMachineType([string]$Path) {
    try {
        $bytes = [System.IO.File]::ReadAllBytes($Path)
        if ($bytes.Length -lt 0x40) {
            return 0
        }
        $peOffset = [System.BitConverter]::ToInt32($bytes, 0x3C)
        if ($peOffset -lt 0 -or ($peOffset + 6) -gt $bytes.Length) {
            return 0
        }
        return [System.BitConverter]::ToUInt16($bytes, $peOffset + 4)
    }
    catch {
        return 0
    }
}

if (-not (Test-IsAdministrator)) {
    throw "Administrator privileges are required. Run this script from an elevated PowerShell session."
}

if ([string]::IsNullOrWhiteSpace($DllPath)) {
    $DllPath = Get-DefaultDllPath
}

$DllPath = (Resolve-Path $DllPath).Path
$dllDir = Split-Path -Parent $DllPath
$repoRoot = Split-Path -Parent $PSScriptRoot
$configSource = Join-Path $repoRoot "config\menu.json"
$configTarget = Join-Path $dllDir "menu.json"
$registrationSource = Join-Path $repoRoot "config\registration.json"
$registrationTarget = Join-Path $dllDir "registration.json"
$iconsSource = Join-Path $repoRoot "config\icons"
$iconsTarget = Join-Path $dllDir "icons"

$regsvr32 = Join-Path $env:Windir "System32\regsvr32.exe"

# A 32-bit DLL must be registered by SysWOW64\regsvr32.exe on a 64-bit OS;
# System32\regsvr32.exe would load a 64-bit DLL instead.
$machineType = Get-DllMachineType $DllPath
if ($machineType -eq 0x14c) {
    $wow64Regsvr = Join-Path $env:Windir "SysWOW64\regsvr32.exe"
    if (Test-Path $wow64Regsvr) {
        $regsvr32 = $wow64Regsvr
    }
}

$registrationBackup = $null
$hadRegistration = $false

if ($Action -eq "register") {
    if (Test-Path $registrationSource) {
        if (-not (Test-RegistrationConfig $registrationSource)) {
            throw "Invalid config\\registration.json; refusing to modify the installed extension."
        }
    }
    else {
        Write-Warning "config\\registration.json not found; using the DLL directory's existing file."
    }

    if (Test-Path $configSource) {
        if (-not (Test-JsonFile $configSource)) {
            throw "Invalid config\\menu.json; refusing to modify the installed extension."
        }

        $validator = Join-Path $repoRoot "tools\validate_menu_json.py"
        if ((Test-Path $validator) -and (Get-Command python -ErrorAction SilentlyContinue)) {
            & python $validator
            if ($LASTEXITCODE -ne 0) {
                throw "config\\menu.json failed validation; refusing to modify the installed extension."
            }
        }
    }

    if (Test-Path $registrationTarget) {
        $registrationBackup = [System.IO.File]::ReadAllBytes($registrationTarget)
        $hadRegistration = $true
    }
    if (Test-Path $registrationSource) {
        Copy-Item -Path $registrationSource -Destination $registrationTarget -Force
        Write-Host "Copied registration config to $registrationTarget"
    }
}

$arguments = if ($Action -eq "unregister") { "/s /u `"$DllPath`"" } else { "/s `"$DllPath`"" }
Write-Host "Running: $regsvr32 $arguments"

try {
    & $regsvr32 $arguments
    if ($LASTEXITCODE -ne 0) {
        throw "regsvr32 failed with exit code $LASTEXITCODE"
    }
}
catch {
    if ($Action -eq "register") {
        if ($hadRegistration) {
            [System.IO.File]::WriteAllBytes($registrationTarget, $registrationBackup)
            Write-Host "Restored previous registration.json after failure."
        }
        elseif (Test-Path $registrationTarget) {
            Remove-Item -LiteralPath $registrationTarget -Force
        }
    }
    throw
}

if ($Action -eq "register") {
    if (Test-Path $configSource) {
        Copy-Item -Path $configSource -Destination $configTarget -Force
        Write-Host "Copied menu config to $configTarget"
    }

    if (Test-Path $iconsSource) {
        if (-not (Test-Path $iconsTarget)) {
            New-Item -ItemType Directory -Path $iconsTarget | Out-Null
        }
        Copy-Item -Path (Join-Path $iconsSource "*") -Destination $iconsTarget -Recurse -Force
        Write-Host "Copied icons to $iconsTarget"
    }
}

Write-Host "Shell extension $Action completed."
