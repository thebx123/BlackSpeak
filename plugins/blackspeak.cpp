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
    const wchar_t* tag;         // e.g. L"Classic Cyber"
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
#define IDC_APPLY_BTN          1003
#define IDC_CANCEL_BTN         1004

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
    p++; // past opening quote

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

        // Show update available popup with Download / Later options
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
        char tempDirBat[MAX_PATH];
        GetTempPathA(MAX_PATH, tempDirBat);
        char batPath[MAX_PATH];
        snprintf(batPath, sizeof(batPath), "%sts3_install_update.bat", tempDirBat);

        FILE* fpBat = NULL;
        fopen_s(&fpBat, batPath, "w");
        if (fpBat) {
            fprintf(fpBat, "@echo off\n");
            fprintf(fpBat, "timeout /t 1 /nobreak >nul\n");
            fprintf(fpBat, "taskkill /IM ts3client_win64.exe /F >nul 2>&1\n");
            fprintf(fpBat, "timeout /t 1 /nobreak >nul\n");
            fprintf(fpBat, "start \"\" \"%s\"\n", destFile);
            fprintf(fpBat, "del \"%%~f0\" >nul 2>&1\n");
            fclose(fpBat);
        }

        ShellExecuteA(NULL, "open", batPath, NULL, NULL, SW_HIDE);

        HWND topTS3 = FindWindowA("Qt5QWindowIcon", NULL);
        if (topTS3 && IsWindow(topTS3)) {
            PostMessage(topTS3, WM_CLOSE, 0, 0);
        }

        Sleep(400);
        ExitProcess(0);
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

    // Fallback to legacy config if new config doesn't exist yet
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

static void UpdateSettingsDb(BOOL enabled) {
    char appData[MAX_PATH];
    GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "python -c \"import sqlite3, time; conn=sqlite3.connect(r'%s\\TS3Client\\settings.db'); c=conn.cursor(); ts=int(time.time()); c.execute(\\\"INSERT OR REPLACE INTO Application (key, value, timestamp) VALUES ('QtStyleSheet', '%s', ?)\\\", (ts,)); c.execute(\\\"INSERT OR REPLACE INTO Application (key, value, timestamp) VALUES ('QtStyle', '0', ?)\\\", (ts,)); conn.commit(); conn.close()\"",
        appData, enabled ? "ModernBlack" : "");
    WinExec(cmd, SW_HIDE);
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

        // Step 1: Clear current stylesheet to force Qt widget cache invalidation
        void* qstrEmpty[2] = {0};
        pFromUtf8(qstrEmpty, "", 0);
        pSetStyleSheet(qApp, qstrEmpty);
        if (pDtorQString) pDtorQString(qstrEmpty);

        // Step 2: Apply the updated BlackSpeak stylesheet
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

    UpdateSettingsDb(g_isThemeEnabled);

    // Apply live to running TeamSpeak instance
    if (g_isThemeEnabled) {
        ApplyLiveQtStyleSheet(generatedQss);
    } else {
        ApplyLiveQtStyleSheet("");
    }

    free(generatedQss);
}

// ==========================================
// Settings Dialog Window Procedure
// ==========================================

static LRESULT CALLBACK ConfigWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBgBrush = NULL;
    static HBRUSH hCardBrush = NULL;
    static HBRUSH hBannerBrush = NULL;
    static HBRUSH hItemSelBrush = NULL;
    static HFONT hHeaderFont = NULL;
    static HFONT hSubFont = NULL;
    static HFONT hSectionFont = NULL;
    static HFONT hNormalFont = NULL;
    static HFONT hBtnFont = NULL;
    static HFONT hHexFont = NULL;
    static HFONT hBadgeFont = NULL;
    static HWND hCombo = NULL;
    static HWND hChk = NULL;
    static HWND hApply = NULL;
    static HWND hCancel = NULL;

    switch (msg) {
    case WM_CREATE: {
        // Deep obsidian theme colors
        hBgBrush = CreateSolidBrush(RGB(8, 10, 15));
        hCardBrush = CreateSolidBrush(RGB(15, 19, 29));
        hBannerBrush = CreateSolidBrush(RGB(16, 20, 31));
        hItemSelBrush = CreateSolidBrush(RGB(28, 35, 52));

        // Segoe UI font hierarchy (matching native TeamSpeak 3 typography)
        hHeaderFont = CreateFontW(19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hSubFont = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hSectionFont = CreateFontW(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hNormalFont = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hBtnFont = CreateFontW(13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        hHexFont = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
        hBadgeFont = CreateFontW(11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        ApplyDarkTitleBar(hwnd);

        // Theme Checkbox
        hChk = CreateWindowW(L"BUTTON", L" Enable BlackSpeak Theme", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 32, 68, 455, 22, hwnd, (HMENU)IDC_ENABLE_CHECKBOX, g_hInst, NULL);
        SendMessage(hChk, WM_SETFONT, (WPARAM)hSectionFont, TRUE);
        SendMessage(hChk, BM_SETCHECK, g_isThemeEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

        // Accent Palette Dropdown
        hCombo = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL, 30, 138, 465, 300, hwnd, (HMENU)IDC_ACCENT_COMBO, g_hInst, NULL);
        SendMessage(hCombo, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

        for (size_t i = 0; i < PALETTE_COUNT; ++i) {
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)g_palettes[i].name);
        }
        SendMessage(hCombo, CB_SETCURSEL, g_selectedPaletteIndex, 0);

        // Bottom Action Buttons
        hCancel = CreateWindowW(L"BUTTON", L"Close", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 300, 306, 84, 32, hwnd, (HMENU)IDC_CANCEL_BTN, g_hInst, NULL);
        SendMessage(hCancel, WM_SETFONT, (WPARAM)hBtnFont, TRUE);

        hApply = CreateWindowW(L"BUTTON", L"Apply", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 395, 306, 100, 32, hwnd, (HMENU)IDC_APPLY_BTN, g_hInst, NULL);
        SendMessage(hApply, WM_SETFONT, (WPARAM)hBtnFont, TRUE);

        g_statusText[0] = L'\0';
        break;
    }

    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT mis = (LPMEASUREITEMSTRUCT)lParam;
        if (mis->CtlID == IDC_ACCENT_COMBO) {
            mis->itemHeight = 32;
            return TRUE;
        }
        break;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;

        if (dis->CtlID == IDC_ACCENT_COMBO) {
            if ((int)dis->itemID < 0 || (int)dis->itemID >= (int)PALETTE_COUNT) {
                return TRUE;
            }
            const AccentPalette& pal = g_palettes[dis->itemID];
            BOOL isSelected = (dis->itemState & ODS_SELECTED);

            HBRUSH hItemBg = CreateSolidBrush(isSelected ? RGB(26, 33, 48) : RGB(14, 17, 24));
            FillRect(dis->hDC, &dis->rcItem, hItemBg);
            DeleteObject(hItemBg);

            if (isSelected) {
                HBRUSH hBar = CreateSolidBrush(pal.colorRef);
                RECT rBar = { dis->rcItem.left, dis->rcItem.top + 2, dis->rcItem.left + 4, dis->rcItem.bottom - 2 };
                FillRect(dis->hDC, &rBar, hBar);
                DeleteObject(hBar);
            }

            int pillLeft = dis->rcItem.left + 14;
            int pillTop = dis->rcItem.top + 6;
            int pillRight = pillLeft + 22;
            int pillBottom = dis->rcItem.bottom - 6;

            HBRUSH hSwatch = CreateSolidBrush(pal.colorRef);
            HPEN hSwatchBorder = CreatePen(PS_SOLID, 1, RGB(60, 70, 90));
            HPEN oldPen = (HPEN)SelectObject(dis->hDC, hSwatchBorder);
            HBRUSH oldBrush = (HBRUSH)SelectObject(dis->hDC, hSwatch);
            RoundRect(dis->hDC, pillLeft, pillTop, pillRight, pillBottom, 4, 4);
            SelectObject(dis->hDC, oldPen);
            SelectObject(dis->hDC, oldBrush);
            DeleteObject(hSwatchBorder);
            DeleteObject(hSwatch);

            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, isSelected ? RGB(255, 255, 255) : RGB(226, 232, 240));
            SelectObject(dis->hDC, hSectionFont);
            RECT rText = { pillRight + 12, dis->rcItem.top, dis->rcItem.right - 100, dis->rcItem.bottom };
            DrawTextW(dis->hDC, pal.name, -1, &rText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            SetTextColor(dis->hDC, isSelected ? RGB(160, 175, 205) : RGB(100, 116, 139));
            SelectObject(dis->hDC, hHexFont);
            RECT rHex = { dis->rcItem.right - 100, dis->rcItem.top, dis->rcItem.right - 12, dis->rcItem.bottom };
            wchar_t hexBuf[32];
            swprintf_s(hexBuf, L"%S", pal.accent);
            DrawTextW(dis->hDC, hexBuf, -1, &rHex, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            return TRUE;
        }
        else if (dis->CtlID == IDC_APPLY_BTN) {
            int curSel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            if (curSel < 0 || curSel >= (int)PALETTE_COUNT) curSel = g_selectedPaletteIndex;
            COLORREF accentColor = g_palettes[curSel].colorRef;

            BOOL isPressed = (dis->itemState & ODS_SELECTED);
            HBRUSH hBtnBg = CreateSolidBrush(isPressed ? RGB(28, 36, 52) : RGB(16, 22, 34));
            FillRect(dis->hDC, &dis->rcItem, hBtnBg);
            DeleteObject(hBtnBg);

            HPEN hBorder = CreatePen(PS_SOLID, isPressed ? 2 : 1, accentColor);
            HPEN oldPen = (HPEN)SelectObject(dis->hDC, hBorder);
            HBRUSH oldBrush = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
            RoundRect(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom, 4, 4);
            SelectObject(dis->hDC, oldPen);
            SelectObject(dis->hDC, oldBrush);
            DeleteObject(hBorder);

            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, RGB(255, 255, 255));
            SelectObject(dis->hDC, hBtnFont);
            DrawTextW(dis->hDC, L"Apply", -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            return TRUE;
        }
        else if (dis->CtlID == IDC_CANCEL_BTN) {
            BOOL isPressed = (dis->itemState & ODS_SELECTED);
            HBRUSH hBtnBg = CreateSolidBrush(isPressed ? RGB(22, 26, 36) : RGB(14, 17, 24));
            FillRect(dis->hDC, &dis->rcItem, hBtnBg);
            DeleteObject(hBtnBg);

            HPEN hBorder = CreatePen(PS_SOLID, 1, RGB(35, 42, 58));
            HPEN oldPen = (HPEN)SelectObject(dis->hDC, hBorder);
            HBRUSH oldBrush = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
            RoundRect(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom, 4, 4);
            SelectObject(dis->hDC, oldPen);
            SelectObject(dis->hDC, oldBrush);
            DeleteObject(hBorder);

            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, RGB(180, 190, 205));
            SelectObject(dis->hDC, hBtnFont);
            DrawTextW(dis->hDC, L"Close", -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            return TRUE;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Top Header Banner
        RECT rHeaderBanner = { 0, 0, 530, 56 };
        FillRect(hdc, &rHeaderBanner, hBannerBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, hHeaderFont);
        RECT rTitle = { 24, 8, 500, 30 };
        DrawTextW(hdc, L"Theme - BlackSpeak", -1, &rTitle, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        SetTextColor(hdc, RGB(148, 163, 184));
        SelectObject(hdc, hSubFont);
        RECT rSub = { 24, 32, 500, 50 };
        DrawTextW(hdc, L"Coretify Studio • Unified Dark Theme, Dark TitleBar & Customizer", -1, &rSub, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        HPEN hLinePen = CreatePen(PS_SOLID, 1, RGB(30, 36, 50));
        HPEN oldPen = (HPEN)SelectObject(hdc, hLinePen);
        MoveToEx(hdc, 0, 56, NULL);
        LineTo(hdc, 530, 56);

        // Subtext for Checkbox
        SetTextColor(hdc, RGB(110, 125, 145));
        SelectObject(hdc, hSubFont);
        RECT rChkSub = { 54, 90, 490, 108 };
        DrawTextW(hdc, L"Sets BlackSpeak as your default active TeamSpeak style.", -1, &rChkSub, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        // Section 2: Accent Palette Label
        SetTextColor(hdc, RGB(226, 232, 240));
        SelectObject(hdc, hSectionFont);
        RECT rSec2 = { 30, 116, 490, 134 };
        DrawTextW(hdc, L"Select Accent Palette:", -1, &rSec2, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        // Palette Live Preview Card
        int curSel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
        if (curSel >= 0 && curSel < (int)PALETTE_COUNT) {
            const AccentPalette& pal = g_palettes[curSel];

            RECT rCard = { 30, 178, 495, 286 };
            FillRect(hdc, &rCard, hCardBrush);

            HPEN hCardPen = CreatePen(PS_SOLID, 1, RGB(32, 40, 56));
            HPEN pOld = (HPEN)SelectObject(hdc, hCardPen);
            HBRUSH bOld = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, 30, 178, 495, 286, 6, 6);
            SelectObject(hdc, pOld);
            SelectObject(hdc, bOld);
            DeleteObject(hCardPen);

            // Left Color Pill Bar
            HBRUSH hSwatch = CreateSolidBrush(pal.colorRef);
            RECT rPill = { 44, 192, 62, 230 };
            FillRect(hdc, &rPill, hSwatch);
            DeleteObject(hSwatch);

            // Palette Title & Tag
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, hSectionFont);
            RECT rSwatchTitle = { 72, 190, 460, 210 };
            DrawTextW(hdc, pal.name, -1, &rSwatchTitle, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

            SetTextColor(hdc, RGB(148, 163, 184));
            SelectObject(hdc, hSubFont);
            RECT rTag = { 72, 210, 460, 228 };
            DrawTextW(hdc, pal.tag, -1, &rTag, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

            // Hex values line
            SetTextColor(hdc, RGB(160, 175, 200));
            SelectObject(hdc, hHexFont);
            RECT rHex = { 44, 236, 480, 254 };
            wchar_t hexBuf[128];
            swprintf_s(hexBuf, L"Primary: %S   |   Hover: %S   |   Glow: %S", pal.primary, pal.accent, pal.glow);
            DrawTextW(hdc, hexBuf, -1, &rHex, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

            // Mini Live Channel Pill
            HBRUSH hSimChan = CreateSolidBrush(pal.colorRef);
            RECT rSimChan = { 44, 256, 200, 278 };
            FillRect(hdc, &rSimChan, hSimChan);
            DeleteObject(hSimChan);

            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, hSubFont);
            RECT rSimChanText = { 50, 256, 194, 278 };
            DrawTextW(hdc, L"🔊 Active Channel", -1, &rSimChanText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            // Mini Action Button Preview
            HBRUSH hSimBtnBg = CreateSolidBrush(RGB(18, 24, 36));
            RECT rSimBtn = { 210, 256, 310, 278 };
            FillRect(hdc, &rSimBtn, hSimBtnBg);
            DeleteObject(hSimBtnBg);

            HPEN hSimBtnBorder = CreatePen(PS_SOLID, 1, pal.colorRef);
            HPEN pOld2 = (HPEN)SelectObject(hdc, hSimBtnBorder);
            HBRUSH bOld2 = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, 210, 256, 310, 278, 4, 4);
            SelectObject(hdc, pOld2);
            SelectObject(hdc, bOld2);
            DeleteObject(hSimBtnBorder);

            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, hSubFont);
            RECT rSimBtnText = { 210, 256, 310, 278 };
            DrawTextW(hdc, L"Button Preview", -1, &rSimBtnText, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // Bottom Separator Line
        MoveToEx(hdc, 0, 296, NULL);
        LineTo(hdc, 530, 296);
        SelectObject(hdc, oldPen);
        DeleteObject(hLinePen);

        // Status text on bottom left
        if (g_statusText[0] != L'\0') {
            SetTextColor(hdc, RGB(52, 211, 153));
            SelectObject(hdc, hSectionFont);
            RECT rStatus = { 30, 312, 280, 334 };
            DrawTextW(hdc, g_statusText, -1, &rStatus, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, RGB(8, 10, 15));
        return (LRESULT)hBgBrush;
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, RGB(8, 10, 15));
        return (LRESULT)hBgBrush;
    }

    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(240, 245, 255));
        SetBkColor(hdc, RGB(14, 17, 24));
        return (LRESULT)hCardBrush;
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

        if (wmId == IDC_ACCENT_COMBO && wmEvent == CBN_SELCHANGE) {
            g_statusText[0] = L'\0';
            InvalidateRect(hwnd, NULL, TRUE);
        }
        else if (wmId == IDC_CANCEL_BTN) {
            DestroyWindow(hwnd);
        }
        else if (wmId == IDC_APPLY_BTN) {
            g_isThemeEnabled = (SendMessage(hChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_selectedPaletteIndex = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);

            SaveConfig();
            ApplyThemeAndPalette();

            wcscpy_s(g_statusText, sizeof(g_statusText) / sizeof(wchar_t), L"✓ Theme applied live!");
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
        if (hCardBrush) DeleteObject(hCardBrush);
        if (hBannerBrush) DeleteObject(hBannerBrush);
        if (hItemSelBrush) DeleteObject(hItemSelBrush);
        if (hHeaderFont) DeleteObject(hHeaderFont);
        if (hSubFont) DeleteObject(hSubFont);
        if (hSectionFont) DeleteObject(hSectionFont);
        if (hNormalFont) DeleteObject(hNormalFont);
        if (hBtnFont) DeleteObject(hBtnFont);
        if (hHexFont) DeleteObject(hHexFont);
        if (hBadgeFont) DeleteObject(hBadgeFont);
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

    int width = 530;
    int height = 385;
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
