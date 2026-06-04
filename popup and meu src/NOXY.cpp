#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <TlHelp32.h>
#include <vector>
#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <ctime>
#include <regex>
#include <string>
#include <mutex>
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#include <shellapi.h>
#include <winhttp.h>
#include "gateway_data.h"

// Overlay window globals
HWND g_overlay_hwnd = NULL;
bool g_overlay_visible = false;
std::string g_overlay_log_text;
std::mutex g_overlay_mutex;

// ================================================================
//  ANSI COLOR CODES — BLUE PREMIUM THEME
// ================================================================
#define RESET        "\033[0m"
#define BOLD         "\033[1m"
#define DIM          "\033[2m"
#define ITALIC       "\033[3m"
#define UNDERLINE    "\033[4m"
#define RED          "\033[91m"
#define GREEN        "\033[92m"
#define YELLOW       "\033[93m"
#define BLUE         "\033[94m"
#define MAGENTA      "\033[95m"
#define CYAN         "\033[96m"
#define WHITE        "\033[97m"
#define DARK_RED     "\033[31m"
#define DARK_GREEN   "\033[32m"
#define DARK_YELLOW  "\033[33m"
#define DARK_CYAN    "\033[36m"
#define DARK_MAGENTA "\033[35m"
#define BG_RED       "\033[41m"
#define BG_GREEN     "\033[42m"
#define BG_DARK      "\033[40m"

// Blue theme 256-color palette
#define BLUE_BRIGHT  "\033[38;5;33m"
#define BLUE_LIGHT   "\033[38;5;75m"
#define BLUE_NEON    "\033[38;5;39m"
#define BLUE_SKY     "\033[38;5;111m"
#define BLUE_ICE     "\033[38;5;117m"
#define BLUE_DEEP    "\033[38;5;21m"
#define BLUE_ROYAL   "\033[38;5;63m"
#define BLUE_STEEL   "\033[38;5;67m"
#define BLUE_PALE    "\033[38;5;153m"
#define BLUE_DARK    "\033[38;5;25m"
#define STEEL        "\033[38;5;244m"
#define SMOKE        "\033[38;5;240m"
#define NEON_GREEN   "\033[38;5;46m"
#define NEON_RED     "\033[38;5;196m"
#define NEON_ORANGE  "\033[38;5;208m"
#define NEON_YELLOW  "\033[38;5;226m"

// ASCII box-drawing — works perfectly on every console
#define BOX_TL  "+"
#define BOX_TR  "+"
#define BOX_BL  "+"
#define BOX_BR  "+"
#define BOX_H   "="
#define BOX_V   "|"
#define BOX_ML  "+"
#define BOX_MR  "+"
#define LINE_H  "-"
#define LINE_V  "|"
#define LINE_TL "+"
#define LINE_TR "+"
#define LINE_BL "+"
#define LINE_BR "+"
#define LINE_ML "+"
#define LINE_MR "+"

// ASCII-safe glyphs
#define DOT       "*"
#define ARROW_R   ">"
#define ARROW_L   "<"
#define BLOCK_F   "#"
#define BLOCK_S   "-"
#define BULLET    "*"

// Log prefix icons — blue premium style
#define ICON_OK    NEON_GREEN  "[+]" RESET
#define ICON_ERR   NEON_RED    "[!]" RESET
#define ICON_INFO  BLUE_LIGHT  "[>]" RESET
#define ICON_WARN  NEON_ORANGE "[~]" RESET
#define ICON_BEAT  BLUE_ICE    "[#]" RESET
#define ICON_UUID  NEON_YELLOW "[U]" RESET
#define ICON_PIPE  BLUE_ROYAL  "[P]" RESET
#define ICON_SEND  BLUE_NEON   "[<]" RESET
#define ICON_CONN  NEON_GREEN  "[C]" RESET
#define ICON_DISC  NEON_RED    "[X]" RESET
#define ICON_HEX   SMOKE       "[H]" RESET

// ================================================================
//  GLOBALS
// ================================================================
#define CURRENT_VERSION 4

std::atomic_bool shutdown_event(false);
std::atomic_bool shutdown_complete(false);
std::atomic_bool stopped_once(false);
std::atomic<HANDLE> g_current_pipe(nullptr);
std::atomic_bool spinner_running(false);
std::atomic_bool title_anim_running(false);
std::mutex log_mutex;

const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\933823D3-C77B-4BAE-89D7-A92B567236BC";

// Gateway endpoints — change region as needed (ap/eu/na/kr)
const wchar_t* GATEWAY_HOST = L"ap.vg.ac.pvp.net";
const wchar_t* GATEWAY_PATH = L"/vanguard/v1/gateway";

// VDS Proxy config — set at runtime
std::wstring g_proxy_str; // e.g. "http://1.2.3.4:8080" or "socks5://1.2.3.4:1080"
bool g_use_proxy = false;

struct VanguardHeader {
    uint32_t magic;
    uint32_t total_size;
    uint32_t message_type;
    uint8_t  unknown1[12];
    uint32_t payload_size;
    uint8_t  unknown2[8];
};

// ================================================================
//  WIN32 OVERLAY WINDOW
// ================================================================
LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SETCURSOR:
        // Prevent cursor changes
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        // Black background
        HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &ps.rcPaint, hBrush);
        DeleteObject(hBrush);
        
        // Static border
        const int width = 420;
        const int height = 200;
        const int offset = 4;
        HPEN borderPen = CreatePen(PS_SOLID, 3, RGB(255, 76, 76));
        SelectObject(hdc, borderPen);
        MoveToEx(hdc, offset, offset, NULL);
        LineTo(hdc, width - offset, offset);
        LineTo(hdc, width - offset, height - offset);
        LineTo(hdc, offset, height - offset);
        LineTo(hdc, offset, offset);
        DeleteObject(borderPen);

        // Draw title with bigger red font
        const char* titleText = "Imperium.sys - VGC Bypass";
        HFONT hTitleFont = CreateFontA(20, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Courier New");
        SelectObject(hdc, hTitleFont);
        
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 76, 76)); // Red for title
        TextOutA(hdc, 10, 10, titleText, (int)strlen(titleText));
        
        HFONT hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Courier New");
        SelectObject(hdc, hFont);
        SetTextColor(hdc, RGB(255, 255, 255)); // White for logs
        {
            std::lock_guard<std::mutex> lock(g_overlay_mutex);
            std::istringstream iss(g_overlay_log_text);
            std::string line;
            int y = 40;
            while (std::getline(iss, line) && y < 180) {
                if (line == titleText) continue;
                COLORREF lineColor = RGB(255, 255, 255);
                if (line.rfind("[+]", 0) == 0) {
                    lineColor = RGB(0, 255, 0);
                } else if (line.rfind("[-]", 0) == 0) {
                    lineColor = RGB(255, 184, 28);
                } else if (line.rfind("[*]", 0) == 0) {
                    lineColor = RGB(180, 180, 180);
                }
                if (line.find("Cleaning Up") != std::string::npos || line.find("Finished Cleaning Up") != std::string::npos) {
                    lineColor = RGB(0, 255, 0);
                }
                SetTextColor(hdc, lineColor);
                TextOutA(hdc, 10, y, line.c_str(), (int)line.length());
                y += 16;
            }
        }
        
        DeleteObject(hFont);
        DeleteObject(hTitleFont);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_CLOSE:
        // Ignore close requests so the overlay can only be closed with F10.
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void create_overlay_window() {
    const char* CLASS_NAME = "ImperiumOverlay";
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = OverlayWndProc;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassA(&wc);

    g_overlay_hwnd = CreateWindowA(
        CLASS_NAME, "Imperium.sys",
        WS_POPUP,
        10, 10, 420, 200,
        NULL, NULL, GetModuleHandleA(NULL), NULL
    );

    if (g_overlay_hwnd) {
        // Make layered overlay always on top, hidden from taskbar/Alt+Tab, do not activate focus, and click-through.
        SetWindowLongA(g_overlay_hwnd, GWL_EXSTYLE, 
            GetWindowLongA(g_overlay_hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT);
        SetLayeredWindowAttributes(g_overlay_hwnd, RGB(0, 0, 0), 210, LWA_ALPHA);
        
        ShowWindow(g_overlay_hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(g_overlay_hwnd, HWND_TOPMOST, 10, 10, 420, 200, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}

void update_overlay_text(const std::string& text) {
    {
        std::lock_guard<std::mutex> lock(g_overlay_mutex);
        g_overlay_log_text = text;
    }
    if (g_overlay_hwnd) {
        InvalidateRect(g_overlay_hwnd, NULL, FALSE);
        UpdateWindow(g_overlay_hwnd);
    }
}

void overlay_message_pump() {
    create_overlay_window();
    update_overlay_text("Imperium.sys - VGC Bypass\n\nInitializing...");

    RegisterHotKey(NULL, 1, MOD_NOREPEAT, VK_F9);  // F9 = toggle overlay
    RegisterHotKey(NULL, 2, MOD_NOREPEAT, VK_F10); // F10 = exit

    MSG msg = { 0 };
    while (!shutdown_complete.load()) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY) {
                if (msg.wParam == 1) {
                    g_overlay_visible = !g_overlay_visible;
                    if (g_overlay_hwnd) {
                        ShowWindow(g_overlay_hwnd, g_overlay_visible ? SW_SHOW : SW_HIDE);
                    }
                } else if (msg.wParam == 2) {
                    shutdown_event.store(true);
                }
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        Sleep(10);
    }

    UnregisterHotKey(NULL, 1);
    UnregisterHotKey(NULL, 2);
    if (g_overlay_hwnd) {
        DestroyWindow(g_overlay_hwnd);
        g_overlay_hwnd = NULL;
    }
}

bool is_user_admin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroup)) {
        if (!CheckTokenMembership(NULL, adminGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

void relaunch_as_admin(int argc, char* argv[]) {
    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    std::string params;
    for (int i = 1; i < argc; ++i) {
        if (!params.empty()) params += " ";
        params += argv[i];
    }

    std::cout << "\n" << YELLOW "[>] Administrator privileges required. Relaunching as admin..." RESET "\n";
    HINSTANCE result = ShellExecuteA(NULL, "runas", exePath,
        params.empty() ? NULL : params.c_str(), NULL, SW_SHOWNORMAL);
    if ((intptr_t)result <= 32) {
        std::cout << RED "[!] Failed to elevate to admin. Please run this program as Administrator." RESET "\n";
    }
}

DWORD get_dnscache_pid() {
    DWORD pid = 0;
    FILE* pipe = _popen("sc queryex dnscache 2>NUL", "r");
    if (!pipe) return 0;

    char line[256];
    while (fgets(line, sizeof(line), pipe)) {
        unsigned long value = 0;
        if (sscanf_s(line, "%*[^:]: %lu", &value) == 1) {
            if (value != 0) {
                pid = static_cast<DWORD>(value);
                break;
            }
        }
    }
    _pclose(pipe);
    return pid;
}

bool toggle_dns_cache(bool resume) {
    DWORD pid = get_dnscache_pid();
    if (pid == 0) {
        return false;
    }

    HANDLE threadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (threadSnap == INVALID_HANDLE_VALUE) {
        return false;
    }

    THREADENTRY32 te = { 0 };
    te.dwSize = sizeof(te);
    bool anyThread = false;

    if (Thread32First(threadSnap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) {
                    anyThread = true;
                    if (resume) ResumeThread(hThread);
                    else SuspendThread(hThread);
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(threadSnap, &te));
    }

    CloseHandle(threadSnap);
    return anyThread;
}

DWORD get_process_id_by_name(const wchar_t* process_name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { 0 };
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, process_name) == 0) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return 0;
}

DWORD get_process_thread_count(DWORD pid) {
    DWORD count = 0;
    HANDLE threadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (threadSnap == INVALID_HANDLE_VALUE) return 0;

    THREADENTRY32 te = { 0 };
    te.dwSize = sizeof(te);

    if (Thread32First(threadSnap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                ++count;
            }
        } while (Thread32Next(threadSnap, &te));
    }

    CloseHandle(threadSnap);
    return count;
}

std::wstring find_riot_client_path() {
    HKEY hKey = NULL;
    std::wstring path;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, L"riotclient\\DefaultIcon", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR value[1024] = { 0 };
        DWORD size = sizeof(value);
        if (RegQueryValueExW(hKey, NULL, NULL, NULL, reinterpret_cast<LPBYTE>(value), &size) == ERROR_SUCCESS) {
            path = value;
            size_t comma = path.find(L',');
            if (comma != std::wstring::npos) {
                path.resize(comma);
            }
            if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"') {
                path = path.substr(1, path.size() - 2);
            }
        }
        RegCloseKey(hKey);
    }
    return path;
}

bool launch_riot_client() {
    std::wcout << L"\n" << L"[AUTO] Launching Riot Client..." << L"\n";
    HINSTANCE result = ShellExecuteW(NULL, L"open", L"riot://launch/valorant", NULL, NULL, SW_SHOWNORMAL);
    if ((intptr_t)result > 32) {
        return true;
    }

    std::wstring clientPath = find_riot_client_path();
    if (!clientPath.empty()) {
        std::wstring args = L"--launch-product=valorant --launch-patchline=live";
        result = ShellExecuteW(NULL, L"open", clientPath.c_str(), args.c_str(), NULL, SW_SHOWNORMAL);
        if ((intptr_t)result > 32) {
            return true;
        }
    }

    std::wcout << L"[AUTO] Could not auto-launch Riot Client using protocol or registry path." << L"\n";
    return false;
}

bool wait_for_vgc_ready(int timeout_ms = 15000) {
    int elapsed = 0;
    const int step = 50;
    while (elapsed < timeout_ms) {
        DWORD pid = get_process_id_by_name(L"vgc.exe");
        if (pid != 0) {
            DWORD threads = get_process_thread_count(pid);
            if (threads > 15) {
                return true;
            }
        }
        Sleep(step);
        elapsed += step;
    }
    return false;
}

// ================================================================
//  CONSOLE SETUP
// ================================================================
void enable_ansi() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    // Set Consolas font — supports Unicode block + box chars
    CONSOLE_FONT_INFOEX cfi;
    memset(&cfi, 0, sizeof(cfi));
    cfi.cbSize = sizeof(cfi);
    cfi.dwFontSize.Y = 18;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    wcscpy_s(cfi.FaceName, L"Consolas");
    SetCurrentConsoleFontEx(hOut, FALSE, &cfi);

    // UTF-8 codepage
    SetConsoleOutputCP(CP_UTF8);

    // Console window / buffer size
    SMALL_RECT windowSize = { 0, 0, 119, 45 };
    SetConsoleWindowInfo(hOut, TRUE, &windowSize);
    COORD bufferSize = { 120, 5000 };
    SetConsoleScreenBufferSize(hOut, bufferSize);

    SetConsoleTitleA("Imperium.sys VGC BYPASS v4.0  |  NO-RESTART Bypass  |  Premium Edition");
}

void clear_screen() {
    std::cout << "\033[2J\033[H";
}

// ================================================================
//  ANIMATION HELPERS
// ================================================================
void type_out(const std::string& text, int delay_ms = 12) {
    for (char c : text) {
        std::cout << c << std::flush;
        Sleep(delay_ms);
    }
}

void fast_type(const std::string& text, int delay_ms = 4) {
    for (char c : text) {
        std::cout << c << std::flush;
        Sleep(delay_ms);
    }
}

void draw_line(int width, const char* color, const char* ch = LINE_H) {
    std::cout << color;
    for (int i = 0; i < width; i++) std::cout << ch;
    std::cout << RESET;
}

void progress_bar_anim(const std::string& label, int steps = 30, int delay = 25) {
    for (int i = 0; i <= steps; i++) {
        std::cout << "\r  " BLUE_LIGHT BOLD << label << "  " RESET BLUE_NEON "[" RESET;
        int filled = i;
        int empty = steps - i;
        for (int j = 0; j < filled; j++) std::cout << BLUE_BRIGHT BLOCK_F RESET;
        for (int j = 0; j < empty; j++) std::cout << SMOKE BLOCK_S RESET;
        int pct = (i * 100) / steps;
        std::cout << BLUE_NEON "]" RESET " " BLUE_ICE BOLD << pct << "%" RESET "  " << std::flush;
        Sleep(delay);
    }
    std::cout << "\n";
}

// ================================================================
//  TITLE REVEAL ANIMATION — loops "Imperium.sys" in title bar
// ================================================================
std::thread title_anim_thread;

void title_reveal_worker() {
    const char* full_name = "Imperium.sys";
    int len = (int)strlen(full_name);
    while (title_anim_running.load()) {
        // Phase 1: reveal character by character
        for (int i = 1; i <= len && title_anim_running.load(); i++) {
            std::string partial(full_name, i);
            std::string title = "[ " + partial + " ]  |  VGC Bypass  |  Imperium.sys";
            SetConsoleTitleA(title.c_str());
            Sleep(100);
        }
        // Phase 2: hold full name
        if (title_anim_running.load()) {
            SetConsoleTitleA("[ Imperium.sys ]  |  VGC Bypass  |  Imperium.sys");
            Sleep(2000);
        }
        // Phase 3: erase character by character
        for (int i = len; i >= 0 && title_anim_running.load(); i--) {
            std::string partial(full_name, i);
            std::string title = "[ " + partial + " ]  |  VGC Bypass  |  Imperium.sys";
            SetConsoleTitleA(title.c_str());
            Sleep(80);
        }
        // Phase 4: brief pause before next loop
        if (title_anim_running.load()) Sleep(500);
    }
}

void start_title_anim() {
    title_anim_running.store(true);
    title_anim_thread = std::thread(title_reveal_worker);
}

void stop_title_anim() {
    title_anim_running.store(false);
    if (title_anim_thread.joinable())
        title_anim_thread.join();
}

// ================================================================
//  BANNER + UI ELEMENTS — BLUE PREMIUM EDITION
// ================================================================
void print_startup_sequence() {
    // Hacker-style boot sequence
    std::cout << "\n";
    std::cout << "  " SMOKE;
    fast_type("[SYS] Initializing Imperium PREMIUM kernel...", 8);
    std::cout << RESET "\n";
    Sleep(100);

    std::cout << "  " SMOKE;
    fast_type("[SYS] Loading cryptographic modules...", 8);
    std::cout << RESET "\n";
    Sleep(80);

    std::cout << "  " SMOKE;
    fast_type("[SYS] Establishing secure pipeline...", 8);
    std::cout << RESET "\n";
    Sleep(80);

    std::cout << "  " SMOKE;
    fast_type("[SYS] Bypassing Vanguard authentication layer...", 8);
    std::cout << RESET "\n";
    Sleep(100);

    progress_bar_anim(">> Imperium PREMIUM BOOT", 40, 18);
    std::cout << "\n";
}

void print_banner() {
    std::cout << "\n";

    // Top decorative edge
    std::cout << "  " BLUE_NEON;
    draw_line(80, BLUE_NEON, "=");
    std::cout << RESET "\n";

    // Imperium.sys ASCII art — all BLUE gradient
    std::cout << BLUE_ICE BOLD;
    std::cout << "   #######  ###   ###  ####### ######## #### ### ###  ###   ### \n";
    std::cout << BLUE_LIGHT BOLD;
    std::cout << "   ##      ## ## # ## ##   ##   ##       ##  #### ##  ## #  ## \n";
    std::cout << BLUE_NEON BOLD;
    std::cout << "   #####   ##  ###  ##   ##   #####     ##   ###  ##  ##    ##  \n";
    std::cout << BLUE_BRIGHT BOLD;
    std::cout << "   ##      ##       ##   ##   ##        ##   ##   ##  ##    ##  \n";
    std::cout << BLUE_DEEP BOLD;
    std::cout << "   ##      ##       ##   ##   ########  ##   ##   ##  ###   ### \n";
    std::cout << RESET;

    // PREMIUM subtitle
    std::cout << BLUE_NEON BOLD;
    std::cout << "                        P R E M I U M                            \n";
    std::cout << RESET;

    // Bottom decorative edge
    std::cout << "  " BLUE_NEON;
    draw_line(80, BLUE_NEON, "=");
    std::cout << RESET "\n";

    std::cout << "\n";

    // === Info Box ===
    // Top border
    std::cout << "  " BLUE_NEON "+";
    draw_line(78, BLUE_NEON, "=");
    std::cout << "+" RESET "\n";

    // Title row
    std::cout << "  " BLUE_NEON "|" RESET
        << "  " BLUE_ICE BOLD "> " RESET
        << BLUE_ICE BOLD "Imperium.sys PREMIUM" RESET
        << "  " SMOKE "|" RESET
        << "  " BLUE_LIGHT BOLD "v4.0" RESET
        << "  " SMOKE "|" RESET
        << "  " BLUE_BRIGHT BOLD "NO-RESTART BYPASS" RESET
        << "                     " BLUE_NEON "|" RESET "\n";

    // Middle divider
    std::cout << "  " BLUE_NEON "+";
    draw_line(78, SMOKE, "-");
    std::cout << BLUE_NEON "+" RESET "\n";

    // Feature row
    std::cout << "  " BLUE_NEON "|" RESET
        << "  " BLUE_LIGHT "[>]" RESET " " STEEL "Gateway Relay" RESET
        << "  " SMOKE "|" RESET
        << "  " BLUE_NEON "[>]" RESET " " STEEL "Auth Handler" RESET
        << "  " SMOKE "|" RESET
        << "  " BLUE_SKY "[>]" RESET " " STEEL "UUID Extract" RESET
        << "  " SMOKE "|" RESET
        << "  " BLUE_ICE "[>]" RESET " " STEEL "Heartbeat" RESET
        << "  " BLUE_NEON "|" RESET "\n";

    // Bottom border
    std::cout << "  " BLUE_NEON "+";
    draw_line(78, BLUE_NEON, "=");
    std::cout << "+" RESET "\n";

    // Tagline
    std::cout << "\n    " SMOKE "\"" STEEL "Where Vanguard sleeps, " BLUE_ICE BOLD "Imperium.sys" RESET STEEL " runs free." SMOKE "\"" RESET "\n\n";
}

void print_separator(const char* label = nullptr) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (label) {
        std::string lbl = label;
        int total_width = 78;
        int label_space = (int)lbl.size() + 6;
        int remaining = total_width - label_space;
        int left_pad = remaining / 2;
        int right_pad = remaining - left_pad;

        std::cout << "  " BLUE_NEON;
        for (int i = 0; i < left_pad; i++) std::cout << "-";
        std::cout << RESET " " BLUE_NEON "[ " BOLD WHITE << label << RESET BLUE_NEON " ]" RESET " " BLUE_NEON;
        for (int i = 0; i < right_pad; i++) std::cout << "-";
        std::cout << RESET "\n";
    }
    else {
        std::cout << "  " SMOKE;
        for (int i = 0; i < 78; i++) std::cout << "-";
        std::cout << RESET "\n";
    }
}

void print_menu() {
    std::cout << "\n";

    // Top border
    std::cout << "  " BLUE_NEON "+";
    for (int i = 0; i < 48; i++) std::cout << "-";
    std::cout << "+" RESET "\n";

    // Header
    std::cout << "  " BLUE_NEON "|" RESET
        << "       " BLUE_ICE BOLD ">>  SELECT YOUR OPTION  <<" RESET
        << "              " BLUE_NEON "|" RESET "\n";

    // Divider
    std::cout << "  " BLUE_NEON "+";
    for (int i = 0; i < 48; i++) std::cout << "-";
    std::cout << "+" RESET "\n";

    std::cout << "  " BLUE_NEON "|" RESET
        << "                                                " BLUE_NEON "|" RESET "\n";

    // Option 1
    std::cout << "  " BLUE_NEON "|" RESET
        << "   " NEON_GREEN BOLD "  [1]" RESET "  " WHITE ">  " NEON_GREEN "Start VGC Bypass" RESET
        << "                    " BLUE_NEON "|" RESET "\n";

    std::cout << "  " BLUE_NEON "|" RESET
        << "                                                " BLUE_NEON "|" RESET "\n";

    // Option 2
    std::cout << "  " BLUE_NEON "|" RESET
        << "   " NEON_RED BOLD "  [2]" RESET "  " WHITE ">  " NEON_RED "Exit" RESET
        << "                            " BLUE_NEON "|" RESET "\n";

    std::cout << "  " BLUE_NEON "|" RESET
        << "                                                " BLUE_NEON "|" RESET "\n";

    // Bottom border
    std::cout << "  " BLUE_NEON "+";
    for (int i = 0; i < 48; i++) std::cout << "-";
    std::cout << "+" RESET "\n\n";

    // Input prompt — blue premium style
    std::cout << "  " BLUE_ICE BOLD "  > " BLUE_NEON "noxy" BLUE_LIGHT "@" BLUE_BRIGHT "premium" RESET SMOKE ":" BLUE_SKY "~" BLUE_ICE BOLD "$ " RESET WHITE;
}

// ================================================================
//  SPINNER
// ================================================================
std::thread spinner_thread_handle;
std::string spinner_label;

void spinner_worker() {
    const char* frames[] = { "/", "-", "\\", "|" };
    // Blue color cycle for the spinner
    const char* blue_cycle[] = {
        BLUE_ICE, BLUE_LIGHT, BLUE_NEON, BLUE_BRIGHT,
        BLUE_SKY, BLUE_ROYAL, BLUE_STEEL, BLUE_PALE
    };
    int i = 0;
    while (spinner_running.load()) {
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            std::cout << "\r  " << blue_cycle[i % 8] << BOLD
                << frames[i % 4] << RESET " " BLUE_LIGHT BOLD
                << spinner_label << RESET "   " << std::flush;
        }
        i++;
        Sleep(100);
    }
    std::cout << "\r" << std::string(100, ' ') << "\r";
}

void start_spinner(const std::string& label) {
    spinner_label = label;
    spinner_running.store(true);
    spinner_thread_handle = std::thread(spinner_worker);
}

void stop_spinner() {
    spinner_running.store(false);
    if (spinner_thread_handle.joinable())
        spinner_thread_handle.join();
}

// ================================================================
//  LOGGING
// ================================================================
std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm bt;
    localtime_s(&bt, &now_time_t);
    std::ostringstream ss;
    ss << std::put_time(&bt, "%H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << now_ms.count();
    return ss.str();
}

enum LogLevel { LOG_INFO, LOG_OK, LOG_WARN, LOG_ERROR, LOG_RECV, LOG_SEND, LOG_HEART, LOG_UUID, LOG_CONN, LOG_DISC, LOG_HEX, LOG_PIPE };

void log_ex(LogLevel level, const char* msg) {
    std::lock_guard<std::mutex> lock(log_mutex);

    const char* icon;
    const char* color;

    switch (level) {
    case LOG_OK:    icon = "[+]"; color = NEON_GREEN;      break;
    case LOG_ERROR:
    case LOG_WARN:  icon = "[-]"; color = NEON_ORANGE;     break;
    default:        icon = "[*]"; color = STEEL;           break;
    }

    std::string log_line;
    log_line += icon;
    log_line += " ";
    log_line += msg;

    std::cout << "  " SMOKE "[" RESET BLUE_STEEL << get_timestamp() << RESET SMOKE "]" RESET
        << " " << color << BOLD << icon << RESET
        << " " << color << msg << RESET << "\n";
    
    // Append to overlay text (keep last 10 lines)
    {
        std::lock_guard<std::mutex> overlay_lock(g_overlay_mutex);
        g_overlay_log_text += log_line + "\n";
        
        // Keep only last 10 lines
        int line_count = 0;
        size_t pos = 0;
        while ((pos = g_overlay_log_text.find('\n', pos)) != std::string::npos) {
            line_count++;
            pos++;
        }
        
        if (line_count > 10) {
            pos = 0;
            for (int i = 0; i < line_count - 10; i++) {
                pos = g_overlay_log_text.find('\n', pos) + 1;
            }
            g_overlay_log_text = g_overlay_log_text.substr(pos);
        }
    }

    if (g_overlay_hwnd) {
        InvalidateRect(g_overlay_hwnd, NULL, FALSE);
    }
}

// Smart auto-level log (for compatibility with old log_message calls)
void log_message(const char* msg) {
    std::string s(msg);

    LogLevel lvl = LOG_INFO;
    if (s.find("===") != std::string::npos && s.find("CONNECTION") != std::string::npos) {
        if (s.find("CLOSED") != std::string::npos) lvl = LOG_DISC;
        else lvl = LOG_CONN;
    }
    else if (s.find("Heartbeat") != std::string::npos) lvl = LOG_HEART;
    else if (s.find("RECV") != std::string::npos)      lvl = LOG_RECV;
    else if (s.find("Response sent") != std::string::npos) lvl = LOG_SEND;
    else if (s.find("UUID") != std::string::npos || s.find("uuid") != std::string::npos) lvl = LOG_UUID;
    else if (s.find("Valorant detected") != std::string::npos
        || s.find("Pipe overridden") != std::string::npos
        || s.find("overridden") != std::string::npos)  lvl = LOG_OK;
    else if (s.find("NOT found") != std::string::npos
        || s.find("INVALID") != std::string::npos)     lvl = LOG_ERROR;
    else if (s.find("Waiting") != std::string::npos
        || s.find("ENTER") != std::string::npos
        || s.find("===") != std::string::npos)         lvl = LOG_WARN;
    else if (s.find("Hex") != std::string::npos)         lvl = LOG_HEX;

    if (s.empty()) { std::cout << "\n"; return; }

    log_ex(lvl, msg);
}

void log_hex(const uint8_t* data, size_t size, const char* prefix = "") {
    std::stringstream ss;
    ss << prefix << " Hex: ";
    for (size_t i = 0; i < min(size, (size_t)32); i++) {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)data[i] << " ";
        if ((i + 1) % 16 == 0 && i < size - 1) {
            log_ex(LOG_HEX, ss.str().c_str());
            ss.str("");
            ss << "       ";
        }
    }
    if (ss.str().length() > 7)
        log_ex(LOG_HEX, ss.str().c_str());
}

// ================================================================
//  UUID UTILS
// ================================================================
void uuid_string_to_binary(const char* uuid_str, uint8_t* binary) {
    unsigned int parts[16];
    sscanf_s(uuid_str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        &parts[0], &parts[1], &parts[2], &parts[3],
        &parts[4], &parts[5], &parts[6], &parts[7],
        &parts[8], &parts[9], &parts[10], &parts[11],
        &parts[12], &parts[13], &parts[14], &parts[15]);

    binary[0] = parts[3] & 0xFF; binary[1] = parts[2] & 0xFF;
    binary[2] = parts[1] & 0xFF; binary[3] = parts[0] & 0xFF;
    binary[4] = parts[5] & 0xFF; binary[5] = parts[4] & 0xFF;
    binary[6] = parts[7] & 0xFF; binary[7] = parts[6] & 0xFF;
    binary[8] = parts[8] & 0xFF; binary[9] = parts[9] & 0xFF;
    binary[10] = parts[10] & 0xFF; binary[11] = parts[11] & 0xFF;
    binary[12] = parts[12] & 0xFF; binary[13] = parts[13] & 0xFF;
    binary[14] = parts[14] & 0xFF; binary[15] = parts[15] & 0xFF;
}

bool find_last_uuid(const uint8_t* data, size_t size, uint8_t* uuid_bin, char* uuid_str) {
    std::string text;
    for (size_t i = 0; i < size; i++)
        text += (data[i] >= 32 && data[i] <= 126) ? (char)data[i] : ' ';

    std::regex uuid_pattern("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");
    std::smatch match;
    std::string last_uuid;
    auto search_start = text.cbegin();
    while (std::regex_search(search_start, text.cend(), match, uuid_pattern)) {
        last_uuid = match[0];
        search_start = match[0].second;
    }

    if (!last_uuid.empty()) {
        strcpy_s(uuid_str, 37, last_uuid.c_str());
        uuid_string_to_binary(last_uuid.c_str(), uuid_bin);
        return true;
    }
    return false;
}

// ================================================================
//  GATEWAY HTTPS CLIENT
// ================================================================
std::vector<uint8_t> send_gateway_request(const uint8_t* auth_payload, size_t auth_size,
                                           const uint8_t* full_buffer, size_t full_size) {
    std::vector<uint8_t> result;

    // Try multiple request body formats until one works
    struct RequestAttempt {
        const char* name;
        const uint8_t* data;
        size_t size;
    };

    // Attempt 1: Raw pipe payload (after VanguardHeader) — might already be valid protobuf
    // Attempt 2: Full pipe buffer (including VanguardHeader)
    // Attempt 3: Captured request_content.x-protobuf (known-good format)
    RequestAttempt attempts[] = {
        { "raw_payload", auth_payload, auth_size },
        { "full_buffer", full_buffer, full_size },
        { "captured_request", g_request_proto, g_request_proto_size },
    };

    // WinHTTP session (shared across attempts)
    HINTERNET hSession;
    if (g_use_proxy) {
        hSession = WinHttpOpen(L"NOXY/4.0",
            WINHTTP_ACCESS_TYPE_NAMED_PROXY, g_proxy_str.c_str(), WINHTTP_NO_PROXY_BYPASS, 0);
        log_ex(LOG_OK, ">> Using VDS proxy for gateway request");
    } else {
        hSession = WinHttpOpen(L"NOXY/4.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!hSession) { log_ex(LOG_ERROR, "WinHttpOpen failed"); return result; }

    HINTERNET hConnect = WinHttpConnect(hSession, GATEWAY_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { log_ex(LOG_ERROR, "WinHttpConnect failed"); WinHttpCloseHandle(hSession); return result; }

    for (auto& attempt : attempts) {
        char bm[128];
        sprintf_s(bm, "Trying gateway format: %s  (%zu bytes)", attempt.name, attempt.size);
        log_ex(LOG_OK, bm);

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", GATEWAY_PATH,
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) { log_ex(LOG_ERROR, "WinHttpOpenRequest failed"); continue; }

        // Disable SSL certificate validation
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                         SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));

        LPCWSTR headers = L"Content-Type: application/x-protobuf\r\n"
                          L"Accept: application/x-protobuf\r\n"
                          L"User-Agent: vgc/1\r\n"
                          L"X-Riot-ClientPlatform: ew0KCSJwbGF0Zm9ybVR5cGUiOiAiUEMiLA0KCSJwbGF0Zm9ybU9TIjogIldpbmRvd3MiLA0KCSJwbGF0Zm9ybU9TVmVyc2lvbiI6ICIxMC4wLjE5MDQ1LjEuMjU2LjEiLA0KCSJwbGF0Zm9ybUNoaXBzZXQiOiAiVW5rbm93biINCn0=\r\n"
                          L"X-Riot-ClientVersion: release-09.08-shipping-18-2594635";

        BOOL bSent = WinHttpSendRequest(hRequest, headers, -1,
            (LPVOID)attempt.data, (DWORD)attempt.size, (DWORD)attempt.size, 0);
        if (!bSent) {
            DWORD err = GetLastError();
            char em[128];
            sprintf_s(em, "  SendRequest failed (error=%lu) — trying next...", err);
            log_ex(LOG_WARN, em);
            WinHttpCloseHandle(hRequest);
            continue;
        }

        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            DWORD err = GetLastError();
            char em[128];
            sprintf_s(em, "  ReceiveResponse failed (error=%lu) — trying next...", err);
            log_ex(LOG_WARN, em);
            WinHttpCloseHandle(hRequest);
            continue;
        }

        DWORD statusCode = 0, statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        char sm[128];
        sprintf_s(sm, "  HTTP %lu from %s format", statusCode, attempt.name);
        log_ex(statusCode == 200 ? LOG_OK : LOG_WARN, sm);

        // Read response body
        DWORD bytesAvail = 0;
        std::vector<uint8_t> body;
        do {
            WinHttpQueryDataAvailable(hRequest, &bytesAvail);
            if (bytesAvail > 0) {
                std::vector<uint8_t> chunk(bytesAvail);
                DWORD bytesDownloaded = 0;
                WinHttpReadData(hRequest, chunk.data(), bytesAvail, &bytesDownloaded);
                body.insert(body.end(), chunk.begin(), chunk.begin() + bytesDownloaded);
            }
        } while (bytesAvail > 0);

        WinHttpCloseHandle(hRequest);

        char rm[128];
        sprintf_s(rm, "  Response body: %zu bytes", body.size());
        log_ex(LOG_OK, rm);

        if (statusCode == 200 && !body.empty()) {
            log_ex(LOG_OK, ">> Gateway returned HTTP 200 — session valid!");
            if (body.size() > 8) {
                char hx[64];
                sprintf_s(hx, "  First 8: %02X %02X %02X %02X %02X %02X %02X %02X",
                    body[0], body[1], body[2], body[3], body[4], body[5], body[6], body[7]);
                log_ex(LOG_OK, hx);
            }
            result = body;
            break; // Success!
        }
        else {
            log_ex(LOG_WARN, "  Not 200 or empty body — trying next format...");
        }
    } // end for loop

    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

// ================================================================
//  VGC LOGIC
// ================================================================
void stop_and_restart_vgc() {
    log_ex(LOG_WARN, "Stopping VGC service...");
    system("sc stop vgc >nul 2>&1");
    Sleep(500);
    log_ex(LOG_OK, "Starting VGC service...");
    system("sc start vgc >nul 2>&1");
    Sleep(500);
}

void override_vgc_pipe() {
    HANDLE pipe = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe);
        log_ex(LOG_OK, "Pipe override successful!");
    }
}

std::vector<uint8_t> create_server_ack(uint32_t magic) {
    std::vector<uint8_t> resp;
    VanguardHeader hdr = { 0 };
    hdr.magic = magic + 1;
    hdr.total_size = 40;
    hdr.message_type = 1;
    hdr.payload_size = 8;
    resp.insert(resp.end(), (uint8_t*)&hdr, (uint8_t*)&hdr + sizeof(hdr));
    resp.insert(resp.end(), 8, 0);
    return resp;
}

std::vector<uint8_t> create_auth_ack(uint32_t magic, int version, const uint8_t* uuid_bin) {
    std::vector<uint8_t> resp;
    VanguardHeader hdr = { 0 };

    char msg[256];
    sprintf_s(msg, "Crafting auth response  |  Protocol version: %d", version);
    log_ex(LOG_INFO, msg);

    switch (version) {
    case 1:
        hdr.magic = magic + 1; hdr.total_size = 40; hdr.message_type = 1; hdr.payload_size = 8;
        resp.insert(resp.end(), (uint8_t*)&hdr, (uint8_t*)&hdr + sizeof(hdr));
        resp.insert(resp.end(), 8, 0);
        log_hex(resp.data(), resp.size(), "v1");
        break;
    case 2:
        hdr.magic = magic + 1; hdr.total_size = 40; hdr.message_type = 1; hdr.payload_size = 8;
        resp.insert(resp.end(), (uint8_t*)&hdr, (uint8_t*)&hdr + sizeof(hdr));
        resp.insert(resp.end(), uuid_bin, uuid_bin + 8);
        log_hex(resp.data(), resp.size(), "v2");
        break;
    case 3:
        hdr.magic = magic + 1; hdr.total_size = 56; hdr.message_type = 1; hdr.payload_size = 16;
        resp.insert(resp.end(), (uint8_t*)&hdr, (uint8_t*)&hdr + sizeof(hdr));
        resp.insert(resp.end(), uuid_bin, uuid_bin + 16);
        log_hex(resp.data(), resp.size(), "v3");
        break;
    case 4: {
        hdr.magic = magic + 1; hdr.total_size = 40; hdr.message_type = 1; hdr.payload_size = 8;
        resp.insert(resp.end(), (uint8_t*)&hdr, (uint8_t*)&hdr + sizeof(hdr));
        uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        resp.insert(resp.end(), (uint8_t*)&ts, (uint8_t*)&ts + 8);
        log_hex(resp.data(), resp.size(), "v4");
        break;
    }
    case 5: {
        hdr.magic = magic + 1; hdr.total_size = 64; hdr.message_type = 1; hdr.payload_size = 24;
        resp.insert(resp.end(), (uint8_t*)&hdr, (uint8_t*)&hdr + sizeof(hdr));
        resp.insert(resp.end(), uuid_bin, uuid_bin + 16);
        uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        resp.insert(resp.end(), (uint8_t*)&ts, (uint8_t*)&ts + 8);
        log_hex(resp.data(), resp.size(), "v5");
        break;
    }
    }
    return resp;
}

std::vector<uint8_t> create_heartbeat_response(const uint8_t* data, size_t size) {
    std::vector<uint8_t> resp(data, data + size);
    VanguardHeader* hdr = (VanguardHeader*)resp.data();
    hdr->magic += 1;
    return resp;
}

// ================================================================
//  CLIENT HANDLER
// ================================================================
void handle_client(HANDLE pipe) {
    std::vector<uint8_t> buffer(16384);
    DWORD bytesRead;
    g_current_pipe.store(pipe);

    uint8_t uuid_bin[16] = { 0 };
    char    uuid_str[37] = { 0 };
    bool    uuid_found = false;

    print_separator("NEW CONNECTION");

    char vmsg[64];
    sprintf_s(vmsg, "Active protocol version: " BOLD YELLOW "%d" RESET, CURRENT_VERSION);
    log_ex(LOG_CONN, vmsg);

    while (!shutdown_event.load()) {
        if (!ReadFile(pipe, buffer.data(), (DWORD)buffer.size(), &bytesRead, NULL) || bytesRead == 0)
            break;

        std::vector<uint8_t> response;

        // Always parse VanguardHeader first — all pipe data is wrapped in it
        VanguardHeader* hdr = (VanguardHeader*)buffer.data();

        log_ex(LOG_RECV, "RECV");

        // Extract payload pointer and size (data after VanguardHeader)
        uint8_t* payload = buffer.data() + sizeof(VanguardHeader);
        size_t payload_len = (bytesRead > sizeof(VanguardHeader)) ? (bytesRead - sizeof(VanguardHeader)) : 0;

        // Check if payload contains RG gateway protobuf (0x52='R', 0x47='G')
        bool has_rg_marker = false;
        if (payload_len >= 7) {
            for (size_t i = 0; i + 1 < payload_len && i < 12; i++) {
                if (payload[i] == 0x52 && payload[i + 1] == 0x47) {
                    has_rg_marker = true;
                    break;
                }
            }
        }

        switch (hdr->message_type) {
        case 2:
            log_ex(LOG_WARN, "Server list request — stopping VGC...");
            response = create_server_ack(hdr->magic);
            if (!stopped_once.exchange(true)) {
                system("sc stop vgc >nul 2>&1");
                Beep(1000, 300);
            }
            break;

        case 3: {
            // Gateway Auth — respond with captured auth.bin blob
            log_ex(LOG_OK, ">> Gateway AUTH (type 3) — replaying auth.bin...");
            VanguardHeader rh = { 0 };
            rh.magic = hdr->magic + 1;
            rh.total_size = (uint32_t)(sizeof(VanguardHeader) + g_auth_data_size);
            rh.message_type = 1;
            rh.payload_size = (uint32_t)g_auth_data_size;
            response.insert(response.end(), (uint8_t*)&rh, (uint8_t*)&rh + sizeof(rh));
            response.insert(response.end(), g_auth_data, g_auth_data + g_auth_data_size);
            char am[128];
            sprintf_s(am, "Auth blob sent  (%zu bytes)", g_auth_data_size);
            log_ex(LOG_OK, am);
            break;
        }

        case 4: {
            // Auth token — UUID extraction
            log_ex(LOG_UUID, "Auth token packet — scanning for UUID...");
            uuid_found = find_last_uuid(buffer.data(), bytesRead, uuid_bin, uuid_str);
            if (uuid_found) {
                char uuid_msg[128];
                sprintf_s(uuid_msg, "UUID extracted: " BOLD YELLOW "%s" RESET, uuid_str);
                log_ex(LOG_UUID, uuid_msg);
                log_hex(uuid_bin, 16, "UUID");
                response = create_auth_ack(hdr->magic, CURRENT_VERSION, uuid_bin);
            }
            else {
                log_ex(LOG_ERROR, "UUID not found in payload — falling back to v1");
                response = create_auth_ack(hdr->magic, 1, uuid_bin);
            }

            // Send auth ack first
            DWORD written;
            WriteFile(pipe, response.data(), (DWORD)response.size(), &written, NULL);
            char smsg[64];
            sprintf_s(smsg, "Auth ACK dispatched  (%lu bytes)", written);
            log_ex(LOG_SEND, smsg);

            // --- NO-RESTART: HTTPS GATEWAY SESSION ---
            // Send the auth payload to Riot's gateway via HTTPS
            uint8_t* auth_payload = buffer.data() + sizeof(VanguardHeader);
            size_t auth_payload_size = (bytesRead > sizeof(VanguardHeader)) ? (bytesRead - sizeof(VanguardHeader)) : 0;

            if (auth_payload_size > 0) {
                char gm[128];
                sprintf_s(gm, ">> Sending auth payload (%zu bytes) to gateway...", auth_payload_size);
                log_ex(LOG_OK, gm);

                auto gw_response = send_gateway_request(auth_payload, auth_payload_size,
                                                        buffer.data(), bytesRead);

                if (!gw_response.empty()) {
                    log_ex(LOG_OK, ">> Gateway session established! Pushing to client...");
                    
                    // Send gateway response through pipe (wrapped in VanguardHeader)
                    VanguardHeader rh = { 0 };
                    rh.magic = hdr->magic + 1;
                    rh.total_size = (uint32_t)(sizeof(VanguardHeader) + gw_response.size());
                    rh.message_type = 1;
                    rh.payload_size = (uint32_t)gw_response.size();

                    std::vector<uint8_t> pipe_resp;
                    pipe_resp.insert(pipe_resp.end(), (uint8_t*)&rh, (uint8_t*)&rh + sizeof(rh));
                    pipe_resp.insert(pipe_resp.end(), gw_response.begin(), gw_response.end());
                    WriteFile(pipe, pipe_resp.data(), (DWORD)pipe_resp.size(), &written, NULL);

                    char pm[128];
                    sprintf_s(pm, ">> Gateway response pushed to pipe  (%lu bytes)", written);
                    log_ex(LOG_OK, pm);

                    // Also try sending as raw protobuf
                    Sleep(50);
                    WriteFile(pipe, gw_response.data(), (DWORD)gw_response.size(), &written, NULL);
                    sprintf_s(pm, ">> RAW gateway response pushed  (%lu bytes)", written);
                    log_ex(LOG_OK, pm);
                }
                else {
                    log_ex(LOG_WARN, ">> Gateway request failed — falling back to captured blobs...");
                    // Fallback: push captured protobuf data
                    WriteFile(pipe, g_auth_data, (DWORD)g_auth_data_size, &written, NULL);
                    WriteFile(pipe, g_request_proto, (DWORD)g_request_proto_size, &written, NULL);
                    WriteFile(pipe, g_response_proto, (DWORD)g_response_proto_size, &written, NULL);
                    log_ex(LOG_WARN, ">> Captured blobs pushed as fallback");
                }
            }

            response.clear();
            break;
        }

        case 5: {
            // Gateway Session Response — respond with captured response protobuf
            log_ex(LOG_OK, ">> Gateway RESPONSE (type 5) — replaying response protobuf...");
            VanguardHeader rh = { 0 };
            rh.magic = hdr->magic + 1;
            rh.total_size = (uint32_t)(sizeof(VanguardHeader) + g_response_proto_size);
            rh.message_type = 1;
            rh.payload_size = (uint32_t)g_response_proto_size;
            response.insert(response.end(), (uint8_t*)&rh, (uint8_t*)&rh + sizeof(rh));
            response.insert(response.end(), g_response_proto, g_response_proto + g_response_proto_size);
            char sm2[128];
            sprintf_s(sm2, "Response proto sent  (%zu bytes)", g_response_proto_size);
            log_ex(LOG_OK, sm2);
            break;
        }

        case 1:
            log_ex(LOG_HEART, "Heartbeat patched");
            response = create_heartbeat_response(buffer.data(), bytesRead);
            break;

        default: {
            // Unknown type — check payload for RG gateway markers before echo
            if (has_rg_marker && payload_len >= 2) {
                uint8_t proto_field = (payload[0] == 0x08) ? payload[1] : 0;
                char dm[128];
                sprintf_s(dm, ">> Unknown type %u with RG marker (proto=%u) — gateway relay...", hdr->message_type, proto_field);
                log_ex(LOG_OK, dm);

                const uint8_t* blob = nullptr;
                size_t blob_sz = 0;
                if (proto_field == 0x03) { blob = g_auth_data; blob_sz = g_auth_data_size; }
                else if (proto_field == 0x04) { blob = g_request_proto; blob_sz = g_request_proto_size; }
                else if (proto_field == 0x05) { blob = g_response_proto; blob_sz = g_response_proto_size; }

                if (blob) {
                    VanguardHeader rh = { 0 };
                    rh.magic = hdr->magic + 1;
                    rh.total_size = (uint32_t)(sizeof(VanguardHeader) + blob_sz);
                    rh.message_type = 1;
                    rh.payload_size = (uint32_t)blob_sz;
                    response.insert(response.end(), (uint8_t*)&rh, (uint8_t*)&rh + sizeof(rh));
                    response.insert(response.end(), blob, blob + blob_sz);
                }
                else {
                    response = create_heartbeat_response(buffer.data(), bytesRead);
                }
            }
            else {
                response = create_heartbeat_response(buffer.data(), bytesRead);
            }
            break;
        }
        }

        if (!response.empty()) {
            DWORD written;
            WriteFile(pipe, response.data(), (DWORD)response.size(), &written, NULL);
            log_ex(LOG_SEND, "Response sent");
        }
        Sleep(10);
    }

    CloseHandle(pipe);
    g_current_pipe.store(nullptr);
    print_separator("CONNECTION CLOSED");
}

// ================================================================
//  PIPE SERVER
// ================================================================
void create_named_pipe() {
    while (!shutdown_event.load()) {
        HANDLE pipe = CreateNamedPipeW(PIPE_NAME, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, 1048576, 1048576, 500, NULL);

        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        log_ex(LOG_PIPE, "Named pipe created — awaiting client...");

        if (ConnectNamedPipe(pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            log_ex(LOG_CONN, "Client connected to pipe!");
            stop_spinner();
            std::thread(handle_client, pipe).detach();
        }
        else {
            CloseHandle(pipe);
        }
    }
}

// ================================================================
//  VALORANT DETECTION
// ================================================================
bool is_valorant_running() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) do {
        if (_wcsicmp(pe.szExeFile, L"VALORANT-Win64-Shipping.exe") == 0) {
            CloseHandle(snap);
            return true;
        }
    } while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    return false;
}

// ================================================================
//  CTRL HANDLER
// ================================================================
BOOL WINAPI ConsoleHandler(DWORD dwType) {
    if (dwType == CTRL_C_EVENT || dwType == CTRL_CLOSE_EVENT) {
        shutdown_event.store(true);
        stop_spinner();
        return TRUE;
    }
    return FALSE;
}

// ================================================================
//  MAIN
// ================================================================
int main(int argc, char* argv[]) {
    if (!is_user_admin()) {
        relaunch_as_admin(argc, argv);
        return 0;
    }

    enable_ansi();
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    // Hide console window
    HWND consoleWnd = GetConsoleWindow();
    if (consoleWnd) ShowWindow(consoleWnd, SW_HIDE);

    // Start overlay thread, which creates the overlay window and processes hotkeys.
    std::thread overlay_thread(overlay_message_pump);

    while (!g_overlay_hwnd && !shutdown_event.load()) Sleep(10);
    Sleep(200);

    try {
        update_overlay_text("Imperium.sys - VGC Bypass\n\nInitializing VGC service...");
        
        // Stop VGC and initialize
        system("sc stop vgc 2>NUL");
        Sleep(500);
        system("sc start vgc 2>NUL");
        Sleep(1000);

        update_overlay_text("Imperium.sys - VGC Bypass\n\nWaiting for Valorant...");

        log_ex(LOG_OK, "Suspending DNS cache...");
        toggle_dns_cache(false);
        
        log_ex(LOG_WARN, "[POPUP] Preparing bypass logic...");
        stop_and_restart_vgc();
        override_vgc_pipe();

        char verline[128];
        sprintf_s(verline, "Protocol version: v%d", CURRENT_VERSION);
        log_ex(LOG_OK, verline);
        log_ex(LOG_OK, "Pipe server starting...");

        std::thread(create_named_pipe).detach();

        log_ex(LOG_WARN, "Waiting for Valorant...");
        start_spinner(">> Waiting for Valorant to launch...");
        
        while (!is_valorant_running() && !shutdown_event.load()) Sleep(500);
        stop_spinner();

        if (!shutdown_event.load()) {
            log_ex(LOG_OK, "Valorant detected! ACTIVE");
            Beep(800, 150);
            Beep(1200, 150);
            log_ex(LOG_WARN, "[POPUP] Waiting for VGC readiness...");
            
            if (wait_for_vgc_ready()) {
                log_ex(LOG_OK, "[POPUP] VGC ready, restoring DNS cache...");
                if (toggle_dns_cache(true)) {
                    log_ex(LOG_OK, "[POPUP] Popup bypass COMPLETE!");
                    update_overlay_text("Imperium.sys - VGC Bypass\n\n[+] Bypass Active!\nF9 = Hide\nF10 = Exit");
                } else {
                    log_ex(LOG_WARN, "[POPUP] Failed to restore DNS cache.");
                }
            } else {
                log_ex(LOG_WARN, "[POPUP] VGC readiness timed out.");
            }
        }

        while (!shutdown_event.load()) {
            Sleep(100);
        }

        update_overlay_text("Imperium.sys - VGC Bypass\n\nCleaning Up.. (10%)");
        Sleep(300);
        update_overlay_text("Imperium.sys - VGC Bypass\n\nCleaning Up.. (20%)");
        Sleep(300);
        update_overlay_text("Imperium.sys - VGC Bypass\n\nCleaning Up.. (30%)");
        Sleep(300);
        update_overlay_text("Imperium.sys - VGC Bypass\n\nCleaning Up.. (40%)");
        Sleep(300);
        update_overlay_text("Imperium.sys - VGC Bypass\n\nCleaning Up.. (50%)");
        Sleep(300);
        update_overlay_text("Imperium.sys - VGC Bypass\n\nCleaning Up.. (60%)");
        Sleep(300);
        update_overlay_text("Imperium.sys - VGC Bypass\n\nCleaning Up.. (70%)");
        Sleep(300);
        update_overlay_text("Imperium.sys - VGC Bypass\n\nCleaning Up.. (80%)");
        Sleep(300);
        update_overlay_text("Imperium.sys - VGC Bypass\n\nCleaning Up.. (90%)");
        Sleep(300);
        update_overlay_text("Imperium.sys - VGC Bypass\n\nCleaning Up.. (100%)");
        Sleep(300);
        update_overlay_text("Imperium.sys - VGC Bypass\n\nFinished Cleaning Up. Closing...");
        Sleep(1000);

        log_ex(LOG_WARN, "Shutting down...");
        Sleep(500);
        shutdown_complete.store(true);

    } catch (std::exception& e) {
        log_ex(LOG_ERROR, e.what());
        Sleep(3000);
    }

    UnregisterHotKey(NULL, 1);
    UnregisterHotKey(NULL, 2);
    
    if (g_overlay_hwnd) {
        DestroyWindow(g_overlay_hwnd);
    }
    
    overlay_thread.join();
    return 0;

    stop_title_anim();
    return 0;
}
