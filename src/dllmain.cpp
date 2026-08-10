// PathlessMark — 「サブフォルダを含まない（末端）フォルダ」にオーバーレイバッジを付ける
// シェル拡張（IShellIconOverlayIdentifier）。2026-08-10 プロトタイプ。
//
// ■ 仕様（ユーザー決定）
//   バッジは**サブフォルダが無いフォルダにのみ**表示する。
//   （大半のフォルダには子がいるため、少数派の「末端」に付ける方が情報量が多い）
//
// ■ 設計原則
//   IsMemberOf は Explorer の表示スレッドから同期で呼ばれる。
//   - ローカル固定ドライブ以外は即 S_FALSE（ネットワーク/リムーバブルはスキャンしない）
//   - 判定はショートサーキット（サブフォルダを1つ見つけた瞬間に打ち切り）
//   - TTL付きキャッシュ（既定30秒）。Explorer は同じパスを何度も聞いてくる
//   - 例外はすべて境界で握りつぶして S_FALSE（Explorer を道連れにしない）
//
// ■ ビルド・登録
//   build.ps1（要 VS C++ ワークロード）→ register.ps1（要管理者）→ Explorer 再起動
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>     // IShellIconOverlayIdentifier
#include <shlwapi.h>
#include <string>
#include <unordered_map>
#include <mutex>

#pragma comment(lib, "shlwapi.lib")

// {FB76B092-3930-496A-9E5C-8167BD96FCEC} PathlessMark 用に生成した固有CLSID（2026-08-10 改名時に振り直し）
static const CLSID CLSID_PathlessMark =
    {0xfb76b092, 0x3930, 0x496a, {0x9e, 0x5c, 0x81, 0x67, 0xbd, 0x96, 0xfc, 0xec}};

// 先頭スペース2個＝Qsync(6個)より後・OneDrive(1個)より前。15枠内を確実に取る
static const wchar_t kOverlayKeyName[] = L"  PathlessMark";

static HMODULE g_hModule = nullptr;
static LONG g_cRefDll = 0;

// ── コア判定 ─────────────────────────────────────────────
// サブフォルダが1つでもあれば true。見つけた瞬間に打ち切る。
static bool HasSubfolder(const wchar_t* dir) {
    std::wstring pat(dir);
    if (!pat.empty() && pat.back() != L'\\') pat += L'\\';
    pat += L'*';
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(pat.c_str(), FindExInfoBasic, &fd,
                                FindExSearchLimitToDirectories, nullptr,
                                FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
            found = true;
            break;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return found;
}

// ローカル固定ドライブだけを対象にする（ネットワーク・リムーバブルは重い/外れやすい）
static bool IsLocalFixedPath(const wchar_t* path) {
    if (!path || path[1] != L':') return false;   // UNC (\\server\...) を弾く
    wchar_t root[4] = {path[0], L':', L'\\', 0};
    return GetDriveTypeW(root) == DRIVE_FIXED;
}

// ── TTLキャッシュ ───────────────────────────────────────
struct CacheEntry { bool hasSub; ULONGLONG at; };
static std::unordered_map<std::wstring, CacheEntry> g_cache;
static std::mutex g_cacheMutex;
static const ULONGLONG kTtlMs = 30 * 1000;

static bool CachedHasSubfolder(const wchar_t* dir) {
    const ULONGLONG now = GetTickCount64();
    std::wstring key(dir);
    CharUpperW(&key[0]);   // パスの大文字小文字ゆれを吸収
    {
        std::lock_guard<std::mutex> lk(g_cacheMutex);
        auto it = g_cache.find(key);
        if (it != g_cache.end() && now - it->second.at < kTtlMs) return it->second.hasSub;
    }
    const bool v = HasSubfolder(dir);
    {
        std::lock_guard<std::mutex> lk(g_cacheMutex);
        if (g_cache.size() > 50000) g_cache.clear();   // 際限なく成長させない
        g_cache[key] = {v, now};
    }
    return v;
}

// ── オーバーレイハンドラ ────────────────────────────────────
class PathlessMarkOverlay : public IShellIconOverlayIdentifier {
    LONG m_cRef = 1;
public:
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IShellIconOverlayIdentifier) {
            *ppv = static_cast<IShellIconOverlayIdentifier*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
    IFACEMETHODIMP_(ULONG) Release() {
        ULONG c = InterlockedDecrement(&m_cRef);
        if (!c) delete this;
        return c;
    }

    // バッジの絵＝自DLL埋め込みの ICON リソース1番
    IFACEMETHODIMP GetOverlayInfo(PWSTR pwszIconFile, int cchMax, int* pIndex, DWORD* pdwFlags) {
        if (!GetModuleFileNameW(g_hModule, pwszIconFile, cchMax)) return E_FAIL;
        *pIndex = 0;
        *pdwFlags = ISIOI_ICONFILE | ISIOI_ICONINDEX;
        return S_OK;
    }

    IFACEMETHODIMP GetPriority(int* pPriority) {
        *pPriority = 50;
        return S_OK;
    }

    // 心臓部：S_OK を返すとバッジが付く。**サブフォルダが無い時に S_OK**。
    IFACEMETHODIMP IsMemberOf(PCWSTR pwszPath, DWORD /*dwAttrib*/) {
        __try {
            if (!pwszPath || !IsLocalFixedPath(pwszPath)) return S_FALSE;
            DWORD attr = GetFileAttributesW(pwszPath);
            if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
                return S_FALSE;                       // ファイルには付けない
            return CachedHasSubfolder(pwszPath) ? S_FALSE : S_OK;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return S_FALSE;                           // 何があっても Explorer を巻き込まない
        }
    }
};

// ── クラスファクトリ／自己登録（定型）───────────────────────────
class ClassFactory : public IClassFactory {
    LONG m_cRef = 1;
public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
    IFACEMETHODIMP_(ULONG) Release() {
        ULONG c = InterlockedDecrement(&m_cRef);
        if (!c) delete this;
        return c;
    }
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) {
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* p = new (std::nothrow) PathlessMarkOverlay();
        if (!p) return E_OUTOFMEMORY;
        HRESULT hr = p->QueryInterface(riid, ppv);
        p->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL lock) {
        lock ? InterlockedIncrement(&g_cRefDll) : InterlockedDecrement(&g_cRefDll);
        return S_OK;
    }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow() { return g_cRefDll ? S_FALSE : S_OK; }

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (rclsid != CLSID_PathlessMark) return CLASS_E_CLASSNOTAVAILABLE;
    auto* f = new (std::nothrow) ClassFactory();
    if (!f) return E_OUTOFMEMORY;
    HRESULT hr = f->QueryInterface(riid, ppv);
    f->Release();
    return hr;
}

// レジストリ登録：HKCR\CLSID\{...}\InProcServer32 と
// HKLM\...\ShellIconOverlayIdentifiers\<先頭スペース2個の名前>
static HRESULT SetKeyValue(HKEY root, const wchar_t* key, const wchar_t* name, const wchar_t* val) {
    HKEY h;
    LONG r = RegCreateKeyExW(root, key, 0, nullptr, 0, KEY_WRITE, nullptr, &h, nullptr);
    if (r != ERROR_SUCCESS) return HRESULT_FROM_WIN32(r);
    r = RegSetValueExW(h, name, 0, REG_SZ, (const BYTE*)val,
                       (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
    return HRESULT_FROM_WIN32(r);
}

STDAPI DllRegisterServer() {
    wchar_t clsid[64];
    StringFromGUID2(CLSID_PathlessMark, clsid, 64);
    wchar_t dll[MAX_PATH];
    GetModuleFileNameW(g_hModule, dll, MAX_PATH);

    std::wstring key = L"CLSID\\";
    key += clsid;
    HRESULT hr = SetKeyValue(HKEY_CLASSES_ROOT, key.c_str(), nullptr, L"PathlessMark Overlay");
    if (FAILED(hr)) return hr;
    hr = SetKeyValue(HKEY_CLASSES_ROOT, (key + L"\\InProcServer32").c_str(), nullptr, dll);
    if (FAILED(hr)) return hr;
    hr = SetKeyValue(HKEY_CLASSES_ROOT, (key + L"\\InProcServer32").c_str(),
                     L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;

    std::wstring ov = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\"
                      L"ShellIconOverlayIdentifiers\\";
    ov += kOverlayKeyName;
    return SetKeyValue(HKEY_LOCAL_MACHINE, ov.c_str(), nullptr, clsid);
}

STDAPI DllUnregisterServer() {
    wchar_t clsid[64];
    StringFromGUID2(CLSID_PathlessMark, clsid, 64);
    std::wstring key = L"CLSID\\";
    key += clsid;
    RegDeleteTreeW(HKEY_CLASSES_ROOT, key.c_str());
    std::wstring ov = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\"
                      L"ShellIconOverlayIdentifiers\\";
    ov += kOverlayKeyName;
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, ov.c_str());
    return S_OK;
}
