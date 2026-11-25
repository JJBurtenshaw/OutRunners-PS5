#include "ps5_platform.hpp"

extern "C" int sceSystemServiceHideSplashScreen(void);

extern "C" int sceSystemServiceLoadExec(
    const char* path, const char** argv);

extern "C" int sceKernelUsleep(unsigned int);

namespace
{
    SDL_Window* window = nullptr;
    SDL_Surface* surface = nullptr;
    bool splash_hidden = false;
}

namespace ps5
{
    bool initialize()
    {
        // Match the hardware-tested initialization
        // sequence. Do not initialize input yet.

        if (SDL_Init(
                SDL_INIT_TIMER |
                SDL_INIT_VIDEO) != 0)
            return false;

        window = SDL_CreateWindow(
            "OutRunners PS5",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            display_width,
            display_height,
            SDL_WINDOW_FULLSCREEN
        );

        if (!window)
            return false;

        surface = SDL_GetWindowSurface(window);

        return surface != nullptr;
    }

    bool initialize_input()
    {
        if (SDL_InitSubSystem(
                SDL_INIT_JOYSTICK |
                SDL_INIT_GAMECONTROLLER) != 0)
            return false;

        SDL_JoystickEventState(SDL_ENABLE);
        SDL_GameControllerEventState(SDL_ENABLE);

        return true;
    }

    void update_input()
    {
        // Critical native PS5 workaround:
        // never use the normal SDL event pump.
        SDL_JoystickUpdate();
    }

    bool next_event(SDL_Event& event)
    {
        return SDL_PeepEvents(
            &event,
            1,
            SDL_GETEVENT,
            SDL_FIRSTEVENT,
            SDL_LASTEVENT
        ) > 0;
    }

    SDL_Surface* screen()
    {
        return surface;
    }

    bool present()
    {
        if (!window || !surface)
            return false;

        if (SDL_UpdateWindowSurface(window) != 0)
            return false;

        if (!splash_hidden)
        {
            sceSystemServiceHideSplashScreen();
            splash_hidden = true;

            if (SDL_UpdateWindowSurface(window) != 0)
                return false;
        }

        return true;
    }

    void shutdown()
    {
        if (window)
        {
            SDL_DestroyWindow(window);
            window = nullptr;
        }

        surface = nullptr;

        SDL_Quit();
    }

    [[noreturn]] void exit_to_dashboard()
    {
        sceSystemServiceLoadExec("exit", nullptr);

        for (;;)
            sceKernelUsleep(100000);
    }
}
