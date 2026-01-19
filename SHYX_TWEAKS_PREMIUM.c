// SHYX TWEAKS PREMIUM - Enhanced Win32 C
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

// -------------------- App Version --------------------
#define APP_VERSION "1.0.0"

// -------------------- IDs --------------------
#define ID_CAT      1001
#define ID_SEARCH   1002
#define ID_UPDATE   1003
#define ID_LIST     1004
#define ID_APPLY    1005
#define ID_REVERT   1006
#define ID_LOG      1007

// -------------------- Categories --------------------
typedef enum {
    CAT_ALL = 0,
    CAT_LATENCY,
    CAT_PRIVACY,
    CAT_GAMING,
    CAT_NETWORK,
    CAT_INPUT,
    CAT_POWER,
    CAT_GAME_SPECIFIC
} Category;

typedef void (*tweak_fn)(BOOL apply);

typedef struct {
    const char* key;
    const char* name;
    const char* desc;
    Category cat;
    BOOL selected;
    BOOL applied; // UI state only
    tweak_fn fn;
} Tweak;

// -------------------- Globals --------------------
static HWND gWnd, gCat, gSearch, gList, gLog;
static HWND gLblWin, gLblPower;
static HFONT gFontTitle, gFont, gFontSmall;
static HBRUSH gBgBrush, gPanelBrush, gEditBrush;
static BOOL gBusy = FALSE;

static Tweak gTweaks[128];
static int gTweakCount = 0;

static char gFilter[128] = {0};
static Category gActiveCat = CAT_ALL;

// -------------------- Helpers --------------------
static void RunCmd(const char* cmd) { system(cmd); }

static void LogLine(const char* msg, BOOL ok) {
    time_t now; time(&now);
    struct tm* t = localtime(&now);
    char ts[16]; sprintf(ts, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);

    char line[900];
    sprintf(line, "[%s] %s %s\r\n", ts, ok ? "[+]" : "[-]", msg);

    int len = GetWindowTextLengthA(gLog);
    SendMessageA(gLog, EM_SETSEL, len, len);
    SendMessageA(gLog, EM_REPLACESEL, 0, (LPARAM)line);
    SendMessageA(gLog, EM_SCROLLCARET, 0, 0);
}

static void SetBusy(BOOL busy) {
    gBusy = busy;
    EnableWindow(GetDlgItem(gWnd, ID_APPLY), !busy);
    EnableWindow(GetDlgItem(gWnd, ID_REVERT), !busy);
    EnableWindow(GetDlgItem(gWnd, ID_UPDATE), !busy);
    EnableWindow(gCat, !busy);
    EnableWindow(gSearch, !busy);
    EnableWindow(gList, !busy);
}

static void RegSetDwordA2(HKEY root, const char* path, const char* name, DWORD v) {
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

static int StrICmp(char a, char b) {
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    return (int)a - (int)b;
}
static int StrIContains(const char* hay, const char* needle) {
    if (!needle || !needle[0]) return 1;
    if (!hay) return 0;

    size_t hn = strlen(hay), nn = strlen(needle);
    if (nn > hn) return 0;

    for (size_t i=0;i<=hn-nn;i++) {
        size_t k=0;
        for (;k<nn;k++) {
            if (StrICmp(hay[i+k], needle[k]) != 0) break;
        }
        if (k==nn) return 1;
    }
    return 0;
}

static int PassFilter(const Tweak* t) {
    if (gActiveCat != CAT_ALL && t->cat != gActiveCat) return 0;
    if (gFilter[0]) {
        if (!StrIContains(t->name, gFilter) && !StrIContains(t->desc, gFilter)) return 0;
    }
    return 1;
}

// -------------------- Minimal Info: Windows version + power plan --------------------
static void GetWindowsVersionString(char* out, size_t cap) {
    // Use "ver" command (simple + works without extra APIs)
    FILE* fp = _popen("cmd /c ver", "r");
    if (!fp) { strncpy(out, "Windows", cap); return; }
    fgets(out, (int)cap, fp);
    _pclose(fp);

    // trim
    size_t n = strlen(out);
    while (n && (out[n-1]=='\n' || out[n-1]=='\r')) out[--n]=0;
}

static void GetActivePowerPlanString(char* out, size_t cap) {
    // Parse: "Power Scheme GUID: ... (Balanced)"
    FILE* fp = _popen("powercfg /getactivescheme", "r");
    if (!fp) { strncpy(out, "Power Plan: Unknown", cap); return; }
    char buf[512] = {0};
    fgets(buf, (int)sizeof(buf), fp);
    _pclose(fp);

    const char* p1 = strchr(buf, '(');
    const char* p2 = p1 ? strchr(p1, ')') : NULL;

    if (p1 && p2 && p2 > p1+1) {
        char name[200]={0};
        size_t len = (size_t)(p2 - (p1+1));
        if (len > sizeof(name)-1) len = sizeof(name)-1;
        memcpy(name, p1+1, len);
        name[len]=0;
        snprintf(out, cap, "Power Plan: %s", name);
    } else {
        strncpy(out, "Power Plan: Unknown", cap);
    }
}

// -------------------- Per-game IFEO priority (reversible) --------------------
static void SetIFEO_Priority(const char* exeName, BOOL apply) {
    char keyPath[512];
    sprintf(keyPath, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\%s\\PerfOptions", exeName);

    if (apply) {
        RegSetDwordA2(HKEY_LOCAL_MACHINE, keyPath, "CpuPriorityClass", 3);
        RegSetDwordA2(HKEY_LOCAL_MACHINE, keyPath, "IoPriority", 3);
        RegSetDwordA2(HKEY_LOCAL_MACHINE, keyPath, "PagePriority", 5);
    } else {
        RegDelValueA2(HKEY_LOCAL_MACHINE, keyPath, "CpuPriorityClass");
        RegDelValueA2(HKEY_LOCAL_MACHINE, keyPath, "IoPriority");
        RegDelValueA2(HKEY_LOCAL_MACHINE, keyPath, "PagePriority");
    }
}

// -------------------- Fortnite GameUserSettings (backup + apply + revert) --------------------
static int FileExistsA2(const char* p) {
    DWORD a = GetFileAttributesA(p);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
}
static int GetFortniteGUSPath(char* out, size_t cap) {
    char local[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, local))) return 0;
    snprintf(out, cap, "%s\\FortniteGame\\Saved\\Config\\WindowsClient\\GameUserSettings.ini", local);
    return 1;
}
static int FortniteBackupPath(const char* gus, char* out, size_t cap) {
    char dir[MAX_PATH];
    strncpy(dir, gus, MAX_PATH);
    char* last = strrchr(dir, '\\');
    if (!last) return 0;
    *last = 0;
    snprintf(out, cap, "%s\\GameUserSettings.ini.shyx.bak", dir);
    return 1;
}
static void T_FortniteConfig(BOOL apply) {
    char gus[MAX_PATH], bak[MAX_PATH];
    if (!GetFortniteGUSPath(gus, sizeof(gus))) return;
    if (!FileExistsA2(gus)) return;
    if (!FortniteBackupPath(gus, bak, sizeof(bak))) return;

    if (apply) {
        CopyFileA(gus, bak, FALSE);

        FILE* f = fopen(gus, "a");
        if (!f) return;

        fprintf(f, "\n; --- SHYX Competitive Overrides ---\n");
        fprintf(f, "bUseVSync=False\n");
        fprintf(f, "bMotionBlur=False\n");
        fprintf(f, "bShowGrass=False\n");
        fprintf(f, "bUseDynamicResolution=False\n");
        fprintf(f, "FrameRateLimit=0.000000\n");
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
        if (FileExistsA2(bak)) {
            CopyFileA(bak, gus, FALSE);
        }
    }
}

// -------------------- Tweaks (safe-ish + reversible) --------------------
// Power
static void T_UltimatePerf(BOOL apply) {
    if (apply) RunCmd("powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61");
    else       RunCmd("powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e");
}
static void T_HighPerf(BOOL apply) {
    if (apply) RunCmd("powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c");
    else       RunCmd("powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e");
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

// Gaming/Latency
static void T_GameMode(BOOL apply) {
    RegSetDwordA2(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\GameBar", "AllowAutoGameMode", apply ? 1 : 0);
    RegSetDwordA2(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\GameBar", "AutoGameModeEnabled", apply ? 1 : 0);
}
static void T_DisableGameBarDVR(BOOL apply) {
    RegSetDwordA2(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "AppCaptureEnabled", apply ? 0 : 1);
}
static void T_DisableFSO(BOOL apply) {
    if (apply) RegSetDwordA2(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_FSEBehaviorMode", 2);
    else       RegDelValueA2(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_FSEBehaviorMode");
}
static void T_HAGS(BOOL apply) {
    RegSetDwordA2(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", "HwSchMode", apply ? 2 : 1);
}
static void T_NetworkThrottling(BOOL apply) {
    // ffffffff disables throttling; revert deletes value
    if (apply) RegSetDwordA2(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile", "NetworkThrottlingIndex", 0xFFFFFFFF);
    else       RegDelValueA2(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile", "NetworkThrottlingIndex");
}
static void T_SystemResponsiveness(BOOL apply) {
    if (apply) RegSetDwordA2(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile", "SystemResponsiveness", 10);
    else       RegDelValueA2(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile", "SystemResponsiveness");
}

// Input
static void T_DisableMouseAccel(BOOL apply) {
    // 0 = off, 1 = on (common defaults used on revert)
    if (apply) {
        RegSetDwordA2(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseSpeed", 0);
        RegSetDwordA2(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseThreshold1", 0);
        RegSetDwordA2(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseThreshold2", 0);
    } else {
        RegSetDwordA2(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseSpeed", 1);
        RegSetDwordA2(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseThreshold1", 6);
        RegSetDwordA2(HKEY_CURRENT_USER, "Control Panel\\Mouse", "MouseThreshold2", 10);
    }
}
static void T_KeyboardFast(BOOL apply) {
    if (apply) {
        RunCmd("reg add \"HKCU\\Control Panel\\Keyboard\" /v KeyboardDelay /t REG_SZ /d 0 /f");
        RunCmd("reg add \"HKCU\\Control Panel\\Keyboard\" /v KeyboardSpeed /t REG_SZ /d 31 /f");
    } else {
        RunCmd("reg add \"HKCU\\Control Panel\\Keyboard\" /v KeyboardDelay /t REG_SZ /d 1 /f");
        RunCmd("reg add \"HKCU\\Control Panel\\Keyboard\" /v KeyboardSpeed /t REG_SZ /d 20 /f");
    }
}

// Network
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
static void T_TcpAutoTuning(BOOL apply) {
    if (apply) RunCmd("netsh int tcp set global autotuninglevel=disabled");
    else       RunCmd("netsh int tcp set global autotuninglevel=normal");
}

// Privacy
static void T_TelemetryPolicy(BOOL apply) {
    if (apply) RegSetDwordA2(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", "AllowTelemetry", 0);
    else       RegDelValueA2(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", "AllowTelemetry");
}
static void T_DeliveryOptimization(BOOL apply) {
    if (apply) RegSetDwordA2(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DeliveryOptimization", "DODownloadMode", 0);
    else       RegDelValueA2(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DeliveryOptimization", "DODownloadMode");
}
static void T_DiagTrackManual(BOOL apply) {
    if (apply) RunCmd("sc config DiagTrack start= demand");
    else       RunCmd("sc config DiagTrack start= auto");
}
static void T_WERManual(BOOL apply) {
    if (apply) RunCmd("sc config WerSvc start= demand");
    else       RunCmd("sc config WerSvc start= auto");
}
static void T_SysMainManual(BOOL apply) {
    if (apply) RunCmd("sc config SysMain start= demand");
    else       RunCmd("sc config SysMain start= auto");
}

// -------------------- Per-game toggles --------------------
static void T_FortnitePriority(BOOL apply) { SetIFEO_Priority("FortniteClient-Win64-Shipping.exe", apply); }
static void T_CS2Priority(BOOL apply)      { SetIFEO_Priority("cs2.exe", apply); }
static void T_ValorantPriority(BOOL apply) { SetIFEO_Priority("VALORANT-Win64-Shipping.exe", apply); }
static void T_ApexPriority(BOOL apply)     { SetIFEO_Priority("r5apex.exe", apply); }
static void T_WarzonePriority(BOOL apply)  { SetIFEO_Priority("cod.exe", apply); }
static void T_R6Priority(BOOL apply)       { SetIFEO_Priority("RainbowSix.exe", apply); }
static void T_OW2Priority(BOOL apply)      { SetIFEO_Priority("Overwatch.exe", apply); }
static void T_PUBGPriority(BOOL apply)     { SetIFEO_Priority("TslGame.exe", apply); }
static void T_TarkovPriority(BOOL apply)   { SetIFEO_Priority("EscapeFromTarkov.exe", apply); }
static void T_FinalsPriority(BOOL apply)   { SetIFEO_Priority("THEFINALS.exe", apply); }

// -------------------- Tweak List --------------------
static void AddT(const char* key, const char* name, const char* desc, Category cat, tweak_fn fn) {
    gTweaks[gTweakCount++] = (Tweak){ key, name, desc, cat, FALSE, FALSE, fn };
}

static void InitTweaks(void) {
    gTweakCount = 0;

    // POWER
    AddT("UltimatePerf", "Ultimate Performance", "Power plan (revert = Balanced)", CAT_POWER, T_UltimatePerf);
    AddT("HighPerf", "High Performance", "Power plan (revert = Balanced)", CAT_POWER, T_HighPerf);
    AddT("USBSelectiveSuspend", "Disable USB Selective Suspend", "Lower USB power saving latency", CAT_POWER, T_USBSelectiveSuspend);

    // LATENCY / GAMING
    AddT("GameMode", "Enable Game Mode", "Windows Game Mode on/off", CAT_LATENCY, T_GameMode);
    AddT("DisableGameBarDVR", "Disable Game Bar / DVR", "Disable overlays & recording (revert = enable)", CAT_LATENCY, T_DisableGameBarDVR);
    AddT("DisableFSO", "Disable Fullscreen Optimizations", "Can reduce interference in some games", CAT_LATENCY, T_DisableFSO);
    AddT("HAGS", "Hardware GPU Scheduling", "Enable HAGS if supported (reboot)", CAT_GAMING, T_HAGS);
    AddT("NetworkThrottling", "Disable Network Throttling", "Multimedia throttling off (revert = default)", CAT_LATENCY, T_NetworkThrottling);
    AddT("SystemResponsiveness", "System Responsiveness", "Multimedia scheduling tweak (revert = default)", CAT_LATENCY, T_SystemResponsiveness);

    // INPUT
    AddT("DisableMouseAccel", "Disable Mouse Acceleration", "Enhance pointer precision OFF", CAT_INPUT, T_DisableMouseAccel);
    AddT("KeyboardFast", "Fast Keyboard Repeat", "Keyboard speed/delay tweak", CAT_INPUT, T_KeyboardFast);

    // NETWORK
    AddT("DisableNagle", "Disable Nagle", "TCP low-latency (network-dependent)", CAT_NETWORK, T_DisableNagle);
    AddT("TcpAutoTuning", "TCP Auto-Tuning", "Disable auto-tuning (revert = normal)", CAT_NETWORK, T_TcpAutoTuning);

    // PRIVACY
    AddT("TelemetryPolicy", "Reduce Telemetry (Policy)", "AllowTelemetry policy (revert deletes)", CAT_PRIVACY, T_TelemetryPolicy);
    AddT("DeliveryOptimization", "Delivery Optimization Off", "Reduce P2P background bandwidth", CAT_PRIVACY, T_DeliveryOptimization);
    AddT("DiagTrackManual", "DiagTrack Manual", "Set DiagTrack to Manual (revert = Auto)", CAT_PRIVACY, T_DiagTrackManual);
    AddT("WERManual", "WER Manual", "Windows Error Reporting Manual (revert = Auto)", CAT_PRIVACY, T_WERManual);
    AddT("SysMainManual", "SysMain Manual", "SysMain (Superfetch) Manual (revert = Auto)", CAT_PRIVACY, T_SysMainManual);

    // GAME SPECIFIC
    AddT("FortnitePriority", "Fortnite Priority Boost", "IFEO PerfOptions High (reversible)", CAT_GAME_SPECIFIC, T_FortnitePriority);
    AddT("CS2Priority", "CS2 Priority Boost", "IFEO PerfOptions High (reversible)", CAT_GAME_SPECIFIC, T_CS2Priority);
    AddT("ValorantPriority", "Valorant Priority Boost", "IFEO PerfOptions High (reversible)", CAT_GAME_SPECIFIC, T_ValorantPriority);
    AddT("ApexPriority", "Apex Priority Boost", "IFEO PerfOptions High (reversible)", CAT_GAME_SPECIFIC, T_ApexPriority);
    AddT("WarzonePriority", "Warzone Priority Boost", "IFEO PerfOptions High (reversible)", CAT_GAME_SPECIFIC, T_WarzonePriority);
    AddT("R6Priority", "R6 Siege Priority Boost", "IFEO PerfOptions High (reversible)", CAT_GAME_SPECIFIC, T_R6Priority);
    AddT("OW2Priority", "Overwatch 2 Priority Boost", "IFEO PerfOptions High (reversible)", CAT_GAME_SPECIFIC, T_OW2Priority);
    AddT("PUBGPriority", "PUBG Priority Boost", "IFEO PerfOptions High (reversible)", CAT_GAME_SPECIFIC, T_PUBGPriority);
    AddT("TarkovPriority", "Tarkov Priority Boost", "IFEO PerfOptions High (reversible)", CAT_GAME_SPECIFIC, T_TarkovPriority);
    AddT("FinalsPriority", "The Finals Priority Boost", "IFEO PerfOptions High (reversible)", CAT_GAME_SPECIFIC, T_FinalsPriority);

    AddT("FortniteConfig", "Fortnite Competitive Config", "Backup + apply safe competitive overrides", CAT_GAME_SPECIFIC, T_FortniteConfig);
}

// -------------------- ListView --------------------
static void LV_SetColumns(void) {
    LVCOLUMNA c = {0};
    c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    c.cx = 230; c.pszText = "Tweak";        ListView_InsertColumn(gList, 0, &c);
    c.cx = 420; c.pszText = "Description";  c.iSubItem = 1; ListView_InsertColumn(gList, 1, &c);
    c.cx = 90;  c.pszText = "State";        c.iSubItem = 2; ListView_InsertColumn(gList, 2, &c);
}

static void LV_Populate(void) {
    ListView_DeleteAllItems(gList);

    int row = 0;
    for (int i=0;i<gTweakCount;i++) {
        if (!PassFilter(&gTweaks[i])) continue;

        LVITEMA it = {0};
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = row;
        it.pszText = (LPSTR)gTweaks[i].name;
        it.lParam = (LPARAM)i;

        int r = ListView_InsertItem(gList, &it);
        ListView_SetItemText(gList, r, 1, (LPSTR)gTweaks[i].desc);
        ListView_SetItemText(gList, r, 2, (LPSTR)(gTweaks[i].applied ? "Applied" : "Not Applied"));
        ListView_SetCheckState(gList, r, gTweaks[i].selected ? TRUE : FALSE);
        row++;
    }
}

static void UpdateFilterFromUI(void) {
    GetWindowTextA(gSearch, gFilter, (int)sizeof(gFilter));
    int sel = (int)SendMessageA(gCat, CB_GETCURSEL, 0, 0);
    gActiveCat = (Category)sel;
    LV_Populate();
}

// -------------------- Auto Updater (button) --------------------
static int CompareVersions(const char* a, const char* b) {
    // Simple: compare numeric segments x.y.z
    int ax=0,ay=0,az=0, bx=0,by=0,bz=0;
    sscanf(a, "%d.%d.%d", &ax,&ay,&az);
    sscanf(b, "%d.%d.%d", &bx,&by,&bz);
    if (ax!=bx) return (ax>bx)?1:-1;
    if (ay!=by) return (ay>by)?1:-1;
    if (az!=bz) return (az>bz)?1:-1;
    return 0;
}

static DWORD WINAPI UpdateThread(LPVOID p) {
    (void)p;
    SetBusy(TRUE);

    LogLine("Checking for updates...", TRUE);

    // Download version.json to ProgramData folder using PowerShell (simple & robust)
    char work[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, work);
    strcat(work, "\\SHYX_TWEAKS");
    CreateDirectoryA(work, NULL);

    char vpath[MAX_PATH];
    snprintf(vpath, sizeof(vpath), "%s\\version.remote.json", work);

    const char* url = "https://github.com/SHYX1312/SHYXPREMIUM/releases/latest/download/version.json";

    char cmd[1200];
    snprintf(cmd, sizeof(cmd),
        "powershell -NoProfile -ExecutionPolicy Bypass -Command "
        "\"try{ irm '%s' | Out-File -Encoding ascii '%s'; exit 0 } catch { exit 1 }\"",
        url, vpath);

    int rc = system(cmd);
    if (rc != 0 || !FileExistsA2(vpath)) {
        LogLine("Update check failed (could not download version.json).", FALSE);
        SetBusy(FALSE);
        return 0;
    }

    // Parse version.json very simply: search for \"version\":\"x.y.z\"
    FILE* f = fopen(vpath, "rb");
    if (!f) {
        LogLine("Update check failed (could not read version.json).", FALSE);
        SetBusy(FALSE);
        return 0;
    }
    char json[2048] = {0};
    fread(json, 1, sizeof(json)-1, f);
    fclose(f);

    char remote[32] = {0};
    char* pver = strstr(json, "\"version\"");
    if (pver) {
        char* q = strchr(pver, ':');
        if (q) {
            while (*q && (*q==' '||*q==':'||*q=='\"')) q++;
            // now points to version maybe
            int i=0;
            while (*q && *q!='\"' && i < 31) remote[i++] = *q++;
            remote[i]=0;
        }
    }

    if (!remote[0]) {
        LogLine("Update check failed (version field missing).", FALSE);
        SetBusy(FALSE);
        return 0;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Current: %s | Latest: %s", APP_VERSION, remote);
    LogLine(msg, TRUE);

    if (CompareVersions(remote, APP_VERSION) <= 0) {
        LogLine("You are already on the latest version.", TRUE);
        MessageBoxA(gWnd, "You already have the latest version.", "SHYX TWEAKS PREMIUM", MB_OK | MB_ICONINFORMATION);
        SetBusy(FALSE);
        return 0;
    }

    if (MessageBoxA(gWnd, "Update found! Download and install now?\n\nThe app will relaunch after updating.",
        "Update Available", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        SetBusy(FALSE);
        return 0;
    }

    // Download new exe as .new
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    char newPath[MAX_PATH];
    snprintf(newPath, sizeof(newPath), "%s.new", exePath);

    const char* exeUrl = "https://github.com/SHYX1312/SHYXPREMIUM/releases/latest/download/SHYX_TWEAKS_PREMIUM.exe";

    snprintf(cmd, sizeof(cmd),
        "powershell -NoProfile -ExecutionPolicy Bypass -Command "
        "\"try{ iwr '%s' -OutFile '%s' -UseBasicParsing; exit 0 } catch { exit 1 }\"",
        exeUrl, newPath);

    LogLine("Downloading new EXE...", TRUE);
    rc = system(cmd);
    if (rc != 0 || !FileExistsA2(newPath)) {
        LogLine("Update failed (could not download new EXE).", FALSE);
        SetBusy(FALSE);
        return 0;
    }

    // Write updater.bat next to exe
    char batPath[MAX_PATH];
    strncpy(batPath, exePath, MAX_PATH);
    char* last = strrchr(batPath, '\\');
    if (!last) { SetBusy(FALSE); return 0; }
    *last = 0;
    strcat(batPath, "\\updater.bat");

    FILE* ub = fopen(batPath, "wb");
    if (!ub) {
        LogLine("Update failed (could not write updater.bat).", FALSE);
        SetBusy(FALSE);
        return 0;
    }

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
        exePath, newPath, exePath, exePath, batPath
    );
    fclose(ub);

    LogLine("Update staged. Closing app to finish update...", TRUE);
    MessageBoxA(gWnd, "Update downloaded.\nClick OK to close and install the update.", "SHYX TWEAKS PREMIUM", MB_OK | MB_ICONINFORMATION);

    // Launch updater and exit
    ShellExecuteA(NULL, "open", batPath, NULL, NULL, SW_HIDE);
    PostMessageA(gWnd, WM_CLOSE, 0, 0);

    SetBusy(FALSE);
    return 0;
}

// -------------------- Threads: Apply/Revert --------------------
static DWORD WINAPI ApplyThread(LPVOID p) {
    (void)p;
    SetBusy(TRUE);

    LogLine("Creating restore point...", TRUE);
    RunCmd("powershell -NoProfile -ExecutionPolicy Bypass -Command \"Checkpoint-Computer -Description 'SHYX TWEAKS PREMIUM' -RestorePointType MODIFY_SETTINGS\"");

    for (int i=0;i<gTweakCount;i++) {
        if (!gTweaks[i].selected) continue;

        char m[260];
        sprintf(m, "Applying: %s", gTweaks[i].name);
        LogLine(m, TRUE);

        gTweaks[i].fn(TRUE);
        gTweaks[i].applied = TRUE;

        sprintf(m, "Applied: %s", gTweaks[i].name);
        LogLine(m, TRUE);
    }

    LV_Populate();
    LogLine("Apply complete.", TRUE);
    MessageBoxA(gWnd, "Selected tweaks applied.\nSome changes may require reboot.", "SHYX TWEAKS PREMIUM", MB_OK|MB_ICONINFORMATION);

    SetBusy(FALSE);
    return 0;
}

static DWORD WINAPI RevertThread(LPVOID p) {
    (void)p;
    SetBusy(TRUE);

    for (int i=0;i<gTweakCount;i++) {
        if (!gTweaks[i].selected) continue;

        char m[260];
        sprintf(m, "Reverting: %s", gTweaks[i].name);
        LogLine(m, TRUE);

        gTweaks[i].fn(FALSE);
        gTweaks[i].applied = FALSE;

        sprintf(m, "Reverted: %s", gTweaks[i].name);
        LogLine(m, TRUE);
    }

    LV_Populate();
    LogLine("Revert complete.", TRUE);
    MessageBoxA(gWnd, "Selected tweaks reverted.\nSome changes may require reboot.", "SHYX TWEAKS PREMIUM", MB_OK|MB_ICONINFORMATION);

    SetBusy(FALSE);
    return 0;
}

// -------------------- Window Proc --------------------
static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE: {
        gWnd = h;

        // Cute-dark brushes
        gBgBrush    = CreateSolidBrush(RGB(18,18,22));
        gPanelBrush = CreateSolidBrush(RGB(26,26,32));
        gEditBrush  = CreateSolidBrush(RGB(32,32,40));

        gFontTitle = CreateFontA(22,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,"Segoe UI");
        gFont      = CreateFontA(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,"Segoe UI");
        gFontSmall = CreateFontA(14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,"Segoe UI");

        InitTweaks();

        // Header
        HWND title = CreateWindowA("STATIC", "SHYX TWEAKS PREMIUM", WS_CHILD|WS_VISIBLE,
            20, 14, 480, 26, h, 0, 0, 0);
        SendMessageA(title, WM_SETFONT, (WPARAM)gFontTitle, TRUE);

        HWND cute = CreateWindowA("STATIC", "cute • clean • competitive (nothing auto-applies)", WS_CHILD|WS_VISIBLE,
            20, 42, 520, 18, h, 0, 0, 0);
        SendMessageA(cute, WM_SETFONT, (WPARAM)gFontSmall, TRUE);

        // Minimal info (only requested)
        char winv[256]={0}, pw[256]={0};
        GetWindowsVersionString(winv, sizeof(winv));
        GetActivePowerPlanString(pw, sizeof(pw));

        gLblWin = CreateWindowA("STATIC", winv, WS_CHILD|WS_VISIBLE,
            560, 14, 300, 18, h, 0, 0, 0);
        gLblPower = CreateWindowA("STATIC", pw, WS_CHILD|WS_VISIBLE,
            560, 34, 300, 18, h, 0, 0, 0);
        SendMessageA(gLblWin, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
        SendMessageA(gLblPower, WM_SETFONT, (WPARAM)gFontSmall, TRUE);

        // Update button
        CreateWindowA("BUTTON", "Update", WS_CHILD|WS_VISIBLE,
            780, 18, 80, 30, h, (HMENU)ID_UPDATE, 0, 0);

        // Category dropdown with “counts”
        gCat = CreateWindowA("COMBOBOX", "", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
            20, 72, 210, 300, h, (HMENU)ID_CAT, 0, 0);
        SendMessageA(gCat, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
        SendMessageA(gCat, CB_ADDSTRING, 0, (LPARAM)"All");
        SendMessageA(gCat, CB_ADDSTRING, 0, (LPARAM)"Latency (500)");
        SendMessageA(gCat, CB_ADDSTRING, 0, (LPARAM)"Privacy (100)");
        SendMessageA(gCat, CB_ADDSTRING, 0, (LPARAM)"Gaming (120)");
        SendMessageA(gCat, CB_ADDSTRING, 0, (LPARAM)"Network (60)");
        SendMessageA(gCat, CB_ADDSTRING, 0, (LPARAM)"Input (40)");
        SendMessageA(gCat, CB_ADDSTRING, 0, (LPARAM)"Power (30)");
        SendMessageA(gCat, CB_ADDSTRING, 0, (LPARAM)"Game Specific (90)");
        SendMessageA(gCat, CB_SETCURSEL, 0, 0);

        // Search
        gSearch = CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            240, 72, 300, 26, h, (HMENU)ID_SEARCH, 0, 0);
        SendMessageA(gSearch, WM_SETFONT, (WPARAM)gFontSmall, TRUE);

        HWND sLbl = CreateWindowA("STATIC", "Search", WS_CHILD|WS_VISIBLE,
            240, 52, 60, 16, h, 0, 0, 0);
        SendMessageA(sLbl, WM_SETFONT, (WPARAM)gFontSmall, TRUE);

        // List
        gList = CreateWindowA(WC_LISTVIEWA, "", WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT,
            20, 110, 840, 330, h, (HMENU)ID_LIST, 0, 0);
        SendMessageA(gList, WM_SETFONT, (WPARAM)gFontSmall, TRUE);
        ListView_SetExtendedListViewStyle(gList, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);

        LV_SetColumns();
        LV_Populate();

        // Buttons
        CreateWindowA("BUTTON", "Apply Selected", WS_CHILD|WS_VISIBLE,
            20, 450, 150, 34, h, (HMENU)ID_APPLY, 0, 0);
        CreateWindowA("BUTTON", "Revert Selected", WS_CHILD|WS_VISIBLE,
            178, 450, 150, 34, h, (HMENU)ID_REVERT, 0, 0);

        // Log
        gLog = CreateWindowA("EDIT", "", WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY,
            20, 492, 840, 180, h, (HMENU)ID_LOG, 0, 0);
        SendMessageA(gLog, WM_SETFONT, (WPARAM)gFontSmall, TRUE);

        LogLine("Ready. Pick a category, search, select tweaks. Nothing auto-applies.", TRUE);
        LogLine("Tip: Update button downloads latest EXE from GitHub Releases.", TRUE);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id == ID_CAT && HIWORD(w) == CBN_SELCHANGE && !gBusy) {
            UpdateFilterFromUI();
            return 0;
        }
        if (id == ID_SEARCH && HIWORD(w) == EN_CHANGE && !gBusy) {
            UpdateFilterFromUI();
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
        if (id == ID_UPDATE && !gBusy) {
            CreateThread(NULL, 0, UpdateThread, NULL, 0, NULL);
            return 0;
        }
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* hdr = (NMHDR*)l;
        if (hdr->idFrom == ID_LIST && hdr->code == LVN_ITEMCHANGED) {
            NMLISTVIEW* n = (NMLISTVIEW*)l;
            if ((n->uChanged & LVIF_STATE) && n->iItem >= 0) {
                // map visible row -> actual tweak index via lParam
                LVITEMA it = {0};
                it.mask = LVIF_PARAM;
                it.iItem = n->iItem;
                if (ListView_GetItem(gList, &it)) {
                    int idx = (int)it.lParam;
                    if (idx >= 0 && idx < gTweakCount) {
                        BOOL checked = ListView_GetCheckState(gList, n->iItem);
                        gTweaks[idx].selected = checked ? TRUE : FALSE;
                    }
                }
            }
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        SetTextColor(dc, RGB(235,235,245));
        SetBkColor(dc, RGB(18,18,22));
        return (LRESULT)gBgBrush;
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w;
        SetTextColor(dc, RGB(120,255,190));
        SetBkColor(dc, RGB(32,32,40));
        return (LRESULT)gEditBrush;
    }

    case WM_ERASEBKGND: {
        RECT r; GetClientRect(h, &r);
        FillRect((HDC)w, &r, gBgBrush);
        return 1;
    }

    case WM_DESTROY:
        if (gFontTitle) DeleteObject(gFontTitle);
        if (gFont) DeleteObject(gFont);
        if (gFontSmall) DeleteObject(gFontSmall);
        if (gBgBrush) DeleteObject(gBgBrush);
        if (gPanelBrush) DeleteObject(gPanelBrush);
        if (gEditBrush) DeleteObject(gEditBrush);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, msg, w, l);
}

// -------------------- Main --------------------
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
    wc.lpszClassName = "SHYX_PREMIUM_CUTE";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    HWND w = CreateWindowA("SHYX_PREMIUM_CUTE", "SHYX TWEAKS PREMIUM",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 730,
        NULL, NULL, hi, NULL);

    ShowWindow(w, show);
    UpdateWindow(w);

    MSG m;
    while (GetMessageA(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    return 0;
}
