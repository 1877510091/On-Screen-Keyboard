#pragma execution_character_set("utf-8")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <vector>
#include <string>

// --- 图标资源引入区 ---
// 如果你已经通过 VS 添加了 .ico 图标资源，请【取消注释】下面这一行：
// #include "resource.h"

// 防报错兜底设计：如果没有添加图标，使用系统默认图标宏
#ifndef IDI_ICON1
#define IDI_ICON1 32512 // 32512 是 IDI_APPLICATION 的底层数值
#endif

// --- DWM 材质常量 ---
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#define DWMSBT_MAINWINDOW 2
#define DWMSBT_TRANSIENTWINDOW 3
#endif

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

static constexpr UINT WM_TRAYICON = WM_APP + 1;

// --- 全局状态与出厂默认配置 ---
static bool g_isDark = true;
static int g_effectType = 2;               
static int g_layoutType = 0;               
static DWORD g_customColor = 0x000000;
static bool g_useCustomColor = false;
static int g_windowOpacity = 100;          
static bool g_textOpaque = false;          
static bool g_topMost = true;              
static bool g_hideFromTaskbar = false;     
static bool g_enableMouseTyping = false;   
static bool g_mouseClickThrough = false;   
static bool g_showSettingsBtn = true;      
static bool g_showHomingKeys = false;      
static bool g_restoredDefault = false;

static HWND g_hwnd = nullptr;
static HHOOK g_hKeyboardHook = nullptr;
static bool g_keyStates[256] = { false };
static int g_pressedKey = -1;

static ID2D1Factory* g_pD2DFactory = nullptr;
static IDWriteFactory* g_pDWriteFactory = nullptr;
static ID2D1HwndRenderTarget* g_pRenderTarget = nullptr;
static IDWriteTextFormat* g_pTextFormat = nullptr;

struct KDef { int vk; const wchar_t* lbl; float x, y, w, h; };
struct RenderKey { int vk = 0; std::wstring lbl; D2D1_RECT_F rect = { 0.0f, 0.0f, 0.0f, 0.0f }; };
static std::vector<RenderKey> g_renderKeys;

// 78键 60%布局 (已修复所有 float 精度截断警告)
static KDef layout78[] = {
    {VK_OEM_3,L"~",0.0f,0.0f,1.0f,1.0f}, {'1',L"1",1.0f,0.0f,1.0f,1.0f}, {'2',L"2",2.0f,0.0f,1.0f,1.0f}, {'3',L"3",3.0f,0.0f,1.0f,1.0f}, {'4',L"4",4.0f,0.0f,1.0f,1.0f}, {'5',L"5",5.0f,0.0f,1.0f,1.0f}, {'6',L"6",6.0f,0.0f,1.0f,1.0f}, {'7',L"7",7.0f,0.0f,1.0f,1.0f}, {'8',L"8",8.0f,0.0f,1.0f,1.0f}, {'9',L"9",9.0f,0.0f,1.0f,1.0f}, {'0',L"0",10.0f,0.0f,1.0f,1.0f}, {VK_OEM_MINUS,L"-",11.0f,0.0f,1.0f,1.0f}, {VK_OEM_PLUS,L"=",12.0f,0.0f,1.0f,1.0f}, {VK_BACK,L"Backspace",13.0f,0.0f,2.0f,1.0f},
    {VK_TAB,L"Tab",0.0f,1.0f,1.5f,1.0f}, {'Q',L"Q",1.5f,1.0f,1.0f,1.0f}, {'W',L"W",2.5f,1.0f,1.0f,1.0f}, {'E',L"E",3.5f,1.0f,1.0f,1.0f}, {'R',L"R",4.5f,1.0f,1.0f,1.0f}, {'T',L"T",5.5f,1.0f,1.0f,1.0f}, {'Y',L"Y",6.5f,1.0f,1.0f,1.0f}, {'U',L"U",7.5f,1.0f,1.0f,1.0f}, {'I',L"I",8.5f,1.0f,1.0f,1.0f}, {'O',L"O",9.5f,1.0f,1.0f,1.0f}, {'P',L"P",10.5f,1.0f,1.0f,1.0f}, {VK_OEM_4,L"[",11.5f,1.0f,1.0f,1.0f}, {VK_OEM_6,L"]",12.5f,1.0f,1.0f,1.0f}, {VK_OEM_5,L"\\",13.5f,1.0f,1.5f,1.0f},
    {VK_CAPITAL,L"Caps",0.0f,2.0f,1.8f,1.0f}, {'A',L"A",1.8f,2.0f,1.0f,1.0f}, {'S',L"S",2.8f,2.0f,1.0f,1.0f}, {'D',L"D",3.8f,2.0f,1.0f,1.0f}, {'F',L"F",4.8f,2.0f,1.0f,1.0f}, {'G',L"G",5.8f,2.0f,1.0f,1.0f}, {'H',L"H",6.8f,2.0f,1.0f,1.0f}, {'J',L"J",7.8f,2.0f,1.0f,1.0f}, {'K',L"K",8.8f,2.0f,1.0f,1.0f}, {'L',L"L",9.8f,2.0f,1.0f,1.0f}, {VK_OEM_1,L";",10.8f,2.0f,1.0f,1.0f}, {VK_OEM_7,L"'",11.8f,2.0f,1.0f,1.0f}, {VK_RETURN,L"Enter",12.8f,2.0f,2.2f,1.0f},
    {VK_LSHIFT,L"Shift",0.0f,3.0f,2.3f,1.0f}, {'Z',L"Z",2.3f,3.0f,1.0f,1.0f}, {'X',L"X",3.3f,3.0f,1.0f,1.0f}, {'C',L"C",4.3f,3.0f,1.0f,1.0f}, {'V',L"V",5.3f,3.0f,1.0f,1.0f}, {'B',L"B",6.3f,3.0f,1.0f,1.0f}, {'N',L"N",7.3f,3.0f,1.0f,1.0f}, {'M',L"M",8.3f,3.0f,1.0f,1.0f}, {VK_OEM_COMMA,L",",9.3f,3.0f,1.0f,1.0f}, {VK_OEM_PERIOD,L".",10.3f,3.0f,1.0f,1.0f}, {VK_OEM_2,L"/",11.3f,3.0f,1.0f,1.0f}, {VK_RSHIFT,L"Shift",12.3f,3.0f,2.7f,1.0f},
    {VK_LCONTROL,L"Ctrl",0.0f,4.0f,1.2f,1.0f}, {255,L"Fn",1.2f,4.0f,1.0f,1.0f}, {VK_LWIN,L"Win",2.2f,4.0f,1.2f,1.0f}, {VK_LMENU,L"Alt",3.4f,4.0f,1.2f,1.0f}, {VK_SPACE,L"",4.6f,4.0f,5.4f,1.0f}, {VK_RCONTROL,L"Ctrl",10.0f,4.0f,1.0f,1.0f}, {VK_RMENU,L"Alt",11.0f,4.0f,1.0f,1.0f}, {VK_LEFT,L"\u25C0",12.0f,4.0f,1.0f,1.0f}, {VK_UP,L"\u25B2",13.0f,4.0f,1.0f,0.5f}, {VK_DOWN,L"\u25BC",13.0f,4.5f,1.0f,0.5f}, {VK_RIGHT,L"\u25B6",14.0f,4.0f,1.0f,1.0f}
};

static KDef layout105[] = {
    {VK_ESCAPE,L"Esc",0.0f,0.0f,1.0f,1.0f}, {VK_F1,L"F1",1.5f,0.0f,1.0f,1.0f}, {VK_F2,L"F2",2.5f,0.0f,1.0f,1.0f}, {VK_F3,L"F3",3.5f,0.0f,1.0f,1.0f}, {VK_F4,L"F4",4.5f,0.0f,1.0f,1.0f}, {VK_F5,L"F5",6.0f,0.0f,1.0f,1.0f}, {VK_F6,L"F6",7.0f,0.0f,1.0f,1.0f}, {VK_F7,L"F7",8.0f,0.0f,1.0f,1.0f}, {VK_F8,L"F8",9.0f,0.0f,1.0f,1.0f}, {VK_F9,L"F9",10.5f,0.0f,1.0f,1.0f}, {VK_F10,L"F10",11.5f,0.0f,1.0f,1.0f}, {VK_F11,L"F11",12.5f,0.0f,1.0f,1.0f}, {VK_F12,L"F12",13.5f,0.0f,1.0f,1.0f}, {VK_SNAPSHOT,L"PrtSc",15.5f,0.0f,1.0f,1.0f}, {VK_SCROLL,L"ScrLk",16.5f,0.0f,1.0f,1.0f}, {VK_PAUSE,L"Pause",17.5f,0.0f,1.0f,1.0f},
    {VK_OEM_3,L"~",0.0f,1.0f,1.0f,1.0f}, {'1',L"1",1.0f,1.0f,1.0f,1.0f}, {'2',L"2",2.0f,1.0f,1.0f,1.0f}, {'3',L"3",3.0f,1.0f,1.0f,1.0f}, {'4',L"4",4.0f,1.0f,1.0f,1.0f}, {'5',L"5",5.0f,1.0f,1.0f,1.0f}, {'6',L"6",6.0f,1.0f,1.0f,1.0f}, {'7',L"7",7.0f,1.0f,1.0f,1.0f}, {'8',L"8",8.0f,1.0f,1.0f,1.0f}, {'9',L"9",9.0f,1.0f,1.0f,1.0f}, {'0',L"0",10.0f,1.0f,1.0f,1.0f}, {VK_OEM_MINUS,L"-",11.0f,1.0f,1.0f,1.0f}, {VK_OEM_PLUS,L"=",12.0f,1.0f,1.0f,1.0f}, {VK_BACK,L"Backspace",13.0f,1.0f,2.0f,1.0f}, {VK_INSERT,L"Ins",15.5f,1.0f,1.0f,1.0f}, {VK_HOME,L"Home",16.5f,1.0f,1.0f,1.0f}, {VK_PRIOR,L"PgUp",17.5f,1.0f,1.0f,1.0f}, {VK_NUMLOCK,L"Num",19.0f,1.0f,1.0f,1.0f}, {VK_DIVIDE,L"/",20.0f,1.0f,1.0f,1.0f}, {VK_MULTIPLY,L"*",21.0f,1.0f,1.0f,1.0f}, {VK_SUBTRACT,L"-",22.0f,1.0f,1.0f,1.0f},
    {VK_TAB,L"Tab",0.0f,2.0f,1.5f,1.0f}, {'Q',L"Q",1.5f,2.0f,1.0f,1.0f}, {'W',L"W",2.5f,2.0f,1.0f,1.0f}, {'E',L"E",3.5f,2.0f,1.0f,1.0f}, {'R',L"R",4.5f,2.0f,1.0f,1.0f}, {'T',L"T",5.5f,2.0f,1.0f,1.0f}, {'Y',L"Y",6.5f,2.0f,1.0f,1.0f}, {'U',L"U",7.5f,2.0f,1.0f,1.0f}, {'I',L"I",8.5f,2.0f,1.0f,1.0f}, {'O',L"O",9.5f,2.0f,1.0f,1.0f}, {'P',L"P",10.5f,2.0f,1.0f,1.0f}, {VK_OEM_4,L"[",11.5f,2.0f,1.0f,1.0f}, {VK_OEM_6,L"]",12.5f,2.0f,1.0f,1.0f}, {VK_OEM_5,L"\\",13.5f,2.0f,1.5f,1.0f}, {VK_DELETE,L"Del",15.5f,2.0f,1.0f,1.0f}, {VK_END,L"End",16.5f,2.0f,1.0f,1.0f}, {VK_NEXT,L"PgDn",17.5f,2.0f,1.0f,1.0f}, {VK_NUMPAD7,L"7",19.0f,2.0f,1.0f,1.0f}, {VK_NUMPAD8,L"8",20.0f,2.0f,1.0f,1.0f}, {VK_NUMPAD9,L"9",21.0f,2.0f,1.0f,1.0f}, {VK_ADD,L"+",22.0f,2.0f,1.0f,2.0f},
    {VK_CAPITAL,L"Caps",0.0f,3.0f,1.8f,1.0f}, {'A',L"A",1.8f,3.0f,1.0f,1.0f}, {'S',L"S",2.8f,3.0f,1.0f,1.0f}, {'D',L"D",3.8f,3.0f,1.0f,1.0f}, {'F',L"F",4.8f,3.0f,1.0f,1.0f}, {'G',L"G",5.8f,3.0f,1.0f,1.0f}, {'H',L"H",6.8f,3.0f,1.0f,1.0f}, {'J',L"J",7.8f,3.0f,1.0f,1.0f}, {'K',L"K",8.8f,3.0f,1.0f,1.0f}, {'L',L"L",9.8f,3.0f,1.0f,1.0f}, {VK_OEM_1,L";",10.8f,3.0f,1.0f,1.0f}, {VK_OEM_7,L"'",11.8f,3.0f,1.0f,1.0f}, {VK_RETURN,L"Enter",12.8f,3.0f,2.2f,1.0f}, {VK_NUMPAD4,L"4",19.0f,3.0f,1.0f,1.0f}, {VK_NUMPAD5,L"5",20.0f,3.0f,1.0f,1.0f}, {VK_NUMPAD6,L"6",21.0f,3.0f,1.0f,1.0f},
    {VK_LSHIFT,L"Shift",0.0f,4.0f,2.3f,1.0f}, {'Z',L"Z",2.3f,4.0f,1.0f,1.0f}, {'X',L"X",3.3f,4.0f,1.0f,1.0f}, {'C',L"C",4.3f,4.0f,1.0f,1.0f}, {'V',L"V",5.3f,4.0f,1.0f,1.0f}, {'B',L"B",6.3f,4.0f,1.0f,1.0f}, {'N',L"N",7.3f,4.0f,1.0f,1.0f}, {'M',L"M",8.3f,4.0f,1.0f,1.0f}, {VK_OEM_COMMA,L",",9.3f,4.0f,1.0f,1.0f}, {VK_OEM_PERIOD,L".",10.3f,4.0f,1.0f,1.0f}, {VK_OEM_2,L"/",11.3f,4.0f,1.0f,1.0f}, {VK_RSHIFT,L"Shift",12.3f,4.0f,2.7f,1.0f}, {VK_UP,L"\u25B2",16.5f,4.0f,1.0f,1.0f}, {VK_NUMPAD1,L"1",19.0f,4.0f,1.0f,1.0f}, {VK_NUMPAD2,L"2",20.0f,4.0f,1.0f,1.0f}, {VK_NUMPAD3,L"3",21.0f,4.0f,1.0f,1.0f}, {VK_RETURN,L"Ent",22.0f,4.0f,1.0f,2.0f},
    {VK_LCONTROL,L"Ctrl",0.0f,5.0f,1.5f,1.0f}, {VK_LWIN,L"Win",1.5f,5.0f,1.5f,1.0f}, {VK_LMENU,L"Alt",3.0f,5.0f,1.5f,1.0f}, {VK_SPACE,L"",4.5f,5.0f,6.0f,1.0f}, {VK_RMENU,L"Alt",10.5f,5.0f,1.5f,1.0f}, {VK_RWIN,L"Win",12.0f,5.0f,1.5f,1.0f}, {VK_APPS,L"\u2261",13.5f,5.0f,1.5f,1.0f}, {VK_LEFT,L"\u25C0",15.5f,5.0f,1.0f,1.0f}, {VK_DOWN,L"\u25BC",16.5f,5.0f,1.0f,1.0f}, {VK_RIGHT,L"\u25B6",17.5f,5.0f,1.0f,1.0f}, {VK_NUMPAD0,L"0",19.0f,5.0f,2.0f,1.0f}, {VK_DECIMAL,L".",21.0f,5.0f,1.0f,1.0f}
};

static void UpdateLayout(int, int);

static void ApplyEffects() {
    DWORD borderColor = 0xFFFFFFFE;
    DwmSetWindowAttribute(g_hwnd, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
    int dark = g_isDark ? 1 : 0;
    DwmSetWindowAttribute(g_hwnd, 20, &dark, sizeof(dark));
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(g_hwnd, &margins);

    int backdrop = g_effectType == 0 ? DWMSBT_MAINWINDOW : (g_effectType == 1 ? DWMSBT_TRANSIENTWINDOW : 1);
    DwmSetWindowAttribute(g_hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    SetLayeredWindowAttributes(g_hwnd, 0, (BYTE)(g_windowOpacity * 2.55f), LWA_ALPHA);
}

static void SaveSettings() {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Win11OSK", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = g_isDark ? 1 : 0; RegSetValueExW(hKey, L"IsDark", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = g_effectType; RegSetValueExW(hKey, L"Effect", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = g_layoutType; RegSetValueExW(hKey, L"Layout", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = g_customColor; RegSetValueExW(hKey, L"Color", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = g_useCustomColor ? 1 : 0; RegSetValueExW(hKey, L"UseColor", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = (DWORD)g_windowOpacity; RegSetValueExW(hKey, L"WindowOpacity", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = g_textOpaque ? 1 : 0; RegSetValueExW(hKey, L"TextOpaque", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = g_topMost ? 1 : 0; RegSetValueExW(hKey, L"TopMost", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = g_hideFromTaskbar ? 1 : 0; RegSetValueExW(hKey, L"HideFromTaskbar", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = g_enableMouseTyping ? 1 : 0; RegSetValueExW(hKey, L"MouseTyping", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = g_mouseClickThrough ? 1 : 0; RegSetValueExW(hKey, L"MouseClickThrough", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = g_showSettingsBtn ? 1 : 0; RegSetValueExW(hKey, L"ShowSettingsBtn", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        val = g_showHomingKeys ? 1 : 0; RegSetValueExW(hKey, L"ShowHomingKeys", 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

static void LoadSettings() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Win11OSK", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size = sizeof(DWORD), val = 0;
        if (RegQueryValueExW(hKey, L"IsDark", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_isDark = (val != 0);
        if (RegQueryValueExW(hKey, L"Effect", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_effectType = val;
        if (RegQueryValueExW(hKey, L"Layout", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_layoutType = val;
        if (RegQueryValueExW(hKey, L"Color", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_customColor = val;
        if (RegQueryValueExW(hKey, L"UseColor", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_useCustomColor = (val != 0);
        if (RegQueryValueExW(hKey, L"WindowOpacity", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_windowOpacity = val;
        if (RegQueryValueExW(hKey, L"TextOpaque", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_textOpaque = (val != 0);
        if (RegQueryValueExW(hKey, L"TopMost", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_topMost = (val != 0);
        if (RegQueryValueExW(hKey, L"HideFromTaskbar", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_hideFromTaskbar = (val != 0);
        if (RegQueryValueExW(hKey, L"MouseTyping", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_enableMouseTyping = (val != 0);
        if (RegQueryValueExW(hKey, L"MouseClickThrough", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_mouseClickThrough = (val != 0);
        if (RegQueryValueExW(hKey, L"ShowSettingsBtn", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_showSettingsBtn = (val != 0);
        if (RegQueryValueExW(hKey, L"ShowHomingKeys", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) g_showHomingKeys = (val != 0);
        RegCloseKey(hKey);
    }
}

static UINT_PTR CALLBACK CCHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
    if (uiMsg == WM_INITDIALOG) {
        RECT rc; GetWindowRect(hdlg, &rc);
        int width = rc.right - rc.left; int height = rc.bottom - rc.top;
        SetWindowPos(hdlg, NULL, 0, 0, width, height + 125, SWP_NOMOVE | SWP_NOZORDER);
        RECT clientRc; GetClientRect(hdlg, &clientRc);
        int oldBottom = clientRc.bottom - 125;
        HFONT hFont = (HFONT)SendMessage(hdlg, WM_GETFONT, 0, 0);

        HWND hLabel = CreateWindowExW(0, L"STATIC", L"全局窗口透明度 (10% - 100%):", WS_CHILD | WS_VISIBLE, 10, oldBottom + 10, 250, 20, hdlg, NULL, NULL, NULL);
        SendMessage(hLabel, WM_SETFONT, (WPARAM)hFont, FALSE);

        HWND hSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_BOTTOM, 5, oldBottom + 30, width - 150, 30, hdlg, (HMENU)9001, NULL, NULL);
        SendMessage(hSlider, TBM_SETRANGE, TRUE, MAKELPARAM(10, 100));
        SendMessage(hSlider, TBM_SETPOS, TRUE, g_windowOpacity);

        HWND hCheck = CreateWindowExW(0, L"BUTTON", L"透明度不对文字生效 (相对增强)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, oldBottom + 65, 240, 20, hdlg, (HMENU)9003, NULL, NULL);
        SendMessage(hCheck, WM_SETFONT, (WPARAM)hFont, FALSE);
        SendMessage(hCheck, BM_SETCHECK, g_textOpaque ? BST_CHECKED : BST_UNCHECKED, 0);

        HWND hCheckHoming = CreateWindowExW(0, L"BUTTON", L"显示 F/J 盲打定位键", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, oldBottom + 90, 240, 20, hdlg, (HMENU)9004, NULL, NULL);
        SendMessage(hCheckHoming, WM_SETFONT, (WPARAM)hFont, FALSE);
        SendMessage(hCheckHoming, BM_SETCHECK, g_showHomingKeys ? BST_CHECKED : BST_UNCHECKED, 0);

        HWND hBtn = CreateWindowExW(0, L"BUTTON", L"恢复默认主题", WS_CHILD | WS_VISIBLE, width - 130, oldBottom + 75, 110, 30, hdlg, (HMENU)9002, NULL, NULL);
        SendMessage(hBtn, WM_SETFONT, (WPARAM)hFont, FALSE);

        SetTimer(hdlg, 1, 30, NULL);
        return TRUE;
    }
    else if (uiMsg == WM_HSCROLL) {
        HWND hCtrl = (HWND)lParam;
        if (hCtrl && GetDlgCtrlID(hCtrl) == 9001) {
            g_windowOpacity = static_cast<int>(SendMessage(hCtrl, TBM_GETPOS, 0, 0));
            ApplyEffects();
            InvalidateRect(g_hwnd, NULL, FALSE);
        }
    }
    else if (uiMsg == WM_COMMAND) {
        if (LOWORD(wParam) == 9003) {
            g_textOpaque = (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
            InvalidateRect(g_hwnd, NULL, FALSE);
        }
        else if (LOWORD(wParam) == 9004) {
            g_showHomingKeys = (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
            InvalidateRect(g_hwnd, NULL, FALSE);
        }
        else if (LOWORD(wParam) == 9002) {
            KillTimer(hdlg, 1);
            g_isDark = true; g_effectType = 2; g_layoutType = 0; g_useCustomColor = false; g_customColor = 0x000000;
            g_windowOpacity = 100; g_textOpaque = false; g_showHomingKeys = false;
            g_topMost = true; g_hideFromTaskbar = false; g_enableMouseTyping = false; g_mouseClickThrough = false; g_showSettingsBtn = true;
            g_restoredDefault = true;

            SendMessage(GetDlgItem(hdlg, 9001), TBM_SETPOS, TRUE, 100);
            SendMessage(GetDlgItem(hdlg, 9003), BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessage(GetDlgItem(hdlg, 9004), BM_SETCHECK, BST_UNCHECKED, 0);

            ApplyEffects();
            LONG_PTR exStyle = GetWindowLongPtr(g_hwnd, GWL_EXSTYLE);
            exStyle &= ~WS_EX_TOOLWINDOW; SetWindowLongPtr(g_hwnd, GWL_EXSTYLE, exStyle);
            SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

            RECT rc = { 0 }; GetClientRect(g_hwnd, &rc); UpdateLayout(rc.right, rc.bottom);
            InvalidateRect(g_hwnd, NULL, FALSE);
            PostMessage(hdlg, WM_COMMAND, IDCANCEL, 0);
        }
    }
    else if (uiMsg == WM_TIMER) {
        if (!g_restoredDefault) {
            BOOL bR = FALSE, bG = FALSE, bB = FALSE;
            UINT r = GetDlgItemInt(hdlg, 726, &bR, FALSE); 
            UINT g = GetDlgItemInt(hdlg, 727, &bG, FALSE); 
            UINT b = GetDlgItemInt(hdlg, 728, &bB, FALSE);
            if (bR && bG && bB) {
                DWORD newColor = RGB(r, g, b);
                if (g_customColor != newColor || !g_useCustomColor) {
                    g_customColor = newColor; g_useCustomColor = true; InvalidateRect(g_hwnd, NULL, FALSE);
                }
            }
        }
    }
    else if (uiMsg == WM_DESTROY) { KillTimer(hdlg, 1); }
    return 0;
}

static void OpenColorPicker() {
    DWORD backupColor = g_customColor; bool backupUseColor = g_useCustomColor;
    int backupOpacity = g_windowOpacity; bool backupTextOpaque = g_textOpaque;
    bool backupHoming = g_showHomingKeys;
    g_restoredDefault = false;

    CHOOSECOLOR cc = { 0 }; cc.lStructSize = sizeof(CHOOSECOLOR);
    static COLORREF custColors[16] = { 0 }; cc.hwndOwner = g_hwnd; cc.lpCustColors = custColors; cc.rgbResult = g_customColor;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ENABLEHOOK | CC_ANYCOLOR; cc.lpfnHook = CCHookProc;

    if (ChooseColor(&cc)) {
        g_customColor = cc.rgbResult; g_useCustomColor = true; SaveSettings();
    }
    else {
        if (g_restoredDefault) SaveSettings();
        else {
            g_customColor = backupColor; g_useCustomColor = backupUseColor;
            g_windowOpacity = backupOpacity; g_textOpaque = backupTextOpaque;
            g_showHomingKeys = backupHoming;
            ApplyEffects(); InvalidateRect(g_hwnd, NULL, FALSE);
        }
    }
}

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (p->vkCode < 256) {
            bool down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            bool up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
            if (down || up) {
                if (g_keyStates[p->vkCode] != down) {
                    g_keyStates[p->vkCode] = down;
                    InvalidateRect(g_hwnd, NULL, FALSE);
                }
            }
        }
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

static void UpdateTextFormat(float keyHeight) {
    if (g_pTextFormat) { g_pTextFormat->Release(); g_pTextFormat = nullptr; }
    float fontSize = max(8.0f, keyHeight * 0.35f);
    g_pDWriteFactory->CreateTextFormat(L"Segoe UI Variable Display", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"en-us", &g_pTextFormat);
    if (g_pTextFormat) {
        g_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
}

static void UpdateLayout(int clientW, int clientH) {
    g_renderKeys.clear();
    float gridW = g_layoutType == 0 ? 15.0f : 23.0f;
    float gridH = g_layoutType == 0 ? 5.0f : 6.0f;

    float titleHeight = g_showSettingsBtn ? 35.0f : 0.0f;
    float padding = 8.0f;
    float w = (clientW - padding) / gridW; float h = (clientH - titleHeight - padding) / gridH;

    KDef* layout = g_layoutType == 0 ? layout78 : layout105;
    int count = g_layoutType == 0 ? (sizeof(layout78) / sizeof(layout78[0])) : (sizeof(layout105) / sizeof(layout105[0]));

    for (int i = 0; i < count; i++) {
        RenderKey rk; rk.vk = layout[i].vk; rk.lbl = layout[i].lbl;
        rk.rect.left = padding + layout[i].x * w; rk.rect.top = titleHeight + padding + layout[i].y * h;
        rk.rect.right = rk.rect.left + layout[i].w * w - padding; rk.rect.bottom = rk.rect.top + layout[i].h * h - padding;
        g_renderKeys.push_back(rk);
    }
    UpdateTextFormat(h);
}

static void DrawStandardKey(D2D1_RECT_F rect, bool highlight, float bgAlpha) {
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 4.0f, 4.0f);
    float fillA = highlight ? (g_isDark ? 0.15f : 0.7f) : (g_isDark ? 0.05f : 0.4f);
    ID2D1SolidColorBrush* brush = nullptr;
    
    g_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, fillA * bgAlpha), &brush);
    if (brush) { 
        g_pRenderTarget->FillRoundedRectangle(rr, brush);
        brush->Release(); brush = nullptr; 
    }

    float borderA = g_effectType == 1 ? (g_isDark ? 0.2f : 0.3f) : (g_isDark ? 0.1f : 0.2f);
    g_pRenderTarget->CreateSolidColorBrush(g_isDark ? D2D1::ColorF(1.0f, 1.0f, 1.0f, borderA * bgAlpha) : D2D1::ColorF(0.0f, 0.0f, 0.0f, borderA * bgAlpha), &brush);
    if (brush) {
        g_pRenderTarget->DrawRoundedRectangle(rr, brush, 1.0f);
        brush->Release();
    }
}

static void DrawLiquidKey(D2D1_RECT_F rect, bool highlight, float bgAlpha) {
    ID2D1SolidColorBrush* sb = nullptr;
    D2D1_ROUNDED_RECT shadow = D2D1::RoundedRect(D2D1::RectF(rect.left + 1.0f, rect.top + 3.0f, rect.right + 1.0f, rect.bottom + 3.0f), 6.0f, 6.0f);
    
    g_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, (g_isDark ? 0.5f : 0.15f) * bgAlpha), &sb);
    if (sb) {
        g_pRenderTarget->FillRoundedRectangle(shadow, sb);
        sb->Release(); sb = nullptr;
    }

    ID2D1LinearGradientBrush* gb = nullptr; ID2D1GradientStopCollection* stops = nullptr; D2D1_GRADIENT_STOP gs[3] = { 0 };
    if (highlight) {
        float a1 = g_isDark ? 0.8f : 0.9f; float a2 = g_isDark ? 0.5f : 0.6f; float a3 = g_isDark ? 0.2f : 0.3f;
        gs[0] = { 0.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, a1 * bgAlpha) }; gs[1] = { 0.5f, D2D1::ColorF(1.0f, 1.0f, 1.0f, a2 * bgAlpha) }; gs[2] = { 1.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, a3 * bgAlpha) };
    }
    else {
        float a1 = g_isDark ? 0.4f : 0.7f; float a2 = g_isDark ? 0.15f : 0.4f; float a3 = g_isDark ? 0.05f : 0.1f;
        gs[0] = { 0.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, a1 * bgAlpha) }; gs[1] = { 0.5f, D2D1::ColorF(1.0f, 1.0f, 1.0f, a2 * bgAlpha) }; gs[2] = { 1.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, a3 * bgAlpha) };
    }
    g_pRenderTarget->CreateGradientStopCollection(gs, 3, &stops);
    if (stops) {
        g_pRenderTarget->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(rect.left, rect.top), D2D1::Point2F(rect.left, rect.bottom)), stops, &gb);
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 6.0f, 6.0f);
        if (gb) { g_pRenderTarget->FillRoundedRectangle(rr, gb); gb->Release(); } stops->Release();
        
        ID2D1SolidColorBrush* borderBrush = nullptr;
        g_pRenderTarget->CreateSolidColorBrush(g_isDark ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.3f * bgAlpha) : D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.15f * bgAlpha), &borderBrush);
        if (borderBrush) { g_pRenderTarget->DrawRoundedRectangle(rr, borderBrush, 1.2f); borderBrush->Release(); }
    }
}

static void OnRender() {
    if (!g_pRenderTarget) {
        RECT rc = { 0 }; GetClientRect(g_hwnd, &rc);
        g_pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f),
            D2D1::HwndRenderTargetProperties(g_hwnd, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)), &g_pRenderTarget);
    }
    g_pRenderTarget->BeginDraw(); g_pRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    float globalAlpha = g_windowOpacity / 100.0f;
    float bgAlpha = 1.0f;
    if (g_textOpaque && globalAlpha < 1.0f) bgAlpha = globalAlpha;

    ID2D1SolidColorBrush* bgBrush = nullptr;
    if (g_useCustomColor) {
        float r = GetRValue(g_customColor) / 255.0f; float g = GetGValue(g_customColor) / 255.0f; float b = GetBValue(g_customColor) / 255.0f;
        g_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(r, g, b, 0.4f * bgAlpha), &bgBrush);
    }
    else {
        if (g_effectType == 2) g_pRenderTarget->CreateSolidColorBrush(g_isDark ? D2D1::ColorF(0.1f, 0.1f, 0.15f, 0.6f * bgAlpha) : D2D1::ColorF(0.9f, 0.9f, 0.95f, 0.6f * bgAlpha), &bgBrush);
        else g_pRenderTarget->CreateSolidColorBrush(g_isDark ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.3f * bgAlpha) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.3f * bgAlpha), &bgBrush);
    }
    if (bgBrush) {
        D2D1_SIZE_F size = g_pRenderTarget->GetSize(); g_pRenderTarget->FillRectangle(D2D1::RectF(0.0f, 0.0f, size.width, size.height), bgBrush);
        bgBrush->Release();
    }

    ID2D1SolidColorBrush* textBrush = nullptr;
    g_pRenderTarget->CreateSolidColorBrush(g_isDark ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f) : D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), &textBrush);

    if (textBrush) {
        for (const auto& key : g_renderKeys) {
            bool isHighlight = g_keyStates[key.vk] || (g_pressedKey == key.vk);

            if (key.vk == 255) {
                for (int f = VK_F1; f <= VK_F12; ++f) {
                    if (g_keyStates[f] || g_pressedKey == f) { isHighlight = true; break; }
                }
            }
            else if (key.vk >= '1' && key.vk <= '9') {
                int f_key = VK_F1 + (key.vk - '1');
                if (g_keyStates[f_key] || g_pressedKey == f_key) isHighlight = true;
            }
            else if (key.vk == '0') {
                if (g_keyStates[VK_F10] || g_pressedKey == VK_F10) isHighlight = true;
            }
            else if (key.vk == VK_OEM_MINUS) {
                if (g_keyStates[VK_F11] || g_pressedKey == VK_F11) isHighlight = true;
            }
            else if (key.vk == VK_OEM_PLUS) {
                if (g_keyStates[VK_F12] || g_pressedKey == VK_F12) isHighlight = true;
            }

            if (g_effectType == 2) DrawLiquidKey(key.rect, isHighlight, bgAlpha);
            else DrawStandardKey(key.rect, isHighlight, bgAlpha);

            if (g_pTextFormat) {
                if (g_textOpaque && globalAlpha < 1.0f) {
                    ID2D1SolidColorBrush* shadowBrush = nullptr;
                    g_pRenderTarget->CreateSolidColorBrush(g_isDark ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.8f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f), &shadowBrush);
                    if (shadowBrush) {
                        D2D1_RECT_F sRect = key.rect;
                        sRect.left += 1.5f; sRect.top += 1.5f; sRect.right += 1.5f; sRect.bottom += 1.5f;
                        g_pRenderTarget->DrawTextW(key.lbl.c_str(), static_cast<UINT32>(key.lbl.length()), g_pTextFormat, sRect, shadowBrush);
                        shadowBrush->Release();
                    }
                }
                g_pRenderTarget->DrawTextW(key.lbl.c_str(), static_cast<UINT32>(key.lbl.length()), g_pTextFormat, key.rect, textBrush);
            }
            
            if (g_showHomingKeys && (key.vk == 'F' || key.vk == 'J')) {
                float barW = 12.0f; float barH = 2.0f;
                float cx = (key.rect.left + key.rect.right) / 2.0f; float cy = key.rect.bottom - 8.0f;
                D2D1_RECT_F barRect = { cx - barW / 2.0f, cy - barH, cx + barW / 2.0f, cy };

                if (g_textOpaque && globalAlpha < 1.0f) {
                    ID2D1SolidColorBrush* shadowBrush = nullptr;
                    g_pRenderTarget->CreateSolidColorBrush(g_isDark ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.8f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f), &shadowBrush);
                    if (shadowBrush) {
                        D2D1_RECT_F sRect = barRect;
                        sRect.left += 1.5f; sRect.top += 1.5f; sRect.right += 1.5f; sRect.bottom += 1.5f;
                        g_pRenderTarget->FillRectangle(sRect, shadowBrush);
                        shadowBrush->Release();
                    }
                }
                
                ID2D1SolidColorBrush* homingBrush = nullptr;
                g_pRenderTarget->CreateSolidColorBrush(g_isDark ? D2D1::ColorF(0.75f, 0.75f, 0.75f, 1.0f) : D2D1::ColorF(0.35f, 0.35f, 0.35f, 1.0f), &homingBrush);
                if (homingBrush) {
                    g_pRenderTarget->FillRectangle(barRect, homingBrush);
                    homingBrush->Release();
                }
            }
        }

        if (g_showSettingsBtn && g_pTextFormat) {
            D2D1_SIZE_F rtSize = g_pRenderTarget->GetSize();
            D2D1_RECT_F setRect = { rtSize.width - 40.0f, 0.0f, rtSize.width, 35.0f };
            g_pRenderTarget->DrawTextW(L"\u2699", 1, g_pTextFormat, setRect, textBrush);
        }
        textBrush->Release();
    }
    
    g_pRenderTarget->EndDraw();
}

static void ShowSettingsMenu(POINT pt) {
    HMENU hMenu = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING | (g_showSettingsBtn ? MF_CHECKED : 0), 111, L"显示设置按钮");
    AppendMenuW(hMenu, MF_STRING | (g_enableMouseTyping ? MF_CHECKED : 0), 110, L"允许鼠标打字输入");

    if (!g_enableMouseTyping) {
        AppendMenuW(hMenu, MF_STRING | (g_mouseClickThrough ? MF_CHECKED : 0), 112, L"鼠标穿透 (无视点击)");
    }

    AppendMenuW(hMenu, MF_STRING | (g_topMost ? MF_CHECKED : 0), 106, L"窗口保持置顶");
    AppendMenuW(hMenu, MF_STRING | (g_hideFromTaskbar ? MF_CHECKED : 0), 109, L"不在任务栏中显示");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(hMenu, MF_STRING, 101, g_isDark ? L"切换至: 浅色模式" : L"切换至: 深色模式");
    HMENU hEffectMenu = CreatePopupMenu();
    AppendMenuW(hEffectMenu, MF_STRING | (g_effectType == 0 ? MF_CHECKED : 0), 102, L"Mica (云母)");
    AppendMenuW(hEffectMenu, MF_STRING | (g_effectType == 1 ? MF_CHECKED : 0), 103, L"Acrylic (亚克力)");
    AppendMenuW(hEffectMenu, MF_STRING | (g_effectType == 2 ? MF_CHECKED : 0), 104, L"Liquid Glass (液态玻璃)");
    AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hEffectMenu), L"材质引擎");
    AppendMenuW(hMenu, MF_STRING, 105, g_layoutType == 0 ? L"切换布局: 105键全键盘" : L"切换布局: 78键笔记本");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 107, L"自定义底色与透明度...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 108, L"退出");

    SetForegroundWindow(g_hwnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(hMenu);

    switch (cmd) {
    case 101: g_isDark = !g_isDark; ApplyEffects(); break;
    case 102: g_effectType = 0; ApplyEffects(); break;
    case 103: g_effectType = 1; ApplyEffects(); break;
    case 104: g_effectType = 2; ApplyEffects(); break;
    case 105: {
        g_layoutType = g_layoutType == 0 ? 1 : 0;
        RECT rc = { 0 }; GetClientRect(g_hwnd, &rc); UpdateLayout(rc.right, rc.bottom); break;
    }
    case 106: {
        g_topMost = !g_topMost;
        SetWindowPos(g_hwnd, g_topMost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); break;
    }
    case 109: {
        g_hideFromTaskbar = !g_hideFromTaskbar;
        LONG_PTR exStyle = GetWindowLongPtr(g_hwnd, GWL_EXSTYLE);
        if (g_hideFromTaskbar) exStyle |= WS_EX_TOOLWINDOW; else exStyle &= ~WS_EX_TOOLWINDOW;
        SetWindowLongPtr(g_hwnd, GWL_EXSTYLE, exStyle); break;
    }
    case 110: { g_enableMouseTyping = !g_enableMouseTyping; break; }
    case 111: {
        g_showSettingsBtn = !g_showSettingsBtn;
        RECT rc = { 0 }; GetClientRect(g_hwnd, &rc); UpdateLayout(rc.right, rc.bottom); break;
    }
    case 112: { g_mouseClickThrough = !g_mouseClickThrough; break; }
    case 107: OpenColorPicker(); break;
    case 108: DestroyWindow(g_hwnd); return;
    }
    SaveSettings(); InvalidateRect(g_hwnd, NULL, FALSE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd; LoadSettings(); ApplyEffects();
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory);
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&g_pDWriteFactory));
        g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);

        NOTIFYICONDATA nid = { 0 }; nid.cbSize = sizeof(NOTIFYICONDATA); nid.hWnd = hwnd; nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; nid.uCallbackMessage = WM_TRAYICON;
        
        // 尝试加载用户添加的图标资源
        nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
        if (!nid.hIcon) nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); // 兜底保护
        
        lstrcpy(nid.szTip, L"Win11 屏幕键盘");
        Shell_NotifyIcon(NIM_ADD, &nid);

        SetTimer(hwnd, 2, 50, NULL);
        break;
    }
    case WM_TIMER: {
        if (wParam == 2) {
            POINT pt; GetCursorPos(&pt); RECT rc; GetWindowRect(hwnd, &rc);
            LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

            bool isHovering = PtInRect(&rc, pt);
            float titleH = g_showSettingsBtn ? 35.0f : 0.0f;
            int localY = pt.y - rc.top;

            bool shouldBeTransparent = !g_enableMouseTyping && g_mouseClickThrough && (!isHovering || localY >= titleH);

            if (shouldBeTransparent) {
                if (!(exStyle & WS_EX_TRANSPARENT)) SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
            }
            else {
                if (exStyle & WS_EX_TRANSPARENT) SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
            }
        }
        break;
    }
    case WM_TRAYICON: {
        if (lParam == WM_LBUTTONUP) {
            if (IsWindowVisible(hwnd)) { ShowWindow(hwnd, SW_HIDE); }
            else { ShowWindow(hwnd, SW_SHOW); SetForegroundWindow(hwnd); }
        }
        else if (lParam == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt); SetForegroundWindow(hwnd); ShowSettingsMenu(pt);
        }
        break;
    }
    case WM_NCACTIVATE: {
        LRESULT res = DefWindowProc(hwnd, msg, TRUE, lParam); InvalidateRect(hwnd, NULL, FALSE); return res;
    }
    case WM_NCPAINT: return 0;
    case WM_NCCALCSIZE: if (wParam == TRUE) return 0; break;
    case WM_SIZE:
        if (g_pRenderTarget) g_pRenderTarget->Resize(D2D1::SizeU(LOWORD(lParam), HIWORD(lParam)));
        UpdateLayout(LOWORD(lParam), HIWORD(lParam)); InvalidateRect(hwnd, NULL, FALSE); break;
    case WM_PAINT: {
        PAINTSTRUCT ps = { 0 }; BeginPaint(hwnd, &ps); OnRender(); EndPaint(hwnd, &ps); return 0;
    }
    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProc(hwnd, msg, wParam, lParam);
        if (hit == HTCLIENT) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; ScreenToClient(hwnd, &pt); RECT rc = { 0 }; GetClientRect(hwnd, &rc);

            float titleHeight = g_showSettingsBtn ? 35.0f : 0.0f;
            if (g_showSettingsBtn && pt.x > rc.right - 40 && pt.y < 35) return HTCLIENT;

            int border = 10;
            if (pt.y < titleHeight) {
                if (pt.x < border && pt.y < border) return HTTOPLEFT;
                if (pt.x > rc.right - border && pt.y < border) return HTTOPRIGHT;
                if (pt.x < border) return HTLEFT;
                if (pt.x > rc.right - border) return HTRIGHT;
                if (pt.y < border) return HTTOP;
                return HTCAPTION;
            }

            if (g_enableMouseTyping) return HTCLIENT;

            if (g_mouseClickThrough) return HTTRANSPARENT;

            if (pt.x < border && pt.y > rc.bottom - border) return HTBOTTOMLEFT;
            if (pt.x > rc.right - border && pt.y > rc.bottom - border) return HTBOTTOMRIGHT;
            if (pt.x < border) return HTLEFT;
            if (pt.x > rc.right - border) return HTRIGHT;
            if (pt.y > rc.bottom - border) return HTBOTTOM;

            return HTCAPTION;
        }
        return hit;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; RECT rc = { 0 }; GetClientRect(hwnd, &rc);
        if (g_showSettingsBtn && pt.x > rc.right - 40 && pt.y < 35) { ClientToScreen(hwnd, &pt); ShowSettingsMenu(pt); return 0; }

        for (const auto& key : g_renderKeys) {
            if (key.vk != 255 && pt.x >= key.rect.left && pt.x <= key.rect.right && pt.y >= key.rect.top && pt.y <= key.rect.bottom) {
                g_pressedKey = key.vk;
                InvalidateRect(hwnd, NULL, FALSE);
                if (g_enableMouseTyping) {
                    INPUT input = { 0 }; input.type = INPUT_KEYBOARD; input.ki.wVk = static_cast<WORD>(key.vk);
                    SendInput(1, &input, sizeof(INPUT));
                }
                break;
            }
        }
        break;
    }
    case WM_LBUTTONUP: {
        if (g_pressedKey != -1) {
            if (g_enableMouseTyping) {
                INPUT input = { 0 }; input.type = INPUT_KEYBOARD; input.ki.wVk = static_cast<WORD>(g_pressedKey);
                input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1, &input, sizeof(INPUT));
            }
            g_pressedKey = -1;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    }
    case WM_DESTROY: {
        KillTimer(hwnd, 2);
        NOTIFYICONDATA nid = { 0 }; nid.cbSize = sizeof(NOTIFYICONDATA); nid.hWnd = hwnd; nid.uID = 1; Shell_NotifyIcon(NIM_DELETE, &nid);
        if (g_hKeyboardHook) UnhookWindowsHookEx(g_hKeyboardHook);
        if (g_pTextFormat) g_pTextFormat->Release();
        if (g_pRenderTarget) g_pRenderTarget->Release();
        if (g_pD2DFactory) g_pD2DFactory->Release();
        PostQuitMessage(0); break;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX icex; icex.dwSize = sizeof(INITCOMMONCONTROLSEX); icex.dwICC = ICC_BAR_CLASSES; InitCommonControlsEx(&icex);
    LoadSettings();

    WNDCLASS wc = { 0 }; wc.lpfnWndProc = WndProc; wc.hInstance = hInstance;
    wc.lpszClassName = L"Win11OSK_Engine"; wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.style = CS_HREDRAW | CS_VREDRAW;
    
    // 尝试加载用户添加的图标资源作为窗口图标
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION); // 兜底保护

    RegisterClass(&wc);

    DWORD exStyle = WS_EX_LAYERED | WS_EX_NOACTIVATE;
    if (g_topMost) exStyle |= WS_EX_TOPMOST;
    if (g_hideFromTaskbar) exStyle |= WS_EX_TOOLWINDOW;

    HWND hwnd = CreateWindowEx(exStyle, wc.lpszClassName, L"Windows 11 Fluid OSK", WS_POPUP | WS_THICKFRAME, 100, 100, g_layoutType == 0 ? 800 : 1100, 300, NULL, NULL, hInstance, NULL);

    ApplyEffects();
    ShowWindow(hwnd, nCmdShow); UpdateWindow(hwnd);

    MSG msg = { 0 }; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return static_cast<int>(msg.wParam);
}
