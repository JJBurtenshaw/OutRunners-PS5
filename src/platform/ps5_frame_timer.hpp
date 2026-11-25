#pragma once

#include <SDL.h>

namespace ps5
{
    class FrameTimer
    {
    public:
        bool initialize()
        {
            frequency_ =
                SDL_GetPerformanceFrequency();

            if (!frequency_)
                return false;

            reset();

            return true;
        }

        void reset()
        {
            start_ =
                SDL_GetPerformanceCounter();

            frames_ = 0;
            fps_ = 0.0;
            milliseconds_ = 0.0;
            valid_ = false;
        }

        // Call once after each successful
        // presentation of an application frame.

        void frame_presented()
        {
            if (!frequency_)
                return;

            ++frames_;

            const Uint64 now =
                SDL_GetPerformanceCounter();

            const Uint64 elapsed = now - start_;

            if (elapsed >= frequency_)
            {
                const double seconds =
                    static_cast<double>(elapsed) /
                    static_cast<double>(frequency_);

                fps_ = frames_ / seconds;

                milliseconds_ =
                    seconds * 1000.0 / frames_;

                frames_ = 0;
                start_ = now;
                valid_ = true;
            }
        }

        bool valid() const
        {
            return valid_;
        }

        double fps() const
        {
            return fps_;
        }

        double frame_ms() const
        {
            return milliseconds_;
        }

    private:
        Uint64 frequency_ = 0;
        Uint64 start_ = 0;

        unsigned int frames_ = 0;

        double fps_ = 0.0;
        double milliseconds_ = 0.0;

        bool valid_ = false;
    };
}
