# PathlessMark のビルド（要：VS の「C++ によるデスクトップ開発」ワークロード）
# 使い方：powershell -ExecutionPolicy Bypass -File build.ps1
#
# ⚠️ cl に /utf-8 は必須。無いと MSVC はソースを CP932 と解釈し、日本語コメントの
#    途中バイトが行継続扱いになって**次の行を飲み込む**（2026-08-10 に実際に踏んだ。
#    C2059 syntax error: 'return' が多発する）。
# ⚠️ このスクリプト自身も、文字列連結の途中に # コメントを書かないこと
#    （行の残りが全部コメント扱いになり、cl の行が丸ごと消えた・同日）。
$ErrorActionPreference = 'Stop'
$vs = 'C:\Program Files\Microsoft Visual Studio\18\Community'
$dev = "$vs\Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path $dev)) { throw "VsDevCmd.bat が見つかりません: $dev" }

$root = $PSScriptRoot
$out = "$root\bin"
New-Item -ItemType Directory -Force $out | Out-Null

# ⚠️ 動いているプロセスが DLL を掴んでいると削除は失敗する。ここを黙って通すと、リンクだけ
#    失敗して**古い DLL が残ったままビルド成功に見える**（2026-08-10 に実際に発生：
#    dllmain.cpp 12:56 に対し DLL は 11:29 のまま）。
# ⚠️ 掴むのは Explorer とは限らない。**ファイルダイアログを開いた任意のアプリ**にシェル拡張
#    として読み込まれる（同日、ZipShrink が掴んでいた）。Explorer を落としても外れない。
# → NTFS は**ロード済み DLL の削除は拒むが、リネーム（移動）は許す**。消せなければ退避する。
$stale = "$out\PathlessMark.dll"
Remove-Item $stale -ErrorAction SilentlyContinue
if (Test-Path $stale) {
    $bak = "$out\_old_PathlessMark_{0}.dll" -f (Get-Item $stale).LastWriteTime.ToString('HHmmss')
    Rename-Item $stale $bak -ErrorAction Stop
    Write-Host "使用中だったので退避しました -> $bak（再起動後に削除されます）"
}
# 退避した古い DLL は、掴んでいたプロセスが終われば消せる。消せるものだけ掃除する。
Get-ChildItem "$out\_old_PathlessMark_*.dll" -ErrorAction SilentlyContinue |
    ForEach-Object { Remove-Item $_.FullName -ErrorAction SilentlyContinue }

$rc = "rc /nologo /fo `"$out\badge.res`" `"$root\src\badge.rc`""
$cl = "cl /nologo /LD /O2 /utf-8 /std:c++17 /W4 /DUNICODE /D_UNICODE " +
      "`"$root\src\dllmain.cpp`" `"$out\badge.res`" " +
      "/Fe:`"$out\PathlessMark.dll`" /Fo:`"$out\\`" " +
      "/link /DEF:`"$root\src\exports.def`" ole32.lib shell32.lib advapi32.lib user32.lib"
$diagExe = "$out\PathlessMark-diagnose.exe"
Remove-Item $diagExe -ErrorAction SilentlyContinue
$diag = "cl /nologo /O2 /utf-8 /std:c++17 /W4 /DUNICODE /D_UNICODE " +
        "`"$root\src\diag.cpp`" /Fe:`"$diagExe`" /Fo:`"$out\\`" " +
        "/link /SUBSYSTEM:WINDOWS advapi32.lib user32.lib"

$setExe = "$out\PathlessMark-settings.exe"
Remove-Item $setExe -ErrorAction SilentlyContinue
$set = "cl /nologo /O2 /utf-8 /std:c++17 /W4 /DUNICODE /D_UNICODE " +
       "`"$root\src\settings.cpp`" /Fe:`"$setExe`" /Fo:`"$out\\`" " +
       "/link /SUBSYSTEM:WINDOWS advapi32.lib user32.lib gdi32.lib comdlg32.lib shell32.lib " +
       "msimg32.lib"

$cmd = "`"$dev`" -arch=x64 -no_logo && $rc && $cl && $diag && $set"

cmd /c $cmd

if (-not (Test-Path "$out\PathlessMark.dll")) { throw "ビルド失敗：DLL が生成されていません" }
if (-not (Test-Path $diagExe)) { throw "ビルド失敗：診断ツールが生成されていません" }
if (-not (Test-Path $setExe))  { throw "ビルド失敗：設定ツールが生成されていません" }
Write-Host ("OK -> $out\PathlessMark.dll  " + (Get-Item "$out\PathlessMark.dll").Length + " bytes")
Write-Host ("OK -> $diagExe  " + (Get-Item $diagExe).Length + " bytes")
Write-Host ("OK -> $setExe  " + (Get-Item $setExe).Length + " bytes")
