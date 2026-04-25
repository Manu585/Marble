cmake_minimum_required (VERSION 3.25)
project (MarbleEngine VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(stb STATIC
    vendor/stb/stb_image.cpp
    vendor/stb/stb_truetype.cpp
)
target_include_directories(stb PUBLIC vendor/stb)

# miniaudio single-header audio library.
add_library(miniaudio STATIC vendor/miniaudio/miniaudio.cpp)

target_include_directories(miniaudio PUBLIC vendor/miniaudio)

if(WIN32)
    target_link_libraries(miniaudio PUBLIC ole32 avrt)
endif()

add_subdirectory(vendor/glfw)
add_subdirectory(vendor/glad)
add_subdirectory(vendor/glm)
add_subdirectory(vendor/imgui)

add_subdirectory(Engine)
add_subdirectory(Game)