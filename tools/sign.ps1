[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Exe,
    [string]$Pfx,
    [securestring]$PfxPassword,
    [string]$TimestampUrl = "http://timestamp.digicert.com",
    [string]$TrustedSigningEndpoint,
    [string]$TrustedSigningAccount,
    [string]$TrustedSigningProfile
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Exe)) { throw "not found: $Exe" }

# Needed for verification on both branches below.
$signtool = Get-Command signtool.exe -ErrorAction SilentlyContinue
if (-not $signtool) {
    $found = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse `
        -Filter signtool.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "x64" } |
        Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $found) { throw "signtool.exe not found" }
    $signtool = $found.FullName
}

if ($TrustedSigningEndpoint) {
    if (-not $TrustedSigningAccount -or -not $TrustedSigningProfile) { throw "missing account/profile" }
    Invoke-TrustedSigning -Endpoint $TrustedSigningEndpoint `
        -CodeSigningAccountName $TrustedSigningAccount `
        -CertificateProfileName $TrustedSigningProfile -Files $Exe
}
elseif ($Pfx) {
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($PfxPassword)
    try {
        $plain = [Runtime.InteropServices.Marshal]::PtrToStringAuto($bstr)
        & $signtool sign /fd SHA256 /f $Pfx /p $plain /tr $TimestampUrl /td SHA256 $Exe
    }
    finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
    }
}
else {
    throw "pass -Pfx or -TrustedSigningEndpoint"
}

& $signtool verify /pa /v $Exe
if ($LASTEXITCODE -ne 0) { throw "verification failed" }

Write-Host "signed and verified: $Exe" -ForegroundColor Green
