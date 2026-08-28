#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dwmapi.h>
#include <wininet.h>
#include <shellapi.h>
#include "qss_template.h"
#include "version_config.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "wininet.lib")

#define PLUGINS_EXPORTDLL __declspec(dllexport)
#define PLUGIN_API_VERSION 26
#define PLUGIN_OFFERS_CONFIGURE_STANDARD 1
#define PLUGIN_MENU_BUFSZ 128

// DWM Dark TitleBar Attributes
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

enum PluginMenuType {
    PLUGIN_MENU_TYPE_GLOBAL = 0,
    PLUGIN_MENU_TYPE_CHANNEL,
    PLUGIN_MENU_TYPE_CLIENT
};

struct PluginMenuItem {
    enum PluginMenuType type;
    int id;
    char text[PLUGIN_MENU_BUFSZ];
    char icon[PLUGIN_MENU_BUFSZ];
};

struct AccentPalette {
    const wchar_t* name;        // e.g. L"Sapphire Blue"
    const wchar_t* tag;         // e.g. L"Classic Cyber Blue"
    const char* primary;        // e.g. "#2563EB"
    const char* accent;         // e.g. "#3B82F6"
    const char* glow;           // e.g. "#60A5FA"
    COLORREF colorRef;          // e.g. RGB(37, 99, 235)
};

static const AccentPalette g_palettes[] = {
    { L"Sapphire Blue",      L"Classic Cyber Blue",        "#2563EB", "#3B82F6", "#60A5FA", RGB(37, 99, 235) },
    { L"Cyberpunk Violet",   L"Synthwave Purple",         "#9333EA", "#A855F7", "#C084FC", RGB(147, 51, 234) },
    { L"Emerald Mint",       L"Hyperion Matrix Green",    "#059669", "#10B981", "#34D399", RGB(5, 150, 105) },
    { L"Inferno Red",        L"Crimson Blood Orange",     "#DC2626", "#EF4444", "#F87171", RGB(220, 38, 38) },
    { L"Solar Gold",         L"Luxury Warm Amber",        "#D97706", "#F59E0B", "#FBBF24", RGB(217, 119, 6) },
    { L"Sakura Rose",        L"Vaporwave Neon Rose",      "#E11D48", "#F43F5E", "#FB7185", RGB(225, 29, 72) },
    { L"Aurora Cyan",        L"Glacier Ice Blue",         "#0891B2", "#06B6D4", "#22D3EE", RGB(8, 145, 178) }
};

#define PALETTE_COUNT (sizeof(g_palettes) / sizeof(g_palettes[0]))

static int g_selectedPaletteIndex = 0;
static BOOL g_isThemeEnabled = TRUE;
static HINSTANCE g_hInst = NULL;
static HWND g_hConfigWnd = NULL;
static char* g_pluginID = NULL;
static wchar_t g_statusText[128] = L"";
static volatile BOOL g_isManualUpdateCheck = FALSE;

// Dark TitleBar subsystem globals
static HWINEVENTHOOK g_hook = NULL;
static HANDLE g_monitorThread = NULL;
static volatile BOOL g_titlebarRunning = FALSE;

#define IDC_ENABLE_CHECKBOX    1001
#define IDC_ACCENT_COMBO       1002
#define IDC_OK_BTN             1003
#define IDC_CANCEL_BTN         1004
#define IDC_APPLY_BTN          1005

// Custom Subclass for ComboBox to render 100% Dark Theme (no white arrow)
static WNDPROC s_origComboProc = NULL;
static HFONT s_hComboFont = NULL;

static LRESULT CALLBACK DarkComboSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Dark Background (matching TeamSpeak 3 ComboBox #0F1117)
        HBRUSH hBg = CreateSolidBrush(RGB(15, 17, 23));
        FillRect(hdc, &rc, hBg);
        DeleteObject(hBg);

        // Dark Border (#1E222D)
        HPEN hBorder = CreatePen(PS_SOLID, 1, RGB(30, 34, 45));
        HPEN oldPen = (HPEN)SelectObject(hdc, hBorder);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 4, 4);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hBorder);

        int curSel = (int)SendMessage(hwnd, CB_GETCURSEL, 0, 0);
        if (curSel >= 0 && curSel < (int)PALETTE_COUNT) {
            const AccentPalette& pal = g_palettes[curSel];

            // Swatch Pill
            int pillLeft = rc.left + 8;
            int pillTop = rc.top + 5;
            int pillRight = pillLeft + 16;
            int pillBottom = rc.bottom - 5;

            HBRUSH hSw = CreateSolidBrush(pal.colorRef);
            HPEN hSwBorder = CreatePen(PS_SOLID, 1, RGB(45, 55, 75));
            HPEN pOld = (HPEN)SelectObject(hdc, hSwBorder);
            HBRUSH bOld = (HBRUSH)SelectObject(hdc, hSw);
            RoundRect(hdc, pillLeft, pillTop, pillRight, pillBottom, 3, 3);
            SelectObject(hdc, pOld);
            SelectObject(hdc, bOld);
            DeleteObject(hSwBorder);
            DeleteObject(hSw);

            // Palette Name
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            if (s_hComboFont) SelectObject(hdc, s_hComboFont);
            RECT rName = { pillRight + 8, rc.top, rc.right - 30, rc.bottom };
            DrawTextW(hdc, pal.name, -1, &rName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // Dropdown Arrow (Chevron ▼) on dark background
        SetTextColor(hdc, RGB(160, 170, 185));
        if (s_hComboFont) SelectObject(hdc, s_hComboFont);
        RECT rArrow = { rc.right - 22, rc.top, rc.right - 6, rc.bottom };
        DrawTextW(hdc, L"▼", -1, &rArrow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }
    return CallWindowProc(s_origComboProc, hwnd, msg, wParam, lParam);
}

// ==========================================
// Dark TitleBar Subsystem
// ==========================================

static void ApplyDarkTitleBar(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId()) return;

    // 1. Enable Windows 11 Immersive Dark Mode
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(hwnd, 19, &darkMode, sizeof(darkMode));

    // 2. Custom Titlebar Background Color: #060709 (RGB: 6, 7, 9)
    COLORREF captionColor = RGB(6, 7, 9);
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));

    // 3. Custom Titlebar Text Color: #FFFFFF (White)
    COLORREF textColor = RGB(255, 255, 255);
    DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor));

    // 4. Custom Window Border Color: #141720 (RGB: 20, 23, 32)
    COLORREF borderColor = RGB(20, 23, 32);
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
}

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    (void)lParam;
    ApplyDarkTitleBar(hwnd);
    return TRUE;
}

static void CALLBACK WinEventProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime
) {
    (void)hWinEventHook;
    (void)event;
    (void)idObject;
    (void)idChild;
    (void)idEventThread;
    (void)dwmsEventTime;

    if (hwnd && idObject == OBJID_WINDOW) {
        ApplyDarkTitleBar(hwnd);
    }
}

static DWORD WINAPI TitleBarMonitorThread(LPVOID lpParam) {
    (void)lpParam;
    while (g_titlebarRunning) {
        EnumWindows(EnumWindowsCallback, 0);
        Sleep(400);
    }
    return 0;
}

static void StartDarkTitleBarEngine() {
    g_titlebarRunning = TRUE;
    EnumWindows(EnumWindowsCallback, 0);

    g_hook = SetWinEventHook(
        EVENT_OBJECT_SHOW,
        EVENT_OBJECT_SHOW,
        NULL,
        WinEventProc,
        GetCurrentProcessId(),
        0,
        WINEVENT_OUTOFCONTEXT
    );

    g_monitorThread = CreateThread(NULL, 0, TitleBarMonitorThread, NULL, 0, NULL);
}

static void StopDarkTitleBarEngine() {
    g_titlebarRunning = FALSE;

    if (g_hook) {
        UnhookWinEvent(g_hook);
        g_hook = NULL;
    }

    if (g_monitorThread) {
        WaitForSingleObject(g_monitorThread, 1000);
        CloseHandle(g_monitorThread);
        g_monitorThread = NULL;
    }
}

// ==========================================
// Auto-Update Subsystem
// ==========================================

enum UpdateStatus {
    UPDATE_STATUS_IDLE = 0,
    UPDATE_STATUS_CHECKING,
    UPDATE_STATUS_UP_TO_DATE,
    UPDATE_STATUS_AVAILABLE,
    UPDATE_STATUS_DOWNLOADING,
    UPDATE_STATUS_DOWNLOADED,
    UPDATE_STATUS_ERROR
};

struct UpdateState {
    UpdateStatus status;
    char currentVersion[32];
    char latestVersion[32];
    char downloadUrl[512];
    char changelog[1024];
    char message[256];
};

static UpdateState g_updateState = { UPDATE_STATUS_IDLE, PLUGIN_VERSION_STR, "", "", "", "Ready to check for updates." };
static HANDLE g_hUpdateThread = NULL;

static void ShowConfigWindow();
static void StartDownloadAndInstallUpdate(HWND hwndNotify);

static int CompareSemVer(const char* v1, const char* v2) {
    if (!v1 || !v2) return 0;
    while (*v1 == 'v' || *v1 == 'V' || *v1 == ' ') v1++;
    while (*v2 == 'v' || *v2 == 'V' || *v2 == ' ') v2++;

    int maj1 = 0, min1 = 0, pat1 = 0;
    int maj2 = 0, min2 = 0, pat2 = 0;

    sscanf_s(v1, "%d.%d.%d", &maj1, &min1, &pat1);
    sscanf_s(v2, "%d.%d.%d", &maj2, &min2, &pat2);

    if (maj1 != maj2) return (maj1 > maj2) ? 1 : -1;
    if (min1 != min2) return (min1 > min2) ? 1 : -1;
    if (pat1 != pat2) return (pat1 > pat2) ? 1 : -1;
    return 0;
}

static bool JsonExtractString(const char* json, const char* key, char* outVal, size_t maxLen) {
    if (!json || !key || !outVal || maxLen == 0) return false;
    outVal[0] = '\0';

    char searchKey[128];
    snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);
    const char* keyPos = strstr(json, searchKey);
    if (!keyPos) return false;

    const char* colon = strchr(keyPos + strlen(searchKey), ':');
    if (!colon) return false;

    const char* p = colon + 1;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != '\"') return false;
    p++;

    size_t i = 0;
    while (*p && *p != '\"' && i < maxLen - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            if (*p == 'n') outVal[i++] = '\n';
            else if (*p == 'r') outVal[i++] = '\r';
            else if (*p == 't') outVal[i++] = '\t';
            else outVal[i++] = *p;
        } else {
            outVal[i++] = *p;
        }
        p++;
    }
    outVal[i] = '\0';
    return (i > 0);
}

static DWORD WINAPI CheckUpdateThreadProc(LPVOID lpParam) {
    HWND hNotifyWnd = (HWND)lpParam;
    HWND hParent = hNotifyWnd ? hNotifyWnd : GetForegroundWindow();
    g_updateState.status = UPDATE_STATUS_CHECKING;
    snprintf(g_updateState.message, sizeof(g_updateState.message), "Checking for updates...");

    const char* url = UPDATE_CHECK_URL;
    if (!url || strlen(url) == 0) {
        g_updateState.status = UPDATE_STATUS_ERROR;
        snprintf(g_updateState.message, sizeof(g_updateState.message), "Update URL not configured.");
        MessageBoxW(hParent,
            L"Update server URL is not configured.",
            L"BlackSpeak - Check for Update",
            MB_OK | MB_ICONWARNING | MB_TOPMOST);
        g_isManualUpdateCheck = FALSE;
        return 0;
    }

    HINTERNET hInternet = InternetOpenA("BlackSpeakUpdater/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        g_updateState.status = UPDATE_STATUS_ERROR;
        snprintf(g_updateState.message, sizeof(g_updateState.message), "Failed to initialize network.");
        MessageBoxW(hParent,
            L"Failed to initialize network connection.",
            L"BlackSpeak - Check for Update",
            MB_OK | MB_ICONWARNING | MB_TOPMOST);
        g_isManualUpdateCheck = FALSE;
        return 0;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE;
    if (strncmp(url, "https://", 8) == 0) {
        flags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
    }

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url, NULL, 0, flags, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        g_updateState.status = UPDATE_STATUS_ERROR;
        snprintf(g_updateState.message, sizeof(g_updateState.message), "Unable to connect to update server.");
        MessageBoxW(hParent,
            L"Unable to connect to the update server.\nPlease check your internet connection and try again.",
            L"BlackSpeak - Check for Update",
            MB_OK | MB_ICONWARNING | MB_TOPMOST);
        g_isManualUpdateCheck = FALSE;
        return 0;
    }

    char buffer[4096];
    DWORD bytesRead = 0;
    char response[8192] = {0};
    size_t totalBytes = 0;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        if (totalBytes + bytesRead < sizeof(response) - 1) {
            memcpy(response + totalBytes, buffer, bytesRead);
            totalBytes += bytesRead;
            response[totalBytes] = '\0';
        }
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    if (totalBytes == 0) {
        g_updateState.status = UPDATE_STATUS_ERROR;
        snprintf(g_updateState.message, sizeof(g_updateState.message), "Empty response from update server.");
        MessageBoxW(hParent,
            L"Received empty response from update server.",
            L"BlackSpeak - Check for Update",
            MB_OK | MB_ICONWARNING | MB_TOPMOST);
        g_isManualUpdateCheck = FALSE;
        return 0;
    }

    char serverVer[32] = {0};
    char dlUrl[512] = {0};
    char changelog[1024] = {0};

    if (!JsonExtractString(response, "version", serverVer, sizeof(serverVer))) {
        g_updateState.status = UPDATE_STATUS_ERROR;
        snprintf(g_updateState.message, sizeof(g_updateState.message), "Invalid response format.");
        MessageBoxW(hParent,
            L"Failed to parse server update metadata.",
            L"BlackSpeak - Check for Update",
            MB_OK | MB_ICONWARNING | MB_TOPMOST);
        g_isManualUpdateCheck = FALSE;
        return 0;
    }

    JsonExtractString(response, "download_url", dlUrl, sizeof(dlUrl));
    JsonExtractString(response, "changelog", changelog, sizeof(changelog));

    strcpy_s(g_updateState.latestVersion, sizeof(g_updateState.latestVersion), serverVer);
    strcpy_s(g_updateState.downloadUrl, sizeof(g_updateState.downloadUrl), dlUrl);
    strcpy_s(g_updateState.changelog, sizeof(g_updateState.changelog), changelog);

    int cmp = CompareSemVer(serverVer, PLUGIN_VERSION_STR);
    if (cmp > 0) {
        g_updateState.status = UPDATE_STATUS_AVAILABLE;
        snprintf(g_updateState.message, sizeof(g_updateState.message), "Update v%s is available!", serverVer);

        // Show update available popup with Download / Later choices
        wchar_t msgBoxText[1024];
        swprintf_s(msgBoxText,
            L"A new version of BlackSpeak is available!\n\n"
            L"• Installed Version: v%S\n"
            L"• Latest Version:    v%S\n\n"
            L"Changelog:\n%S\n\n"
            L"Click 'Yes' to Download & Update now, or 'No' to update Later.",
            PLUGIN_VERSION_STR,
            serverVer,
            changelog[0] ? changelog : "Performance optimizations and styling improvements.");

        int userChoice = MessageBoxW(hParent,
            msgBoxText,
            L"BlackSpeak - Update Available",
            MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST | MB_DEFBUTTON1);

        if (userChoice == IDYES) {
            StartDownloadAndInstallUpdate(hNotifyWnd);
        }
    } else {
        g_updateState.status = UPDATE_STATUS_UP_TO_DATE;
        snprintf(g_updateState.message, sizeof(g_updateState.message), "BlackSpeak is up to date (v%s).", PLUGIN_VERSION_STR);

        wchar_t msgBoxText[512];
        swprintf_s(msgBoxText,
            L"BlackSpeak is up to date!\n\n"
            L"You are currently running the latest version (v%S).\n"
            L"No updates are needed.",
            PLUGIN_VERSION_STR);

        MessageBoxW(hParent,
            msgBoxText,
            L"BlackSpeak - Check for Update",
            MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    }

    g_isManualUpdateCheck = FALSE;
    return 0;
}

static void TriggerGracefulTS3Quit() {
    // 1. Call Qt's official QCoreApplication::quit() to trigger graceful disconnect from all connected servers
    HMODULE hCore = GetModuleHandleA("Qt5Core.dll");
    if (hCore) {
        typedef void (*fn_quit)();
        fn_quit pQuit = (fn_quit)GetProcAddress(hCore, "?quit@QCoreApplication@@SAXXZ");
        if (pQuit) {
            pQuit();
        }
    }

    // 2. Also send standard WM_CLOSE message to top-level TeamSpeak 3 Qt window
    HWND topTS3 = FindWindowA("Qt5QWindowIcon", NULL);
    if (topTS3 && IsWindow(topTS3)) {
        PostMessage(topTS3, WM_CLOSE, 0, 0);
    }
}

static DWORD WINAPI DownloadUpdateThreadProc(LPVOID lpParam) {
    HWND hNotifyWnd = (HWND)lpParam;
    HWND hParent = hNotifyWnd ? hNotifyWnd : GetForegroundWindow();
    g_updateState.status = UPDATE_STATUS_DOWNLOADING;
    snprintf(g_updateState.message, sizeof(g_updateState.message), "Downloading update package...");

    const char* url = g_updateState.downloadUrl;
    if (!url || strlen(url) == 0) {
        g_updateState.status = UPDATE_STATUS_ERROR;
        snprintf(g_updateState.message, sizeof(g_updateState.message), "Download URL is not specified.");
        MessageBoxW(hParent, L"Download URL is missing from update metadata.", L"Update Error", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        return 0;
    }

    char tempDir[MAX_PATH];
    GetTempPathA(MAX_PATH, tempDir);
    char destFile[MAX_PATH];
    snprintf(destFile, sizeof(destFile), "%sBlackSpeak_Update_%s.ts3_addon", tempDir, g_updateState.latestVersion);

    HINTERNET hInternet = InternetOpenA("BlackSpeakUpdater/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        g_updateState.status = UPDATE_STATUS_ERROR;
        snprintf(g_updateState.message, sizeof(g_updateState.message), "Network initialization failed.");
        MessageBoxW(hParent, L"Network initialization failed.", L"Update Error", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        return 0;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE;
    if (strncmp(url, "https://", 8) == 0) {
        flags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
    }

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url, NULL, 0, flags, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        g_updateState.status = UPDATE_STATUS_ERROR;
        snprintf(g_updateState.message, sizeof(g_updateState.message), "Failed to start package download.");
        MessageBoxW(hParent, L"Failed to start package download.", L"Update Error", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        return 0;
    }

    FILE* fp = NULL;
    fopen_s(&fp, destFile, "wb");
    if (!fp) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        g_updateState.status = UPDATE_STATUS_ERROR;
        snprintf(g_updateState.message, sizeof(g_updateState.message), "Could not create temporary file.");
        MessageBoxW(hParent, L"Could not create temporary file for update.", L"Update Error", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        return 0;
    }

    char buffer[8192];
    DWORD bytesRead = 0;
    size_t totalBytes = 0;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        fwrite(buffer, 1, bytesRead, fp);
        totalBytes += bytesRead;
    }

    fclose(fp);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    if (totalBytes == 0) {
        DeleteFileA(destFile);
        g_updateState.status = UPDATE_STATUS_ERROR;
        snprintf(g_updateState.message, sizeof(g_updateState.message), "Downloaded update file was empty.");
        MessageBoxW(hParent, L"Downloaded update file was empty.", L"Update Error", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        return 0;
    }

    g_updateState.status = UPDATE_STATUS_DOWNLOADED;
    snprintf(g_updateState.message, sizeof(g_updateState.message), "Update downloaded! Ready to install.");

    wchar_t msgBoxText[512];
    swprintf_s(msgBoxText,
        L"BlackSpeak update package v%S has been downloaded successfully!\n\n"
        L"TeamSpeak 3 must restart to complete installation of updated components.\n\n"
        L"Close TeamSpeak 3 and start installation now?",
        g_updateState.latestVersion);

    int userChoice = MessageBoxW(hParent,
        msgBoxText,
        L"BlackSpeak Auto-Updater",
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);

    if (userChoice == IDYES) {
        char ts3ExePath[MAX_PATH];
        GetModuleFileNameA(NULL, ts3ExePath, MAX_PATH);

        // Extract TS3 installation folder directory (for correct CWD on restart)
        char ts3Dir[MAX_PATH];
        strcpy_s(ts3Dir, sizeof(ts3Dir), ts3ExePath);
        char* lastSlash = strrchr(ts3Dir, '\\');
        if (lastSlash) {
            *lastSlash = '\0';
        }

        // Package installer path
        char packageInstPath[MAX_PATH];
        snprintf(packageInstPath, sizeof(packageInstPath), "%s\\package_inst.exe", ts3Dir);

        DWORD currentPid = GetCurrentProcessId();

        char vbsPath[MAX_PATH];
        snprintf(vbsPath, sizeof(vbsPath), "%sts3_blackspeak_updater.vbs", tempDir);

        FILE* fpVbs = NULL;
        fopen_s(&fpVbs, vbsPath, "w");
        if (fpVbs) {
            fprintf(fpVbs, "Set WshShell = CreateObject(\"WScript.Shell\")\n");
            fprintf(fpVbs, "Set fso = CreateObject(\"Scripting.FileSystemObject\")\n\n");
            fprintf(fpVbs, "' 1. Wait until TeamSpeak 3 (PID %lu) closes completely and cleanly disconnects from server\n", currentPid);
            fprintf(fpVbs, "Do\n");
            fprintf(fpVbs, "    Set procList = GetObject(\"winmgmts:\").ExecQuery(\"Select ProcessId from Win32_Process Where ProcessId = %lu\")\n", currentPid);
            fprintf(fpVbs, "    If procList.Count = 0 Then Exit Do\n");
            fprintf(fpVbs, "    WScript.Sleep 500\n");
            fprintf(fpVbs, "Loop\n\n");
            fprintf(fpVbs, "WScript.Sleep 1000\n\n");
            fprintf(fpVbs, "' 2. Run TeamSpeak Package Installer for the addon and WAIT for user to finish installation\n");
            fprintf(fpVbs, "installerPath = \"%s\"\n", packageInstPath);
            fprintf(fpVbs, "addonPath = \"%s\"\n", destFile);
            fprintf(fpVbs, "If fso.FileExists(installerPath) Then\n");
            fprintf(fpVbs, "    WshShell.Run \"\"\"\" & installerPath & \"\"\" \"\"\" & addonPath & \"\"\"\", 1, True\n");
            fprintf(fpVbs, "Else\n");
            fprintf(fpVbs, "    WshShell.Run \"\"\"\" & addonPath & \"\"\"\", 1, True\n");
            fprintf(fpVbs, "End If\n\n");
            fprintf(fpVbs, "' 3. Set Working Directory and relaunch TeamSpeak 3 cleanly\n");
            fprintf(fpVbs, "WshShell.CurrentDirectory = \"%s\"\n", ts3Dir);
            fprintf(fpVbs, "WshShell.Run \"\"\"%s\"\"\", 1, False\n\n", ts3ExePath);
            fprintf(fpVbs, "' 4. Clean up updater script\n");
            fprintf(fpVbs, "On Error Resume Next\n");
            fprintf(fpVbs, "fso.DeleteFile WScript.ScriptFullName, True\n");
            fclose(fpVbs);
        }

        // Launch wscript.exe — wscript is a native Windows GUI app (0 console window)
        char wscriptCmd[MAX_PATH + 64];
        snprintf(wscriptCmd, sizeof(wscriptCmd), "wscript.exe \"%s\"", vbsPath);

        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = { 0 };

        if (CreateProcessA(NULL, wscriptCmd, NULL, NULL, FALSE,
            CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL, &si, &pi)) {
            if (pi.hProcess) CloseHandle(pi.hProcess);
            if (pi.hThread)  CloseHandle(pi.hThread);
        }

        // Standard TeamSpeak 3 graceful exit (disconnects from servers before terminating)
        TriggerGracefulTS3Quit();
    }

    return 0;
}

static void StartCheckForUpdates(HWND hwndNotify, BOOL isManual) {
    if (g_updateState.status == UPDATE_STATUS_CHECKING || g_updateState.status == UPDATE_STATUS_DOWNLOADING) return;
    g_isManualUpdateCheck = isManual;
    if (g_hUpdateThread) {
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
    }
    g_hUpdateThread = CreateThread(NULL, 0, CheckUpdateThreadProc, (LPVOID)hwndNotify, 0, NULL);
}

static void StartDownloadAndInstallUpdate(HWND hwndNotify) {
    if (g_updateState.status == UPDATE_STATUS_DOWNLOADING) return;
    if (g_hUpdateThread) {
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
    }
    g_hUpdateThread = CreateThread(NULL, 0, DownloadUpdateThreadProc, (LPVOID)hwndNotify, 0, NULL);
}

// ==========================================
// Theme Engine & Configuration File Storage
// ==========================================

static void GetAppStylesDir(char* outPath, size_t maxLen) {
    char appData[MAX_PATH];
    GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    snprintf(outPath, maxLen, "%s\\TS3Client\\styles", appData);
}

static void GetConfigFilePath(char* outPath, size_t maxLen) {
    char appData[MAX_PATH];
    GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    snprintf(outPath, maxLen, "%s\\TS3Client\\blackspeak_config.ini", appData);
}

static void LoadConfig() {
    char cfgPath[MAX_PATH];
    GetConfigFilePath(cfgPath, sizeof(cfgPath));

    if (GetFileAttributesA(cfgPath) == INVALID_FILE_ATTRIBUTES) {
        char oldCfg[MAX_PATH];
        char appData[MAX_PATH];
        GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
        snprintf(oldCfg, sizeof(oldCfg), "%s\\TS3Client\\modern_black_config.ini", appData);
        if (GetFileAttributesA(oldCfg) != INVALID_FILE_ATTRIBUTES) {
            g_isThemeEnabled = GetPrivateProfileIntA("ModernBlack", "Enabled", 1, oldCfg);
            g_selectedPaletteIndex = GetPrivateProfileIntA("ModernBlack", "PaletteIndex", 0, oldCfg);
            return;
        }
    }

    g_isThemeEnabled = GetPrivateProfileIntA("BlackSpeak", "Enabled", 1, cfgPath);
    g_selectedPaletteIndex = GetPrivateProfileIntA("BlackSpeak", "PaletteIndex", 0, cfgPath);
    if (g_selectedPaletteIndex < 0 || g_selectedPaletteIndex >= (int)PALETTE_COUNT) {
        g_selectedPaletteIndex = 0;
    }
}

static void SaveConfig() {
    char cfgPath[MAX_PATH];
    GetConfigFilePath(cfgPath, sizeof(cfgPath));
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", g_isThemeEnabled);
    WritePrivateProfileStringA("BlackSpeak", "Enabled", buf, cfgPath);
    snprintf(buf, sizeof(buf), "%d", g_selectedPaletteIndex);
    WritePrivateProfileStringA("BlackSpeak", "PaletteIndex", buf, cfgPath);
}

static char* GenerateCustomQss(const AccentPalette& pal) {
    size_t templateLen = strlen(g_qssTemplate);
    char* result = (char*)malloc(templateLen + 4096);
    if (!result) return NULL;
    strcpy_s(result, templateLen + 4096, g_qssTemplate);

    const char* tokenPrim = "@ACCENT_PRIMARY@";
    size_t tokenPrimLen = strlen(tokenPrim);
    char* pos = NULL;
    while ((pos = strstr(result, tokenPrim)) != NULL) {
        memcpy(pos, pal.primary, 7);
        memmove(pos + 7, pos + tokenPrimLen, strlen(pos + tokenPrimLen) + 1);
    }

    const char* tokenHov = "@ACCENT_HOVER@";
    size_t tokenHovLen = strlen(tokenHov);
    while ((pos = strstr(result, tokenHov)) != NULL) {
        memcpy(pos, pal.accent, 7);
        memmove(pos + 7, pos + tokenHovLen, strlen(pos + tokenHovLen) + 1);
    }

    const char* tokenGlow = "@ACCENT_GLOW@";
    size_t tokenGlowLen = strlen(tokenGlow);
    while ((pos = strstr(result, tokenGlow)) != NULL) {
        memcpy(pos, pal.glow, 7);
        memmove(pos + 7, pos + tokenGlowLen, strlen(pos + tokenGlowLen) + 1);
    }

    return result;
}

static void SaveStringToFile(const char* filePath, const char* content) {
    FILE* fp = NULL;
    fopen_s(&fp, filePath, "wb");
    if (fp) {
        fwrite(content, 1, strlen(content), fp);
        fclose(fp);
    }
}

// Direct runtime injection into Qt within TeamSpeak 3 process
static bool ApplyLiveQtStyleSheet(const char* qssContent) {
    __try {
        HMODULE hCore = GetModuleHandleA("Qt5Core.dll");
        HMODULE hWidgets = GetModuleHandleA("Qt5Widgets.dll");
        if (!hCore || !hWidgets) return false;

        typedef void* (*fn_instance)();
        typedef void* (*fn_fromUtf8)(void* outStr, const char* str, int size);
        typedef void (*fn_dtorQString)(void* str);
        typedef void (*fn_setStyleSheet)(void* app, const void* qstr);

        fn_instance pInstance = (fn_instance)GetProcAddress(hCore, "?instance@QCoreApplication@@SAPEAV1@XZ");
        fn_fromUtf8 pFromUtf8 = (fn_fromUtf8)GetProcAddress(hCore, "?fromUtf8@QString@@SA?AV1@PEBDH@Z");
        fn_dtorQString pDtorQString = (fn_dtorQString)GetProcAddress(hCore, "??1QString@@QEAA@XZ");
        fn_setStyleSheet pSetStyleSheet = (fn_setStyleSheet)GetProcAddress(hWidgets, "?setStyleSheet@QApplication@@QEAAXAEBVQString@@@Z");

        if (!pInstance || !pFromUtf8 || !pSetStyleSheet) return false;

        void* qApp = pInstance();
        if (!qApp) return false;

        void* qstrEmpty[2] = {0};
        pFromUtf8(qstrEmpty, "", 0);
        pSetStyleSheet(qApp, qstrEmpty);
        if (pDtorQString) pDtorQString(qstrEmpty);

        if (qssContent && strlen(qssContent) > 0) {
            void* qstrNew[2] = {0};
            pFromUtf8(qstrNew, qssContent, (int)strlen(qssContent));
            pSetStyleSheet(qApp, qstrNew);
            if (pDtorQString) pDtorQString(qstrNew);
        }

        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static void ApplyThemeAndPalette() {
    const AccentPalette& pal = g_palettes[g_selectedPaletteIndex];
    char* generatedQss = GenerateCustomQss(pal);
    if (!generatedQss) return;

    char appStylesDir[MAX_PATH];
    GetAppStylesDir(appStylesDir, sizeof(appStylesDir));
    CreateDirectoryA(appStylesDir, NULL);
    char appQssPath[MAX_PATH];
    snprintf(appQssPath, sizeof(appQssPath), "%s\\ModernBlack.qss", appStylesDir);
    SaveStringToFile(appQssPath, generatedQss);

    if (g_isThemeEnabled) {
        ApplyLiveQtStyleSheet(generatedQss);
    } else {
        ApplyLiveQtStyleSheet("");
    }

    free(generatedQss);
}

// Helper to draw TeamSpeak 3 style GroupBoxes
static void DrawTs3GroupBox(HDC hdc, RECT rc, const wchar_t* title, HFONT hFont) {
    SelectObject(hdc, hFont);
    SIZE sz = { 0, 0 };
    GetTextExtentPoint32W(hdc, title, (int)wcslen(title), &sz);

    int titleLeft = rc.left + 10;
    int titleRight = titleLeft + sz.cx + 6;
    int topY = rc.top + sz.cy / 2;

    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(30, 34, 45));
    HPEN oldPen = (HPEN)SelectObject(hdc, hBorderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    RoundRect(hdc, rc.left, topY, rc.right, rc.bottom, 6, 6);

    // Erase border behind title
    HBRUSH hBg = CreateSolidBrush(RGB(6, 7, 9));
    RECT rClear = { titleLeft - 2, topY - 1, titleRight + 2, topY + 2 };
    FillRect(hdc, &rClear, hBg);
    DeleteObject(hBg);

    // Draw Title
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(160, 160, 160));
    RECT rTitle = { titleLeft + 2, rc.top, titleRight, rc.top + sz.cy };
    DrawTextW(hdc, title, -1, &rTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(hBorderPen);
}

// ==========================================
// Settings Dialog Window Procedure (TeamSpeak 3 Options Style)
// ==========================================

// Hover tracking state for bottom buttons (OK/Cancel/Apply)
static int  s_hoveredBtnId  = 0;
static BOOL s_trackingLeave = FALSE;

static LRESULT CALLBACK ConfigWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBgBrush    = NULL;
    static HFONT  hHeaderFont = NULL;  // QSS: font: 16px 'Segoe UI'; font-weight: bold
    static HFONT  hSubFont    = NULL;  // QSS: font-size: 9pt  (#8A8F9D description)
    static HFONT  hSectionFont= NULL;  // QSS: font-size: 9pt  (#A0A0A0 group title)
    static HFONT  hNormalFont = NULL;  // QSS: font-size: 9pt  (#FFFFFF body)
    static HFONT  hBtnFont    = NULL;  // QSS: font-size: 9pt  (QPushButton)
    static HFONT  hHexFont    = NULL;  // Consolas 10px for hex codes
    static HFONT  hBadgeFont  = NULL;  // Segoe UI 10px Bold for checkmark / badge
    static HWND   hCombo      = NULL;
    static HWND   hChk        = NULL;
    static HWND   hOk         = NULL;
    static HWND   hCancel     = NULL;
    static HWND   hApply      = NULL;

    switch (msg) {
    case WM_CREATE: {
        // -------------------------------------------------------
        // Color tokens directly from qss_template.h:
        //   Window bg        : #060709  RGB(6,7,9)
        //   Button bg        : #0F1117  RGB(15,17,23)
        //   Button border    : #1E222D  RGB(30,34,45)
        //   Button hover bg  : #1A1E26  RGB(26,30,38)
        //   GroupBox border  : #1E222D  RGB(30,34,45)
        //   GroupBox title   : #A0A0A0  RGB(160,160,160)
        //   Body text        : #FFFFFF
        //   Sub-desc text    : #8A8F9D  RGB(138,143,157)
        //   Muted text       : #64748B  RGB(100,116,139)
        // -------------------------------------------------------
        hBgBrush = CreateSolidBrush(RGB(6, 7, 9));

        // DPI-correct font sizes (matching QSS pt values at 96 dpi baseline)
        HDC hScreen = GetDC(NULL);
        int dpi = GetDeviceCaps(hScreen, LOGPIXELSY);
        ReleaseDC(NULL, hScreen);
        // QSS headline  : font: 16px Segoe UI Bold  => -MulDiv(16, dpi, 96)
        // QSS 9pt body  : 9pt = 12px at 96dpi       => -MulDiv(12, dpi, 96)
        // Consolas hex  : 10px
        int hPx16 = -MulDiv(16, dpi, 96);
        int hPx12 = -MulDiv(12, dpi, 96);
        int hPx11 = -MulDiv(11, dpi, 96);
        int hPx10 = -MulDiv(10, dpi, 96);

        hHeaderFont  = CreateFontW(hPx16, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hSubFont     = CreateFontW(hPx12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hSectionFont = CreateFontW(hPx12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hNormalFont  = CreateFontW(hPx12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hBtnFont     = CreateFontW(hPx12, 0, 0, 0, 500,       FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hHexFont     = CreateFontW(hPx10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
        hBadgeFont   = CreateFontW(hPx11, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        s_hComboFont = hNormalFont;

        ApplyDarkTitleBar(hwnd);

        // Owner-draw Checkbox — matches QCheckBox::indicator from QSS (14x14, #0F1117 bg, #1E222D border, accent filled when checked)
        hChk = CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 32, 75, 236, 22, hwnd, (HMENU)IDC_ENABLE_CHECKBOX, g_hInst, NULL);

        // Owner-draw ComboBox (dark subclass replaces default white arrow)
        // Matches QComboBox from QSS: bg #0F1117, border #1E222D, padding 4px 8px
        hCombo = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL, 32, 172, 236, 300, hwnd, (HMENU)IDC_ACCENT_COMBO, g_hInst, NULL);
        SendMessage(hCombo, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

        for (size_t i = 0; i < PALETTE_COUNT; ++i) {
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)g_palettes[i].name);
        }
        SendMessage(hCombo, CB_SETCURSEL, g_selectedPaletteIndex, 0);
        s_origComboProc = (WNDPROC)SetWindowLongPtrW(hCombo, GWLP_WNDPROC, (LONG_PTR)DarkComboSubclassProc);

        // Owner-draw Bottom Buttons — matches QPushButton from QSS:
        // bg #0F1117, border 1px #1E222D, border-radius 4px, padding 5px 16px, font-weight 500
        // Hover: bg #1A1E26, border accent color
        // Pressed: bg #060709, border #141720
        hOk     = CreateWindowW(L"BUTTON", L"OK",     WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 351, 396, 76, 26, hwnd, (HMENU)IDC_OK_BTN,     g_hInst, NULL);
        hCancel = CreateWindowW(L"BUTTON", L"Cancel", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 434, 396, 76, 26, hwnd, (HMENU)IDC_CANCEL_BTN, g_hInst, NULL);
        hApply  = CreateWindowW(L"BUTTON", L"Apply",  WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 517, 396, 76, 26, hwnd, (HMENU)IDC_APPLY_BTN,  g_hInst, NULL);
        SendMessage(hOk,     WM_SETFONT, (WPARAM)hBtnFont, TRUE);
        SendMessage(hCancel, WM_SETFONT, (WPARAM)hBtnFont, TRUE);
        SendMessage(hApply,  WM_SETFONT, (WPARAM)hBtnFont, TRUE);

        s_hoveredBtnId  = 0;
        s_trackingLeave = FALSE;
        g_statusText[0] = L'\0';
        break;
    }

    case WM_MOUSEMOVE: {
        // Track hover over OK/Cancel/Apply buttons for QSS-style hover effect
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        int newHover = 0;
        HWND btns[3] = { hOk, hCancel, hApply };
        int  ids[3]  = { IDC_OK_BTN, IDC_CANCEL_BTN, IDC_APPLY_BTN };
        for (int i = 0; i < 3; ++i) {
            if (!btns[i]) continue;
            RECT r;
            GetWindowRect(btns[i], &r);
            ScreenToClient(hwnd, (POINT*)&r.left);
            ScreenToClient(hwnd, (POINT*)&r.right);
            if (PtInRect(&r, pt)) { newHover = ids[i]; break; }
        }
        if (newHover != s_hoveredBtnId) {
            s_hoveredBtnId = newHover;
            InvalidateRect(hOk,     NULL, TRUE);
            InvalidateRect(hCancel, NULL, TRUE);
            InvalidateRect(hApply,  NULL, TRUE);
        }
        if (!s_trackingLeave) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            s_trackingLeave = TRUE;
        }
        break;
    }

    case WM_MOUSELEAVE: {
        s_trackingLeave = FALSE;
        if (s_hoveredBtnId != 0) {
            s_hoveredBtnId = 0;
            InvalidateRect(hOk,     NULL, TRUE);
            InvalidateRect(hCancel, NULL, TRUE);
            InvalidateRect(hApply,  NULL, TRUE);
        }
        break;
    }

    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT mis = (LPMEASUREITEMSTRUCT)lParam;
        if (mis->CtlID == IDC_ACCENT_COMBO) {
            mis->itemHeight = 28;
            return TRUE;
        }
        break;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;

        if (dis->CtlID == IDC_ENABLE_CHECKBOX) {
            HBRUSH hBg = CreateSolidBrush(RGB(6, 7, 9));
            FillRect(dis->hDC, &dis->rcItem, hBg);
            DeleteObject(hBg);

            BOOL isChecked = g_isThemeEnabled;
            int boxSize = 14;
            int boxLeft = dis->rcItem.left + 1;
            int boxTop = dis->rcItem.top + (dis->rcItem.bottom - dis->rcItem.top - boxSize) / 2;
            int boxRight = boxLeft + boxSize;
            int boxBottom = boxTop + boxSize;

            COLORREF accentColor = g_palettes[g_selectedPaletteIndex].colorRef;

            if (isChecked) {
                // TeamSpeak 3 Style Solid Blue Rounded Square (Image 2 style)
                HBRUSH hBoxBrush = CreateSolidBrush(accentColor);
                HPEN hBoxPen = CreatePen(PS_SOLID, 1, accentColor);
                HPEN oldPen = (HPEN)SelectObject(dis->hDC, hBoxPen);
                HBRUSH oldBrush = (HBRUSH)SelectObject(dis->hDC, hBoxBrush);
                RoundRect(dis->hDC, boxLeft, boxTop, boxRight, boxBottom, 3, 3);
                SelectObject(dis->hDC, oldPen);
                SelectObject(dis->hDC, oldBrush);
                DeleteObject(hBoxPen);
                DeleteObject(hBoxBrush);

                // White Checkmark
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, RGB(255, 255, 255));
                SelectObject(dis->hDC, hBadgeFont);
                RECT rCheck = { boxLeft, boxTop - 1, boxRight, boxBottom };
                DrawTextW(dis->hDC, L"✓", -1, &rCheck, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                // Unchecked Box
                HBRUSH hBoxBrush = CreateSolidBrush(RGB(15, 17, 23));
                HPEN hBoxPen = CreatePen(PS_SOLID, 1, RGB(45, 55, 75));
                HPEN oldPen = (HPEN)SelectObject(dis->hDC, hBoxPen);
                HBRUSH oldBrush = (HBRUSH)SelectObject(dis->hDC, hBoxBrush);
                RoundRect(dis->hDC, boxLeft, boxTop, boxRight, boxBottom, 3, 3);
                SelectObject(dis->hDC, oldPen);
                SelectObject(dis->hDC, oldBrush);
                DeleteObject(hBoxPen);
                DeleteObject(hBoxBrush);
            }

            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, RGB(240, 240, 240));
            SelectObject(dis->hDC, hNormalFont);
            RECT rText = { boxRight + 8, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom };
            DrawTextW(dis->hDC, L"Enable BlackSpeak Theme", -1, &rText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            return TRUE;
        }
        else if (dis->CtlID == IDC_ACCENT_COMBO) {
            if ((int)dis->itemID < 0 || (int)dis->itemID >= (int)PALETTE_COUNT) {
                return TRUE;
            }
            const AccentPalette& pal = g_palettes[dis->itemID];
            BOOL isSelected = (dis->itemState & ODS_SELECTED);

            HBRUSH hItemBg = CreateSolidBrush(isSelected ? RGB(22, 27, 38) : RGB(15, 17, 23));
            FillRect(dis->hDC, &dis->rcItem, hItemBg);
            DeleteObject(hItemBg);

            if (isSelected) {
                HBRUSH hBar = CreateSolidBrush(pal.colorRef);
                RECT rBar = { dis->rcItem.left, dis->rcItem.top + 2, dis->rcItem.left + 3, dis->rcItem.bottom - 2 };
                FillRect(dis->hDC, &rBar, hBar);
                DeleteObject(hBar);
            }

            int pillLeft = dis->rcItem.left + 10;
            int pillTop = dis->rcItem.top + 6;
            int pillRight = pillLeft + 16;
            int pillBottom = dis->rcItem.bottom - 6;

            HBRUSH hSwatch = CreateSolidBrush(pal.colorRef);
            HPEN hSwatchBorder = CreatePen(PS_SOLID, 1, RGB(60, 70, 90));
            HPEN oldPen = (HPEN)SelectObject(dis->hDC, hSwatchBorder);
            HBRUSH oldBrush = (HBRUSH)SelectObject(dis->hDC, hSwatch);
            RoundRect(dis->hDC, pillLeft, pillTop, pillRight, pillBottom, 3, 3);
            SelectObject(dis->hDC, oldPen);
            SelectObject(dis->hDC, oldBrush);
            DeleteObject(hSwatchBorder);
            DeleteObject(hSwatch);

            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, isSelected ? RGB(255, 255, 255) : RGB(226, 232, 240));
            SelectObject(dis->hDC, hNormalFont);
            RECT rText = { pillRight + 8, dis->rcItem.top, dis->rcItem.right - 80, dis->rcItem.bottom };
            DrawTextW(dis->hDC, pal.name, -1, &rText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            SetTextColor(dis->hDC, isSelected ? RGB(180, 195, 220) : RGB(148, 163, 184));
            SelectObject(dis->hDC, hHexFont);
            RECT rHex = { dis->rcItem.right - 80, dis->rcItem.top, dis->rcItem.right - 8, dis->rcItem.bottom };
            wchar_t hexBuf[32];
            swprintf_s(hexBuf, L"%S", pal.accent);
            DrawTextW(dis->hDC, hexBuf, -1, &rHex, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            return TRUE;
        }
        else if (dis->CtlID == IDC_OK_BTN || dis->CtlID == IDC_CANCEL_BTN || dis->CtlID == IDC_APPLY_BTN) {
            // Match QSS QPushButton exactly:
            //   Normal : bg #0F1117  border 1px #1E222D  radius 4px  padding 5px 16px  font-weight 500
            //   Hover  : bg #1A1E26  border 1px accent
            //   Pressed: bg #060709  border 1px #141720
            BOOL isPressed = (dis->itemState & ODS_SELECTED);
            BOOL isHovered = (s_hoveredBtnId == (int)dis->CtlID) && !isPressed;

            const wchar_t* btnLabel = L"";
            if (dis->CtlID == IDC_OK_BTN)     btnLabel = L"OK";
            else if (dis->CtlID == IDC_CANCEL_BTN) btnLabel = L"Cancel";
            else if (dis->CtlID == IDC_APPLY_BTN)  btnLabel = L"Apply";

            COLORREF accentBorder = g_palettes[g_selectedPaletteIndex].colorRef;

            COLORREF bgColor     = isPressed ? RGB(6,7,9)    : (isHovered ? RGB(26,30,38)  : RGB(15,17,23));
            COLORREF borderColor = isPressed ? RGB(20,23,32) : (isHovered ? accentBorder   : RGB(30,34,45));

            HBRUSH hBtnBg = CreateSolidBrush(bgColor);
            FillRect(dis->hDC, &dis->rcItem, hBtnBg);
            DeleteObject(hBtnBg);

            HPEN hBorder = CreatePen(PS_SOLID, 1, borderColor);
            HPEN oldPen  = (HPEN)SelectObject(dis->hDC, hBorder);
            HBRUSH oldBr = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
            RoundRect(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom, 4, 4);
            SelectObject(dis->hDC, oldPen);
            SelectObject(dis->hDC, oldBr);
            DeleteObject(hBorder);

            SetBkMode(dis->hDC, TRANSPARENT);
            // QPushButton text: #FFFFFF normal, #FFFFFF hover/pressed  — disabled: #555A64
            SetTextColor(dis->hDC, RGB(255, 255, 255));
            SelectObject(dis->hDC, hBtnFont);
            DrawTextW(dis->hDC, btnLabel, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            return TRUE;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // -------------------------------------------------------
        // QSS token reference used below:
        //  #OptionsHeadlineTitleLabel  : font 16px Segoe UI Bold, #FFFFFF
        //  #OptionsHeadlineDescriptionLabel : 9pt Segoe UI, #8A8F9D
        //  QGroupBox::title            : #A0A0A0
        //  QWidget (body)              : 9pt Segoe UI, #FFFFFF
        //  Muted / secondary text      : #64748B
        // -------------------------------------------------------

        // Headline — QSS: QLabel#OptionsHeadlineTitleLabel { font: 16px 'Segoe UI'; font-weight: bold; color: #FFFFFF }
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, hHeaderFont);
        RECT rTitle = { 18, 10, 600, 32 };
        DrawTextW(hdc, L"Design - BlackSpeak Theme", -1, &rTitle, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        // Sub-description — QSS: QLabel#OptionsHeadlineDescriptionLabel { color: #8A8F9D; font-size: 9pt }
        SetTextColor(hdc, RGB(138, 143, 157));   // #8A8F9D
        SelectObject(hdc, hSubFont);
        RECT rSub = { 18, 34, 620, 50 };
        DrawTextW(hdc, L"Configure the BlackSpeak Dark Theme and Accent Palette for TeamSpeak", -1, &rSub, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        // GroupBox 1 — QSS: QGroupBox { border: 1px solid #1E222D; border-radius: 4px }
        //              QGroupBox::title { color: #A0A0A0 }
        RECT rcGb1 = { 18, 56, 280, 134 };
        DrawTs3GroupBox(hdc, rcGb1, L"Theme Activation", hSectionFont);

        // Description inside GroupBox 1 — muted, #64748B, 9pt Segoe UI
        SetTextColor(hdc, RGB(100, 116, 139));   // #64748B
        SelectObject(hdc, hSubFont);
        RECT rGb1Desc = { 32, 103, 270, 126 };
        DrawTextW(hdc, L"Sets BlackSpeak as your default active style.", -1, &rGb1Desc, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        // GroupBox 2: Accent Palette Selection (Left Bottom)
        RECT rcGb2 = { 18, 144, 280, 380 };
        DrawTs3GroupBox(hdc, rcGb2, L"Accent Palette", hSectionFont);

        int curSel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
        if (curSel < 0 || curSel >= (int)PALETTE_COUNT) curSel = g_selectedPaletteIndex;
        const AccentPalette& pal = g_palettes[curSel];

        // Color token info — #8A8F9D label, Consolas hex values — matches QSS 9pt body
        SetTextColor(hdc, RGB(138, 143, 157));   // #8A8F9D
        SelectObject(hdc, hSubFont);
        RECT rTokHead = { 32, 214, 270, 232 };
        DrawTextW(hdc, L"Palette Color Codes:", -1, &rTokHead, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        wchar_t bufTok1[64], bufTok2[64], bufTok3[64];
        swprintf_s(bufTok1, L"• Primary : %S", pal.primary);
        swprintf_s(bufTok2, L"• Hover   : %S", pal.accent);
        swprintf_s(bufTok3, L"• Glow    : %S", pal.glow);

        // Hex values in Consolas — matches QStatusBar label-like styling
        SetTextColor(hdc, RGB(226, 232, 240));   // near-white for code values
        SelectObject(hdc, hHexFont);
        RECT rT1 = { 34, 236, 270, 252 };
        RECT rT2 = { 34, 254, 270, 270 };
        RECT rT3 = { 34, 272, 270, 288 };
        DrawTextW(hdc, bufTok1, -1, &rT1, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextW(hdc, bufTok2, -1, &rT2, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextW(hdc, bufTok3, -1, &rT3, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        SetTextColor(hdc, RGB(148, 163, 184));
        SelectObject(hdc, hSubFont);
        RECT rTagLine = { 32, 305, 270, 360 };
        wchar_t bufTag[128];
        swprintf_s(bufTag, L"Variant: %s\nStyle: Modern Obsidian", pal.tag);
        DrawTextW(hdc, bufTag, -1, &rTagLine, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        // GroupBox 3: Live Preview Frame (Right Column)
        RECT rcGb3 = { 295, 54, 600, 375 };
        DrawTs3GroupBox(hdc, rcGb3, L"Channel & UI Preview", hSubFont);

        // 1. Channel Tree Simulation Card
        RECT rTreeBox = { 312, 80, 584, 240 };
        HBRUSH hTreeBg = CreateSolidBrush(RGB(10, 12, 16));
        FillRect(hdc, &rTreeBox, hTreeBg);
        DeleteObject(hTreeBg);

        HPEN hTreeBorder = CreatePen(PS_SOLID, 1, RGB(20, 23, 32));
        HPEN oldP1 = (HPEN)SelectObject(hdc, hTreeBorder);
        HBRUSH oldB1 = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, 312, 80, 584, 240, 4, 4);
        SelectObject(hdc, oldP1);
        SelectObject(hdc, oldB1);
        DeleteObject(hTreeBorder);

        // Channel items
        SetTextColor(hdc, RGB(200, 205, 215));
        SelectObject(hdc, hNormalFont);
        RECT rChan1 = { 324, 90, 570, 110 };
        DrawTextW(hdc, L"📁  [cspacer] ~ BlackSpeak Server ~", -1, &rChan1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT rChan2 = { 334, 114, 570, 134 };
        DrawTextW(hdc, L"📁  Lobby / General Chat", -1, &rChan2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Active Channel Pill (Glowing Accent)
        HBRUSH hActChan = CreateSolidBrush(pal.colorRef);
        RECT rActChanPill = { 332, 140, 568, 166 };
        FillRect(hdc, &rActChanPill, hActChan);
        DeleteObject(hActChan);

        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, hNormalFont);
        RECT rActChanText = { 342, 140, 560, 166 };
        DrawTextW(hdc, L"🔊  TeamSpeak Active Channel (Talking)", -1, &rActChanText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SetTextColor(hdc, RGB(160, 170, 185));
        RECT rClient1 = { 354, 172, 570, 192 };
        DrawTextW(hdc, L"• User Client (Online)", -1, &rClient1, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT rChan3 = { 334, 198, 570, 218 };
        DrawTextW(hdc, L"📁  Gaming Lounge", -1, &rChan3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // 2. Button Simulation Box inside Preview
        SetTextColor(hdc, RGB(148, 163, 184));
        SelectObject(hdc, hSubFont);
        RECT rBtnPrevTitle = { 312, 255, 584, 275 };
        DrawTextW(hdc, L"Interface Buttons Preview:", -1, &rBtnPrevTitle, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        // Simulated Normal Button
        HBRUSH hSimB1 = CreateSolidBrush(RGB(15, 17, 23));
        RECT rSimB1 = { 312, 280, 430, 310 };
        FillRect(hdc, &rSimB1, hSimB1);
        DeleteObject(hSimB1);

        HPEN hSimP1 = CreatePen(PS_SOLID, 1, RGB(30, 34, 45));
        HPEN pOldS1 = (HPEN)SelectObject(hdc, hSimP1);
        HBRUSH bOldS1 = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, 312, 280, 430, 310, 4, 4);
        SelectObject(hdc, pOldS1);
        SelectObject(hdc, bOldS1);
        DeleteObject(hSimP1);

        SetTextColor(hdc, RGB(220, 220, 220));
        SelectObject(hdc, hNormalFont);
        RECT rSimT1 = { 312, 280, 430, 310 };
        DrawTextW(hdc, L"Normal Button", -1, &rSimT1, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Simulated Accent Button
        HBRUSH hSimB2 = CreateSolidBrush(RGB(15, 17, 23));
        RECT rSimB2 = { 450, 280, 584, 310 };
        FillRect(hdc, &rSimB2, hSimB2);
        DeleteObject(hSimB2);

        HPEN hSimP2 = CreatePen(PS_SOLID, 1, pal.colorRef);
        HPEN pOldS2 = (HPEN)SelectObject(hdc, hSimP2);
        HBRUSH bOldS2 = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, 450, 280, 584, 310, 4, 4);
        SelectObject(hdc, pOldS2);
        SelectObject(hdc, bOldS2);
        DeleteObject(hSimP2);

        SetTextColor(hdc, RGB(255, 255, 255));
        RECT rSimT2 = { 450, 280, 584, 310 };
        DrawTextW(hdc, L"Accent Hover", -1, &rSimT2, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Active Color Pill Indicator Bar
        HBRUSH hPalBar = CreateSolidBrush(pal.colorRef);
        RECT rPalBar = { 312, 330, 584, 350 };
        FillRect(hdc, &rPalBar, hPalBar);
        DeleteObject(hPalBar);

        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, hBadgeFont);
        RECT rPalBarText = { 312, 330, 584, 350 };
        wchar_t bufBar[64];
        swprintf_s(bufBar, L"Active Palette: %s", pal.name);
        DrawTextW(hdc, bufBar, -1, &rPalBarText, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Status text on bottom left
        if (g_statusText[0] != L'\0') {
            SetTextColor(hdc, RGB(16, 185, 129));
            SelectObject(hdc, hSectionFont);
            RECT rStatus = { 18, 398, 330, 422 };
            DrawTextW(hdc, g_statusText, -1, &rStatus, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, RGB(6, 7, 9));
        return (LRESULT)hBgBrush;
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, RGB(6, 7, 9));
        return (LRESULT)hBgBrush;
    }

    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(240, 245, 255));
        SetBkColor(hdc, RGB(15, 17, 23));
        return (LRESULT)hBgBrush;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect(hdc, &rect, hBgBrush);
        return 1;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        if (wmId == IDC_ENABLE_CHECKBOX) {
            g_isThemeEnabled = !g_isThemeEnabled;
            InvalidateRect(hChk, NULL, TRUE);
        }
        else if (wmId == IDC_ACCENT_COMBO && wmEvent == CBN_SELCHANGE) {
            g_statusText[0] = L'\0';
            InvalidateRect(hwnd, NULL, TRUE);
            InvalidateRect(hChk, NULL, TRUE);
        }
        else if (wmId == IDC_OK_BTN) {
            g_selectedPaletteIndex = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            SaveConfig();
            ApplyThemeAndPalette();
            DestroyWindow(hwnd);
        }
        else if (wmId == IDC_CANCEL_BTN) {
            DestroyWindow(hwnd);
        }
        else if (wmId == IDC_APPLY_BTN) {
            g_selectedPaletteIndex = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            SaveConfig();
            ApplyThemeAndPalette();

            wcscpy_s(g_statusText, sizeof(g_statusText) / sizeof(wchar_t), L"✓  Theme applied live!");
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateWindow(hwnd);
        }
        break;
    }

    case WM_CLOSE: {
        DestroyWindow(hwnd);
        return 0;
    }

    case WM_DESTROY: {
        if (hBgBrush) DeleteObject(hBgBrush);
        if (hHeaderFont) DeleteObject(hHeaderFont);
        if (hSubFont) DeleteObject(hSubFont);
        if (hSectionFont) DeleteObject(hSectionFont);
        if (hNormalFont) DeleteObject(hNormalFont);
        if (hBtnFont) DeleteObject(hBtnFont);
        if (hHexFont) DeleteObject(hHexFont);
        if (hBadgeFont) DeleteObject(hBadgeFont);
        s_hComboFont = NULL;
        g_hConfigWnd = NULL;
        break;
    }

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static void ShowConfigWindow() {
    if (g_hConfigWnd && IsWindow(g_hConfigWnd)) {
        SetForegroundWindow(g_hConfigWnd);
        return;
    }

    LoadConfig();

    static BOOL s_isClassRegistered = FALSE;
    if (!s_isClassRegistered) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = ConfigWndProc;
        wc.hInstance = g_hInst;
        wc.lpszClassName = L"BlackSpeakTS3CustomizerV2";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClassExW(&wc);
        s_isClassRegistered = TRUE;
    }

    int width = 635;
    int height = 475;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    HWND topTS3 = FindWindowA("Qt5QWindowIcon", NULL);

    g_hConfigWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"BlackSpeakTS3CustomizerV2",
        L"Theme - BlackSpeak",
        WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, width, height,
        topTS3, NULL, g_hInst, NULL
    );

    if (!g_hConfigWnd) return;

    ShowWindow(g_hConfigWnd, SW_SHOW);
    SetForegroundWindow(g_hConfigWnd);
    UpdateWindow(g_hConfigWnd);
}

// ==========================================
// TeamSpeak 3 Plugin API Exports
// ==========================================

struct TS3Functions { void* dummy; };

extern "C" {

PLUGINS_EXPORTDLL const char* ts3plugin_name() {
    return "BlackSpeak";
}

PLUGINS_EXPORTDLL const char* ts3plugin_version() {
    return PLUGIN_VERSION_STR;
}

PLUGINS_EXPORTDLL int ts3plugin_apiVersion() {
    return PLUGIN_API_VERSION;
}

PLUGINS_EXPORTDLL const char* ts3plugin_author() {
    return "Coretify Studio";
}

PLUGINS_EXPORTDLL const char* ts3plugin_description() {
    return "Official BlackSpeak Suite for TeamSpeak 3: Sleek Dark Theme, Windows 11 Dark Titlebars, Live Accent Customizer, and Auto-Update support.\nDesigned by Coretify Studio.";
}

PLUGINS_EXPORTDLL void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    (void)funcs;
}

PLUGINS_EXPORTDLL void ts3plugin_registerPluginID(const char* id) {
    if (g_pluginID) free(g_pluginID);
    const size_t sz = strlen(id) + 1;
    g_pluginID = (char*)malloc(sz * sizeof(char));
    if (g_pluginID) {
        strcpy_s(g_pluginID, sz, id);
    }
}

PLUGINS_EXPORTDLL void ts3plugin_freeMemory(void* data) {
    if (data) free(data);
}

static struct PluginMenuItem* createMenuItem(enum PluginMenuType type, int id, const char* text, const char* icon) {
    struct PluginMenuItem* menuItem = (struct PluginMenuItem*)malloc(sizeof(struct PluginMenuItem));
    if (!menuItem) return NULL;
    menuItem->type = type;
    menuItem->id = id;
    strncpy_s(menuItem->text, PLUGIN_MENU_BUFSZ, text, _TRUNCATE);
    strncpy_s(menuItem->icon, PLUGIN_MENU_BUFSZ, icon, _TRUNCATE);
    return menuItem;
}

PLUGINS_EXPORTDLL void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon) {
    if (menuIcon) {
        *menuIcon = NULL;
    }

    *menuItems = (struct PluginMenuItem**)malloc(sizeof(struct PluginMenuItem*) * 3);
    if (!*menuItems) return;
    (*menuItems)[0] = createMenuItem(PLUGIN_MENU_TYPE_GLOBAL, 1, "Theme", "");
    (*menuItems)[1] = createMenuItem(PLUGIN_MENU_TYPE_GLOBAL, 2, "Check for Update", "");
    (*menuItems)[2] = NULL;
}

PLUGINS_EXPORTDLL void ts3plugin_onMenuItemEvent(uint64_t serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64_t selectedItemID) {
    (void)serverConnectionHandlerID;
    (void)type;
    (void)selectedItemID;
    if (menuItemID == 1) {
        ShowConfigWindow();
    } else if (menuItemID == 2) {
        StartCheckForUpdates(NULL, TRUE);
    }
}

PLUGINS_EXPORTDLL int ts3plugin_offersConfigure() {
    return PLUGIN_OFFERS_CONFIGURE_STANDARD;
}

PLUGINS_EXPORTDLL void ts3plugin_configure(void* qParentWidget, void* qPluginWidget) {
    (void)qParentWidget;
    (void)qPluginWidget;
    ShowConfigWindow();
}

PLUGINS_EXPORTDLL int ts3plugin_init() {
    // 1. Start Windows 11 Dark TitleBar Engine
    StartDarkTitleBarEngine();

    // 2. Load and Apply BlackSpeak theme stylesheet
    LoadConfig();
    ApplyThemeAndPalette();

    return 0; // 0 = Success
}

PLUGINS_EXPORTDLL void ts3plugin_shutdown() {
    StopDarkTitleBarEngine();

    if (g_hConfigWnd && IsWindow(g_hConfigWnd)) {
        SendMessage(g_hConfigWnd, WM_CLOSE, 0, 0);
    }
    if (g_hUpdateThread) {
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
    }
    if (g_pluginID) {
        free(g_pluginID);
        g_pluginID = NULL;
    }
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hInst = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}
