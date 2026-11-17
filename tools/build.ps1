[CmdletBinding()]
param(
    [ValidateSet("x64","arm64")][string]$Arch = "x64",
    [string]$Pfx,
    [securestring]$PfxPassword,
    [string]$TrustedSigningEndpoint,
    [switch]$Installer,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot | Split-Path
$out = Join-Path $root "dist"

if (-not $SkipTests) {
    cmake -S $root -B (Join-Path $root "build-tests") -DCMAKE_BUILD_TYPE=Release | Out-Null
    cmake --build (Join-Path $root "build-tests") --config Release | Out-Null
    ctest --test-dir (Join-Path $root "build-tests") -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "tests failed" }
}

python (Join-Path $root "tools\make_icon.py")
cmake -S $root -B (Join-Path $root "build") -A $Arch -DCMAKE_BUILD_TYPE=Release | Out-Null
cmake --build (Join-Path $root "build") --config Release
if ($LASTEXITCODE -ne 0) { throw "build failed" }

New-Item -ItemType Directory -Force -Path $out | Out-Null
$staged = Join-Path $out "crowman_study.exe"
Copy-Item (Join-Path $root "build\Release\crowman_study.exe") $staged -Force

if ($Pfx -or $TrustedSigningEndpoint) {
    & (Join-Path $root "tools\sign.ps1") -Exe $staged -Pfx $Pfx -PfxPassword $PfxPassword `
        -TrustedSigningEndpoint $TrustedSigningEndpoint
}

& (Join-Path $root "tools\make_portable.ps1") -Exe $staged

if ($Installer) {
    $iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue
    if (-not $iscc) { throw "Inno Setup not installed" }
    & $iscc.FullName (Join-Path $root "installer\crowman.iss")
}

Write-Host "done: $out" -ForegroundColor Green
