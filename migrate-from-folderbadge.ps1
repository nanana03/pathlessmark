# 旧 FolderBadge → 新 PathlessMark へ入れ替える（管理者で1回だけ実行）
#
#   & "F:\Nanabit_CC\Product\PathlessMark\migrate-from-folderbadge.ps1"
#
# ⚠️ 順序が命：**先に旧版の登録を解除**してから新版を登録する。
#    逆順・あるいは旧フォルダを先に消すと「登録は残っているのに DLL が無い」状態になり、
#    エクスプローラーが毎回ロードを試みる（一番やってはいけない置き土産）。

$ErrorActionPreference = 'Continue'
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "管理者で実行してください（Win+X → ターミナル（管理者））" -ForegroundColor Red
    exit 1
}

$ov  = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers'
$old = 'F:\Nanabit_CC\Product\FolderBadge\bin\FolderBadge.dll'
$new = "$PSScriptRoot\bin\PathlessMark.dll"

Write-Host "=== 1. 旧 FolderBadge の登録解除 ===" -ForegroundColor Cyan
if (Test-Path $old) {
    & regsvr32 /u /s $old
    Start-Sleep -Milliseconds 500
}
# regsvr32 が失敗しても残骸を確実に消す（旧DLLを既に消していた場合の保険）
Get-ChildItem $ov | Where-Object { $_.PSChildName -match 'FolderBadge' } |
    Remove-Item -Force -ErrorAction SilentlyContinue
Remove-Item 'HKCR:\CLSID\{7F1C6B10-8A44-4D8B-9E4E-2B6D5C0F1A21}' -Recurse -Force -ErrorAction SilentlyContinue
$leftover = @(Get-ChildItem $ov | Where-Object { $_.PSChildName -match 'FolderBadge' })
Write-Host ("  旧登録の残り: " + $leftover.Count + " 件（0 が正解）")

Write-Host "=== 2. 新 PathlessMark を登録 ===" -ForegroundColor Cyan
if (-not (Test-Path $new)) {
    Write-Host "  DLL がありません。先に build.ps1 を実行してください: $new" -ForegroundColor Red
    exit 1
}
& regsvr32 /s $new
Start-Sleep -Milliseconds 500
$reg = @(Get-ChildItem $ov | Where-Object { $_.PSChildName -match 'PathlessMark' })
if ($reg.Count -gt 0) {
    Write-Host ("  登録OK: [" + $reg[0].PSChildName + "] = " + (Get-ItemProperty $reg[0].PSPath).'(default)')
} else {
    Write-Host "  登録に失敗しました" -ForegroundColor Red
}

Write-Host "=== 3. 現在のオーバーレイ登録（辞書順・上位15件が有効）===" -ForegroundColor Cyan
Get-ChildItem $ov | Sort-Object PSChildName | ForEach-Object {
    Write-Host ("    [" + $_.PSChildName + "]")
}

Write-Host ""
Write-Host "=== 4. エクスプローラー再起動 ===" -ForegroundColor Cyan
$ans = Read-Host "再起動しますか？ (y/N)"
if ($ans -eq 'y') {
    Stop-Process -Name explorer -Force
    Start-Sleep -Seconds 1
    Start-Process explorer
    Write-Host "  完了。旧フォルダ Product\FolderBadge は、この後で削除して構いません。"
} else {
    Write-Host "  スキップしました。手動でエクスプローラーを再起動すると反映されます。"
}
