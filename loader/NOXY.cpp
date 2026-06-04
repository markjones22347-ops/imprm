#include <windows.h>
#include <tchar.h>
#include <TlHelp32.h>
#include <cstdlib>
#include <thread>
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
#include <d3d9.h>
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "d3d9.lib")
#include <shellapi.h>
#include <winhttp.h>
#include <urlmon.h>
#include "gateway_data.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"
#include "examples/example_win32_directx9/custom.h"

// Local UI variables (from globals.h)
extern float accent_colour[4];
extern float content_animation;
extern float dpi_scale;

// Overlay window globals
HWND g_overlay_hwnd = NULL;
bool g_overlay_visible = false;
std::string g_overlay_log_text;
std::mutex g_overlay_mutex;

// ImGui globals
HWND g_hwnd = NULL;
IDirect3D9* g_pD3D = NULL;
IDirect3DDevice9* g_pd3dDevice = NULL;
D3DPRESENT_PARAMETERS g_d3dpp = {0};

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

// Process tracking
PROCESS_INFORMATION g_emu_pi = { 0 };
PROCESS_INFORMATION g_popup_pi = { 0 };
bool g_emu_running = false;
bool g_popup_running = false;

// Dynamic product management
std::map<std::string, PROCESS_INFORMATION> g_product_processes;
std::map<std::string, bool> g_product_running;
std::vector<std::string> g_available_products;
std::map<std::string, std::string> g_product_urls;

// Downloaded file tracking for cleanup
std::vector<std::string> g_downloaded_files;

// Log system for UI
std::vector<std::string> g_log_messages;
std::mutex g_log_mutex;

// Background animation
struct Snowflake {
    float x, y;
    float speed;
    float size;
};
std::vector<Snowflake> g_snowflakes;
float g_time_accumulator = 0.0f;

// UI animation
float g_animation_progress = 0.0f;
bool g_animation_complete = false;

// Modal states
bool g_emu_modal_open = false;
bool g_popup_modal_open = false;
bool g_download_in_progress = false;
bool g_launch_success = false;
float g_download_progress = 0.0f;

// Authentication panel globals
bool g_authenticated = false;
char g_auth_username[64] = "";
char g_auth_password[64] = "";
char g_auth_key[64] = "";
char g_auth_hwid[128] = "";
char g_auth_error[256] = "";
bool g_auth_remember_me = false;
bool g_auth_loading = false;

void add_log(const char* message) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_log_messages.push_back(message);
    if (g_log_messages.size() > 50) {
        g_log_messages.erase(g_log_messages.begin());
    }
}

// Forward declarations
void load_available_products();

void render_auth_panel(ImVec2 size, ImVec2 pos) {
    // Centered auth panel
    ImVec2 panel_size(340, 300);
    ImVec2 panel_pos(pos.x + (size.x - panel_size.x) / 2.0f, pos.y + (size.y - panel_size.y) / 2.0f);
    
    ImGui::SetCursorPos(ImVec2(panel_pos.x - pos.x, panel_pos.y - pos.y));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.15f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(accent_colour[0], accent_colour[1], accent_colour[2], 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
    
    ImGui::BeginChild("AuthPanel", panel_size, true);
    {
        ImGui::TextColored(ImVec4(accent_colour[0], accent_colour[1], accent_colour[2], 1.0f), "Account Login");
        ImGui::Spacing();
        
        ImGui::PushItemWidth(310);
        ImGui::Text("Username:");
        ImGui::InputText("##username", g_auth_username, sizeof(g_auth_username));
        ImGui::Spacing();
        
        ImGui::Text("Password:");
        ImGui::InputText("##password", g_auth_password, sizeof(g_auth_password), ImGuiInputTextFlags_Password);
        ImGui::Spacing();
        
        ImGui::Text("License Key:");
        ImGui::InputText("##key", g_auth_key, sizeof(g_auth_key));
        ImGui::PopItemWidth();
        
        if (strlen(g_auth_error) > 0) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), g_auth_error);
        }
        
        ImGui::Spacing();
        ImGui::Checkbox("##remember_me", &g_auth_remember_me);
        ImGui::SameLine();
        ImGui::Text("Remember Me");
        
        ImGui::Separator();
        
        if (g_auth_loading) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
            ImGui::Button("Authenticating...", ImVec2(300, 40));
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent_colour[0], accent_colour[1], accent_colour[2], 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent_colour[0] * 1.2f, accent_colour[1] * 1.2f, accent_colour[2] * 1.2f, 1.0f));
            if (ImGui::Button("Login", ImVec2(300, 40))) {
                g_auth_loading = true;
                if (custom::authenticate(g_auth_username, g_auth_password, g_auth_key, "LOADER_HWID", g_auth_error, sizeof(g_auth_error))) {
                    g_authenticated = true;
                    load_available_products();
                    if (g_auth_remember_me) {
                        custom::save_credentials(g_auth_username, g_auth_password, g_auth_key);
                    }
                    memset(g_auth_error, 0, sizeof(g_auth_error));
                } else {
                    g_authenticated = false;
                }
                g_auth_loading = false;
            }
            ImGui::PopStyleColor(2);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void init_snowflakes(int count) {
    g_snowflakes.clear();
    for (int i = 0; i < count; i++) {
        Snowflake s;
        s.x = (float)(rand() % 1000);
        s.y = (float)(rand() % 1000);
        s.speed = 20.0f + (float)(rand() % 50);
        s.size = 1.0f + (float)(rand() % 3);
        g_snowflakes.push_back(s);
    }
}

void load_available_products() {
    g_available_products = custom::get_available_products();
    for (const auto& product : g_available_products) {
        std::string url = custom::get_product_link(product.c_str());
        if (!url.empty()) {
            g_product_urls[product] = url;
        }
    }
}

void update_snowflakes(float delta_time, float screen_width, float screen_height) {
    g_time_accumulator += delta_time;
    for (auto& s : g_snowflakes) {
        s.y += s.speed * delta_time;
        if (s.y > screen_height) {
            s.y = -10.0f;
            s.x = (float)(rand() % (int)screen_width);
        }
    }
}

void render_snowflakes(ImDrawList* draw, ImVec2 offset) {
    for (const auto& s : g_snowflakes) {
        draw->AddCircleFilled(ImVec2(offset.x + s.x, offset.y + s.y), s.size, ImColor(accent_colour[0], accent_colour[1], accent_colour[2], 0.3f));
    }
}

// Forward declaration
void terminate_process(PROCESS_INFORMATION* pi, bool* running_flag, const char* name);

void render_modal(const char* title, bool* modal_open) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(300, 120), ImGuiCond_Always);
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.15f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(accent_colour[0], accent_colour[1], accent_colour[2], 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.15f, 0.15f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.15f, 0.15f, 0.2f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    
    if (ImGui::Begin(title, modal_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        
        ImGui::Text("%s", title);
        ImGui::Spacing();
        
        if (g_download_in_progress) {
            // Update progress (3 seconds total)
            g_download_progress += ImGui::GetIO().DeltaTime / 3.0f;
            if (g_download_progress > 1.0f) g_download_progress = 1.0f;
            
            // Progress bar
            ImGui::ProgressBar(g_download_progress, ImVec2(280, 20));
            ImGui::Spacing();
            ImGui::Text("Downloading...");
        } else if (g_launch_success) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Success!");
            ImGui::Spacing();
            
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(accent_colour[0], accent_colour[1], accent_colour[2], 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent_colour[0], accent_colour[1], accent_colour[2], 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent_colour[0] * 1.2f, accent_colour[1] * 1.2f, accent_colour[2] * 1.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(accent_colour[0] * 0.8f, accent_colour[1] * 0.8f, accent_colour[2] * 0.8f, 1.0f));
            
            if (ImGui::Button("Close", ImVec2(100, 30))) {
                *modal_open = false;
                g_download_progress = 0.0f;
            }
            
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
        } else {
            ImGui::Text("Failed to launch.");
            ImGui::Spacing();
            
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(accent_colour[0], accent_colour[1], accent_colour[2], 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent_colour[0], accent_colour[1], accent_colour[2], 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent_colour[0] * 1.2f, accent_colour[1] * 1.2f, accent_colour[2] * 1.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(accent_colour[0] * 0.8f, accent_colour[1] * 0.8f, accent_colour[2] * 0.8f, 1.0f));
            
            if (ImGui::Button("Close", ImVec2(100, 30))) {
                *modal_open = false;
                g_download_progress = 0.0f;
            }
            
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
        }
    } else {
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }
    ImGui::End();
}

// Download URLs
const char* EMU_URL = "https://cdn.discordapp.com/attachments/1511509584790159400/1511847610468536360/Imperium.exe?ex=6a21f14f&is=6a209fcf&hm=0bba2741cde0888cba62046ff629af140fbebc850e5fe7f77dd66bcc08598d17&";
const char* POPUP_URL = "https://cdn.discordapp.com/attachments/1496318686737203342/1511724616450572390/Imperium_Bypass.exe?ex=6a217ec3&is=6a202d43&hm=a273a997c4487e521503b83468e0bca3fa21249b38eac0fb0c73645aea09faca&";

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
                if (line.rfind(titleText, 0) == 0) continue;
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
        // Make layered overlay always on top, hidden from taskbar/Alt+Tab, and do not activate focus.
        SetWindowLongA(g_overlay_hwnd, GWL_EXSTYLE, 
            GetWindowLongA(g_overlay_hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
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
//  DIRECTX 9 SETUP
// ================================================================
bool CreateDeviceD3D(HWND hWnd) {
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return false;

    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D() {
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = NULL; }
}

void ResetDevice() {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3D_OK) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
}

// ================================================================
//  DOWNLOAD & EXECUTE
// ================================================================
bool download_and_run(const char* url, const char* filename, PROCESS_INFORMATION* pi) {
    char temp_path[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_path);
    char file_path[MAX_PATH];
    sprintf_s(file_path, "%s%s", temp_path, filename);

    char log_msg[512];
    sprintf_s(log_msg, "[>] Downloading: %s", filename);
    add_log(log_msg);

    HRESULT hr = URLDownloadToFileA(NULL, url, file_path, 0, NULL);
    if (SUCCEEDED(hr)) {
        sprintf_s(log_msg, "[+] Download complete: %s", filename);
        add_log(log_msg);
        
        // Track file for cleanup
        g_downloaded_files.push_back(file_path);
        
        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOW;

        if (CreateProcessA(file_path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, pi)) {
            add_log("[+] Process started successfully");
            return true;
        } else {
            add_log("[!] Failed to start process");
            return false;
        }
    } else {
        sprintf_s(log_msg, "[!] Download failed (HRESULT: 0x%X)", hr);
        add_log(log_msg);
        return false;
    }
}

void terminate_process(PROCESS_INFORMATION* pi, bool* running_flag, const char* name) {
    if (*running_flag && pi->hProcess != NULL) {
        char log_msg[128];
        sprintf_s(log_msg, "[!] Terminating %s...", name);
        add_log(log_msg);
        
        TerminateProcess(pi->hProcess, 0);
        CloseHandle(pi->hProcess);
        CloseHandle(pi->hThread);
        pi->hProcess = NULL;
        pi->hThread = NULL;
        *running_flag = false;
        
        sprintf_s(log_msg, "[+] %s terminated", name);
        add_log(log_msg);
    } else {
        char log_msg[128];
        sprintf_s(log_msg, "[*] %s is not running", name);
        add_log(log_msg);
    }
}

void cleanup_downloaded_files() {
    char log_msg[512];
    for (const auto& file : g_downloaded_files) {
        if (DeleteFileA(file.c_str())) {
            sprintf_s(log_msg, "[*] Deleted: %s", file.c_str());
            add_log(log_msg);
        }
    }
    g_downloaded_files.clear();
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

    // Option 1 - Emu
    if (g_emu_running) {
        std::cout << "  " BLUE_NEON "|" RESET
            << "   " NEON_RED BOLD "  [1]" RESET "  " WHITE ">  " NEON_RED "Terminate Emu" RESET
            << "                        " BLUE_NEON "|" RESET "\n";
    } else {
        std::cout << "  " BLUE_NEON "|" RESET
            << "   " NEON_GREEN BOLD "  [1]" RESET "  " WHITE ">  " NEON_GREEN "Emu" RESET
            << "                              " BLUE_NEON "|" RESET "\n";
    }

    std::cout << "  " BLUE_NEON "|" RESET
        << "                                                " BLUE_NEON "|" RESET "\n";

    // Option 2 - Popup
    if (g_popup_running) {
        std::cout << "  " BLUE_NEON "|" RESET
            << "   " NEON_RED BOLD "  [2]" RESET "  " WHITE ">  " NEON_RED "Terminate Popup" RESET
            << "                      " BLUE_NEON "|" RESET "\n";
    } else {
        std::cout << "  " BLUE_NEON "|" RESET
            << "   " NEON_YELLOW BOLD "  [2]" RESET "  " WHITE ">  " NEON_YELLOW "Popup" RESET
            << "                            " BLUE_NEON "|" RESET "\n";
    }

    std::cout << "  " BLUE_NEON "|" RESET
        << "                                                " BLUE_NEON "|" RESET "\n";

    // Option 3 - Exit
    std::cout << "  " BLUE_NEON "|" RESET
        << "   " NEON_RED BOLD "  [3]" RESET "  " WHITE ">  " NEON_RED "Exit" RESET
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
//  IMGUI UI
// ================================================================
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            g_d3dpp.BackBufferWidth = LOWORD(lParam);
            g_d3dpp.BackBufferHeight = HIWORD(lParam);
            ResetDevice();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_NCHITTEST:
        // Only allow dragging from the header area (top 40px)
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        ScreenToClient(hWnd, &pt);
        if (pt.y < 40)
            return HTCAPTION;
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void RenderImGuiUI() {
    // Update animation (smoother)
    if (!g_animation_complete) {
        g_animation_progress += ImGui::GetIO().DeltaTime * 2.0f;
        if (g_animation_progress >= 1.0f) {
            g_animation_progress = 1.0f;
            g_animation_complete = true;
        }
    }
    
    // Smoothstep easing for smooth motion
    float ease = g_animation_progress * g_animation_progress * (3.0f - 2.0f * g_animation_progress);
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    ImGui::Begin("Imperium Loader", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    {
        ImGui::PopStyleVar(3);
        
        auto draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        
        // Background with snowflakes
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImColor(10, 10, 15));
        update_snowflakes(ImGui::GetIO().DeltaTime, size.x, size.y);
        render_snowflakes(draw, pos);
        
        // Custom header (increased height to 60)
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + 60), ImColor(15, 15, 20), 0.0f);
        
        // Center header and subtitle
        ImVec2 title_size = ImGui::CalcTextSize("Imperium Loader - Release");
        ImVec2 subtitle_size = ImGui::CalcTextSize(".gg/eX5Kc9ap");
        
        float title_x = pos.x + (size.x - title_size.x) / 2.0f;
        float subtitle_x = pos.x + (size.x - subtitle_size.x) / 2.0f;
        
        // Animate everything slide from top with fade
        float offset = (1.0f - ease) * -100.0f;
        float alpha = ease;
        
        // Header with glow
        ImVec2 title_pos = ImVec2(title_x, pos.y + 12 + offset);
        ImVec2 subtitle_pos = ImVec2(subtitle_x, pos.y + 32 + offset);
        
        // Glow effect (multiple layers with decreasing opacity)
        for (int i = 3; i >= 0; i--) {
            float glow_alpha = alpha * 0.15f * (1.0f - i * 0.25f);
            draw->AddText(ImVec2(title_pos.x - i, title_pos.y - i), ImColor(accent_colour[0], accent_colour[1], accent_colour[2], glow_alpha), "Imperium Loader - Release");
            draw->AddText(ImVec2(title_pos.x + i, title_pos.y - i), ImColor(accent_colour[0], accent_colour[1], accent_colour[2], glow_alpha), "Imperium Loader - Release");
            draw->AddText(ImVec2(title_pos.x - i, title_pos.y + i), ImColor(accent_colour[0], accent_colour[1], accent_colour[2], glow_alpha), "Imperium Loader - Release");
            draw->AddText(ImVec2(title_pos.x + i, title_pos.y + i), ImColor(accent_colour[0], accent_colour[1], accent_colour[2], glow_alpha), "Imperium Loader - Release");
        }
        
        draw->AddText(title_pos, ImColor(accent_colour[0], accent_colour[1], accent_colour[2], alpha), "Imperium Loader - Release");
        draw->AddText(subtitle_pos, ImColor(0.5f, 0.5f, 0.5f, alpha), ".gg/eX5Kc9ap");
        draw->AddLine(ImVec2(pos.x, pos.y + 60), ImVec2(pos.x + size.x, pos.y + 60), ImColor(accent_colour[0], accent_colour[1], accent_colour[2]));
        
        // Authentication Panel or Main UI
        if (!g_authenticated) {
            render_auth_panel(size, pos);
        } else {
            // Main content area with slide animation from top
            // Render product buttons dynamically based on available products
            ImGui::SetCursorPos(ImVec2(size.x / 2 - 100, 100 + offset));
        
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(accent_colour[0], accent_colour[1], accent_colour[2], 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent_colour[0], accent_colour[1], accent_colour[2], 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent_colour[0] * 1.2f, accent_colour[1] * 1.2f, accent_colour[2] * 1.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(accent_colour[0] * 0.8f, accent_colour[1] * 0.8f, accent_colour[2] * 0.8f, 1.0f));
        
            // Render dynamic product buttons
            float button_y = 100 + offset;
            for (const auto& product : g_available_products) {
                ImGui::SetCursorPos(ImVec2(size.x / 2 - 100, button_y));
                
                bool is_running = g_product_running[product];
                
                if (is_running) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                    
                    std::string terminate_label = "Terminate " + product;
                    if (ImGui::Button(terminate_label.c_str(), ImVec2(200, 40))) {
                        if (g_product_processes.find(product) != g_product_processes.end()) {
                            terminate_process(&g_product_processes[product], &g_product_running[product], product.c_str());
                        }
                    }
                    ImGui::PopStyleColor(3);
                } else {
                    if (ImGui::Button(product.c_str(), ImVec2(200, 40))) {
                        if (g_product_urls.find(product) != g_product_urls.end()) {
                            std::string filename = "Imperium_" + product + ".exe";
                            if (download_and_run(g_product_urls[product].c_str(), filename.c_str(), &g_product_processes[product])) {
                                g_product_running[product] = true;
                            }
                        }
                    }
                }
                
                button_y += 60;
            }
        
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
            
            // Exit button
            ImGui::SetCursorPos(ImVec2(size.x / 2 - 100, button_y + 20));
            
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
            
            if (ImGui::Button("Exit", ImVec2(200, 40))) {
                // Terminate all running products
                for (auto& [product, running] : g_product_running) {
                    if (running && g_product_processes.find(product) != g_product_processes.end()) {
                        terminate_process(&g_product_processes[product], &g_product_running[product], product.c_str());
                    }
                }
                cleanup_downloaded_files();
            
                // Terminate any Imperium.exe processes (emu/popup)
                HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                if (snapshot != INVALID_HANDLE_VALUE) {
                    PROCESSENTRY32W pe = { sizeof(pe) };
                    if (Process32FirstW(snapshot, &pe)) {
                        do {
                            if (_wcsicmp(pe.szExeFile, L"Imperium.exe") == 0 ||
                                _wcsicmp(pe.szExeFile, L"Imperium_Bypass.exe") == 0) {
                                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                                if (hProcess) {
                                    TerminateProcess(hProcess, 0);
                                    CloseHandle(hProcess);
                                }
                            }
                        } while (Process32NextW(snapshot, &pe));
                    }
                    CloseHandle(snapshot);
                }
                
                PostQuitMessage(0);
            }
            
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
        
        }  // end of else (authenticated main UI)
        
        if (g_authenticated) {
            // Log window at bottom with title and slide animation from top
            ImGui::SetCursorPos(ImVec2(20, size.y - 200 + offset));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.04f, 0.06f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(accent_colour[0], accent_colour[1], accent_colour[2], 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::BeginChild("Logs", ImVec2(size.x - 40, 180), true);
            {
                ImGui::TextColored(ImVec4(accent_colour[0], accent_colour[1], accent_colour[2], 1.0f), "Logs:");
                ImGui::Separator();
                std::lock_guard<std::mutex> lock(g_log_mutex);
                for (const auto& msg : g_log_messages) {
                    ImGui::TextWrapped("%s", msg.c_str());
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
    }
    ImGui::End();
    
    // Render modals (disabled)
    // if (g_emu_modal_open) {
    //     render_modal("Emu", &g_emu_modal_open);
    // }
    // if (g_popup_modal_open) {
    //     render_modal("Popup", &g_popup_modal_open);
    // }
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

        char msg[256];
        sprintf_s(msg, "RECV  magic=" CYAN "0x%02X" RESET "  type=" YELLOW "%u" RESET "  total=" WHITE "%u" RESET "  payload=" WHITE "%u" RESET "  bytes=" WHITE "%u",
            hdr->magic, hdr->message_type, hdr->total_size, hdr->payload_size, bytesRead);
        log_ex(LOG_RECV, msg);

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
            log_ex(LOG_HEART, "Heartbeat  ♥");
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
            char smsg[64];
            sprintf_s(smsg, "Response dispatched  (%lu bytes)", written);
            log_ex(LOG_SEND, smsg);
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
    // Hide console window
    HWND consoleWnd = GetConsoleWindow();
    if (consoleWnd) ShowWindow(consoleWnd, SW_HIDE);
    
    if (!is_user_admin()) {
        relaunch_as_admin(argc, argv);
        return 0;
    }

    // Create window - truly borderless window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Loader"), NULL };
    RegisterClassEx(&wc);
    g_hwnd = CreateWindow(wc.lpszClassName, _T("Imperium Loader - Release"), WS_POPUP, 100, 100, 600, 500, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(g_hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // Initialize snowflakes (reduced for performance)
    init_snowflakes(30);

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    // Main loop
    MSG msg;
    while (true) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
                goto cleanup;
        }

        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX9_NewFrame();
        ImGui::NewFrame();

        RenderImGuiUI();

        ImGui::Render();
        g_pd3dDevice->BeginScene();
        g_pd3dDevice->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        g_pd3dDevice->EndScene();
        g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
    }

cleanup:
    // Cleanup
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(g_hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    
    // Cleanup downloaded files
    cleanup_downloaded_files();

    return 0;
}
