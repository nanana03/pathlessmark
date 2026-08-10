# PathlessMark の登録解除（要・管理者）
#   powershell -ExecutionPolicy Bypass -File unregister.ps1
$dll = "$PSScriptRoot\bin\PathlessMark.dll"
& regsvr32 /u /s $dll
Write-Host "解除しました。Explorer 再起動で完全に消えます。"
