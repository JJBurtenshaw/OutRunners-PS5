/*
 * Lightweight software-rendered controller diagnostics.
 *
 * Mapped SDL GameController axes and raw joystick axes
 * are displayed separately to distinguish mapping
 * problems from input backend problems.
 *
 * This module observes controller state. It does not
 * pump SDL events or modify controller mappings.
 */

#pragma once

#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cctype>

namespace controller_diag
{
    inline Uint32 colour(
        SDL_Surface* surface,
        int r, int g, int b)
    {
        return SDL_MapRGB(
            surface->format, r, g, b);
    }

    inline void rect(
        SDL_Surface* surface,
        int x, int y, int w, int h,
        Uint32 pixel)
    {
        SDL_Rect r{x, y, w, h};
        SDL_FillRect(surface, &r, pixel);
    }

    // Compact uppercase bitmap font.
    inline const char* glyph(char c)
    {
        switch (c)
        {
            case 'A': return "010101111101101";
            case 'B': return "110101110101110";
            case 'C': return "111100100100111";
            case 'D': return "110101101101110";
            case 'E': return "111100110100111";
            case 'F': return "111100110100100";
            case 'G': return "111100101101111";
            case 'H': return "101101111101101";
            case 'I': return "111010010010111";
            case 'J': return "001001001101111";
            case 'K': return "101101110101101";
            case 'L': return "100100100100111";
            case 'M': return "101111111101101";
            case 'N': return "101111111111101";
            case 'O': return "111101101101111";
            case 'P': return "110101110100100";
            case 'Q': return "111101101111001";
            case 'R': return "110101110101101";
            case 'S': return "111100111001111";
            case 'T': return "111010010010010";
            case 'U': return "101101101101111";
            case 'V': return "101101101101010";
            case 'W': return "101101111111101";
            case 'X': return "101101010101101";
            case 'Y': return "101101010010010";
            case 'Z': return "111001010100111";
            case '0': return "111101101101111";
            case '1': return "010110010010111";
            case '2': return "111001111100111";
            case '3': return "111001111001111";
            case '4': return "101101111001001";
            case '5': return "111100111001111";
            case '6': return "111100111101111";
            case '7': return "111001001001001";
            case '8': return "111101111101111";
            case '9': return "111101111001111";
            case '-': return "000000111000000";
            case '+': return "000010111010000";
            case ':': return "000010000010000";
            case '.': return "000000000000010";
            case '/': return "001001010100100";
            case '_': return "000000000000111";
            case '?': return "111001010000010";
            default:  return "000000000000000";
        }
    }

    inline void text(
        SDL_Surface* surface,
        int x, int y,
        const char* value,
        int scale,
        Uint32 pixel)
    {
        if (!value)
            return;

        for (; *value; ++value)
        {
            const char c = static_cast<char>(
                std::toupper(
                    static_cast<unsigned char>(
                        *value)));

            const char* bits = glyph(c);

            for (int row = 0; row < 5; ++row)
            {
                for (int col = 0; col < 3; ++col)
                {
                    if (bits[row * 3 + col] == '1')
                    {
                        rect(
                            surface,
                            x + col * scale,
                            y + row * scale,
                            scale, scale,
                            pixel);
                    }
                }
            }

            x += scale * 4;
        }
    }

    inline int axis(
        SDL_GameController* controller,
        SDL_GameControllerAxis which)
    {
        return controller
            ? SDL_GameControllerGetAxis(
                controller, which)
            : 0;
    }

    inline void stick(
        SDL_Surface* surface,
        int x, int y,
        int vx, int vy)
    {
        constexpr int size = 160;
        constexpr int centre = size / 2;

        const Uint32 background =
            colour(surface, 24, 36, 54);

        const Uint32 grid =
            colour(surface, 78, 96, 118);

        const Uint32 green =
            colour(surface, 74, 222, 128);

        rect(
            surface, x, y,
            size, size, background);

        rect(
            surface, x + centre, y,
            2, size, grid);

        rect(
            surface, x, y + centre,
            size, 2, grid);

        const int px = std::clamp(
            centre + vx * 72 / 32768,
            6, size - 6);

        const int py = std::clamp(
            centre + vy * 72 / 32768,
            6, size - 6);

        rect(
            surface,
            x + px - 5,
            y + py - 5,
            10, 10,
            green);
    }

    inline void trigger(
        SDL_Surface* surface,
        int x, int y,
        int value)
    {
        constexpr int width = 260;

        rect(
            surface, x, y,
            width, 15,
            colour(surface, 45, 55, 75));

        const int filled =
            std::clamp(value, 0, 32767)
            * width / 32767;

        if (filled > 0)
        {
            rect(
                surface, x, y,
                filled, 15,
                colour(surface, 56, 189, 248));
        }
    }

    inline void draw(
        SDL_Surface* surface,
        SDL_GameController* controller)
    {
        if (!surface)
            return;

        const Uint32 white =
            colour(surface, 235, 240, 250);

        const Uint32 muted =
            colour(surface, 145, 160, 180);

        const Uint32 green =
            colour(surface, 74, 222, 128);

        const Uint32 red =
            colour(surface, 248, 113, 113);

        const bool connected =
            controller &&
            SDL_GameControllerGetAttached(
                controller);

        SDL_GameController* active =
            connected ? controller : nullptr;

        SDL_Joystick* joystick = active
            ? SDL_GameControllerGetJoystick(active)
            : nullptr;

        const int count = SDL_NumJoysticks();

        char buffer[128]{};

        // Draw in output resolution, independent
        // of the internal framebuffer resolution.

        const int x = 20;
        const int y = 140;

        rect(
            surface, x, y,
            1040, 880,
            colour(surface, 5, 10, 18));

        text(
            surface, x + 20, y + 16,
            "CONTROLLER DIAGNOSTICS",
            5, white);

        std::snprintf(
            buffer, sizeof(buffer),
            "%s  ID: %d  DEVICES: %d",
            connected ? "CONNECTED" : "DISCONNECTED",
            joystick
                ? static_cast<int>(
                    SDL_JoystickInstanceID(joystick))
                : -1,
            count);

        text(
            surface, x + 20, y + 56,
            buffer, 4,
            connected ? green : red);

        const char* name = active
            ? SDL_GameControllerName(active)
            : "NO ACTIVE CONTROLLER";

        std::snprintf(
            buffer, sizeof(buffer),
            "%.60s",
            name ? name : "UNKNOWN");

        text(
            surface, x + 20, y + 86,
            buffer, 4, white);

        // List connected joystick devices,
        // including devices not selected by SDL.

        for (int i = 0;
             i < std::min(count, 4);
             ++i)
        {
            const char* device_name =
                SDL_JoystickNameForIndex(i);

            std::snprintf(
                buffer, sizeof(buffer),
                "%d ID:%d %.46s",
                i,
                static_cast<int>(
                    SDL_JoystickGetDeviceInstanceID(i)),
                device_name ? device_name : "UNKNOWN");

            text(
                surface,
                x + 20,
                y + 120 + i * 22,
                buffer, 3, muted);
        }

        // SDL mapped axes, before deadzone.

        const int lx = axis(
            active, SDL_CONTROLLER_AXIS_LEFTX);

        const int ly = axis(
            active, SDL_CONTROLLER_AXIS_LEFTY);

        const int rx = axis(
            active, SDL_CONTROLLER_AXIS_RIGHTX);

        const int ry = axis(
            active, SDL_CONTROLLER_AXIS_RIGHTY);

        const int lt = axis(
            active, SDL_CONTROLLER_AXIS_TRIGGERLEFT);

        const int rt = axis(
            active, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

        text(
            surface, x + 20, y + 220,
            "LEFT STICK", 4, white);

        text(
            surface, x + 530, y + 220,
            "RIGHT STICK", 4, white);

        stick(
            surface,
            x + 20, y + 250,
            lx, ly);

        stick(
            surface,
            x + 530, y + 250,
            rx, ry);

        std::snprintf(
            buffer, sizeof(buffer),
            "LX: %d", lx);

        text(
            surface, x + 200, y + 280,
            buffer, 4, white);

        std::snprintf(
            buffer, sizeof(buffer),
            "LY: %d", ly);

        text(
            surface, x + 200, y + 320,
            buffer, 4, white);

        std::snprintf(
            buffer, sizeof(buffer),
            "RX: %d", rx);

        text(
            surface, x + 710, y + 280,
            buffer, 4, white);

        std::snprintf(
            buffer, sizeof(buffer),
            "RY: %d", ry);

        text(
            surface, x + 710, y + 320,
            buffer, 4, white);

        std::snprintf(
            buffer, sizeof(buffer),
            "L2: %d", lt);

        text(
            surface, x + 20, y + 430,
            buffer, 4, white);

        std::snprintf(
            buffer, sizeof(buffer),
            "R2: %d", rt);

        text(
            surface, x + 530, y + 430,
            buffer, 4, white);

        trigger(
            surface, x + 20, y + 460, lt);

        trigger(
            surface, x + 530, y + 460, rt);

        // Raw joystick axes, before SDL's
        // GameController mapping.

        const int raw_axes = joystick
            ? SDL_JoystickNumAxes(joystick)
            : 0;

        std::snprintf(
            buffer, sizeof(buffer),
            "RAW JOYSTICK AXES: %d",
            raw_axes);

        text(
            surface, x + 20, y + 500,
            buffer, 4, white);

        for (int i = 0;
             i < std::min(raw_axes, 12);
             ++i)
        {
            const int value =
                SDL_JoystickGetAxis(
                    joystick, i);

            std::snprintf(
                buffer, sizeof(buffer),
                "A%d: %d", i, value);

            text(
                surface,
                x + 20 + (i % 4) * 250,
                y + 530 + (i / 4) * 25,
                buffer, 3, white);
        }

        struct Button
        {
            SDL_GameControllerButton id;
            const char* name;
        };

        static constexpr Button buttons[] = {
            {SDL_CONTROLLER_BUTTON_A, "CROSS"},
            {SDL_CONTROLLER_BUTTON_B, "CIRCLE"},
            {SDL_CONTROLLER_BUTTON_X, "SQUARE"},
            {SDL_CONTROLLER_BUTTON_Y, "TRIANGLE"},
            {SDL_CONTROLLER_BUTTON_DPAD_UP, "UP"},
            {SDL_CONTROLLER_BUTTON_DPAD_DOWN, "DOWN"},
            {SDL_CONTROLLER_BUTTON_DPAD_LEFT, "LEFT"},
            {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, "RIGHT"},
            {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, "L1"},
            {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "R1"},
            {SDL_CONTROLLER_BUTTON_LEFTSTICK, "L3"},
            {SDL_CONTROLLER_BUTTON_RIGHTSTICK, "R3"},
            {SDL_CONTROLLER_BUTTON_BACK, "SHARE"},
            {SDL_CONTROLLER_BUTTON_START, "OPTIONS"},
            {SDL_CONTROLLER_BUTTON_GUIDE, "PS"},
            {SDL_CONTROLLER_BUTTON_TOUCHPAD, "TOUCHPAD"}
        };

        text(
            surface, x + 20, y + 620,
            "BUTTON STATUS", 4, white);

        for (int i = 0; i < 16; ++i)
        {
            const bool pressed =
                active &&
                SDL_GameControllerGetButton(
                    active, buttons[i].id);

            const int bx =
                x + 20 + (i % 4) * 250;

            const int by =
                y + 650 + (i / 4) * 39;

            rect(
                surface,
                bx, by, 235, 32,
                pressed
                    ? colour(surface, 22, 101, 52)
                    : colour(surface, 31, 41, 55));

            text(
                surface,
                bx + 10, by + 7,
                buttons[i].name,
                3,
                pressed ? green : muted);
        }

        text(
            surface, x + 20, y + 830,
            "L3: TOGGLE DIAGNOSTICS",
            4, muted);
    }
}
