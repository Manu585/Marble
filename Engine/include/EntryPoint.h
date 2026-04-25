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
// Splash screens are configured in CMakeLists.txt via marble_configure_executable()
// and baked into GameMetadata::MakeSplashScreens() at build time. MARBLE_MAIN
// inserts the SplashLayer automatically when any screens are configured.

#include <Windows.h>
#include <stdexcept>
#include <vector>

// MARBLE_MAIN(GameLayerType)
//   Expands to wWinMain. Constructs Application from GameMetadata::Window,
//   sets the window icon if GameMetadata::IconPath is non-empty, then:
//     - If SPLASH_SCREENS were configured in CMakeLists.txt → runs SplashLayer first
//     - Otherwise → runs the game layer directly
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
            }                                                                       \
            GameLayerType layer;                                                    \
            auto _splashes = GameMetadata::MakeSplashScreens();                    \
            if (!_splashes.empty()) {                                               \
                Marble::SplashLayer splash(std::move(_splashes), layer);            \
                app.Run(splash);                                                    \
            } else {                                                                \
                app.Run(layer);                                                     \
            }                                                                       \
        }                                                                           \
        catch (const std::exception& e) {                                           \
            MessageBoxA(nullptr, e.what(), "Fatal Error", MB_OK | MB_ICONERROR);    \
            return 1;                                                               \
        }                                                                           \
        return 0;                                                                   \
    }
