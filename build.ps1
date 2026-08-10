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
Remove-Item "$out\PathlessMark.dll" -ErrorAction SilentlyContinue

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
       "/link /SUBSYSTEM:WINDOWS advapi32.lib user32.lib gdi32.lib comdlg32.lib shell32.lib"

$cmd = "`"$dev`" -arch=x64 -no_logo && $rc && $cl && $diag && $set"

cmd /c $cmd

if (-not (Test-Path "$out\PathlessMark.dll")) { throw "ビルド失敗：DLL が生成されていません" }
if (-not (Test-Path $diagExe)) { throw "ビルド失敗：診断ツールが生成されていません" }
if (-not (Test-Path $setExe))  { throw "ビルド失敗：設定ツールが生成されていません" }
Write-Host ("OK -> $out\PathlessMark.dll  " + (Get-Item "$out\PathlessMark.dll").Length + " bytes")
Write-Host ("OK -> $diagExe  " + (Get-Item $diagExe).Length + " bytes")
Write-Host ("OK -> $setExe  " + (Get-Item $setExe).Length + " bytes")
