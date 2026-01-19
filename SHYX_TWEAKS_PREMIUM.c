// SHYX TWEAKS PREMIUM - Win32 C (Modern UI)
// Build: gcc -O2 -s -mwindows -o SHYX_TWEAKS_PREMIUM.exe SHYX_TWEAKS_PREMIUM.c -lcomctl32 -lshell32 -luxtheme -lgdi32

#define _WIN32_IE 0x0600
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winreg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma comment(lib,"comctl32.lib")

// ---------- IDs ----------
#define ID_LIST     1001
#define ID_APPLY    1002
#define ID_REVERT   1003
#define ID_LOG      1004

// ---------- Tweak model ----------
typedef void (*tweak_fn)(BOOL apply);

typedef struct {
    const char* key;
    const char* name;
    const char* desc;
    BOOL selected;
    BOOL applied;     // UI state
    tweak_fn fn;
} Tweak;

static HWND gWnd, gList, gLog;
static HFONT gFont;
static HBRUSH gBgBrush, gEditBrush;
static BOOL gBusy = FALSE;

static Tweak gTweaks[64];
static int gTweakCount = 0;

// ---------- Helpers ----------
static void LogLine(const char* msg, BOOL ok) {
    time_t now; time(&now);
    struct tm* t = localtime(&now);
    char ts[16]; sprintf(ts, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);

    char line[800];
    sprintf(line, "[%s] %s %s\r\n", ts, ok ? "[+]" : "[-]", msg);

    int len = GetWindowTextLengthA(gLog);
    SendMessageA(gLog, EM_SETSEL, len, len);
    SendMessageA(gLog, EM_REPLACESEL, 0, (LPARAM)line);
    SendMessageA(gLog, EM_SCROLLCARET, 0, 0);
}

static void RunCmd(const char* cmd) { system(cmd); }

static void RegSetDwordA(HKEY root, const char* path, const char* name, DWORD v) {
    HKEY k;
    if (RegCreateKeyExA(root, path, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(k, name, 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(k);
    }
}
static void RegDelValueA2(HKEY root, const char* path, const char* name) {
    HKEY k;
    if (RegOpenKeyExA(root, path, 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
        RegDeleteValueA(k, name);
        RegCloseKey(k);
    }
}

// ---------- IFEO Per-game priority (reversible) ----------
static void SetIFEO_Priority(const char* exeName, BOOL apply) {
    char keyPath[512];
    sprintf(keyPath, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\%s\\PerfOptions", exeName);

    if (apply) {
        // 3 = High priority class (documented), IoPriority 3 (High), PagePriority 5 (High-ish)
        RegSetDwordA(HKEY_LOCAL_MACHINE, keyPath, "CpuPriorityClass", 3);
        RegSetDwordA(HKEY_LOCAL_MACHINE, keyPath, "IoPriority", 3);
        RegSetDwordA(HKEY_LOCAL_MACHINE, keyPath, "PagePriority", 5);
    } else {
        RegDelValueA2(HKEY_LOCAL_MACHINE, keyPath, "CpuPriorityClass");
        RegDelValueA2(HKEY_LOCAL_MACHINE, keyPath, "IoPriority");
        RegDelValueA2(HKEY_LOCAL_MACHINE, keyPath, "PagePriority");
    }
}

// ---------- Fortnite config (backup + apply, revert restores backup) ----------
static BOOL GetFortniteGUSPath(char* outPath, size_t cap) {
    char local[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, local))) return FALSE;
    snprintf(outPath, cap, "%s\\FortniteGame\\Saved\\Config\\WindowsClient\\GameUserSettings.ini", local);
    return TRUE;
}

static BOOL FileExistsA2(const char* p) {
    DWORD a = GetFileAttributesA(p);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
}

static BOOL CopyFileBackupLatest(const char* src, char* outBackup, size_t cap) {
    char dir[MAX_PATH];
    strncpy(dir, src, MAX_PATH);
    char* last = strrchr(dir, '\\');
    if (!last) return FALSE;
    *last = 0;

    // backup name: GameUserSettings.ini.shyx.bak
    snprintf(outBackup, cap, "%s\\GameUserSettings.ini.shyx.bak", dir);
    return CopyFileA(src, outBackup, FALSE);
}

static void Fortnite_ApplyConfig(BOOL apply) {
    char gus[MAX_PATH], bak[MAX_PATH];
    if (!GetFortniteGUSPath(gus, sizeof(gus))) return;
    if (!FileExistsA2(gus)) return;

    if (apply) {
        if (!CopyFileBackupLatest(gus, bak, sizeof(bak))) return;

        // Very safe “competitive-ish” edits (no resolution changes)
        // We rewrite by appending overrides; Fortnite reads last occurrence.
        FILE* f = fopen(gus, "a");
        if (!f) return;

        fprintf(f, "\n; --- SHYX Competitive Overrides ---\n");
        fprintf(f, "bUseVSync=False\n");
        fprintf(f, "bMotionBlur=False\n");
        fprintf(f, "bShowGrass=False\n");
        fprintf(f, "FrameRateLimit=0.000000\n");
        fprintf(f, "sg.ResolutionQuality=100.000000\n");
        fprintf(f, "sg.ViewDistanceQuality=0\n");
        fprintf(f, "sg.AntiAliasingQuality=0\n");
        fprintf(f, "sg.ShadowQuality=0\n");
        fprintf(f, "sg.PostProcessQuality=0\n");
        fprintf(f, "sg.TextureQuality=0\n");
        fprintf(f, "sg.EffectsQuality=0\n");
        fprintf(f, "sg.FoliageQuality=0\n");
        fprintf(f, "bShowFPS=True\n");
        fclose(f);
    } else {
        // revert: restore backup if exists
        char dir[MAX_PATH];
        strncpy(dir, gus, MAX_PATH);
        char* last = strrchr(dir, '\\');
        if (!last) return;
        *last = 0;
        snprintf(bak, sizeof(bak), "%s\\GameUserSettings.ini.shyx.bak", dir);
        if (FileExistsA2(bak)) {
            CopyFileA(bak, gus, FALSE);
        }
    }
}

// ---------- System tweaks (safe-ish + reversible) ----------
static void T_UltimatePerf(BOOL apply) {
    if (apply) RunCmd("powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61");
    else       RunCmd("powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e");
}
static void T_GameMode(BOOL apply) {
    RegSetDwordA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\GameBar", "AllowAutoGameMode", apply ? 1 : 0);
    RegSetDwordA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\GameBar", "AutoGameModeEnabled", apply ? 1 : 0);
}
static void T_GameBarDVR(BOOL apply) {
    RegSetDwordA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "AppCaptureEnabled", apply ? 0 : 1);
}
static void T_HAGS(BOOL apply) {
    RegSetDwordA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", "HwSchMode", apply ? 2 : 1);
}
static void T_USBSelectiveSuspend(BOOL apply) {
    if (apply) {
        RunCmd("powercfg /setacvalueindex scheme_current sub_usb USBSELECTIVE 0");
        RunCmd("powercfg /setdcvalueindex scheme_current sub_usb USBSELECTIVE 0");
    } else {
        RunCmd("powercfg /setacvalueindex scheme_current sub_usb USBSELECTIVE 1");
        RunCmd("powercfg /setdcvalueindex scheme_current sub_usb USBSELECTIVE 1");
    }
    RunCmd("powercfg /S scheme_current");
}
static void T_TelemetryPolicy(BOOL apply) {
    if (apply) RegSetDwordA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", "AllowTelemetry", 0);
    else       RegDelValueA2(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", "AllowTelemetry");
}
static void T_BackgroundApps(BOOL apply) {
    if (apply) RegSetDwordA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications", "GlobalUserDisabled", 1);
    else       RegDelValueA2(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications", "GlobalUserDisabled");
}
static void T_DisableNagle(BOOL apply) {
    if (apply) {
        RunCmd("powershell -NoProfile -ExecutionPolicy Bypass -Command "
               "\"$p='HKLM:\\\\SYSTEM\\\\CurrentControlSet\\\\Services\\\\Tcpip\\\\Parameters\\\\Interfaces';"
               "Get-ChildItem $p | ForEach-Object {"
               "New-ItemProperty -Path $_.PsPath -Name TcpAckFrequency -PropertyType DWord -Value 1 -Force | Out-Null;"
               "New-ItemProperty -Path $_.PsPath -Name TCPNoDelay -PropertyType DWord -Value 1 -Force | Out-Null;"
               "New-ItemProperty -Path $_.PsPath -Name TcpDelAckTicks -PropertyType DWord -Value 0 -Force | Out-Null"
               "}\"");
    } else {
        RunCmd("powershell -NoProfile -ExecutionPolicy Bypass -Command "
               "\"$p='HKLM:\\\\SYSTEM\\\\CurrentControlSet\\\\Services\\\\Tcpip\\\\Parameters\\\\Interfaces';"
               "Get-ChildItem $p | ForEach-Object {"
               "Remove-ItemProperty -Path $_.PsPath -Name TcpAckFrequency -ErrorAction SilentlyContinue;"
               "Remove-ItemProperty -Path $_.PsPath -Name TCPNoDelay -ErrorAction SilentlyContinue;"
               "Remove-ItemProperty -Path $_.PsPath -Name TcpDelAckTicks -ErrorAction SilentlyContinue"
               "}\"");
    }
}

// ---------- Per-game toggles ----------
static void T_FortnitePriority(BOOL apply) { SetIFEO_Priority("FortniteClient-Win64-Shipping.exe", apply); }
static void T_CS2Priority(BOOL apply)      { SetIFEO_Priority("cs2.exe", apply); }
static void T_ValorantPriority(BOOL apply) { SetIFEO_Priority("VALORANT-Win64-Shipping.exe", apply); }
static void T_ApexPriority(BOOL apply)     { SetIFEO_Priority("r5apex.exe", apply); }
static void T_WarzonePriority(BOOL apply)  { SetIFEO_Priority("cod.exe", apply); }

// ---------- Tweak list init ----------
static void AddT(const char* key, const char* name, const char* desc, tweak_fn fn) {
    gTweaks[gTweakCount++] = (Tweak){ key, name, desc, FALSE, FALSE, fn };
}

static void InitTweaks(void) {
    gTweakCount = 0;

    // System / Gaming
    AddT("UltimatePerformance", "Ultimate Performance", "Power plan (revert = Balanced)", T_UltimatePerf);
    AddT("GameMode", "Enable Game Mode", "Windows Game Mode on/off", T_GameMode);
    AddT("GameBarDVR", "Disable Game Bar / DVR", "Disable capture/overlays (revert = enable)", T_GameBarDVR);
    AddT("HAGS", "Hardware GPU Scheduling", "Enable HAGS if supported (reboot)", T_HAGS);
    AddT("USBSelectiveSuspend", "Disable USB Selective Suspend", "Lower USB power saving latency", T_USBSelectiveSuspend);
    AddT("TelemetryPolicy", "Reduce Telemetry (Policy)", "Policy-based telemetry reduction", T_TelemetryPolicy);
    AddT("BackgroundApps", "Disable Background Apps", "Reduce background activity", T_BackgroundApps);
    AddT("DisableNagle", "Disable Nagle (TCP)", "Low-latency TCP settings (network-dependent)", T_DisableNagle);

    // Game specific
    AddT("FortnitePriority", "Fortnite Priority Boost", "High CPU/IO/Page priority via IFEO (reversible)", T_FortnitePriority);
    AddT("CS2Priority", "CS2 Priority Boost", "High CPU/IO/Page priority via IFEO (reversible)", T_CS2Priority);
    AddT("ValorantPriority", "Valorant Priority Boost", "High CPU/IO/Page priority via IFEO (reversible)", T_ValorantPriority);
    AddT("ApexPriority", "Apex Priority Boost", "High CPU/IO/Page priority via IFEO (reversible)", T_ApexPriority);
    AddT("WarzonePriority", "Warzone Priority Boost", "High CPU/IO/Page priority via IFEO (reversible)", T_WarzonePriority);

    // Fortnite config
    AddT("FortniteConfig", "Fortnite Competitive Config", "Backup + apply safe competitive overrides (revert restores backup)", Fortnite_ApplyConfig);
}

// ---------- ListView UI ----------
static void LV_SetColumns(void) {
    LVCOLUMNA c = {0};
    c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    c.cx = 240; c.pszText = "Tweak";        ListView_InsertColumn(gList, 0, &c);
    c.cx = 380; c.pszText = "Description";  c.iSubItem = 1; ListView_InsertColumn(gList, 1, &c);
    c.cx = 90;  c.pszText = "State";        c.iSubItem = 2; ListView_InsertColumn(gList, 2, &c);
}

static void LV_Populate(void) {
    ListView_DeleteAllItems(gList);

    for (int i=0;i<gTweakCount;i++) {
        LVITEMA it = {0};
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = i;
        it.pszText = (LPSTR)gTweaks[i].name;
        it.lParam = (LPARAM)i;

        int row = ListView_InsertItem(gList, &it);
        ListView_SetItemText(gList, row, 1, (LPSTR)gTweaks[i].desc);
        ListView_SetItemText(gList, row, 2, (LPSTR)(gTweaks[i].applied ? "Applied" : "Not Applied"));
        ListView_SetCheckState(gList, row, gTweaks[i].selected ? TRUE : FALSE);
    }
}

static void SetBusy(BOOL busy) {
    gBusy = busy;
    EnableWindow(GetDlgItem(gWnd, ID_APPLY), !busy);
    EnableWindow(GetDlgItem(gWnd, ID_REVERT), !busy);
    EnableWindow(gList, !busy);
}

// ---------- Apply/Revert threads ----------
static DWORD WINAPI ApplyThread(LPVOID p) {
    (void)p;
    SetBusy(TRUE);

    LogLine("Creating restore point...", TRUE);
    RunCmd("powershell -NoProfile -ExecutionPolicy Bypass -Command \"Checkpoint-Computer -Description 'SHYX TWEAKS PREMIUM' -RestorePointType MODIFY_SETTINGS\"");

    for (int i=0;i<gTweakCount;i++) {
        if (!gTweaks[i].selected) continue;
        char m[256];
        sprintf(m, "Applying: %s", gTweaks[i].name);
        LogLine(m, TRUE);

        gTweaks[i].fn(TRUE);
        gTweaks[i].applied = TRUE;

        sprintf(m, "Applied: %s", gTweaks[i].name);
        LogLine(m, TRUE);
    }

    LogLine("Apply complete.", TRUE);
    LV_Populate();
    MessageBoxA(gWnd, "Selected tweaks applied.\nSome changes may require reboot.", "SHYX TWEAKS PREMIUM", MB_OK|MB_ICONINFORMATION);

    SetBusy(FALSE);
    return 0;
}

static DWORD WINAPI RevertThread(LPVOID p) {
    (void)p;
    SetBusy(TRUE);

    for (int i=0;i<gTweakCount;i++) {
        if (!gTweaks[i].selected) continue;
        char m[256];
        sprintf(m, "Reverting: %s", gTweaks[i].name);
        LogLine(m, TRUE);

        gTweaks[i].fn(FALSE);
        gTweaks[i].applied = FALSE;

        sprintf(m, "Reverted: %s", gTweaks[i].name);
        LogLine(m, TRUE);
    }

    LogLine("Revert complete.", TRUE);
    LV_Populate();
    MessageBoxA(gWnd, "Selected tweaks reverted.\nSome changes may require reboot.", "SHYX TWEAKS PREMIUM", MB_OK|MB_ICONINFORMATION);

    SetBusy(FALSE);
    return 0;
}

// ---------- Window proc ----------
static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch(msg) {
    case WM_CREATE: {
        gWnd = h;

        gBgBrush = CreateSolidBrush(RGB(16,16,18));
        gEditBrush = CreateSolidBrush(RGB(28,28,34));
        gFont = CreateFontA(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,"Segoe UI");

        InitTweaks();

        CreateWindowA("STATIC", "SHYX TWEAKS PREMIUM", WS_CHILD|WS_VISIBLE,
            18, 14, 520, 22, h, 0, 0, 0);

        CreateWindowA("STATIC", "Modern UI • Scrollable • Apply/Revert • Per-Game + Fortnite Config", WS_CHILD|WS_VISIBLE,
            18, 38, 700, 18, h, 0, 0, 0);

        gList = CreateWindowA(WC_LISTVIEWA, "", WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT,
            18, 65, 744, 300, h, (HMENU)ID_LIST, 0, 0);
        SendMessage(gList, WM_SETFONT, (WPARAM)gFont, TRUE);
        ListView_SetExtendedListViewStyle(gList, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);

        LV_SetColumns();
        LV_Populate();

        CreateWindowA("BUTTON", "APPLY SELECTED", WS_CHILD|WS_VISIBLE,
            18, 375, 170, 34, h, (HMENU)ID_APPLY, 0, 0);

        CreateWindowA("BUTTON", "REVERT SELECTED", WS_CHILD|WS_VISIBLE,
            198, 375, 170, 34, h, (HMENU)ID_REVERT, 0, 0);

        gLog = CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY,
            18, 420, 744, 200, h, (HMENU)ID_LOG, 0, 0);
        SendMessage(gLog, WM_SETFONT, (WPARAM)gFont, TRUE);

        LogLine("Ready. Select tweaks in the list. Nothing runs automatically.", TRUE);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id == ID_APPLY && !gBusy) {
            CreateThread(NULL, 0, ApplyThread, NULL, 0, NULL);
            return 0;
        }
        if (id == ID_REVERT && !gBusy) {
            CreateThread(NULL, 0, RevertThread, NULL, 0, NULL);
            return 0;
        }
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* hdr = (NMHDR*)l;
        if (hdr->idFrom == ID_LIST && hdr->code == LVN_ITEMCHANGED) {
            NMLISTVIEW* n = (NMLISTVIEW*)l;
            if ((n->uChanged & LVIF_STATE) && (n->iItem >= 0) && (n->iItem < gTweakCount)) {
                BOOL checked = ListView_GetCheckState(gList, n->iItem);
                gTweaks[n->iItem].selected = checked ? TRUE : FALSE;
            }
        }
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w;
        SetTextColor(dc, RGB(0,255,120));
        SetBkColor(dc, RGB(28,28,34));
        return (LRESULT)gEditBrush;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        SetTextColor(dc, RGB(240,240,240));
        SetBkColor(dc, RGB(16,16,18));
        return (LRESULT)gBgBrush;
    }

    case WM_ERASEBKGND: {
        RECT r; GetClientRect(h, &r);
        FillRect((HDC)w, &r, gBgBrush);
        return 1;
    }

    case WM_DESTROY:
        if (gFont) DeleteObject(gFont);
        if (gBgBrush) DeleteObject(gBgBrush);
        if (gEditBrush) DeleteObject(gEditBrush);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, msg, w, l);
}

// ---------- main ----------
int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show) {
    (void)hp; (void)cmd;

    // Elevate
    BOOL isAdmin = FALSE;
    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION e; DWORD sz = sizeof(e);
        if (GetTokenInformation(token, TokenElevation, &e, sizeof(e), &sz)) isAdmin = e.TokenIsElevated;
        CloseHandle(token);
    }
    if (!isAdmin) {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        ShellExecuteA(NULL, "runas", path, NULL, NULL, SW_SHOWNORMAL);
        return 0;
    }

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.lpszClassName = "SHYX_PREMIUM";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    HWND w = CreateWindowA("SHYX_PREMIUM", "SHYX TWEAKS PREMIUM",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 680,
        NULL, NULL, hi, NULL);

    ShowWindow(w, show);
    UpdateWindow(w);

    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return 0;
}
