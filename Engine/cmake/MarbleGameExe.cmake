# ---------------------------------------------------------------------------
# marble_configure_executable(target
#     # Identity
#     PRODUCT_NAME    "My Game"
#     COMPANY_NAME    "My Studio"
#     COPYRIGHT       "Copyright (C) 2026 My Studio"
#     DESCRIPTION     "Short description shown in file properties"
#     VERSION         "1.0.0.0"          # major.minor.patch.build
#
#     # Window / render (mirrors Marble::WindowSpec)
#     WINDOW_WIDTH    0                  # 0 = primary monitor width
#     WINDOW_HEIGHT   0                  # 0 = primary monitor height
#     VSYNC           TRUE
#     FULLSCREEN      TRUE
#     RENDER_WIDTH    0                  # 0 = match window
#     RENDER_HEIGHT   0                  # 0 = match window
#     RENDER_STYLE    PixelArt           # PixelArt (GL_NEAREST) or Smooth (GL_LINEAR)
#
#     # Icons
#     ICON_ICO  "${CMAKE_CURRENT_SOURCE_DIR}/icon.ico"   # .exe icon  (Explorer / taskbar)
#     ICON_PNG  "assets/icon.png"                         # runtime    (GLFW window icon)
# )
#
# Generates (in <binary_dir>/):
#   GameMetadata.h         — constexpr WindowSpec + metadata; #included by main.cpp
#   <T>_GameResource.rc    — VERSIONINFO + icon + manifest embedded in the .exe
#   <T>_GameManifest.manifest
#
# The binary dir is added to the target's private include path so that
# #include <GameMetadata.h> resolves without a path prefix.
# ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.25)

set(_MARBLE_CMAKE_DIR     "${CMAKE_CURRENT_LIST_DIR}")
set(_MARBLE_TEMPLATES_DIR "${_MARBLE_CMAKE_DIR}/../templates")

function(marble_configure_executable TARGET)
    cmake_parse_arguments(ARG
        ""
        "PRODUCT_NAME;COMPANY_NAME;COPYRIGHT;DESCRIPTION;VERSION;WINDOW_WIDTH;WINDOW_HEIGHT;VSYNC;FULLSCREEN;RENDER_WIDTH;RENDER_HEIGHT;RENDER_STYLE;ICON_ICO;ICON_PNG"
        ""
        ${ARGN}
    )

    # ------------------------------------------------------------------
    # Identity defaults
    # ------------------------------------------------------------------
    if(NOT ARG_PRODUCT_NAME)
        set(ARG_PRODUCT_NAME "${TARGET}")
    endif()
    if(NOT ARG_COMPANY_NAME)
        set(ARG_COMPANY_NAME "Unknown")
    endif()
    if(NOT ARG_COPYRIGHT)
        set(ARG_COPYRIGHT "")
    endif()
    if(NOT ARG_DESCRIPTION)
        set(ARG_DESCRIPTION "${ARG_PRODUCT_NAME}")
    endif()
    if(NOT ARG_VERSION)
        set(ARG_VERSION "1.0.0.0")
    endif()

    # ------------------------------------------------------------------
    # Window / render defaults  (match Marble::WindowSpec defaults)
    # ------------------------------------------------------------------
    if(NOT DEFINED ARG_WINDOW_WIDTH OR ARG_WINDOW_WIDTH STREQUAL "")
        set(ARG_WINDOW_WIDTH 0)
    endif()
    if(NOT DEFINED ARG_WINDOW_HEIGHT OR ARG_WINDOW_HEIGHT STREQUAL "")
        set(ARG_WINDOW_HEIGHT 0)
    endif()
    if(NOT DEFINED ARG_RENDER_WIDTH OR ARG_RENDER_WIDTH STREQUAL "")
        set(ARG_RENDER_WIDTH 0)
    endif()
    if(NOT DEFINED ARG_RENDER_HEIGHT OR ARG_RENDER_HEIGHT STREQUAL "")
        set(ARG_RENDER_HEIGHT 0)
    endif()
    if(NOT DEFINED ARG_VSYNC OR ARG_VSYNC STREQUAL "")
        set(ARG_VSYNC TRUE)
    endif()
    if(NOT DEFINED ARG_FULLSCREEN OR ARG_FULLSCREEN STREQUAL "")
        set(ARG_FULLSCREEN TRUE)
    endif()
    if(NOT DEFINED ARG_RENDER_STYLE OR ARG_RENDER_STYLE STREQUAL "")
        set(ARG_RENDER_STYLE "PixelArt")
    endif()

    # Convert CMake booleans to C++ literals
    if(ARG_VSYNC)
        set(MARBLE_VSYNC_BOOL "true")
    else()
        set(MARBLE_VSYNC_BOOL "false")
    endif()
    if(ARG_FULLSCREEN)
        set(MARBLE_FULLSCREEN_BOOL "true")
    else()
        set(MARBLE_FULLSCREEN_BOOL "false")
    endif()
    if(ARG_RENDER_STYLE STREQUAL "Smooth")
        set(MARBLE_RENDER_STYLE "Smooth")
    else()
        set(MARBLE_RENDER_STYLE "PixelArt")
    endif()

    set(MARBLE_WINDOW_WIDTH  "${ARG_WINDOW_WIDTH}")
    set(MARBLE_WINDOW_HEIGHT "${ARG_WINDOW_HEIGHT}")
    set(MARBLE_RENDER_WIDTH  "${ARG_RENDER_WIDTH}")
    set(MARBLE_RENDER_HEIGHT "${ARG_RENDER_HEIGHT}")

    # ------------------------------------------------------------------
    # Parse version string into four numeric components
    # ------------------------------------------------------------------
    string(REPLACE "." ";" _ver_list "${ARG_VERSION}")
    list(LENGTH _ver_list _ver_len)

    list(GET _ver_list 0 MARBLE_VER_MAJOR)
    if(_ver_len GREATER 1)
        list(GET _ver_list 1 MARBLE_VER_MINOR)
    else()
        set(MARBLE_VER_MINOR 0)
    endif()
    if(_ver_len GREATER 2)
        list(GET _ver_list 2 MARBLE_VER_PATCH)
    else()
        set(MARBLE_VER_PATCH 0)
    endif()
    if(_ver_len GREATER 3)
        list(GET _ver_list 3 MARBLE_VER_BUILD)
    else()
        set(MARBLE_VER_BUILD 0)
    endif()

    set(MARBLE_VERSION_STR
        "${MARBLE_VER_MAJOR}.${MARBLE_VER_MINOR}.${MARBLE_VER_PATCH}.${MARBLE_VER_BUILD}")

    # ------------------------------------------------------------------
    # Identity strings used in RC + GameMetadata.h
    # ------------------------------------------------------------------
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" MARBLE_INTERNAL_NAME "${ARG_PRODUCT_NAME}")
    set(MARBLE_ORIGINAL_FILENAME "${MARBLE_INTERNAL_NAME}.exe")
    set(MARBLE_PRODUCT_NAME  "${ARG_PRODUCT_NAME}")
    set(MARBLE_COMPANY_NAME  "${ARG_COMPANY_NAME}")
    set(MARBLE_COPYRIGHT     "${ARG_COPYRIGHT}")
    set(MARBLE_DESCRIPTION   "${ARG_DESCRIPTION}")

    # ------------------------------------------------------------------
    # Runtime icon (PNG path stored in GameMetadata.h, loaded by GLFW)
    # ------------------------------------------------------------------
    if(ARG_ICON_PNG)
        set(MARBLE_ICON_PNG_PATH "${ARG_ICON_PNG}")
    else()
        set(MARBLE_ICON_PNG_PATH "")
    endif()

    # ------------------------------------------------------------------
    # Build-time icon (.ico embedded in the .exe resource)
    # ------------------------------------------------------------------
    if(ARG_ICON_ICO AND EXISTS "${ARG_ICON_ICO}")
        file(TO_CMAKE_PATH "${ARG_ICON_ICO}" _ico_path)
        set(MARBLE_ICON_RC_LINE "IDI_ICON1 ICON \"${_ico_path}\"")
    else()
        if(ARG_ICON_ICO)
            message(WARNING
                "[MarbleEngine] icon.ico not found: ${ARG_ICON_ICO}\n"
                "  The .exe will have no icon in Explorer / taskbar.\n"
                "  Add ICON_ICO to marble_configure_executable() once you have one.")
        endif()
        set(MARBLE_ICON_RC_LINE "// No .ico provided")
    endif()

    # ------------------------------------------------------------------
    # Generate manifest
    # ------------------------------------------------------------------
    set(_manifest_out "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_GameManifest.manifest")
    configure_file(
        "${_MARBLE_TEMPLATES_DIR}/GameManifest.manifest.in"
        "${_manifest_out}"
        @ONLY
    )
    file(TO_CMAKE_PATH "${_manifest_out}" MARBLE_MANIFEST_PATH)

    # ------------------------------------------------------------------
    # Generate RC resource file
    # ------------------------------------------------------------------
    configure_file(
        "${_MARBLE_TEMPLATES_DIR}/GameResource.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_GameResource.rc"
        @ONLY
    )

    # ------------------------------------------------------------------
    # Generate GameMetadata.h  (C++ constexpr bridge for main.cpp)
    # ------------------------------------------------------------------
    configure_file(
        "${_MARBLE_TEMPLATES_DIR}/GameMetadata.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/GameMetadata.h"
        @ONLY
    )

    # ------------------------------------------------------------------
    # Apply to target
    # ------------------------------------------------------------------
    target_sources("${TARGET}" PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_GameResource.rc"
    )

    # Expose binary dir so  #include <GameMetadata.h>  resolves
    target_include_directories("${TARGET}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

    # Suppress CMake's auto-generated manifest — ours is embedded in the RC
    if(MSVC)
        target_link_options("${TARGET}" PRIVATE "/MANIFEST:NO")
    endif()

    # Rename the .exe to match the product name
    set_target_properties("${TARGET}" PROPERTIES OUTPUT_NAME "${MARBLE_INTERNAL_NAME}")

    message(STATUS "[MarbleEngine] ${MARBLE_INTERNAL_NAME}.exe  v${MARBLE_VERSION_STR}  (${ARG_COMPANY_NAME})")
endfunction()
