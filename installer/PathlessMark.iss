; PathlessMark インストーラ（Inno Setup 6）
;
; ■ シェル拡張を配るうえでの必須事項（ここを外すと事故る）
;   1. インストール/アンインストールとも **管理者権限**（HKLM に書くため）
;   2. **DLL が使用中だと上書きできない**＝Explorer が掴んでいる。
;      → インストール前に登録解除し、必要なら Explorer を再起動する
;   3. アンインストール時は**必ず登録を解除してから DLL を消す**。
;      逆順だと「登録は残っているのに DLL が無い」状態になり、Explorer が
;      毎回ロードを試みる（ユーザーの環境を壊す最悪の置き土産）
;   4. 64bit 専用（Explorer は 64bit プロセス）
;
; ビルド： "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\PathlessMark.iss

#define AppName    "PathlessMark"
#define AppVersion "1.0.0"
#define Publisher  "Nanana"
#define AppURL     "https://nanana03.github.io/pathlessmark/"

[Setup]
AppId={{CB7D3143-933B-43BF-87DF-D6D53CA4D4C6}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE.txt
OutputDir=..\dist
OutputBaseFilename=PathlessMark-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; 64bit 専用（Explorer は 64bit。32bit 環境では動かない）
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayIcon={app}\PathlessMark.dll

[Languages]
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "english";  MessagesFile: "compiler:Default.isl"

[Files]
; regserver で登録まで行う。uninsrestartdelete＝使用中なら再起動時に削除
Source: "..\bin\PathlessMark.dll"; DestDir: "{app}"; \
  Flags: ignoreversion regserver 64bit uninsrestartdelete
Source: "..\bin\PathlessMark-diagnose.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist_readme\README.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "..\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
; 15枠問題をユーザー自身が確認できる診断ツール（README で案内している導線）
Name: "{group}\{#AppName} 診断"; Filename: "{app}\PathlessMark-diagnose.exe"
Name: "{group}\{#AppName} のページ"; Filename: "{#AppURL}"
Name: "{group}\{#AppName} をアンインストール"; Filename: "{uninstallexe}"

[Run]
; インストール直後に Explorer を再起動して反映（任意）
Filename: "{cmd}"; Parameters: "/c taskkill /f /im explorer.exe & start explorer.exe"; \
  Description: "エクスプローラーを再起動して今すぐ反映する（開いているウィンドウは閉じます）"; \
  Flags: postinstall runhidden

[UninstallRun]
; ⚠️ 登録解除は Files の regserver が自動で行うが、Explorer が掴んだままだと
;    DLL が残ることがある。アンインストール後の Explorer 再起動を促す。
Filename: "{cmd}"; Parameters: "/c taskkill /f /im explorer.exe & start explorer.exe"; \
  RunOnceId: "RestartExplorer"; Flags: runhidden

[Messages]
japanese.WelcomeLabel2=このプログラムは、%n%n【サブフォルダを含まないフォルダ】にだけ小さな印を付けます。%n%nどこまで掘れば終わりなのかが、開かずに分かるようになります。%n%n※ エクスプローラーの拡張機能のため、インストール後にエクスプローラーの再起動が必要です。
