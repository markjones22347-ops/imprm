// Dear ImGui: standalone example application for DirectX 9
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <d3d9.h>
#include <tchar.h>
#include <vector>
#include <string>

#include "globals.h"
#include "custom.h"
#include "bytearray.h"

// Defines
#define ALPHA (ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_PickerHueBar)
#define NO_ALPHA (ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_PickerHueBar)

// Fonts
ImFont* tab_title;
ImFont* icon;
ImFont* small_icon;

// Data
static LPDIRECT3D9              g_pD3D = NULL;
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Dear ImGui DirectX9 Example"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // Load Fonts
    ImFontConfig font_config;
    font_config.PixelSnapH = false;
    font_config.OversampleH = 5;
    font_config.OversampleV = 5;
    font_config.RasterizerMultiply = 1.2f;

    static const ImWchar ranges[] =
    {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
        0x2DE0, 0x2DFF, // Cyrillic Extended-A
        0xA640, 0xA69F, // Cyrillic Extended-B
        0xE000, 0xE226, // icons
        0,
    };

    font_config.GlyphRanges = ranges;

    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdana.ttf", 14.0f, &font_config, ranges);
    tab_title = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arialbd.ttf", 17.0f, &font_config, ranges);
    icon = io.Fonts->AddFontFromMemoryTTF(loadericon, sizeof(loadericon), 14.0f, &font_config, ranges);
    small_icon = io.Fonts->AddFontFromMemoryTTF(loadericon, sizeof(loadericon), 11.0f, &font_config, ranges);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Load saved credentials if available
    if (custom::load_credentials(username, password, key)) {
        remember_me = true;
    }

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(500 * dpi_scale, 300 * dpi_scale));
        ImGui::Begin("loader", nullptr, ImGuiWindowFlags_NoDecoration);
        {
            auto draw = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetWindowPos();
            ImVec2 size = ImGui::GetWindowSize();
            ImVec2 cheat_name_size = ImGui::CalcTextSize(cheat_name);

            draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + 23), ImColor(0, 0, 0), 8.0f, ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersTopRight);

            draw->AddText(icon, 14.0f, ImVec2(pos.x + 9, pos.y + 4), ImColor(accent_colour[0], accent_colour[1], accent_colour[2]), cheat_icon_symbol);
            draw->AddText(ImVec2(pos.x + 27, pos.y + 4), ImColor(200, 200, 200), cheat_name);
            draw->AddText(ImVec2(pos.x + 27 + cheat_name_size.x, pos.y + 4), ImColor(accent_colour[0], accent_colour[1], accent_colour[2]), cheat_domain);

            draw->AddLine(ImVec2(pos.x, pos.y + 23), ImVec2(pos.x + size.x, pos.y + 23), ImColor(46, 46, 46)); // upper line
            draw->AddRectFilledMultiColor(ImVec2(pos.x, pos.y + 24), ImVec2(pos.x + size.x, pos.y + 34), ImColor(0, 0, 0), ImColor(0, 0, 0), ImColor(0, 0, 0, 0), ImColor(0, 0, 0, 0)); // upper 'glow'

            ImGui::SetCursorPos(ImVec2(size.x - 15, 3));
            custom::selected("x");

            ImGui::SetCursorPos(ImVec2(size.x - 31, 5));
            ImGui::PushFont(small_icon);
            if (custom::selected("U", 2 == tab)) {
                if (tab != 2) old_tab = tab;
                tab == 2 ? tab = old_tab, content_animation = 0.0f : tab = 2, content_animation = 0.0f;
            }
            ImGui::PopFont();

            if (update_on_f5) { if (GetAsyncKeyState(VK_F5)) content_animation = 0.0f; } // lol
            content_animation = ImLerp(content_animation, content_animation < 1.0f ? 1.0f : 0.0f, 0.07f * (1.0f - ImGui::GetIO().DeltaTime));

            if (tab == 0) {
                draw->AddText(tab_title, 17.0f, ImVec2(pos.x + size.x / 2 - ImGui::CalcTextSize("Authorization").x / 2, pos.y + 45 * content_animation), ImColor(0.8f, 0.8f, 0.8f, content_animation), "Authorization");

                ImGui::SetCursorPos(ImVec2(155 * dpi_scale, 85 * content_animation));

                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, content_animation);
                ImGui::BeginChild("##auth_content", ImVec2(size.x / 2 - 40, size.y - 140));
                {
                    ImGui::InputText("Username", username, sizeof(username));
                    ImGui::InputText("Password", password, sizeof(password), ImGuiInputTextFlags_Password);
                    ImGui::InputText("Key", key, sizeof(key));
                    
                    if (auth_error[0] != '\0') {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", auth_error);
                    }
                    
                    if (ImGui::Button("Submit", ImVec2(162 * dpi_scale, 25 * dpi_scale))) {
                        // Authenticate with bot
                        std::string hw = custom::get_hwid();
                        if (custom::authenticate(username, password, key, hw.c_str(), auth_error, sizeof(auth_error))) {
                            auth_error[0] = '\0';
                            if (remember_me) {
                                custom::save_credentials(username, password, key);
                            }
                            tab = 1;
                            content_animation = 0.0f;
                        } else {
                            // Error is already in auth_error
                        }
                    }
                    ImGui::Checkbox("Remember me", &remember_me);
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
            }
            else if (tab == 1) {
                draw->AddText(tab_title, 17.0f, ImVec2(pos.x + size.x / 2 - ImGui::CalcTextSize("Panel").x / 2, pos.y + 45 * content_animation), ImColor(0.8f, 0.8f, 0.8f, content_animation), "Panel");
                draw->AddText(ImVec2(pos.x + 9, pos.y + size.y - 22), ImColor(0.8f, 0.8f, 0.8f, content_animation), "welcome back,");
                draw->AddText(ImVec2(pos.x + 11 + ImGui::CalcTextSize("welcome back,").x, pos.y + size.y - 22), ImColor(accent_colour[0], accent_colour[1], accent_colour[2], content_animation), username);

                ImGui::SetCursorPos(ImVec2(size.x - 32 * dpi_scale, size.y - 32 * dpi_scale));
                ImGui::PushFont(icon);
                if (ImGui::Button("P", ImVec2(25 * dpi_scale, 25 * dpi_scale))) tab = 0, content_animation = 0.0f;
                ImGui::PopFont();

                ImGui::SetCursorPos(ImVec2(size.x - 123 * dpi_scale, size.y - 32 * dpi_scale));
                ImGui::Button("Launch", ImVec2(85 * dpi_scale, 25 * dpi_scale));

                ImGui::SetCursorPos(ImVec2(155 * dpi_scale, 85 * content_animation));

                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, content_animation);
                ImGui::BeginChild("##panel_content", ImVec2(size.x / 2 - 40, size.y - 140));
                {
                    ImGui::Combo("Select a game", &game, games_list, IM_ARRAYSIZE(games_list));
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
            }
            else if (tab == 2) {
                draw->AddText(tab_title, 17.0f, ImVec2(pos.x + size.x / 2 - ImGui::CalcTextSize("Settings").x / 2, pos.y + 45 * content_animation), ImColor(0.8f, 0.8f, 0.8f, content_animation), "Settings");

                ImGui::SetCursorPos(ImVec2(155 * dpi_scale, 85 * content_animation));

                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, content_animation);
                ImGui::BeginChild("##settings_content", ImVec2(size.x / 2 - 40, size.y - 140));
                {
                    ImGui::Checkbox("Update on F5", &update_on_f5);
                    ImGui::SliderFloat("DPI Scale", &dpi_scale, 1.0f, 1.5f, "%.3f", ImGuiSliderFlags_NoInput);
                    ImGui::Text("Accent Color"); ImGui::SameLine(); ImGui::ColorEdit4("##accent_colour", accent_colour, NO_ALPHA);
                    ImGui::SameLine();
                    if (ImGui::Button("##reset", ImVec2(37 * dpi_scale, 16))) {
                        accent_colour[0] = { 173 / 255.f };
                        accent_colour[1] = { 57 / 255.f };
                        accent_colour[2] = { 57 / 255.f };
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
            }
        }
        ImGui::End();

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x*clear_color.w*255.0f), (int)(clear_color.y*clear_color.w*255.0f), (int)(clear_color.z*clear_color.w*255.0f), (int)(clear_color.w*255.0f));
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);

        // Handle loss of D3D9 device
        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return false;

    // Create the D3DDevice
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = NULL; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            g_d3dpp.BackBufferWidth = LOWORD(lParam);
            g_d3dpp.BackBufferHeight = HIWORD(lParam);
            ResetDevice();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
