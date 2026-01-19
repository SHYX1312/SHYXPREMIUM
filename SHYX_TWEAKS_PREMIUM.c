// ============================================
// SHYX TWEAKS PREMIUM - Win32 GUI (C)
// ============================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <winreg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma comment(lib, "comctl32.lib")

#define ID_CHECKBOX_START 1000
#define ID_BUTTON_APPLY   2000
#define ID_BUTTON_REVERT  2001
#define ID_EDIT_LOG       2003

typedef struct {
    int id;
    char name[100];
    char description[200];
    BOOL applied;
    BOOL selected;
    void (*apply_func)(BOOL apply);
} Tweak;

static HWND hMainWindow;
static HWND hLog;
static HFONT hFont;
static HBRUSH hDarkBrush;
static HBRUSH hEditBrush;
static BOOL operationInProgress = FALSE;

static Tweak tweaks[64];
static int tweakCount = 0;

// -------------------- LOG --------------------
static void LogMessage(const char* message, BOOL success) {
    if (!hLog) return;

    time_t now;
    time(&now);
    struct tm* t = localtime(&now);

    char ts[16];
    sprintf(ts, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);

    char buf[600];
    sprintf(buf, "[%s] %s %s\r\n", ts, success ? "[+]" : "[-]", message);

    int len = GetWindowTextLength(hLog);
    SendMessage(hLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(hLog, EM_REPLACESEL, 0, (LPARAM)buf);
    SendMessage(hLog, EM_SCROLLCARET, 0, 0);
}

static void SetButtonsEnabled(BOOL enabled) {
    EnableWindow(GetDlgItem(hMainWindow, ID_BUTTON_APPLY), enabled);
    EnableWindow(GetDlgItem(hMainWindow, ID_BUTTON_REVERT), enabled);
    operationInProgress = !enabled;
}

// -------------------- TWEAKS --------------------
static void TweakUltimatePerf(BOOL apply) {
    if (apply) system("powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61");
    else       system("powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e");
}

static void TweakGameMode(BOOL apply) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\GameBar", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD v = apply ? 1 : 0;
        RegSetValueExA(hKey, "AllowAutoGameMode", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegSetValueExA(hKey, "AutoGameModeEnabled", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
}

static void TweakGameBarDVR(BOOL apply) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\GameDVR", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD v = apply ? 0 : 1; // apply = disable capture
        RegSetValueExA(hKey, "AppCaptureEnabled", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
}

static void TweakTelemetry(BOOL apply) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        if (apply) {
            DWORD v = 0;
            RegSetValueExA(hKey, "AllowTelemetry", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        } else {
            RegDeleteValueA(hKey, "AllowTelemetry");
        }
        RegCloseKey(hKey);
    }
}

static void TweakCPUPriority(BOOL apply) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\PriorityControl", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        DWORD v = apply ? 38 : 2;
        RegSetValueExA(hKey, "Win32PrioritySeparation", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
}

static void TweakHPET(BOOL apply) {
    if (apply) {
        system("bcdedit /set disabledynamictick yes");
        system("bcdedit /set useplatformclock no");
    } else {
        system("bcdedit /deletevalue disabledynamictick");
        system("bcdedit /deletevalue useplatformclock");
    }
}

static void TweakUSBSuspend(BOOL apply) {
    // Use powercfg aliases (safer than wrong registry edits)
    // 0 = Disabled, 1 = Enabled
    if (apply) {
        system("powercfg /setacvalueindex scheme_current sub_usb USBSELECTIVE 0");
        system("powercfg /setdcvalueindex scheme_current sub_usb USBSELECTIVE 0");
    } else {
        system("powercfg /setacvalueindex scheme_current sub_usb USBSELECTIVE 1");
        system("powercfg /setdcvalueindex scheme_current sub_usb USBSELECTIVE 1");
    }
    system("powercfg /S scheme_current");
}

static void InitializeTweaks(void) {
    tweakCount = 0;

    tweaks[tweakCount++] = (Tweak){0, "Ultimate Performance", "Enable Ultimate Performance power plan", FALSE, FALSE, TweakUltimatePerf};
    tweaks[tweakCount++] = (Tweak){1, "CPU Priority", "Optimize CPU priority distribution", FALSE, FALSE, TweakCPUPriority};
    tweaks[tweakCount++] = (Tweak){2, "HPET Disable", "Disable HPET-related timers (reversible)", FALSE, FALSE, TweakHPET};
    tweaks[tweakCount++] = (Tweak){3, "USB Selective Suspend", "Disable USB selective suspend (reversible)", FALSE, FALSE, TweakUSBSuspend};
    tweaks[tweakCount++] = (Tweak){4, "Game Mode", "Enable Windows Game Mode", FALSE, FALSE, TweakGameMode};
    tweaks[tweakCount++] = (Tweak){5, "Game Bar & DVR", "Disable Game Bar capture / DVR", FALSE, FALSE, TweakGameBarDVR};
    tweaks[tweakCount++] = (Tweak){6, "Telemetry Reduction", "Reduce diagnostic telemetry (policy-based)", FALSE, FALSE, TweakTelemetry};
}

// -------------------- THREADS --------------------
static DWORD WINAPI ApplySelectedThread(LPVOID p) {
    (void)p;
    SetButtonsEnabled(FALSE);

    LogMessage("Creating system restore point...", TRUE);
    system("powershell -NoProfile -ExecutionPolicy Bypass -Command \"Checkpoint-Computer -Description 'SHYX TWEAKS PREMIUM' -RestorePointType MODIFY_SETTINGS\"");

    for (int i = 0; i < tweakCount; i++) {
        if (!tweaks[i].selected) continue;

        char msg[256];
        sprintf(msg, "Applying: %s", tweaks[i].name);
        LogMessage(msg, TRUE);

        tweaks[i].apply_func(TRUE);
        tweaks[i].applied = TRUE;

        sprintf(msg, "Applied: %s", tweaks[i].name);
        LogMessage(msg, TRUE);
    }

    LogMessage("Apply complete.", TRUE);
    MessageBoxA(hMainWindow, "Selected tweaks applied.\nSome changes may require a reboot.", "SHYX TWEAKS PREMIUM", MB_ICONINFORMATION);
    SetButtonsEnabled(TRUE);
    InvalidateRect(hMainWindow, NULL, TRUE);
    return 0;
}

static DWORD WINAPI RevertSelectedThread(LPVOID p) {
    (void)p;
    SetButtonsEnabled(FALSE);

    for (int i = 0; i < tweakCount; i++) {
        if (!tweaks[i].selected) continue;

        char msg[256];
        sprintf(msg, "Reverting: %s", tweaks[i].name);
        LogMessage(msg, TRUE);

        tweaks[i].apply_func(FALSE);
        tweaks[i].applied = FALSE;

        sprintf(msg, "Reverted: %s", tweaks[i].name);
        LogMessage(msg, TRUE);
    }

    LogMessage("Revert complete.", TRUE);
    MessageBoxA(hMainWindow, "Selected tweaks reverted.\nSome changes may require a reboot.", "SHYX TWEAKS PREMIUM", MB_ICONINFORMATION);
    SetButtonsEnabled(TRUE);
    InvalidateRect(hMainWindow, NULL, TRUE);
    return 0;
}

// -------------------- UI --------------------
static void OnCheckboxClick(int id) {
    int idx = id - ID_CHECKBOX_START;
    if (idx >= 0 && idx < tweakCount) {
        tweaks[idx].selected = !tweaks[idx].selected;
    }
}

static void OnApplyButton(void) {
    if (operationInProgress) return;

    int count = 0;
    for (int i = 0; i < tweakCount; i++) if (tweaks[i].selected) count++;

    if (count == 0) {
        MessageBoxA(hMainWindow, "Select at least one tweak.", "SHYX TWEAKS PREMIUM", MB_ICONINFORMATION);
        return;
    }

    if (MessageBoxA(hMainWindow,
        "Apply selected tweaks?\n\nA system restore point will be created first.",
        "Confirm", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        CreateThread(NULL, 0, ApplySelectedThread, NULL, 0, NULL);
    }
}

static void OnRevertButton(void) {
    if (operationInProgress) return;

    int count = 0;
    for (int i = 0; i < tweakCount; i++) if (tweaks[i].selected) count++;

    if (count == 0) {
        MessageBoxA(hMainWindow, "Select at least one tweak.", "SHYX TWEAKS PREMIUM", MB_ICONINFORMATION);
        return;
    }

    if (MessageBoxA(hMainWindow,
        "Revert selected tweaks?",
        "Confirm", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        CreateThread(NULL, 0, RevertSelectedThread, NULL, 0, NULL);
    }
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        hMainWindow = hwnd;

        InitializeTweaks(); // IMPORTANT: before creating checkboxes

        hFont = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        CreateWindowA("STATIC", "SHYX TWEAKS PREMIUM",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 10, 680, 40, hwnd, NULL, NULL, NULL);

        HWND sub = CreateWindowA("STATIC", "Competitive Gaming Optimization",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 50, 680, 25, hwnd, NULL, NULL, NULL);

        SendMessage(GetDlgItem(hwnd, 0), WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(sub, WM_SETFONT, (WPARAM)hFont, TRUE);

        int y = 100;
        for (int i = 0; i < tweakCount; i++) {
            HWND chk = CreateWindowA("BUTTON", tweaks[i].name,
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                20, y, 300, 25, hwnd, (HMENU)(ID_CHECKBOX_START + i), NULL, NULL);
            SendMessage(chk, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND desc = CreateWindowA("STATIC", tweaks[i].description,
                WS_CHILD | WS_VISIBLE,
                330, y, 330, 25, hwnd, NULL, NULL, NULL);
            SendMessage(desc, WM_SETFONT, (WPARAM)hFont, TRUE);

            y += 30;
        }

        CreateWindowA("BUTTON", "APPLY SELECTED TWEAKS",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            20, 450, 200, 40, hwnd, (HMENU)ID_BUTTON_APPLY, NULL, NULL);

        CreateWindowA("BUTTON", "REVERT SELECTED TWEAKS",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            240, 450, 200, 40, hwnd, (HMENU)ID_BUTTON_REVERT, NULL, NULL);

        hLog = CreateWindowA("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | ES_MULTILINE | ES_READONLY,
            20, 500, 660, 220, hwnd, (HMENU)ID_EDIT_LOG, NULL, NULL);

        SendMessage(hLog, WM_SETFONT, (WPARAM)hFont, TRUE);

        CreateWindowA("STATIC", "© SHYX TWEAKS",
            WS_CHILD | WS_VISIBLE,
            20, 730, 200, 20, hwnd, NULL, NULL, NULL);

        LogMessage("Ready. Select tweaks and click APPLY or REVERT.", TRUE);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id >= ID_CHECKBOX_START && id < ID_CHECKBOX_START + tweakCount) {
            OnCheckboxClick(id);
        }
        else if (id == ID_BUTTON_APPLY) {
            OnApplyButton();
        }
        else if (id == ID_BUTTON_REVERT) {
            OnRevertButton();
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, RGB(10, 10, 10));
        return (LRESULT)hDarkBrush;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(0, 255, 0));
        SetBkColor(hdc, RGB(30, 30, 30));
        return (LRESULT)hEditBrush;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (hFont) DeleteObject(hFont);
        if (hDarkBrush) DeleteObject(hDarkBrush);
        if (hEditBrush) DeleteObject(hEditBrush);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hPrev; (void)lpCmd;

    // Require admin: relaunch elevated
    BOOL isAdmin = FALSE;
    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elev;
        DWORD sz = sizeof(elev);
        if (GetTokenInformation(token, TokenElevation, &elev, sz, &sz)) isAdmin = elev.TokenIsElevated;
        CloseHandle(token);
    }

    if (!isAdmin) {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        ShellExecuteA(NULL, "runas", path, NULL, NULL, SW_SHOWNORMAL);
        return 0;
    }

    hDarkBrush = CreateSolidBrush(RGB(10, 10, 10));
    hEditBrush = CreateSolidBrush(RGB(30, 30, 30));

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "SHYX_TWEAKS_CLASS";
    wc.hbrBackground = hDarkBrush;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        0, "SHYX_TWEAKS_CLASS", "SHYX TWEAKS PREMIUM",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 820,
        NULL, NULL, hInst, NULL
    );

    if (!hwnd) return 1;

    // Center
    RECT rc; GetWindowRect(hwnd, &rc);
    int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return 0;
}
