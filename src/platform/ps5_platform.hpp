#pragma once

#include <SDL.h>

namespace ps5
{
    constexpr int display_width = 1920;
    constexpr int display_height = 1080;

    bool initialize();

    bool initialize_input();

    void update_input();

    bool next_event(SDL_Event& event);

    SDL_Surface* screen();

    bool present();

    void shutdown();

    [[noreturn]] void exit_to_dashboard();
}
