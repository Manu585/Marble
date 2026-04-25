#include "IO/PakBuilder.h"
#include <cstdio>
#include <stdexcept>

// PakTool <input_dir> <output.pak>
//
// Recursively packs all files in <input_dir> into <output.pak>.
// Virtual paths in the pak mirror the directory structure:
//   input_dir/textures/foo.png  →  "textures/foo.png"
//
// To include the directory name itself as a prefix:
//   PakTool Game/assets assets.pak
//   → virtual paths like "assets/textures/foo.png"
//
// Usage from CMake (Game/CMakeLists.txt):
//   $<TARGET_FILE:PakTool> ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:Game>/assets.pak

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: PakTool <input_dir> <output.pak>\n");
        return 1;
    }

    const char* inputDir   = argv[1];
    const char* outputPath = argv[2];

    try {
        Marble::PakBuilder builder;
        builder.AddDirectory(inputDir, "assets");
        builder.Build(outputPath);
        std::printf("Built '%s' with %zu file(s).\n", outputPath, builder.FileCount());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "PakTool error: %s\n", e.what());
        return 1;
    }
}
