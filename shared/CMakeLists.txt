add_library(hi_shared STATIC
    mem.cpp
    scanner.cpp
    wts.cpp
    log.cpp
    config.cpp
    overlay.cpp
    menu.cpp
)
target_include_directories(hi_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hi_shared
    PUBLIC
        imgui
        nlohmann_json::nlohmann_json
        d3d11
        dxgi
        d3dcompiler
        dwmapi
        winhttp
        shlwapi
)
