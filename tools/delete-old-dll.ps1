#Requires -Version 5.1
<#
  古いビルドの残骸 bin\_old_PathlessMark_112916.dll を消す。

      powershell -ExecutionPolicy Bypass -File tools\delete-old-dll.ps1

  なぜ普通に消せないか：この DLL は**シェル拡張として読み込まれた状態のままリネームされた**ため、
  読み込んだプロセス（Explorer など）が像を掴んだままになっている。掴まれている間は
  管理者でも削除できない。だからこの順で試す。

    ① そのまま削除     … 掴みが外れていれば、これで終わり（再起動後はこれ）
    ② 再起動時削除を予約 … MoveFileEx(MOVEFILE_DELAY_UNTIL_REBOOT)。
                          HKLM に書くので**管理者権限が要る**（無いと ② は失敗する）

  ⚠️ 登録中の本体は bin\PathlessMark.dll（222,720 bytes）。このスクリプトは触らない。
#>
$ErrorActionPreference = 'Stop'
$target = Join-Path (Split-Path $PSScriptRoot -Parent) 'bin\_old_PathlessMark_112916.dll'

if (-not (Test-Path $target)) { Write-Output "すでにありません: $target"; exit 0 }
$size = (Get-Item $target).Length
Write-Output "対象: $target ($size bytes)"
if ($size -eq 222720) { Write-Output '中止：これは登録中の本体と同じサイズです。手を止めます。'; exit 1 }

try {
  Remove-Item $target -Force -ErrorAction Stop
  Write-Output '削除しました（掴みは外れていました）'
  exit 0
} catch {
  Write-Output "いま削除できません: $($_.Exception.Message)"
}

# ② 再起動時削除の予約
$sig = @'
[DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
public static extern bool MoveFileEx(string lpExistingFileName, string lpNewFileName, int dwFlags);
'@
$k = Add-Type -MemberDefinition $sig -Name 'NativeMove' -Namespace 'Win32' -PassThru
$MOVEFILE_DELAY_UNTIL_REBOOT = 0x4
# ⚠️ 第2引数は **NULL** でなければならない（削除の意味）。PowerShell の $null は空文字として
#    渡ってしまい、Win32 エラー 3（パスが見つからない）になる＝[NullString]::Value を使う。
if ($k::MoveFileEx($target, [NullString]::Value, $MOVEFILE_DELAY_UNTIL_REBOOT)) {
  Write-Output '次回の再起動時に削除するよう予約しました（PendingFileRenameOperations）'
  exit 0
}
$err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
Write-Output "予約も失敗（Win32 エラー $err）。管理者で実行し直してください。"
exit 1
