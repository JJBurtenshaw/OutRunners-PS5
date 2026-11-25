#pragma once

#include <SDL.h>
#include <cstddef>

namespace ps5
{
    class Audio
    {
    public:
        Audio() = default;

        ~Audio();

        Audio(const Audio&) = delete;
        Audio& operator=(const Audio&) = delete;

        bool initialize();

        void shutdown();

        bool ready() const
        {
            return device_ != 0;
        }

        int sample_rate() const
        {
            return 48000;
        }

        int channels() const
        {
            return 2;
        }

        bool queue(
            const Sint16* samples,
            std::size_t frames
        );

        Uint32 queued_frames() const;

        void clear();

        // ORUNNERS_AUDIO_PAUSE_V1
        // Pause device consumption without clearing PCM.

        void set_paused(bool paused)
        {
            if (ready())
                SDL_PauseAudioDevice(
                    device_, paused ? 1 : 0);
        }


        void set_volume(int percent);

        int volume() const
        {
            return volume_;
        }

        void set_muted(bool muted);

        bool muted() const
        {
            return muted_;
        }

    private:
        SDL_AudioDeviceID device_ = 0;

        bool subsystem_ = false;

        int volume_ = 50;

        bool muted_ = false;

        // Small fixed scratch buffer avoids large
        // allocations from the native libc heap.
        Sint16 scratch_[2048]{};
    };
}
