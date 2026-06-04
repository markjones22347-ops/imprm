#pragma once

#include <cstdint>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

extern float accent_colour[4];

namespace custom {
    bool selected(const char* label, bool tab = false);
    bool authenticate(const char* username, const char* password, const char* key, const char* hwid, char* error_out, size_t error_size);
    std::string get_hwid();
    void save_credentials(const char* username, const char* password, const char* key);
    bool load_credentials(char* username, char* password, char* key);
    
    // Product management
    std::vector<std::string> get_available_products();
    std::string get_product_link(const char* product);
}
