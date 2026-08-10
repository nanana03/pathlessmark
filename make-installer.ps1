# インストーラを作る（Inno Setup 6 が必要）
#   powershell -ExecutionPolicy Bypass -File make-installer.ps1
#
# ⚠️ winget で入れた Inno Setup は **Program Files ではなく %LOCALAPPDATA%\Programs** に入る
#    （2026-08-10 に踏んだ。公式インストーラで入れた場合は Program Files (x86) になる）
$ErrorActionPreference = 'Stop'

$candidates = @(
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
)
$iscc = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) {
    throw "ISCC.exe が見つかりません。winget install --id JRSoftware.InnoSetup で導入してください。"
}

if (-not (Test-Path "$PSScriptRoot\bin\PathlessMark.dll")) {
    throw "先に build.ps1 を実行してください（bin\PathlessMark.dll がありません）"
}

& $iscc "$PSScriptRoot\installer\PathlessMark.iss"
if ($LASTEXITCODE -ne 0) { throw "インストーラの作成に失敗しました" }

$setup = Get-ChildItem "$PSScriptRoot\dist\*-setup.exe" | Sort-Object LastWriteTime | Select-Object -Last 1
Write-Host ("OK -> " + $setup.FullName + "  " + [math]::Round($setup.Length / 1KB) + " KB")
Write-Host ("SHA256: " + (Get-FileHash $setup.FullName -Algorithm SHA256).Hash)
