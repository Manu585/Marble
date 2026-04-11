file(GLOB_RECURSE ENGINE_SOURCES "src/*.cpp")
file(GLOB_RECURSE ENGINE_HEADERS "src/*.h")

add_library(Engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_include_directories(Engine
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Engine
    PUBLIC  glfw
    PUBLIC  glad
    PUBLIC  glm
    PUBLIC  stb
    PUBLIC  miniaudio
    PRIVATE imgui
    PRIVATE dwmapi
    PRIVATE Psapi
)

target_compile_options(Engine PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /wd4100>
)

# MARBLE_DEBUG is defined for Debug builds only. It enables DebugDraw and other
# dev-mode diagnostics. In Release the DebugDraw API becomes inline no-ops with
# zero runtime cost.
target_compile_definitions(Engine PUBLIC
    $<$<CONFIG:Debug>:MARBLE_DEBUG>
)