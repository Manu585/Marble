# PakTool — offline utility that packs an asset directory into a .pak file.
# Compiled directly against PakBuilder.cpp so it does not pull in the Engine's
# OpenGL / GLFW / audio dependencies.
add_executable(PakTool
    ${CMAKE_SOURCE_DIR}/Engine/src/IO/PakBuilder.cpp
    main.cpp
)

target_include_directories(PakTool PRIVATE ${CMAKE_SOURCE_DIR}/Engine/src)

# Output to a separate tools/ directory so PakTool.exe is not deployed alongside the game.
set_target_properties(PakTool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)
