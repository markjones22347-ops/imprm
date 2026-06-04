#include "custom.h"

#pragma warning (disable : 4244) // O_o

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

// ─── Product Storage ────────────────────────────────────────────────────────
static std::vector<std::string> g_available_products;
static std::map<std::string, std::string> g_product_links;

struct tab_anim
{
    float label_anim;
    float hovered_anim;
    float active_anim;
};

bool custom::selected(const char* label, bool tab)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    ImVec2 pos = window->DC.CursorPos;

    const ImRect rect(pos, ImVec2(pos.x + label_size.x, pos.y + label_size.y));
    ImGui::ItemSize(ImVec4(rect.Min.x, rect.Min.y, rect.Max.x, rect.Max.y), style.FramePadding.y);
    if (!ImGui::ItemAdd(rect, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(rect, id, &hovered, &held, NULL);

    static std::map <ImGuiID, tab_anim> anim;
    auto it_anim = anim.find(id);
    if (it_anim == anim.end())
    {
        anim.insert({ id, {0.6f, 0.f, 0.f} });
        it_anim = anim.find(id);
    }

    it_anim->second.label_anim = ImLerp(it_anim->second.label_anim, tab ? 0.0f : 0.6f, 0.05f * (1.0f - ImGui::GetIO().DeltaTime));
    it_anim->second.hovered_anim = ImLerp(it_anim->second.hovered_anim, hovered ? 0.2f : 0.0f, 0.03f * (1.0f - ImGui::GetIO().DeltaTime));
    it_anim->second.active_anim = ImLerp(it_anim->second.active_anim, tab ? 1.0f : 0.0f, 0.08f * (1.0f - ImGui::GetIO().DeltaTime));

    window->DrawList->AddText(ImVec2((rect.Min.x + rect.Max.x) / 2.f - (label_size.x / 2.f), (rect.Min.y + rect.Max.y) / 2.f - (label_size.y / 2.f)), ImColor(1.0f, 1.0f, 1.0f, it_anim->second.label_anim + it_anim->second.hovered_anim), label);
    window->DrawList->AddText(ImVec2((rect.Min.x + rect.Max.x) / 2.f - (label_size.x / 2.f), (rect.Min.y + rect.Max.y) / 2.f - (label_size.y / 2.f)), ImColor(accent_colour[0], accent_colour[1], accent_colour[2], it_anim->second.active_anim), label);

    return pressed;
}

// ─── Authentication ─────────────────────────────────────────────────────────

std::string custom::get_hwid()
{
    // Simplified HWID generation from CPU + Disk serial
    // In production, use WMI or Windows API for proper HWID
    return "HARDCODED_HWID_12345";
}

// ─── Simple JSON Parser ─────────────────────────────────────────────────────
static void parse_products_response(const std::string& json_response)
{
    // Simple JSON parser for {"products": [...], "links": {...}}
    g_available_products.clear();
    g_product_links.clear();
    
    // Parse products array
    size_t products_start = json_response.find("\"products\":");
    if (products_start != std::string::npos) {
        size_t array_start = json_response.find("[", products_start);
        size_t array_end = json_response.find("]", array_start);
        if (array_start != std::string::npos && array_end != std::string::npos) {
            std::string array_content = json_response.substr(array_start + 1, array_end - array_start - 1);
            size_t pos = 0;
            while ((pos = array_content.find("\"", pos)) != std::string::npos) {
                size_t end = array_content.find("\"", pos + 1);
                if (end != std::string::npos) {
                    std::string product = array_content.substr(pos + 1, end - pos - 1);
                    if (!product.empty() && product != ",") {
                        g_available_products.push_back(product);
                    }
                    pos = end + 1;
                } else break;
            }
        }
    }
    
    // Parse links object
    size_t links_start = json_response.find("\"links\":");
    if (links_start != std::string::npos) {
        size_t obj_start = json_response.find("{", links_start);
        size_t obj_end = json_response.find("}", obj_start);
        if (obj_start != std::string::npos && obj_end != std::string::npos) {
            std::string obj_content = json_response.substr(obj_start + 1, obj_end - obj_start - 1);
            size_t pos = 0;
            while ((pos = obj_content.find("\"", pos)) != std::string::npos) {
                size_t key_end = obj_content.find("\"", pos + 1);
                if (key_end != std::string::npos) {
                    std::string key = obj_content.substr(pos + 1, key_end - pos - 1);
                    size_t colon = obj_content.find(":", key_end);
                    size_t val_start = obj_content.find("\"", colon);
                    if (val_start != std::string::npos) {
                        size_t val_end = obj_content.find("\"", val_start + 1);
                        if (val_end != std::string::npos) {
                            std::string value = obj_content.substr(val_start + 1, val_end - val_start - 1);
                            g_product_links[key] = value;
                            pos = val_end + 1;
                        } else break;
                    } else break;
                } else break;
            }
        }
    }
}

bool custom::authenticate(const char* username, const char* password, const char* key, const char* hwid, char* error_out, size_t error_size)
{
    if (!username || !password || !key) {
        strncpy_s(error_out, error_size, "Missing credentials", error_size - 1);
        return false;
    }

    // Build request JSON
    std::string post_data = "{\"username\":\"" + std::string(username) + "\",\"password\":\"" + std::string(password) + "\",\"hwid\":\"" + std::string(hwid) + "\"}";

    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    BOOL bResults = FALSE;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    LPSTR pszOutBuffer = NULL;
    SIZE_T cBytesRead = 0;

    // Use WinHTTP
    hSession = WinHttpOpen(L"Imperium Loader/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    if (!hSession) {
        strncpy_s(error_out, error_size, "WinHttp init failed", error_size - 1);
        return false;
    }

    // Connect to the bot server (example: imprm.onrender.com)
    hConnect = WinHttpConnect(hSession, L"imprm.onrender.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        strncpy_s(error_out, error_size, "Connection failed", error_size - 1);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Open request to /auth endpoint
    hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/auth", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        strncpy_s(error_out, error_size, "Request creation failed", error_size - 1);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Add headers
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", -1L, WINHTTP_ADDREQ_FLAG_ADD);

    // Send request
    bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)post_data.c_str(), (DWORD)post_data.length(), (DWORD)post_data.length(), 0);
    if (!bResults) {
        strncpy_s(error_out, error_size, "Send failed", error_size - 1);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Wait for response
    bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResults) {
        strncpy_s(error_out, error_size, "Receive failed", error_size - 1);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Read response
    std::string response;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;

        pszOutBuffer = new char[dwSize + 1];
        ZeroMemory(pszOutBuffer, dwSize + 1);

        if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) break;

        response.append(pszOutBuffer, dwDownloaded);
        delete[] pszOutBuffer;
        pszOutBuffer = NULL;
    } while (dwSize > 0);

    // Cleanup
    if (pszOutBuffer) delete[] pszOutBuffer;
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Parse response
    // Success: {"products": [...], "links": {...}}
    // Error: error message string
    if (response.find("\"products\"") != std::string::npos) {
        parse_products_response(response);
        return true;
    } else if (response.find("OK") != std::string::npos) {
        // Fallback for legacy "OK" response
        g_available_products.clear();
        g_product_links.clear();
        return true;
    } else {
        strncpy_s(error_out, error_size, response.c_str(), error_size - 1);
        return false;
    }
}

void custom::save_credentials(const char* username, const char* password, const char* key)
{
    CHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        strcat_s(path, sizeof(path), "\\Imperium");
        CreateDirectoryA(path, NULL);
        strcat_s(path, sizeof(path), "\\creds.ini");

        FILE* f = NULL;
        fopen_s(&f, path, "w");
        if (f) {
            fprintf(f, "username=%s\npassword=%s\nkey=%s\n", username, password, key);
            fclose(f);
        }
    }
}

bool custom::load_credentials(char* username, char* password, char* key)
{
    CHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        strcat_s(path, sizeof(path), "\\Imperium");
        strcat_s(path, sizeof(path), "\\creds.ini");

        FILE* f = NULL;
        errno_t err = fopen_s(&f, path, "r");
        if (err == 0 && f) {
            CHAR line[512];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "username=", 9) == 0) {
                    sscanf_s(line + 9, "%63s", username, 64);
                } else if (strncmp(line, "password=", 9) == 0) {
                    sscanf_s(line + 9, "%63s", password, 64);
                } else if (strncmp(line, "key=", 4) == 0) {
                    sscanf_s(line + 4, "%63s", key, 64);
                }
            }
            fclose(f);
            return true;
        }
    }
    return false;
}

std::vector<std::string> custom::get_available_products()
{
    return g_available_products;
}

std::string custom::get_product_link(const char* product)
{
    if (g_product_links.find(product) != g_product_links.end()) {
        return g_product_links[product];
    }
    return "";
}
