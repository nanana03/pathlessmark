// PathlessMark 設定 — バッジの色を選ぶ（2026-08-10 ユーザー要望）。
//
// ■ 仕組み
//   選んだ色で %APPDATA%\PathlessMark\badge.ico をその場で描き、色を HKCU に保存する。
//   DLL 側は起動時にこのファイルがあればそれを、無ければ埋め込みアイコンを使う。
//   ＝**DLL を作り直さずに色を変えられる**。
//
// ⚠️ エクスプローラーはオーバーレイのアイコンを**起動時に読んで保持する**。
//    色を変えたら**エクスプローラーの再起動が要る**。これを黙っていると
//    「変えたのに反映されない＝壊れている」と受け取られるので、画面に明記して再起動も促す。
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <cwchar>
#include <vector>
#include "badgeicon.h"

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

static COLORREF g_color = badge::kDefaultColor;
static bool g_outline = true;

// 並べる色見本。左上から。最後の1枠は「その他の色…」
static const COLORREF kPresets[] = {
    RGB(46, 125, 91),   RGB(28, 100, 60),   RGB(90, 160, 80),   RGB(120, 180, 60),
    RGB(30, 110, 180),  RGB(60, 150, 200),  RGB(90, 90, 200),   RGB(130, 80, 190),
    RGB(200, 60, 60),   RGB(220, 110, 40),  RGB(225, 170, 40),  RGB(190, 60, 130),
    RGB(70, 70, 78),    RGB(130, 130, 138), RGB(250, 250, 250), RGB(20, 20, 20),
};
constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));
constexpr int kCols = 8, kSw = 34, kGap = 6, kSwX = 20, kSwY = 60;

constexpr int ID_APPLY = 101, ID_CUSTOM = 102, ID_OUTLINE = 103, ID_RESTART = 104;

// プレビュー：**アイコン生成そのもの**（badge::RenderSquare）の出力を貼る。
// ⚠️ ここで円を描き直さない。かつて GDI の Ellipse で似せて描いていたため、
//    ・寸法の基準がずれる（バッジ寸法基準 vs アイコン寸法基準）
//    ・図柄の t が反映されない
//    という食い違いが起きた（2026-08-10）。同じ関数を通せば構造的にずれない。
// ⚠️ 引数は「アイコン全体の左上と辺長」。オーバーレイはアイコンと同じ寸法で重ねられ、
//    絵柄はその左下に描かれる（badgeicon.h 参照）。
static void DrawDot(HDC dc, int iconX, int iconY, int size, COLORREF fill, bool outline) {
    if (size <= 0) return;
    std::vector<badge::BGRA> src = badge::RenderSquare(size, fill, outline);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;        // 上から下（RenderSquare は下から上なので反転して渡す）
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) { DeleteDC(mem); return; }
    // AlphaBlend は**乗算済みアルファ**を要求する
    auto* dst = static_cast<badge::BGRA*>(bits);
    for (int y = 0; y < size; ++y) {
        const badge::BGRA* row = &src[static_cast<size_t>(size - 1 - y) * size];
        for (int x = 0; x < size; ++x) {
            badge::BGRA p = row[x];
            p.b = static_cast<unsigned char>(p.b * p.a / 255);
            p.g = static_cast<unsigned char>(p.g * p.a / 255);
            p.r = static_cast<unsigned char>(p.r * p.a / 255);
            dst[static_cast<size_t>(y) * size + x] = p;
        }
    }
    HGDIOBJ old = SelectObject(mem, bmp);
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(dc, iconX, iconY, size, size, mem, 0, 0, size, size, bf);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static void PaintWindow(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(dc, &rc, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    SetBkMode(dc, TRANSPARENT);

    HFONT font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                             0, 0, CLEARTYPE_QUALITY, 0, L"Yu Gothic UI");
    HGDIOBJ oldFont = SelectObject(dc, font);

    TextOutW(dc, 20, 18, L"バッジの色を選んでください", 13);
    TextOutW(dc, 20, 36, L"（サブフォルダが無いフォルダに付く印の色です）", 22);

    // 色見本
    for (int i = 0; i < kPresetCount; ++i) {
        const int x = kSwX + (i % kCols) * (kSw + kGap);
        const int y = kSwY + (i / kCols) * (kSw + kGap);
        RECT r{x, y, x + kSw, y + kSw};
        HBRUSH b = CreateSolidBrush(kPresets[i]);
        FillRect(dc, &r, b);
        DeleteObject(b);
        // 選択中の枠
        if (kPresets[i] == g_color) {
            HPEN pen = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
            HGDIOBJ op = SelectObject(dc, pen);
            HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, x - 3, y - 3, x + kSw + 3, y + kSw + 3);
            SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(pen);
        } else {
            FrameRect(dc, &r, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
        }
    }

    // プレビュー（実寸16px と 拡大）
    const int py = kSwY + 2 * (kSw + kGap) + 26;
    // ⚠️ 文字数を直書きしない（文言を直すたびにズレる）
    const wchar_t* kPrevLabel = L"見え方（左がアイコン32px・右が48px）";
    TextOutW(dc, 20, py - 22, kPrevLabel, static_cast<int>(wcslen(kPrevLabel)));

    // 明るい背景と暗い背景の2通り
    for (int k = 0; k < 2; ++k) {
        const int bx = 20 + k * 150;
        RECT bg{bx, py, bx + 130, py + 74};
        HBRUSH bb = CreateSolidBrush(k == 0 ? RGB(250, 250, 250) : RGB(40, 40, 44));
        FillRect(dc, &bg, bb);
        DeleteObject(bb);
        FrameRect(dc, &bg, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
        // フォルダらしい四角
        for (int f = 0; f < 2; ++f) {
            const int fx = bx + 14 + f * 62, fy = py + 18;
            const int fs = f == 0 ? 32 : 48;
            RECT fr{fx, fy, fx + fs, fy + static_cast<int>(fs * 0.8)};
            HBRUSH fb = CreateSolidBrush(RGB(245, 199, 92));
            FillRect(dc, &fr, fb);
            DeleteObject(fb);
            // アイコン全体の箱＝fx,fy から一辺 fs。バッジはその左下1/4に描かれる
            DrawDot(dc, fx, fy, fs, g_color, g_outline);
        }
    }

    TextOutW(dc, 20, py + 88,
             L"⚠ 色を変えたら、エクスプローラーの再起動で反映されます。", 29);

    SelectObject(dc, oldFont);
    DeleteObject(font);
    EndPaint(hwnd, &ps);
}

static void ApplyColor(HWND hwnd) {
    wchar_t ico[MAX_PATH];
    if (!badge::UserIcoPath(ico, MAX_PATH, true) ||
        !badge::WriteIco(ico, g_color, g_outline)) {
        MessageBoxW(hwnd, L"アイコンの書き出しに失敗しました。", L"PathlessMark 設定", MB_ICONERROR);
        return;
    }
    badge::SaveSettings(g_color, g_outline);
    const int r = MessageBoxW(hwnd,
        L"色を保存しました。\n\n"
        L"エクスプローラーはアイコンを起動時に読み込むため、\n"
        L"再起動するまで表示は変わりません。\n\n"
        L"いま再起動しますか？（開いているフォルダは閉じます）",
        L"PathlessMark 設定", MB_ICONQUESTION | MB_YESNO);
    if (r == IDYES) {
        // taskkill ではなく ShellExecute 経由で自然に再起動させる
        ShellExecuteW(nullptr, L"open", L"cmd.exe",
                      L"/c taskkill /f /im explorer.exe & start explorer.exe",
                      nullptr, SW_HIDE);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowW(L"BUTTON", L"その他の色…", WS_CHILD | WS_VISIBLE,
                      20, 300, 120, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CUSTOM)), nullptr, nullptr);
        HWND cb = CreateWindowW(L"BUTTON", L"白いフチを付ける",
                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                      150, 305, 150, 22, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_OUTLINE)), nullptr, nullptr);
        SendMessageW(cb, BM_SETCHECK, g_outline ? BST_CHECKED : BST_UNCHECKED, 0);
        CreateWindowW(L"BUTTON", L"この色にする", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      320, 300, 130, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_APPLY)), nullptr, nullptr);
        return 0;
    }
    case WM_PAINT:
        PaintWindow(hwnd);
        return 0;
    case WM_LBUTTONDOWN: {
        const int x = LOWORD(lp), y = HIWORD(lp);
        for (int i = 0; i < kPresetCount; ++i) {
            const int sx = kSwX + (i % kCols) * (kSw + kGap);
            const int sy = kSwY + (i / kCols) * (kSw + kGap);
            if (x >= sx && x < sx + kSw && y >= sy && y < sy + kSw) {
                g_color = kPresets[i];
                InvalidateRect(hwnd, nullptr, TRUE);
                break;
            }
        }
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_CUSTOM: {
            static COLORREF custom[16] = {};
            CHOOSECOLORW cc = {};
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hwnd;
            cc.lpCustColors = custom;
            cc.rgbResult = g_color;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColorW(&cc)) {
                g_color = cc.rgbResult;
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }
        case ID_OUTLINE:
            g_outline = (SendMessageW(GetDlgItem(hwnd, ID_OUTLINE), BM_GETCHECK, 0, 0) == BST_CHECKED);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case ID_APPLY:
            ApplyColor(hwnd);
            return 0;
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    // 画面を出さずに .ico を書くオプション（ビルドと作り直し用）。
    //   /writeico <出力先>   既定色で書く（DLL に埋め込む予備アイコン src\badge.ico 用）
    //   /writeico user       保存済みの色で %APPDATA%\PathlessMark\badge.ico を書き直す
    // ⚠️ どちらも**設定ツールと同じコード**（badge::WriteIco）を通す。別実装で描き直すと、
    //    寸法を変えたときに片方だけ取り残される（2026-08-10 に左下1/4へ作り替えた際に実際に
    //    予備アイコンと %APPDATA% 側が古い形のまま残りかけた）。
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv) {
            const bool write = (argc >= 3 && _wcsicmp(argv[1], L"/writeico") == 0);
            bool ok = false;
            if (write) {
                if (_wcsicmp(argv[2], L"user") == 0) {
                    wchar_t p[MAX_PATH];
                    ok = badge::UserIcoPath(p, MAX_PATH, true) &&
                         badge::WriteIco(p, badge::LoadColor(), badge::LoadOutline());
                } else {
                    ok = badge::WriteIco(argv[2], badge::kDefaultColor, true);
                }
            }
            LocalFree(argv);
            if (write) return ok ? 0 : 1;
        }
    }

    g_color = badge::LoadColor();
    g_outline = badge::LoadOutline();

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"PathlessMarkSettings";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"PathlessMark 設定",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 500, 390,
                              nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOWNORMAL);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}
