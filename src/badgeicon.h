// バッジの .ico をその場で描いて書き出す（設定ツールと DLL の共通コード）。
//
// ■ なぜ画像を持たずに描くのか
//   採用した図柄は「塗り丸＋白フチ」＝**幾何学的に厳密**。
//   生成画像を縮小すると 16px では輪郭がにじむが、各サイズを直接描けば常に鮮明。
//   さらに**色を実行時に差し替えられる**（ユーザーが好きな色を選べる）。
//
// ■ ICO の構造（ここを間違えると Windows が読まない）
//   ICONDIR → ICONDIRENTRY×N → 各画像（BITMAPINFOHEADER ＋ XOR(BGRA,下から上) ＋ ANDマスク）
//   ⚠️ BITMAPINFOHEADER の biHeight は **画像高さの2倍**（XOR+AND を積む仕様のため）
//   ⚠️ AND マスクは 32bpp でも**省略できない**。1bpp・各行4バイト境界に揃える
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <cstdio>
#include <cmath>

namespace badge {

// 図形の比率（サイズによらず一定に見えるよう割合で持つ）
constexpr double kOuterR = 0.46;   // 外周（白フチの外側）／辺長に対する半径
constexpr double kRing   = 0.13;   // 白フチの太さ／辺長
constexpr int    kSS     = 4;      // 1画素あたりのスーパーサンプル数（4×4）

struct BGRA { unsigned char b, g, r, a; };

/** 1サイズぶんの画素を作る（下から上＝ICO/BMP の並び）。 */
inline std::vector<BGRA> RenderSquare(int s, COLORREF fill, bool outline) {
    const double c = (s - 1) / 2.0;
    const double rOut = s * kOuterR;
    const double rIn  = outline ? rOut - s * kRing : rOut;
    const unsigned char fr = GetRValue(fill), fg = GetGValue(fill), fb = GetBValue(fill);

    std::vector<BGRA> px(static_cast<size_t>(s) * s);
    for (int y = 0; y < s; ++y) {
        for (int x = 0; x < s; ++x) {
            int inOut = 0, inIn = 0;
            for (int sy = 0; sy < kSS; ++sy) {
                for (int sx = 0; sx < kSS; ++sx) {
                    const double px_ = x + (sx + 0.5) / kSS;
                    const double py_ = y + (sy + 0.5) / kSS;
                    const double d = std::hypot(px_ - c - 0.5, py_ - c - 0.5);
                    if (d <= rOut) ++inOut;
                    if (d <= rIn)  ++inIn;
                }
            }
            const int total = kSS * kSS;
            const double aOut = static_cast<double>(inOut) / total;   // 全体の不透明度
            const double aIn  = static_cast<double>(inIn) / total;    // 中身（色）の割合
            const double aRing = aOut - aIn;                          // フチ（白）の割合

            BGRA p{};
            if (aOut <= 0.0) {
                p = {0, 0, 0, 0};
            } else {
                // 直乗せ（ストレート）アルファで色を合成する
                const double r = (fr * aIn + 255.0 * aRing) / aOut;
                const double g = (fg * aIn + 255.0 * aRing) / aOut;
                const double b = (fb * aIn + 255.0 * aRing) / aOut;
                p.r = static_cast<unsigned char>(r + 0.5);
                p.g = static_cast<unsigned char>(g + 0.5);
                p.b = static_cast<unsigned char>(b + 0.5);
                p.a = static_cast<unsigned char>(aOut * 255.0 + 0.5);
            }
            // ICO は下から上に並べる
            px[static_cast<size_t>(s - 1 - y) * s + x] = p;
        }
    }
    return px;
}

/** 複数サイズを1つの .ico に書き出す。成功なら true。 */
inline bool WriteIco(const wchar_t* path, COLORREF fill, bool outline,
                     const std::vector<int>& sizes = {16, 20, 24, 32, 48, 64}) {
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path, L"wb") != 0 || !fp) return false;

    const int n = static_cast<int>(sizes.size());
    // ── ICONDIR ──
    unsigned short hdr[3] = {0, 1, static_cast<unsigned short>(n)};
    fwrite(hdr, sizeof(hdr), 1, fp);

    // 各画像のサイズを先に計算（ディレクトリにオフセットを書くため）
    std::vector<unsigned int> bytes(n), offset(n);
    unsigned int cur = 6 + 16u * n;
    for (int i = 0; i < n; ++i) {
        const int s = sizes[i];
        const unsigned int andRow = ((s + 31) / 32) * 4u;   // 1bpp・4バイト境界
        bytes[i] = 40u + 4u * s * s + andRow * s;
        offset[i] = cur;
        cur += bytes[i];
    }

    // ── ICONDIRENTRY ──
    for (int i = 0; i < n; ++i) {
        const int s = sizes[i];
        unsigned char e[16] = {};
        e[0] = static_cast<unsigned char>(s == 256 ? 0 : s);   // 幅（256は0で表す）
        e[1] = static_cast<unsigned char>(s == 256 ? 0 : s);   // 高さ
        e[2] = 0;   // パレット数（32bppは0）
        e[3] = 0;
        *reinterpret_cast<unsigned short*>(e + 4) = 1;    // プレーン
        *reinterpret_cast<unsigned short*>(e + 6) = 32;   // ビット深度
        *reinterpret_cast<unsigned int*>(e + 8) = bytes[i];
        *reinterpret_cast<unsigned int*>(e + 12) = offset[i];
        fwrite(e, sizeof(e), 1, fp);
    }

    // ── 各画像 ──
    for (int i = 0; i < n; ++i) {
        const int s = sizes[i];
        BITMAPINFOHEADER bi = {};
        bi.biSize = sizeof(bi);
        bi.biWidth = s;
        bi.biHeight = s * 2;      // ⚠️ XOR と AND を積むので2倍
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        bi.biSizeImage = 4u * s * s;
        fwrite(&bi, sizeof(bi), 1, fp);

        const auto px = RenderSquare(s, fill, outline);
        fwrite(px.data(), sizeof(BGRA), px.size(), fp);

        // AND マスク（32bpp のアルファを使うので全0＝不透明扱いでよい）
        const unsigned int andRow = ((s + 31) / 32) * 4u;
        std::vector<unsigned char> mask(static_cast<size_t>(andRow) * s, 0);
        fwrite(mask.data(), 1, mask.size(), fp);
    }

    fclose(fp);
    return true;
}

// ── 設定の保存先 ───────────────────────────────────────────
// 色は HKCU（ユーザーごと）。アイコン本体は %APPDATA%\PathlessMark\badge.ico。
// ⚠️ DLL は HKLM に登録されるが**色はユーザー設定**なので HKCU に置く。
inline const wchar_t* kRegPath = L"Software\\PathlessMark";
inline const wchar_t* kRegColor = L"BadgeColor";
inline const wchar_t* kRegOutline = L"BadgeOutline";
constexpr COLORREF kDefaultColor = RGB(46, 125, 91);   // 深い緑

/** %APPDATA%\PathlessMark\badge.ico のパスを組み立てる（フォルダも作る）。 */
inline bool UserIcoPath(wchar_t* out, size_t cch, bool create) {
    wchar_t appdata[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;
    if (swprintf_s(out, cch, L"%s\\PathlessMark", appdata) < 0) return false;
    if (create) CreateDirectoryW(out, nullptr);
    return swprintf_s(out, cch, L"%s\\PathlessMark\\badge.ico", appdata) > 0;
}

inline COLORREF LoadColor() {
    HKEY h;
    COLORREF v = kDefaultColor;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &h) == ERROR_SUCCESS) {
        DWORD d = 0, cb = sizeof(d), type = 0;
        if (RegQueryValueExW(h, kRegColor, nullptr, &type, reinterpret_cast<LPBYTE>(&d), &cb)
            == ERROR_SUCCESS && type == REG_DWORD) {
            v = static_cast<COLORREF>(d);
        }
        RegCloseKey(h);
    }
    return v;
}

inline bool LoadOutline() {
    HKEY h;
    bool v = true;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &h) == ERROR_SUCCESS) {
        DWORD d = 1, cb = sizeof(d), type = 0;
        if (RegQueryValueExW(h, kRegOutline, nullptr, &type, reinterpret_cast<LPBYTE>(&d), &cb)
            == ERROR_SUCCESS && type == REG_DWORD) {
            v = (d != 0);
        }
        RegCloseKey(h);
    }
    return v;
}

inline bool SaveSettings(COLORREF color, bool outline) {
    HKEY h;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0, KEY_WRITE, nullptr, &h, nullptr)
        != ERROR_SUCCESS) return false;
    DWORD c = static_cast<DWORD>(color), o = outline ? 1 : 0;
    RegSetValueExW(h, kRegColor, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&c), sizeof(c));
    RegSetValueExW(h, kRegOutline, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&o), sizeof(o));
    RegCloseKey(h);
    return true;
}

}  // namespace badge
