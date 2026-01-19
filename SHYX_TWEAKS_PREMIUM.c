// SHYX TWEAKS PREMIUM - Win32 C
// Apply/Revert + Auto-Updater (GitHub Releases) + Release Notes popup
//
// Build:
// gcc -O2 -s -mwindows -o SHYX_TWEAKS_PREMIUM.exe SHYX_TWEAKS_PREMIUM.c -lcomctl32 -lshell32 -lurlmon

#define _WIN32_IE 0x0600
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <urlmon.h>
#include <winreg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "urlmon.lib")

// -------------------- App Version --------------------
#define APP_VERSION "1.0.0"

// -------------------- Update URLs --------------------
#define UPDATE_VERSION_URL "https://github.com/SHYX1312/SHYXPREMIUM/releases/latest/download/version.json"
#define UPDATE_EXE_URL     "https://github.com/SHYX1312/SHYXPREMIUM/releases/latest/download/SHYX_TWEAKS_PREMIUM.exe"

// -------------------- UI IDs --------------------
#define ID_CHECKBOX_START 1000
#define ID_BUTTON_APPLY   2000
#define ID_BUTTON_REVERT  2001
#define ID_BUTTON_UPDATE  2002
#define ID_LOG            3000

typedef void (*tweak_fn)(BOOL);

typedef struct {
    int id;
    const char* name;
    const char* description;
    BOOL applied;
    BOOL selected;
    tweak_fn apply_func;
} Tweak;

static HWND hMainWindow;
static HWND hLog;
static HFONT hFont;
static HBRUSH hDarkBrush;
static Tweak tweaks[128];
static int tweakCount = 0;
static BOOL operationInProgress = FALSE;
static char workingDir[MAX_PATH];

// -------------------- Logging --------------------
static void LogMessage(const char* message, BOOL success) {
    time_t now; time(&now);
    struct tm* timeinfo = localtime(&now);
    char timestamp[20];
    sprintf(timestamp, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    char buffer[900];
    sprintf(buffer, "[%s] %s %s\r\n", timestamp, success ? "[+]" : "[-]", message);

    int len = GetWindowTextLengthA(hLog);
    SendMessageA(hLog, EM_SETSEL, len, len);
    SendMessageA(hLog, EM_REPLACESEL, 0, (LPARAM)buffer);
    SendMessageA(hLog, EM_SCROLLCARET, 0, 0);
}

static void SetButtonsEnabled(BOOL enabled) {
    EnableWindow(GetDlgItem(hMainWindow, ID_BUTTON_APPLY), enabled);
    EnableWindow(GetDlgItem(hMainWindow, ID_BUTTON_REVERT), enabled);
    EnableWindow(GetDlgItem(hMainWindow, ID_BUTTON_UPDATE), enabled);
    operationInProgress = !enabled;
}

// -------------------- Helpers --------------------
static void EnsureWorkingDir(void) {
    char progData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, progData))) {
        snprintf(workingDir, sizeof(workingDir), "%s\\SHYX_TWEAKS", progData);
        CreateDirectoryA(workingDir, NULL);
    } else {
        // fallback
        GetTempPathA((DWORD)sizeof(workingDir), workingDir);
    }
}

static BOOL FileExistsA2(const char* p) {
    DWORD a = GetFileAttributesA(p);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
}

static int CompareVersions(const char* a, const char* b) {
    int ax=0, ay=0, az=0, bx=0, by=0, bz=0;
    sscanf(a, "%d.%d.%d", &ax, &ay, &az);
    sscanf(b, "%d.%d.%d", &bx, &by, &bz);
    if (ax != bx) return (ax > bx) ? 1 : -1;
    if (ay != by) return (ay > by) ? 1 : -1;
    if (az != bz) return (az > bz) ? 1 : -1;
    return 0;
}

static void JsonExtractString(const char* json, const char* key, char* out, size_t cap) {
    out[0] = 0;
    if (!json || !key) return;

    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char* p = strstr(json, pattern);
    if (!p) return;

    p = strchr(p, ':');
    if (!p) return;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return;
    p++;

    size_t i = 0;
    while (*p && *p != '"' && i + 1 < cap) {
        // Basic unescape for \n
        if (p[0] == '\\' && p[1] == 'n') {
            out[i++] = '\n';
            p += 2;
            continue;
        }
        out[i++] = *p++;
    }
    out[i] = 0;
}

static HRESULT DownloadToFile(const char* url, const char* outPath) {
    // URLDownloadToFileA follows redirects and works well for GitHub release assets.
    DeleteFileA(outPath);
    return URLDownloadToFileA(NULL, url, outPath, 0, NULL);
}

static void ShowPendingReleaseNotesIfAny(void) {
    char notesPath[MAX_PATH];
    snprintf(notesPath, sizeof(notesPath), "%s\\pending_update.txt", workingDir);

    if (!FileExistsA2(notesPath)) return;

    FILE* f = fopen(notesPath, "rb");
    if (!f) return;

    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;

    // Delete first so it doesn't repeat if user closes popup
    DeleteFileA(notesPath);

    MessageBoxA(hMainWindow, buf, "SHYX TWEAKS PREMIUM - Updated", MB_OK | MB_ICONINFORMATION);
}

// -------------------- Tweak implementations (keep what you already had, but safer defaults) --------------------
static void TweakUltimatePerf(BOOL apply) {
    if (apply) system("powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61");
    else       system("powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e");
}

static void TweakGameMode(BOOL apply) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\GameBar", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = apply ? 1 : 0;
        RegSetValueExA(hKey, "AllowAutoGameMode", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegSetValueExA(hKey, "AutoGameModeEnabled", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

static void TweakGameBar(BOOL apply) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\GameDVR", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = apply ? 0 : 1;
        RegSetValueExA(hKey, "AppCaptureEnabled", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegSetValueExA(hKey, "AudioCaptureEnabled", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

static void TweakTelemetry(BOOL apply) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        if (apply) {
            DWORD value = 0;
            RegSetValueExA(hKey, "AllowTelemetry", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        } else {
            RegDeleteValueA(hKey, "AllowTelemetry");
        }
        RegCloseKey(hKey);
    }
}

static void TweakCPUPriority(BOOL apply) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\PriorityControl", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        DWORD value = apply ? 38 : 2;
        RegSetValueExA(hKey, "Win32PrioritySeparation", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

static void TweakGPUScheduling(BOOL apply) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        DWORD value = apply ? 2 : 1;
        RegSetValueExA(hKey, "HwSchMode", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

static void TweakSysMainManual(BOOL apply) {
    if (apply) {
        system("sc config SysMain start= demand");
    } else {
        system("sc config SysMain start= auto");
    }
}

static void TweakDeliveryOptimization(BOOL apply) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DeliveryOptimization", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        if (apply) {
            DWORD value = 0;
            RegSetValueExA(hKey, "DODownloadMode", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        } else {
            RegDeleteValueA(hKey, "DODownloadMode");
        }
        RegCloseKey(hKey);
    }
}

// -------------------- Tweaks list --------------------
static void InitializeTweaks(void) {
    tweakCount = 0;
    tweaks[tweakCount++] = (Tweak){0, "Ultimate Performance", "Enable Ultimate Performance power plan", FALSE, FALSE, TweakUltimatePerf};
    tweaks[tweakCount++] = (Tweak){1, "Game Mode", "Enable Windows Game Mode", FALSE, FALSE, TweakGameMode};
    tweaks[tweakCount++] = (Tweak){2, "Disable Game Bar/DVR", "Disable overlays and recording", FALSE, FALSE, TweakGameBar};
    tweaks[tweakCount++] = (Tweak){3, "GPU Scheduling (HAGS)", "Enable Hardware GPU Scheduling (reboot)", FALSE, FALSE, TweakGPUScheduling};
    tweaks[tweakCount++] = (Tweak){4, "CPU Priority Separation", "Optimize CPU scheduler distribution", FALSE, FALSE, TweakCPUPriority};
    tweaks[tweakCount++] = (Tweak){5, "Reduce Telemetry", "Set AllowTelemetry policy value", FALSE, FALSE, TweakTelemetry};
    tweaks[tweakCount++] = (Tweak){6, "Delivery Optimization Off", "Reduce background bandwidth", FALSE, FALSE, TweakDeliveryOptimization};
    tweaks[tweakCount++] = (Tweak){7, "SysMain Manual", "Set SysMain to Manual (safer than disabling)", FALSE, FALSE, TweakSysMainManual};
}

static void SyncSelectionsFromUI(void) {
    for (int i = 0; i < tweakCount; i++) {
        HWND hCheck = GetDlgItem(hMainWindow, ID_CHECKBOX_START + i);
        if (!hCheck) continue;
        tweaks[i].selected = (SendMessageA(hCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
}

// -------------------- Threads: Apply/Revert --------------------
static DWORD WINAPI ApplyTweaksThread(LPVOID param) {
    (void)param;
    SetButtonsEnabled(FALSE);
    LogMessage("Starting tweak application...", TRUE);

    LogMessage("Creating system restore point...", TRUE);
    system("powershell -NoProfile -ExecutionPolicy Bypass -Command \"Checkpoint-Computer -Description 'SHYX TWEAKS PREMIUM' -RestorePointType MODIFY_SETTINGS\"");

    for (int i = 0; i < tweakCount; i++) {
        if (tweaks[i].selected && tweaks[i].apply_func) {
            char msg[300];
            sprintf(msg, "Applying: %s", tweaks[i].name);
            LogMessage(msg, TRUE);

            tweaks[i].apply_func(TRUE);
            tweaks[i].applied = TRUE;

            sprintf(msg, "Applied: %s", tweaks[i].name);
            LogMessage(msg, TRUE);
        }
    }

    LogMessage("Tweak application complete!", TRUE);
    MessageBoxA(hMainWindow, "Selected tweaks applied successfully!\n\nSome changes may require restart.", "SHYX TWEAKS PREMIUM", MB_OK | MB_ICONINFORMATION);

    SetButtonsEnabled(TRUE);
    return 0;
}

static DWORD WINAPI RevertTweaksThread(LPVOID param) {
    (void)param;
    SetButtonsEnabled(FALSE);
    LogMessage("Starting tweak revert...", TRUE);

    for (int i = 0; i < tweakCount; i++) {
        if (tweaks[i].selected && tweaks[i].apply_func) {
            char msg[300];
            sprintf(msg, "Reverting: %s", tweaks[i].name);
            LogMessage(msg, TRUE);

            tweaks[i].apply_func(FALSE);
            tweaks[i].applied = FALSE;

            sprintf(msg, "Reverted: %s", tweaks[i].name);
            LogMessage(msg, TRUE);
        }
    }

    LogMessage("Revert complete!", TRUE);
    MessageBoxA(hMainWindow, "Selected tweaks reverted successfully!\n\nSome changes may require restart.", "SHYX TWEAKS PREMIUM", MB_OK | MB_ICONINFORMATION);

    SetButtonsEnabled(TRUE);
    return 0;
}

// -------------------- Updater --------------------
static DWORD WINAPI UpdateThread(LPVOID param) {
    BOOL silent = (param != NULL);
    SetButtonsEnabled(FALSE);

    char localVersionPath[MAX_PATH];
    snprintf(localVersionPath, sizeof(localVersionPath), "%s\\version.remote.json", workingDir);

    LogMessage("Checking for updates...", TRUE);
    HRESULT hr = DownloadToFile(UPDATE_VERSION_URL, localVersionPath);
    if (FAILED(hr) || !FileExistsA2(localVersionPath)) {
        LogMessage("Update check failed (could not download version.json).", FALSE);
        if (!silent) MessageBoxA(hMainWindow, "Could not download version.json.\n\nCheck your internet or GitHub release assets.", "Update", MB_OK | MB_ICONWARNING);
        SetButtonsEnabled(TRUE);
        return 0;
    }

    FILE* f = fopen(localVersionPath, "rb");
    if (!f) {
        LogMessage("Update check failed (could not read version.json).", FALSE);
        SetButtonsEnabled(TRUE);
        return 0;
    }
    char json[16384];
    size_t n = fread(json, 1, sizeof(json) - 1, f);
    fclose(f);
    json[n] = 0;

    char remoteVer[64];
    char remoteNotes[12000];
    JsonExtractString(json, "version", remoteVer, sizeof(remoteVer));
    JsonExtractString(json, "notes", remoteNotes, sizeof(remoteNotes));

    if (!remoteVer[0]) {
        LogMessage("Update check failed (missing version field).", FALSE);
        SetButtonsEnabled(TRUE);
        return 0;
    }

    char info[256];
    snprintf(info, sizeof(info), "Current: %s | Latest: %s", APP_VERSION, remoteVer);
    LogMessage(info, TRUE);

    if (CompareVersions(remoteVer, APP_VERSION) <= 0) {
        if (!silent) {
            LogMessage("You are already on the latest version.", TRUE);
            MessageBoxA(hMainWindow, "You are already on the latest version.", "Update", MB_OK | MB_ICONINFORMATION);
        }
        SetButtonsEnabled(TRUE);
        return 0;
    }

    if (!silent) {
        if (MessageBoxA(hMainWindow, "New update available. Update now?\n\nThe app will relaunch after updating.",
            "Update Available", MB_YESNO | MB_ICONQUESTION) != IDYES) {
            SetButtonsEnabled(TRUE);
            return 0;
        }
    } else {
        // silent check on launch: prompt, but don't spam if user cancels
        if (MessageBoxA(hMainWindow, "New update available. Update now?\n\nThe app will relaunch after updating.",
            "Update Available", MB_YESNO | MB_ICONQUESTION) != IDYES) {
            SetButtonsEnabled(TRUE);
            return 0;
        }
    }

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    char newExePath[MAX_PATH];
    snprintf(newExePath, sizeof(newExePath), "%s.new", exePath);

    LogMessage("Downloading new version...", TRUE);
    hr = DownloadToFile(UPDATE_EXE_URL, newExePath);
    if (FAILED(hr) || !FileExistsA2(newExePath)) {
        LogMessage("Update failed (could not download new EXE).", FALSE);
        MessageBoxA(hMainWindow, "Failed to download the new EXE.", "Update", MB_OK | MB_ICONERROR);
        SetButtonsEnabled(TRUE);
        return 0;
    }

    // Write pending release notes file (new EXE will display it)
    char notesPath[MAX_PATH];
    snprintf(notesPath, sizeof(notesPath), "%s\\pending_update.txt", workingDir);
    FILE* nf = fopen(notesPath, "wb");
    if (nf) {
        fprintf(nf, "Updated to v%s\r\n\r\n", remoteVer);
        if (remoteNotes[0]) {
            // convert \n to CRLF in display text
            for (char* p = remoteNotes; *p; p++) {
                if (*p == '\n') fputs("\r\n", nf);
                else fputc(*p, nf);
            }
            fputs("\r\n", nf);
        } else {
            fputs("(No release notes provided in version.json)", nf);
        }
        fclose(nf);
    }

    // Create updater.bat next to exe
    char dir[MAX_PATH];
    strncpy(dir, exePath, sizeof(dir));
    char* last = strrchr(dir, '\\');
    if (!last) {
        SetButtonsEnabled(TRUE);
        return 0;
    }
    *last = 0;

    char batPath[MAX_PATH];
    snprintf(batPath, sizeof(batPath), "%s\\updater.bat", dir);

    FILE* ub = fopen(batPath, "wb");
    if (!ub) {
        LogMessage("Update failed (could not write updater.bat).", FALSE);
        SetButtonsEnabled(TRUE);
        return 0;
    }

    // Wait for process to exit, then replace exe and relaunch
    fprintf(ub,
        "@echo off\r\n"
        "timeout /t 1 /nobreak >nul\r\n"
        ":wait\r\n"
        "tasklist | find /i \"SHYX_TWEAKS_PREMIUM.exe\" >nul\r\n"
        "if not errorlevel 1 (timeout /t 1 /nobreak >nul & goto wait)\r\n"
        "del /f /q \"%s\" >nul 2>&1\r\n"
        "move /y \"%s\" \"%s\" >nul\r\n"
        "start \"\" \"%s\"\r\n"
        "del /f /q \"%s\" >nul 2>&1\r\n",
        exePath, newExePath, exePath, exePath, batPath
    );
    fclose(ub);

    LogMessage("Update staged. Restarting...", TRUE);
    MessageBoxA(hMainWindow, "Update downloaded.\nClick OK to restart and install.", "Update", MB_OK | MB_ICONINFORMATION);

    ShellExecuteA(NULL, "open", batPath, NULL, NULL, SW_HIDE);
    PostMessageA(hMainWindow, WM_CLOSE, 0, 0);

    SetButtonsEnabled(TRUE);
    return 0;
}

// -------------------- UI events --------------------
static void OnApplyButton(void) {
    if (operationInProgress) return;
    SyncSelectionsFromUI();
    CreateThread(NULL, 0, ApplyTweaksThread, NULL, 0, NULL);
}

static void OnRevertButton(void) {
    if (operationInProgress) return;
    SyncSelectionsFromUI();
    CreateThread(NULL, 0, RevertTweaksThread, NULL, 0, NULL);
}

static void OnUpdateButton(BOOL silent) {
    if (operationInProgress) return;
    CreateThread(NULL, 0, UpdateThread, (LPVOID)(silent ? 1 : 0), 0, NULL);
}

// -------------------- Window proc --------------------
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            InitializeTweaks(); // IMPORTANT: before creating checkboxes

            hFont = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            HWND hHeader = CreateWindowA("STATIC", "SHYX TWEAKS PREMIUM",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 10, 680, 32, hwnd, NULL, NULL, NULL);
            SendMessageA(hHeader, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hSubtitle = CreateWindowA("STATIC", "Competitive Gaming Optimization (nothing auto-applies)",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 46, 680, 20, hwnd, NULL, NULL, NULL);
            SendMessageA(hSubtitle, WM_SETFONT, (WPARAM)hFont, TRUE);

            int y = 85;
            for (int i = 0; i < tweakCount; i++) {
                CreateWindowA("BUTTON", tweaks[i].name,
                    WS_CHILD | WS_VISIBLE | BS_CHECKBOX,
                    20, y, 300, 22, hwnd, (HMENU)(ID_CHECKBOX_START + i), NULL, NULL);

                CreateWindowA("STATIC", tweaks[i].description,
                    WS_CHILD | WS_VISIBLE,
                    330, y, 350, 22, hwnd, NULL, NULL, NULL);

                y += 26;
            }

            CreateWindowA("BUTTON", "Apply Selected",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                20, 420, 160, 36, hwnd, (HMENU)ID_BUTTON_APPLY, NULL, NULL);

            CreateWindowA("BUTTON", "Revert Selected",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                190, 420, 160, 36, hwnd, (HMENU)ID_BUTTON_REVERT, NULL, NULL);

            CreateWindowA("BUTTON", "Update",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                520, 420, 160, 36, hwnd, (HMENU)ID_BUTTON_UPDATE, NULL, NULL);

            hLog = CreateWindowA("EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | ES_MULTILINE | ES_READONLY,
                20, 470, 660, 250, hwnd, (HMENU)ID_LOG, NULL, NULL);
            SendMessageA(hLog, WM_SETFONT, (WPARAM)hFont, TRUE);

            CreateWindowA("STATIC", "© SHYX TWEAKS",
                WS_CHILD | WS_VISIBLE,
                20, 730, 200, 18, hwnd, NULL, NULL, NULL);

            // show notes if app was just updated
            ShowPendingReleaseNotesIfAny();

            // auto-check on launch (prompt if update is available)
            OnUpdateButton(TRUE);

            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_BUTTON_APPLY) {
                OnApplyButton();
            } else if (id == ID_BUTTON_REVERT) {
                OnRevertButton();
            } else if (id == ID_BUTTON_UPDATE) {
                OnUpdateButton(FALSE);
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(255, 255, 255));
            SetBkColor(hdcStatic, RGB(10, 10, 10));
            return (LRESULT)hDarkBrush;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, RGB(120, 255, 190));
            SetBkColor(hdcEdit, RGB(20, 20, 24));
            static HBRUSH hEditBrush = NULL;
            if (!hEditBrush) hEditBrush = CreateSolidBrush(RGB(20, 20, 24));
            return (LRESULT)hEditBrush;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (hFont) DeleteObject(hFont);
            if (hDarkBrush) DeleteObject(hDarkBrush);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// -------------------- main --------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine;

    // Check admin rights
    BOOL isAdmin = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, size, &size)) {
            isAdmin = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }

    if (!isAdmin) {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        ShellExecuteA(NULL, "runas", path, NULL, NULL, SW_SHOWNORMAL);
        return 0;
    }

    EnsureWorkingDir();

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "SHYX_TWEAKS_CLASS";
    wc.hbrBackground = CreateSolidBrush(RGB(10, 10, 10));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassA(&wc);

    hMainWindow = CreateWindowExA(
        0,
        "SHYX_TWEAKS_CLASS",
        "SHYX TWEAKS PREMIUM",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 800,
        NULL, NULL, hInstance, NULL
    );

    if (!hMainWindow) return 1;

    RECT rc;
    GetWindowRect(hMainWindow, &rc);
    int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
    SetWindowPos(hMainWindow, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    hDarkBrush = CreateSolidBrush(RGB(10, 10, 10));

    ShowWindow(hMainWindow, nCmdShow);
    UpdateWindow(hMainWindow);

    LogMessage("SHYX TWEAKS PREMIUM", TRUE);
    LogMessage("Running with administrator privileges", TRUE);
    {
        char v[128];
        snprintf(v, sizeof(v), "App version: %s", APP_VERSION);
        LogMessage(v, TRUE);
    }

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return 0;
}
