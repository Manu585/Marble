#pragma once
// Windows entry point helper.
//
// Usage:
//   #include <Marble.h>
//   #include <GameMetadata.h>
//   #include <EntryPoint.h>
//   #include "MyGame.h"
//
//   MARBLE_MAIN(MyGame)
//

#include <Windows.h>
#include <stdexcept>

// MARBLE_MAIN(GameLayerType)
//   Expands to wWinMain. Constructs Application from GameMetadata::Window,
//   sets the window icon if GameMetadata::IconPath is non-empty, then runs
//   the game layer.
#define MARBLE_MAIN(GameLayerType)                                                  \
    int WINAPI wWinMain(                                                            \
        _In_     HINSTANCE,                                                         \
        _In_opt_ HINSTANCE,                                                         \
        _In_     PWSTR,                                                             \
        _In_     int)                                                               \
    {                                                                               \
        try {                                                                       \
            Marble::Application app(GameMetadata::Window);                          \
            if (GameMetadata::IconPath[0] != '\0') {                                \
                app.SetIcon(GameMetadata::IconPath);                                \
               }                                                                    \
            GameLayerType layer;                                                    \
            app.Run(layer);                                                         \
        }                                                                           \
        catch (const std::exception& e) {                                           \
            MessageBoxA(nullptr, e.what(), "Fatal Error", MB_OK | MB_ICONERROR);    \
            return 1;                                                               \
        }                                                                           \
        return 0;                                                                   \
    }
