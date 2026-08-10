# PathlessMark の登録（要・管理者）。実行後は Explorer の再起動で有効化。
#   powershell -ExecutionPolicy Bypass -File register.ps1
$ErrorActionPreference = 'Stop'
$dll = "$PSScriptRoot\bin\PathlessMark.dll"
if (-not (Test-Path $dll)) { throw "先に build.ps1 を実行してください（$dll がありません）" }
& regsvr32 /s $dll
if ($LASTEXITCODE -ne 0) { throw "regsvr32 失敗（管理者で実行していますか？）" }
Write-Host "登録完了。反映するには Explorer を再起動します（開いているウィンドウは閉じます）。"
$ans = Read-Host "Explorer を今すぐ再起動しますか？ (y/N)"
if ($ans -eq 'y') {
    Stop-Process -Name explorer -Force
    Start-Process explorer
}
