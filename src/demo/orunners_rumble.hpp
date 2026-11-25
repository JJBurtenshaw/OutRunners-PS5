#pragma once

#include <SDL.h>

#include <cstdint>

namespace orunners
{
    /*
     * ORUNNERS_RUMBLE_V1
     *
     * Original arcade wheel-motor signal
     * translated into conventional SDL rumble.
     *
     * Each request expires after 100 ms.
     * Sustained vibration is refreshed at 50 ms.
     */

    class Rumble
    {
    public:

        static constexpr Uint32 duration_ms = 100;
        static constexpr Uint32 refresh_ms = 50;

        void attach(SDL_GameController* pad)
        {
            pad_ = pad;

            requested_ = false;
            running_ = false;
            strength_ = 0;
            last_refresh_ = 0;

            supported_ =
                pad_ &&
                SDL_GameControllerHasRumble(pad_) ==
                    SDL_TRUE;
        }

        void update(bool motor, int strength)
        {
            if (strength < 0)
                strength = 0;

            if (strength > 100)
                strength = 100;

            requested_ =
                motor && strength > 0;

            if (!requested_)
            {
                stop();
                return;
            }

            ++motor_frames_;

            if (!pad_ || !supported_)
                return;

            if (!SDL_GameControllerGetAttached(pad_))
            {
                running_ = false;
                return;
            }

            const Uint64 now = SDL_GetTicks64();

            if (running_ &&
                strength_ == strength &&
                now - last_refresh_ < refresh_ms)
            {
                return;
            }

            const Uint16 low =
                static_cast<Uint16>(
                    65535u *
                    static_cast<unsigned>(strength) /
                    100u);

            const Uint16 high =
                static_cast<Uint16>(low / 2);

            ++requests_;

            const int result =
                SDL_GameControllerRumble(
                    pad_,
                    low,
                    high,
                    duration_ms);

            last_refresh_ = now;
            strength_ = strength;

            if (result == 0)
            {
                running_ = true;
                ++accepted_;
            }
            else
            {
                running_ = false;
                supported_ = false;
                ++errors_;
            }
        }

        void stop()
        {
            requested_ = false;

            if (running_ &&
                pad_ &&
                SDL_GameControllerGetAttached(pad_))
            {
                if (SDL_GameControllerRumble(
                        pad_, 0, 0, 0) != 0)
                {
                    ++errors_;
                }
            }

            running_ = false;
            strength_ = 0;
            last_refresh_ = 0;
        }

        void detach()
        {
            stop();

            pad_ = nullptr;
            supported_ = false;
        }

        bool attached() const
        {
            return pad_ != nullptr;
        }

        bool supported() const
        {
            return supported_;
        }

        bool requested() const
        {
            return requested_;
        }

        bool running() const
        {
            return running_;
        }

        unsigned motor_frames() const
        {
            return motor_frames_;
        }

        unsigned requests() const
        {
            return requests_;
        }

        unsigned accepted() const
        {
            return accepted_;
        }

        unsigned errors() const
        {
            return errors_;
        }

    private:

        SDL_GameController* pad_ = nullptr;

        bool supported_ = false;
        bool requested_ = false;
        bool running_ = false;

        int strength_ = 0;

        Uint64 last_refresh_ = 0;

        unsigned motor_frames_ = 0;
        unsigned requests_ = 0;
        unsigned accepted_ = 0;
        unsigned errors_ = 0;
    };
}
