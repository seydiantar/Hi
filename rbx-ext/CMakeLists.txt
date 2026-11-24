add_executable(rbx-ext WIN32
    main.cpp
    sigs.cpp
    instance.cpp
    player.cpp
    esp.cpp
    menu.cpp
)
target_link_libraries(rbx-ext PRIVATE hi_shared)
