# PathlessMark を安全な順序で作り直す（要・管理者）。
#   管理者 PowerShell で:  powershell -ExecutionPolicy Bypass -File "F:\Nanabit_CC\Product\PathlessMark\rebuild-all.ps1"
#
# 順序は **解除 → ビルド → 登録 → Explorer 再起動**。
# ⚠️ DLL を掴むのは Explorer とは限らない（2026-08-10 は ZipShrink が掴んでいた）＝プロセスを
#    落として回るのは筋が悪い。build.ps1 側で「消せなければリネーム退避」するので停止は不要。
# ⚠️ 最後に Explorer を再起動するので、開いているエクスプローラのウィンドウは閉じる。
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$dll = "$root\bin\PathlessMark.dll"

$isAdmin = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) { throw "管理者として実行してください（regsvr32 に必要）。" }

$before = if (Test-Path $dll) { (Get-Item $dll).LastWriteTime } else { $null }
Write-Host "[1/6] 登録解除"
& regsvr32 /u /s $dll

Write-Host "[2/5] ビルド"
& powershell -ExecutionPolicy Bypass -File "$root\build.ps1"
if ($LASTEXITCODE -ne 0) { throw "ビルド失敗。登録は戻していないので、直してから再実行してください。" }

$after = (Get-Item $dll).LastWriteTime
if ($before -and $after -le $before) {
    throw "DLL の日時が更新されていません（$before → $after）＝作り直せていません。"
}
Write-Host ("      DLL 更新: {0} -> {1}" -f $before, $after)

Write-Host "[3/5] 登録"
& regsvr32 /s $dll
if ($LASTEXITCODE -ne 0) { throw "regsvr32 失敗。" }

Write-Host "[4/5] Explorer を再起動（バッジの再描画）"
Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
if (-not (Get-Process -Name explorer -ErrorAction SilentlyContinue)) { Start-Process explorer }

Write-Host "[5/5] 完了"
Write-Host ""
Write-Host "=== 完了 ==="
Write-Host ("DLL      : {0}  {1} bytes  {2}" -f $dll, (Get-Item $dll).Length, $after)
Write-Host "次の確認 : bin\PathlessMark-settings.exe で色を選び、末端フォルダに反映されるか見る"
Write-Host "          bin\PathlessMark-diagnose.exe でオーバーレイ枠の順位を確認"
