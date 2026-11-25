#pragma once

#include <SDL.h>
#include <cstdint>

/*
 * OutRunners PS5 controller bridge.
 *
 * Uses SDL controller state already updated
 * through the verified native PS5 input path.
 *
 * Arcade input encoding and menu handling
 * remain in the engine-side adapter.
 */

namespace orunners
{
    enum PadButton : uint32_t
    {
        PadUp       = 1u << 0,
        PadDown     = 1u << 1,
        PadLeft     = 1u << 2,
        PadRight    = 1u << 3,
        PadCross    = 1u << 4,
        PadCircle   = 1u << 5,
        PadStart    = 1u << 6,
        PadShiftUp  = 1u << 7,
        PadShiftDn  = 1u << 8,
        PadMusic    = 1u << 9
    };

    struct PadState
    {
        int32_t stick_x = 0;
        int32_t stick_y = 0;
        int32_t accelerator = 0;
        int32_t brake = 0;

        uint32_t buttons = 0;
    };

    inline PadState read_pad(SDL_GameController* pad)
    {
        PadState state{};

        if (!pad ||
            !SDL_GameControllerGetAttached(pad))
            return state;

        state.stick_x =
            SDL_GameControllerGetAxis(
                pad, SDL_CONTROLLER_AXIS_LEFTX);

        state.stick_y =
            SDL_GameControllerGetAxis(
                pad, SDL_CONTROLLER_AXIS_LEFTY);

        state.accelerator =
            SDL_GameControllerGetAxis(
                pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

        state.brake =
            SDL_GameControllerGetAxis(
                pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT);

        const auto held =
            [pad](SDL_GameControllerButton button)
            {
                return SDL_GameControllerGetButton(
                    pad, button) != 0;
            };

        if (held(SDL_CONTROLLER_BUTTON_DPAD_UP))
            state.buttons |= PadUp;

        if (held(SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            state.buttons |= PadDown;

        if (held(SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            state.buttons |= PadLeft;

        if (held(SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            state.buttons |= PadRight;

        if (held(SDL_CONTROLLER_BUTTON_A))
            state.buttons |= PadCross;

        if (held(SDL_CONTROLLER_BUTTON_B))
            state.buttons |= PadCircle;

        if (held(SDL_CONTROLLER_BUTTON_START))
            state.buttons |= PadStart;

        if (held(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
            state.buttons |= PadShiftUp;

        if (held(SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
            state.buttons |= PadShiftDn;

        if (held(SDL_CONTROLLER_BUTTON_X) ||
            held(SDL_CONTROLLER_BUTTON_Y))
            state.buttons |= PadMusic;

        return state;
    }
}
