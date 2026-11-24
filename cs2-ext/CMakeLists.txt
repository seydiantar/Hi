add_executable(cs2-ext WIN32
    main.cpp
    offsets.cpp
    entity.cpp
    player.cpp
    esp.cpp
    menu.cpp
)
target_link_libraries(cs2-ext PRIVATE hi_shared)

# Copy bundled offset snapshots next to the exe on build so the loader can fallback to them.
# These files are produced by cs2-dumper at kindmops's PC - may be absent in this container.
foreach(JSON IN ITEMS offsets.json client_dll.json)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${JSON})
        add_custom_command(TARGET cs2-ext POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/${JSON}
                $<TARGET_FILE_DIR:cs2-ext>/${JSON})
    endif()
endforeach()
