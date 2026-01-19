#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")

#define ID_CHECK_START 1000
#define ID_APPLY 2000
#define ID_REVERT 2001
#define ID_LOG 2002

typedef void (*tweak_fn)(BOOL apply);

typedef struct {
    const char* key;
    const char* name;
    const char* desc;
    BOOL applied;
    BOOL selected;
    tweak_fn fn;
} Tweak;

static HWND gLog;
static HWND gWnd;
static BOOL gBusy = FALSE;

static void LogLine(const char* msg) {
    int len = GetWindowTextLengthA(gLog);
    SendMessageA(gLog, EM_SETSEL, len, len);
    SendMessageA(gLog, EM_REPLACESEL, 0, (LPARAM)msg);
    SendMessageA(gLog, EM_REPLACESEL, 0, (LPARAM)"\r\n");
    SendMessageA(gLog, EM_SCROLLCARET, 0, 0);
}

static void RunCmd(const char* cmd) { system(cmd); }

// ---- Tweaks (safe-ish, reversible) ----
static void T_UltimatePerformance(BOOL apply) {
    if (apply) RunCmd("powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61");
    else       RunCmd("powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e");
}

static void T_EnableGameMode(BOOL apply) {
    HKEY k;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\GameBar", 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL)==ERROR_SUCCESS) {
        DWORD v = apply ? 1 : 0;
        RegSetValueExA(k, "AllowAutoGameMode", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegSetValueExA(k, "AutoGameModeEnabled", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(k);
    }
}

static void T_DisableGameBarDVR(BOOL apply) {
    HKEY k;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\GameDVR", 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL)==ERROR_SUCCESS) {
        DWORD v = apply ? 0 : 1;
        RegSetValueExA(k, "AppCaptureEnabled", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(k);
    }
}

static void T_ReduceTelemetry(BOOL apply) {
    HKEY k;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL)==ERROR_SUCCESS) {
        if (apply) {
            DWORD v = 0;
            RegSetValueExA(k, "AllowTelemetry", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        } else {
            RegDeleteValueA(k, "AllowTelemetry");
        }
        RegCloseKey(k);
    }
}

static void T_DisableUSBSelectiveSuspend(BOOL apply) {
    if (apply) {
        RunCmd("powercfg /setacvalueindex scheme_current sub_usb USBSELECTIVE 0");
        RunCmd("powercfg /setdcvalueindex scheme_current sub_usb USBSELECTIVE 0");
    } else {
        RunCmd("powercfg /setacvalueindex scheme_current sub_usb USBSELECTIVE 1");
        RunCmd("powercfg /setdcvalueindex scheme_current sub_usb USBSELECTIVE 1");
    }
    RunCmd("powercfg /S scheme_current");
}

static Tweak gTweaks[] = {
    {"UltimatePerformance", "Ultimate Performance", "Set Ultimate Performance power plan (revert = Balanced)", FALSE, FALSE, T_UltimatePerformance},
    {"EnableGameMode", "Enable Game Mode", "Enable Windows Game Mode (revert = disable)", FALSE, FALSE, T_EnableGameMode},
    {"DisableGameBarDVR", "Disable Game Bar/DVR", "Disable Xbox Game Bar capture + DVR (revert = enable)", FALSE, FALSE, T_DisableGameBarDVR},
    {"ReduceTelemetry", "Reduce Telemetry", "Policy-based telemetry reduction (revert = remove policy)", FALSE, FALSE, T_ReduceTelemetry},
    {"DisableUSBSelectiveSuspend", "Disable USB Selective Suspend", "Disable USB selective suspend (revert = enable)", FALSE, FALSE, T_DisableUSBSelectiveSuspend},
};
static const int gTweakCount = sizeof(gTweaks)/sizeof(gTweaks[0]);

static void SetBusy(BOOL busy) {
    gBusy = busy;
    EnableWindow(GetDlgItem(gWnd, ID_APPLY), !busy);
    EnableWindow(GetDlgItem(gWnd, ID_REVERT), !busy);
}

static DWORD WINAPI ApplyThread(LPVOID p) {
    (void)p;
    SetBusy(TRUE);

    LogLine("[*] Creating restore point...");
    RunCmd("powershell -NoProfile -ExecutionPolicy Bypass -Command \"Checkpoint-Computer -Description 'SHYX TWEAKS PREMIUM' -RestorePointType MODIFY_SETTINGS\"");

    for (int i=0;i<gTweakCount;i++) {
        if (!gTweaks[i].selected) continue;
        char buf[256];
        wsprintfA(buf, "[*] Applying: %s", gTweaks[i].name);
        LogLine(buf);
        gTweaks[i].fn(TRUE);
        gTweaks[i].applied = TRUE;
        wsprintfA(buf, "[+] Applied: %s", gTweaks[i].name);
        LogLine(buf);
    }

    LogLine("[+] Done. Some changes may need reboot.");
    MessageBoxA(gWnd, "Selected tweaks applied.\nSome changes may require a reboot.", "SHYX TWEAKS PREMIUM", MB_OK|MB_ICONINFORMATION);
    SetBusy(FALSE);
    return 0;
}

static DWORD WINAPI RevertThread(LPVOID p) {
    (void)p;
    SetBusy(TRUE);

    for (int i=0;i<gTweakCount;i++) {
        if (!gTweaks[i].selected) continue;
        char buf[256];
        wsprintfA(buf, "[*] Reverting: %s", gTweaks[i].name);
        LogLine(buf);
        gTweaks[i].fn(FALSE);
        gTweaks[i].applied = FALSE;
        wsprintfA(buf, "[+] Reverted: %s", gTweaks[i].name);
        LogLine(buf);
    }

    LogLine("[+] Done. Some changes may need reboot.");
    MessageBoxA(gWnd, "Selected tweaks reverted.\nSome changes may require a reboot.", "SHYX TWEAKS PREMIUM", MB_OK|MB_ICONINFORMATION);
    SetBusy(FALSE);
    return 0;
}

static void ToggleSelected(int idx) {
    gTweaks[idx].selected = !gTweaks[idx].selected;
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        gWnd = h;
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        CreateWindowA("STATIC", "SHYX TWEAKS PREMIUM", WS_CHILD|WS_VISIBLE|SS_CENTER,
                      10, 10, 680, 24, h, 0, 0, 0);
        CreateWindowA("STATIC", "User-controlled competitive optimization", WS_CHILD|WS_VISIBLE|SS_CENTER,
                      10, 34, 680, 18, h, 0, 0, 0);

        int y = 70;
        for (int i=0;i<gTweakCount;i++) {
            HWND cb = CreateWindowA("BUTTON", gTweaks[i].name, WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
                                    20, y, 320, 20, h, (HMENU)(INT_PTR)(ID_CHECK_START + i), 0, 0);
            SendMessage(cb, WM_SETFONT, (WPARAM)font, TRUE);

            HWND tx = CreateWindowA("STATIC", gTweaks[i].desc, WS_CHILD|WS_VISIBLE,
                                    350, y, 330, 20, h, 0, 0, 0);
            SendMessage(tx, WM_SETFONT, (WPARAM)font, TRUE);

            y += 26;
        }

        CreateWindowA("BUTTON", "APPLY SELECTED", WS_CHILD|WS_VISIBLE,
                      20, 220, 160, 34, h, (HMENU)ID_APPLY, 0, 0);
        CreateWindowA("BUTTON", "REVERT SELECTED", WS_CHILD|WS_VISIBLE,
                      200, 220, 160, 34, h, (HMENU)ID_REVERT, 0, 0);

        gLog = CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY,
                             20, 270, 660, 220, h, (HMENU)ID_LOG, 0, 0);

        LogLine("[+] Ready. Select tweaks then Apply/Revert. Nothing runs automatically.");
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id >= ID_CHECK_START && id < ID_CHECK_START + gTweakCount) {
            ToggleSelected(id - ID_CHECK_START);
            return 0;
        }
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
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show) {
    (void)hp; (void)cmd;

    // Require admin (relaunch elevated)
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

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.lpszClassName = "SHYX_TWEAKS_PREMIUM";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    RegisterClassA(&wc);

    HWND w = CreateWindowA("SHYX_TWEAKS_PREMIUM", "SHYX TWEAKS PREMIUM",
                           WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 720, 560,
                           NULL, NULL, hi, NULL);

    ShowWindow(w, show);
    UpdateWindow(w);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
