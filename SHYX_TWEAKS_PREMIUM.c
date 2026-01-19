// ============================================
// SHYX TWEAKS PREMIUM - C++ GUI Application
// Compile with: g++ -o SHYX_TWEAKS_PREMIUM.exe SHYX_TWEAKS_PREMIUM.c -mwindows -lcomctl32 -lwininet
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
#pragma comment(lib, "wininet.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define ID_CHECKBOX_START 1000
#define ID_BUTTON_APPLY 2000
#define ID_BUTTON_REVERT 2001
#define ID_BUTTON_CLEANER 2002
#define ID_LISTBOX 2003
#define ID_PRESET_COMBO 2004
#define ID_BUTTON_LOAD 2005

typedef struct {
    int id;
    char name[100];
    char description[200];
    BOOL applied;
    BOOL selected;
    void (*apply_func)(BOOL);
} Tweak;

HWND hMainWindow;
HWND hLog;
HFONT hFont;
HBRUSH hDarkBrush;
HICON hIcon;
Tweak tweaks[50];
int tweakCount = 0;
BOOL operationInProgress = FALSE;
char workingDir[MAX_PATH];

// ==================== UTILITY FUNCTIONS ====================

void LogMessage(const char* message, BOOL success) {
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    char timestamp[20];
    sprintf(timestamp, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    
    char buffer[500];
    sprintf(buffer, "[%s] %s %s\r\n", timestamp, success ? "[+]" : "[-]", message);
    
    int len = GetWindowTextLength(hLog);
    SendMessage(hLog, EM_SETSEL, len, len);
    SendMessage(hLog, EM_REPLACESEL, 0, (LPARAM)buffer);
    
    SendMessage(hLog, EM_SCROLLCARET, 0, 0);
}

void SetButtonsEnabled(BOOL enabled) {
    EnableWindow(GetDlgItem(hMainWindow, ID_BUTTON_APPLY), enabled);
    EnableWindow(GetDlgItem(hMainWindow, ID_BUTTON_REVERT), enabled);
    EnableWindow(GetDlgItem(hMainWindow, ID_BUTTON_LOAD), enabled);
    operationInProgress = !enabled;
}

DWORD WINAPI ApplyTweaksThread(LPVOID param) {
    SetButtonsEnabled(FALSE);
    LogMessage("Starting tweak application...", TRUE);
    
    // Create restore point
    LogMessage("Creating system restore point...", TRUE);
    system("powershell -Command \"Checkpoint-Computer -Description 'SHYX TWEAKS PREMIUM' -RestorePointType MODIFY_SETTINGS\"");
    
    // Apply selected tweaks
    for (int i = 0; i < tweakCount; i++) {
        if (tweaks[i].selected) {
            char msg[200];
            sprintf(msg, "Applying: %s", tweaks[i].name);
            LogMessage(msg, TRUE);
            
            if (tweaks[i].apply_func) {
                tweaks[i].apply_func(TRUE);
                tweaks[i].applied = TRUE;
                
                sprintf(msg, "Applied: %s", tweaks[i].name);
                LogMessage(msg, TRUE);
                
                // Update UI
                InvalidateRect(hMainWindow, NULL, TRUE);
            }
        }
    }
    
    LogMessage("Tweak application complete!", TRUE);
    MessageBox(hMainWindow, "Selected tweaks applied successfully!\n\nSome changes may require restart.", "SHYX TWEAKS - Complete", MB_ICONINFORMATION);
    
    SetButtonsEnabled(TRUE);
    return 0;
}

// ==================== TWEAK IMPLEMENTATIONS ====================

void TweakUltimatePerf(BOOL apply) {
    if (apply) {
        system("powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61");
    } else {
        system("powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e");
    }
}

void TweakGameMode(BOOL apply) {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\GameBar", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = apply ? 1 : 0;
        RegSetValueEx(hKey, "AllowAutoGameMode", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegSetValueEx(hKey, "AutoGameModeEnabled", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

void TweakGameBar(BOOL apply) {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\GameDVR", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = apply ? 0 : 1;
        RegSetValueEx(hKey, "AppCaptureEnabled", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegSetValueEx(hKey, "AudioCaptureEnabled", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

void TweakSysMain(BOOL apply) {
    if (apply) {
        system("sc config SysMain start= disabled");
        system("sc stop SysMain");
    } else {
        system("sc config SysMain start= auto");
        system("sc start SysMain");
    }
}

void TweakTelemetry(BOOL apply) {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        if (apply) {
            DWORD value = 0;
            RegSetValueEx(hKey, "AllowTelemetry", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        } else {
            RegDeleteValue(hKey, "AllowTelemetry");
        }
        RegCloseKey(hKey);
    }
}

void TweakCPUPriority(BOOL apply) {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\PriorityControl", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        DWORD value = apply ? 38 : 2;
        RegSetValueEx(hKey, "Win32PrioritySeparation", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

void TweakHPET(BOOL apply) {
    if (apply) {
        system("bcdedit /set disabledynamictick yes");
        system("bcdedit /set useplatformclock no");
    } else {
        system("bcdedit /set disabledynamictick no");
        system("bcdedit /set useplatformclock yes");
    }
}

void TweakUSBSuspend(BOOL apply) {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Power", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        DWORD value = apply ? 0 : 1;
        RegSetValueEx(hKey, "HibernateEnabled", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

void TweakGPUScheduling(BOOL apply) {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        DWORD value = apply ? 2 : 1;
        RegSetValueEx(hKey, "HwSchMode", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

void TweakSearchIndex(BOOL apply) {
    if (apply) {
        system("sc config WSearch start= disabled");
        system("sc stop WSearch");
    } else {
        system("sc config WSearch start= auto");
        system("sc start WSearch");
    }
}

// ==================== UI FUNCTIONS ====================

void InitializeTweaks() {
    // Power & Performance
    tweaks[tweakCount++] = (Tweak){0, "Ultimate Performance", "Enable Ultimate Performance power plan", FALSE, FALSE, TweakUltimatePerf};
    tweaks[tweakCount++] = (Tweak){1, "CPU Priority", "Optimize CPU priority distribution", FALSE, FALSE, TweakCPUPriority};
    tweaks[tweakCount++] = (Tweak){2, "HPET Disable", "Disable High Precision Event Timer", FALSE, FALSE, TweakHPET};
    
    // Input & USB
    tweaks[tweakCount++] = (Tweak){3, "USB Selective Suspend", "Disable USB selective suspend", FALSE, FALSE, TweakUSBSuspend};
    
    // Graphics & Gaming
    tweaks[tweakCount++] = (Tweak){4, "Game Mode", "Enable Windows Game Mode", FALSE, FALSE, TweakGameMode};
    tweaks[tweakCount++] = (Tweak){5, "Game Bar & DVR", "Disable Game Bar and recording", FALSE, FALSE, TweakGameBar};
    tweaks[tweakCount++] = (Tweak){6, "GPU Scheduling", "Enable hardware GPU scheduling", FALSE, FALSE, TweakGPUScheduling};
    
    // Background Services
    tweaks[tweakCount++] = (Tweak){7, "SysMain Disable", "Disable SysMain (SuperFetch)", FALSE, FALSE, TweakSysMain};
    tweaks[tweakCount++] = (Tweak){8, "Search Indexing", "Disable Windows Search indexing", FALSE, FALSE, TweakSearchIndex};
    
    // Privacy & Telemetry
    tweaks[tweakCount++] = (Tweak){9, "Telemetry Reduction", "Reduce diagnostic data collection", FALSE, FALSE, TweakTelemetry};
}

void DrawTweakStatus(HDC hdc, int x, int y, BOOL applied) {
    HBRUSH hBrush = CreateSolidBrush(applied ? RGB(0, 255, 0) : RGB(255, 0, 0));
    HPEN hPen = CreatePen(PS_SOLID, 1, applied ? RGB(0, 200, 0) : RGB(200, 0, 0));
    
    HBRUSH hOldBrush = SelectBrush(hdc, hBrush);
    HPEN hOldPen = SelectPen(hdc, hPen);
    
    Ellipse(hdc, x, y, x + 12, y + 12);
    
    SelectBrush(hdc, hOldBrush);
    SelectPen(hdc, hOldPen);
    
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

void OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    
    // Draw tweak status indicators
    HFONT hOldFont = SelectFont(hdc, hFont);
    SetBkColor(hdc, RGB(10, 10, 10));
    SetTextColor(hdc, RGB(255, 255, 255));
    
    int y = 180;
    for (int i = 0; i < tweakCount; i++) {
        char status[20];
        strcpy(status, tweaks[i].applied ? "Applied" : "Not Applied");
        
        TextOut(hdc, 620, y, status, strlen(status));
        DrawTweakStatus(hdc, 600, y, tweaks[i].applied);
        
        y += 30;
    }
    
    SelectFont(hdc, hOldFont);
    EndPaint(hwnd, &ps);
}

void OnApplyButton() {
    if (operationInProgress) return;
    
    int selectedCount = 0;
    for (int i = 0; i < tweakCount; i++) {
        if (tweaks[i].selected) selectedCount++;
    }
    
    if (selectedCount == 0) {
        MessageBox(hMainWindow, "Please select at least one tweak to apply.", "SHYX TWEAKS", MB_ICONINFORMATION);
        return;
    }
    
    if (MessageBox(hMainWindow, 
        "Apply selected tweaks?\n\nA system restore point will be created first.\nThis may take a few minutes.",
        "Confirm Application", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        
        CreateThread(NULL, 0, ApplyTweaksThread, NULL, 0, NULL);
    }
}

void OnCheckboxClick(int id) {
    int tweakIndex = id - ID_CHECKBOX_START;
    if (tweakIndex >= 0 && tweakIndex < tweakCount) {
        tweaks[tweakIndex].selected = !tweaks[tweakIndex].selected;
    }
}

// ==================== WINDOW PROCEDURE ====================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Create font
            hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            
            // Header
            HWND hHeader = CreateWindow("STATIC", "SHYX TWEAKS PREMIUM",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 10, 680, 40, hwnd, NULL, NULL, NULL);
            SendMessage(hHeader, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            HWND hSubtitle = CreateWindow("STATIC", "Competitive Gaming Optimization",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 50, 680, 25, hwnd, NULL, NULL, NULL);
            
            // Tweaks checkboxes
            int y = 100;
            for (int i = 0; i < tweakCount; i++) {
                HWND hCheck = CreateWindow("BUTTON", tweaks[i].name,
                    WS_CHILD | WS_VISIBLE | BS_CHECKBOX,
                    20, y, 300, 25, hwnd, (HMENU)(ID_CHECKBOX_START + i), NULL, NULL);
                y += 30;
                
                // Description label
                char desc[250];
                sprintf(desc, "%s", tweaks[i].description);
                CreateWindow("STATIC", desc,
                    WS_CHILD | WS_VISIBLE,
                    330, y - 25, 250, 25, hwnd, NULL, NULL, NULL);
            }
            
            // Buttons
            CreateWindow("BUTTON", "APPLY SELECTED TWEAKS",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                20, 450, 200, 40, hwnd, (HMENU)ID_BUTTON_APPLY, NULL, NULL);
            
            CreateWindow("BUTTON", "REVERT SELECTED TWEAKS",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                240, 450, 200, 40, hwnd, (HMENU)ID_BUTTON_REVERT, NULL, NULL);
            
            // Log area
            hLog = CreateWindow("EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | ES_MULTILINE | ES_READONLY,
                20, 500, 660, 200, hwnd, (HMENU)ID_LISTBOX, NULL, NULL);
            
            // Footer
            CreateWindow("STATIC", "© SHYX TWEAKS",
                WS_CHILD | WS_VISIBLE,
                20, 710, 200, 20, hwnd, NULL, NULL, NULL);
            
            InitializeTweaks();
            return 0;
        }
        
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id >= ID_CHECKBOX_START && id < ID_CHECKBOX_START + tweakCount) {
                OnCheckboxClick(id);
            } else if (id == ID_BUTTON_APPLY) {
                OnApplyButton();
            } else if (id == ID_BUTTON_REVERT) {
                LogMessage("Revert feature - Coming in next update", TRUE);
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
            SetTextColor(hdcEdit, RGB(0, 255, 0));
            SetBkColor(hdcEdit, RGB(30, 30, 30));
            return (LRESULT)CreateSolidBrush(RGB(30, 30, 30));
        }
        
        case WM_CTLCOLORBTN: {
            HDC hdcButton = (HDC)wParam;
            SetTextColor(hdcButton, RGB(255, 255, 255));
            SetBkColor(hdcButton, RGB(0, 120, 212));
            return (LRESULT)CreateSolidBrush(RGB(0, 120, 212));
        }
        
        case WM_PAINT:
            OnPaint(hwnd);
            return 0;
            
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            DeleteObject(hFont);
            DeleteObject(hDarkBrush);
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ==================== MAIN ENTRY POINT ====================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
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
        // Restart as admin
        char path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);
        
        ShellExecute(NULL, "runas", path, NULL, NULL, SW_SHOWNORMAL);
        return 0;
    }
    
    // Register window class
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "SHYX_TWEAKS_CLASS";
    wc.hbrBackground = CreateSolidBrush(RGB(10, 10, 10));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClass(&wc);
    
    // Create window
    hMainWindow = CreateWindowEx(
        0,
        "SHYX_TWEAKS_CLASS",
        "SHYX TWEAKS PREMIUM",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 800,
        NULL, NULL, hInstance, NULL
    );
    
    if (!hMainWindow) return 1;
    
    // Center window
    RECT rc;
    GetWindowRect(hMainWindow, &rc);
    int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
    SetWindowPos(hMainWindow, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    // Create dark brush for background
    hDarkBrush = CreateSolidBrush(RGB(10, 10, 10));
    
    ShowWindow(hMainWindow, nCmdShow);
    UpdateWindow(hMainWindow);
    
    // Welcome message
    LogMessage("SHYX TWEAKS PREMIUM v2.2.0", TRUE);
    LogMessage("Running with administrator privileges", TRUE);
    LogMessage("Select tweaks and click APPLY to optimize", TRUE);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}