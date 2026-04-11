include(${CMAKE_SOURCE_DIR}/Engine/cmake/MarbleGameExe.cmake)

file(GLOB_RECURSE GAME_SOURCES "src/*.cpp")

add_executable(Game WIN32 ${GAME_SOURCES})

target_include_directories(Game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(Game PRIVATE Engine)

# ---------------------------------------------------------------------------
# All game identity, window, and icon settings live here.
# GameMetadata.h is auto-generated from these values.
# ---------------------------------------------------------------------------
marble_configure_executable(Game
    PRODUCT_NAME    "Pong"
    COMPANY_NAME    "Gogg Software Solutions"
    COPYRIGHT       "Copyright (C) 2026 Gogg Software Solutions"
    DESCRIPTION     "A game made with Marble Engine"
    VERSION         "1.0.0"

    WINDOW_WIDTH    1920
    WINDOW_HEIGHT   1080
    FULLSCREEN      FALSE
    VSYNC           TRUE
    RENDER_STYLE    Smooth

    ICON_ICO        "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/icon/icon.ico"
    ICON_PNG        "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/icon/icon.png"
)

add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Game>/assets
)
