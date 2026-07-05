param(
    [ValidateSet("x64", "Win32")]
    [string]$Platform = "x64",

    [string]$BuildDir = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $repoRoot "build"
}

$generator = "Visual Studio 18 2026"
$architecture = if ($Platform -eq "x64") { "x64" } else { "Win32" }

Write-Host "Generating Visual Studio solution in: $BuildDir"
Write-Host "Generator: $generator ($architecture)"

cmake -S $repoRoot -B $BuildDir -G $generator -A $architecture
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed."
}

$sln = Get-ChildItem -Path $BuildDir -Include *.sln,*.slnx -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.DirectoryName -eq $BuildDir } |
    Select-Object -First 1

if ($null -eq $sln) {
    throw "No solution file was generated."
}

Write-Host ""
Write-Host "Solution: $($sln.FullName)"
Write-Host "Projects: $(Join-Path $BuildDir 'CppShellExtContextMenuHandler.vcxproj')"
Write-Host "Build with:"
Write-Host "  cmake --build `"$BuildDir`" --config Release"
Write-Host "Or open the solution in Visual Studio."