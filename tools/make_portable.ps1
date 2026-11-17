[CmdletBinding()]
param([Parameter(Mandatory)][string]$Exe)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot | Split-Path
$stage = Join-Path $env:TEMP ("crowman-portable-" + [guid]::NewGuid())
New-Item -ItemType Directory -Force -Path $stage | Out-Null

try {
    Copy-Item $Exe (Join-Path $stage "crowman_study.exe") -Force
    Copy-Item (Join-Path $root "docs\PORTABLE.txt") (Join-Path $stage "README.txt") -Force
    @"
; Portable mode. Rename to config.ini beside the exe.
[study]
mode=openai
endpoint=https://api.openai.com/v1/chat/completions
model=gpt-4o-mini
capture_mode=monitor
"@ | Set-Content -Path (Join-Path $stage "config.sample.ini") -Encoding UTF8

    $zip = Join-Path $root ("dist\crowman_study-portable-" + (Get-Date -Format "yyyyMMdd") + ".zip")
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -CompressionLevel Optimal
    Write-Host "portable zip: $zip" -ForegroundColor Green
} finally {
    Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
}
