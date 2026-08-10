// PathlessMark 診断ツール — 「なぜ印が出ないのか」をユーザー自身が確認できるようにする。
//
// ■ なぜ要るか
//   アイコンオーバーレイは **Windows 全体で15枠しかない**（Microsoft の実装上の制限）。
//   OneDrive が7枠、Dropbox が3〜4枠…と各社が取り合うため、枠が埋まっている PC では
//   どんなに正しく作っても印は出ない。これを説明せずに配ると
//   「壊れている」と受け取られる＝**製品側の説明責任**。
//
// ■ 何を出すか
//   登録されている全オーバーレイを**Windows が実際に見る順（辞書順）**で並べ、
//   15番目に線を引き、PathlessMark が線の上か下かを判定して日本語で結論を出す。
//
// ビルド： build.ps1 が PathlessMark-診断.exe として一緒に作る（/utf-8 必須）
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>

static const wchar_t kOverlayPath[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers";
static const wchar_t kOurName[] = L"  PathlessMark";
static const int kSlots = 15;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kOverlayPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        MessageBoxW(nullptr, L"レジストリを読めませんでした。", L"PathlessMark 診断", MB_ICONERROR);
        return 1;
    }

    std::vector<std::wstring> names;
    wchar_t buf[512];
    DWORD idx = 0, len = 512;
    while (RegEnumKeyExW(hKey, idx++, buf, &len, nullptr, nullptr, nullptr, nullptr)
           == ERROR_SUCCESS) {
        names.push_back(buf);
        len = 512;
    }
    RegCloseKey(hKey);

    // Windows が採用するのは「辞書順で先頭15件」。各社が先頭にスペースを入れて
    // 順番を稼いでいるのはこのため（PathlessMark もスペース2個で参加している）。
    std::sort(names.begin(), names.end());

    int ourPos = -1;
    for (size_t i = 0; i < names.size(); ++i)
        if (names[i] == kOurName) { ourPos = (int)i + 1; break; }

    std::wstring msg;
    msg += L"■ この PC のアイコンオーバーレイ枠\n\n";
    msg += L"Windows は全体で " + std::to_wstring(kSlots) + L" 枠しか持っていません。\n";
    msg += L"登録数：" + std::to_wstring(names.size()) + L" 件";
    if (names.size() > kSlots)
        msg += L"（うち " + std::to_wstring(names.size() - kSlots) + L" 件は枠あふれで無効）";
    else
        msg += L"（空き " + std::to_wstring(kSlots - names.size()) + L" 枠）";
    msg += L"\n\n";

    msg += L"― 有効（Windows が見る順）―\n";
    for (size_t i = 0; i < names.size(); ++i) {
        if ((int)i == kSlots) msg += L"―― ここから下は無効 ――\n";
        msg += std::to_wstring(i + 1) + L". " + names[i];
        if (names[i] == kOurName) msg += L"   ← PathlessMark";
        msg += L"\n";
    }

    msg += L"\n■ 結論\n";
    if (ourPos < 0) {
        msg += L"PathlessMark が登録されていません。インストールし直してください。";
    } else if (ourPos <= kSlots) {
        msg += L"PathlessMark は " + std::to_wstring(ourPos) + L" 番目＝有効です。\n"
               L"印が見えない場合は、エクスプローラーを再起動してください。";
    } else {
        msg += L"PathlessMark は " + std::to_wstring(ourPos) + L" 番目で、15枠からあふれています。\n"
               L"＝この PC では印が表示されません。\n\n"
               L"対処：使っていない同期ソフト（OneDrive・Dropbox 等）の\n"
               L"アイコン表示を減らすと枠が空きます。\n"
               L"※ これは Windows 側の制限で、PathlessMark の不具合ではありません。";
    }

    MessageBoxW(nullptr, msg.c_str(), L"PathlessMark 診断", MB_ICONINFORMATION);
    return 0;
}
