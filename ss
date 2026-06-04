[33mcommit d2281b06460cb25803d004088b0d14bdada11dfa[m[33m ([m[1;36mHEAD[m[33m -> [m[1;32mmain[m[33m)[m
Author: imperium-bot <imperium@local>
Date:   Thu Jun 4 14:57:20 2026 -0700

    remove env file and update bot

 BEST VAL FT. sysinfo (1).rar                       |   Bin [31m12512224[m -> [32m0[m bytes
 loader/LICENSE.txt                                 |    21 [31m-[m
 loader/NOXY.cpp                                    |  2149 [31m---[m
 loader/backends/imgui_impl_allegro5.cpp            |   581 [31m-[m
 loader/backends/imgui_impl_allegro5.h              |    32 [31m-[m
 loader/backends/imgui_impl_android.cpp             |   276 [31m-[m
 loader/backends/imgui_impl_android.h               |    28 [31m-[m
 loader/backends/imgui_impl_dx10.cpp                |   579 [31m-[m
 loader/backends/imgui_impl_dx10.h                  |    25 [31m-[m
 loader/backends/imgui_impl_dx11.cpp                |   595 [31m-[m
 loader/backends/imgui_impl_dx11.h                  |    26 [31m-[m
 loader/backends/imgui_impl_dx12.cpp                |   747 [31m-[m
 loader/backends/imgui_impl_dx12.h                  |    38 [31m-[m
 loader/backends/imgui_impl_dx9.cpp                 |   378 [31m-[m
 loader/backends/imgui_impl_dx9.h                   |    25 [31m-[m
 loader/backends/imgui_impl_glfw.cpp                |   662 [31m-[m
 loader/backends/imgui_impl_glfw.h                  |    46 [31m-[m
 loader/backends/imgui_impl_glut.cpp                |   297 [31m-[m
 loader/backends/imgui_impl_glut.h                  |    39 [31m-[m
 loader/backends/imgui_impl_metal.h                 |    67 [31m-[m
 loader/backends/imgui_impl_metal.mm                |   566 [31m-[m
 loader/backends/imgui_impl_opengl2.cpp             |   286 [31m-[m
 loader/backends/imgui_impl_opengl2.h               |    32 [31m-[m
 loader/backends/imgui_impl_opengl3.cpp             |   871 [31m--[m
 loader/backends/imgui_impl_opengl3.h               |    55 [31m-[m
 loader/backends/imgui_impl_opengl3_loader.h        |   786 [31m--[m
 loader/backends/imgui_impl_osx.h                   |    24 [31m-[m
 loader/backends/imgui_impl_osx.mm                  |   746 [31m-[m
 loader/backends/imgui_impl_sdl.cpp                 |   564 [31m-[m
 loader/backends/imgui_impl_sdl.h                   |    36 [31m-[m
 loader/backends/imgui_impl_sdlrenderer.cpp         |   250 [31m-[m
 loader/backends/imgui_impl_sdlrenderer.h           |    29 [31m-[m
 loader/backends/imgui_impl_vulkan.cpp              |  1507 [31m---[m
 loader/backends/imgui_impl_vulkan.h                |   155 [31m-[m
 loader/backends/imgui_impl_wgpu.cpp                |   719 [31m-[m
 loader/backends/imgui_impl_wgpu.h                  |    25 [31m-[m
 loader/backends/imgui_impl_win32.cpp               |   791 [31m--[m
 loader/backends/imgui_impl_win32.h                 |    44 [31m-[m
 loader/backends/vulkan/generate_spv.sh             |     6 [31m-[m
 loader/backends/vulkan/glsl_shader.frag            |    14 [31m-[m
 loader/backends/vulkan/glsl_shader.vert            |    25 [31m-[m
 loader/docs/BACKENDS.md                            |   144 [31m-[m
 loader/docs/CHANGELOG.txt                          |  3666 [31m-----[m
 loader/docs/CONTRIBUTING.md                        |    77 [31m-[m
 loader/docs/EXAMPLES.md                            |   246 [31m-[m
 loader/docs/FAQ.md                                 |   676 [31m-[m
 loader/docs/FONTS.md                               |   401 [31m-[m
 loader/docs/README.md                              |   237 [31m-[m
 loader/docs/TODO.txt                               |   370 [31m-[m
 loader/examples/README.txt                         |     9 [31m-[m
 .../example_win32_directx9/build_win32.bat         |     8 [31m-[m
 loader/examples/example_win32_directx9/bytearray.h |   399 [31m-[m
 loader/examples/example_win32_directx9/custom.cpp  |   286 [31m-[m
 loader/examples/example_win32_directx9/custom.h    |    26 [31m-[m
 .../example_win32_directx9.vcxproj                 |   177 [31m-[m
 .../example_win32_directx9.vcxproj.filters         |    75 [31m-[m
 .../example_win32_directx9.vcxproj.user            |     4 [31m-[m
 loader/examples/example_win32_directx9/globals.cpp |     6 [31m-[m
 loader/examples/example_win32_directx9/globals.h   |    53 [31m-[m
 loader/examples/example_win32_directx9/imgui.ini   |    10 [31m-[m
 loader/examples/example_win32_directx9/main.cpp    |   340 [31m-[m
 loader/examples/imgui_examples.sln                 |    31 [31m-[m
 loader/examples/libs/glfw/COPYING.txt              |    22 [31m-[m
 loader/examples/libs/glfw/include/GLFW/glfw3.h     |  4227 [31m------[m
 .../examples/libs/glfw/include/GLFW/glfw3native.h  |   456 [31m-[m
 loader/examples/libs/usynergy/README.txt           |     8 [31m-[m
 loader/examples/libs/usynergy/uSynergy.c           |   636 [31m-[m
 loader/examples/libs/usynergy/uSynergy.h           |   420 [31m-[m
 loader/gateway_data.h                              |   959 [31m--[m
 loader/imconfig.h                                  |   125 [31m-[m
 loader/imgui.cpp                                   | 13499 [31m-------------------[m
 loader/imgui.h                                     |  3110 [31m-----[m
 loader/imgui_demo.cpp                              |  8144 [31m-----------[m
 loader/imgui_draw.cpp                              |  5133 [31m-------[m
 loader/imgui_internal.h                            |  2980 [31m----[m
 loader/imgui_tables.cpp                            |  4068 [31m------[m
 loader/imgui_widgets.cpp                           |  8456 [31m------------[m
 loader/imstb_rectpack.h                            |   627 [31m-[m
 loader/imstb_textedit.h                            |  1447 [31m--[m
 loader/imstb_truetype.h                            |  5085 [31m-------[m
 loader/misc/README.txt                             |    23 [31m-[m
 loader/misc/cpp/README.txt                         |    13 [31m-[m
 loader/misc/cpp/imgui_stdlib.cpp                   |    72 [31m-[m
 loader/misc/cpp/imgui_stdlib.h                     |    18 [31m-[m
 loader/misc/debuggers/README.txt                   |    16 [31m-[m
 loader/misc/debuggers/imgui.gdb                    |    12 [31m-[m
 loader/misc/debuggers/imgui.natstepfilter          |    30 [31m-[m
 loader/misc/debuggers/imgui.natvis                 |    58 [31m-[m
 loader/misc/fonts/Cousine-Regular.ttf              |   Bin [31m43912[m -> [32m0[m bytes
 loader/misc/fonts/DroidSans.ttf                    |   Bin [31m190044[m -> [32m0[m bytes
 loader/misc/fonts/Karla-Regular.ttf                |   Bin [31m16848[m -> [32m0[m bytes
 loader/misc/fonts/ProggyClean.ttf                  |   Bin [31m41208[m -> [32m0[m bytes
 loader/misc/fonts/ProggyTiny.ttf                   |   Bin [31m35656[m -> [32m0[m bytes
 loader/misc/fonts/Roboto-Medium.ttf                |   Bin [31m162588[m -> [32m0[m bytes
 loader/misc/fonts/binary_to_compressed_c.cpp       |   388 [31m-[m
 loader/misc/freetype/README.md                     |    37 [31m-[m
 loader/misc/freetype/imgui_freetype.cpp            |   779 [31m--[m
 loader/misc/freetype/imgui_freetype.h              |    50 [31m-[m
 loader/misc/single_file/imgui_single_file.h        |    18 [31m-[m
 media/IMPRM banner.png                             |   Bin [31m994758[m -> [32m0[m bytes
 media/IMPRM.png                                    |   Bin [31m918029[m -> [32m0[m bytes
 media/Logo.png                                     |   Bin [31m542873[m -> [32m0[m bytes
 popup and meu src/NOXY.cpp                         |  1571 [31m---[m
 popup and meu src/auth_b64.txt                     |   Bin [31m6530[m -> [32m0[m bytes
 popup and meu src/auth_blob.h                      |   158 [31m-[m
 popup and meu src/gateway_data.h                   |   959 [31m--[m
 popup and meu src/p.py                             |   296 [31m-[m
 popup and meu src/popup_bypass.spec                |    38 [31m-[m
 popup and meu src/python                           |     0