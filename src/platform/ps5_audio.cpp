#include "ps5_audio.hpp"

#include <algorithm>

namespace ps5
{
    Audio::~Audio()
    {
        shutdown();
    }

    bool Audio::initialize()
    {
        if (ready())
            return true;

        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
            return false;

        subsystem_ = true;

        SDL_AudioSpec desired{};
        SDL_AudioSpec obtained{};

        desired.freq = 48000;
        desired.format = AUDIO_S16SYS;
        desired.channels = 2;
        desired.samples = 512;
        desired.callback = nullptr;

        device_ = SDL_OpenAudioDevice(
            nullptr,
            0,
            &desired,
            &obtained,
            0
        );

        if (!device_)
        {
            shutdown();
            return false;
        }

        if (obtained.freq != 48000 ||
            obtained.format != AUDIO_S16SYS ||
            obtained.channels != 2)
        {
            shutdown();

            SDL_SetError(
                "Unexpected PCM audio format");

            return false;
        }

        SDL_PauseAudioDevice(device_, 0);

        return true;
    }

    void Audio::shutdown()
    {
        if (device_)
        {
            SDL_PauseAudioDevice(device_, 1);
            SDL_ClearQueuedAudio(device_);
            SDL_CloseAudioDevice(device_);

            device_ = 0;
        }

        if (subsystem_)
        {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            subsystem_ = false;
        }
    }

    bool Audio::queue(
        const Sint16* samples,
        std::size_t frames)
    {
        if (!ready())
            return false;

        if (frames == 0)
            return true;

        if (!samples)
            return false;

        // Limit queued playback to approximately
        // 250ms to avoid unbounded memory growth.
        constexpr Uint32 maximum_frames = 12000;

        const Uint32 pending = queued_frames();

        if (pending > maximum_frames ||
            frames > maximum_frames - pending)
        {
            return SDL_SetError(
                "Audio queue capacity exceeded") == 0;
        }

        const int gain = muted_ ? 0 : volume_;

        while (frames)
        {
            const std::size_t count =
                std::min(
                    frames,
                    static_cast<std::size_t>(1024)
                );

            const std::size_t sample_count =
                count * 2;

            for (std::size_t i = 0;
                 i < sample_count;
                 ++i)
            {
                scratch_[i] =
                    static_cast<Sint16>(
                        static_cast<int>(samples[i]) *
                        gain / 100
                    );
            }

            if (SDL_QueueAudio(
                    device_,
                    scratch_,
                    static_cast<Uint32>(
                        sample_count *
                        sizeof(Sint16)
                    )) != 0)
                return false;

            samples += sample_count;
            frames -= count;
        }

        return true;
    }

    Uint32 Audio::queued_frames() const
    {
        if (!ready())
            return 0;

        return SDL_GetQueuedAudioSize(device_) /
            (2 * sizeof(Sint16));
    }

    void Audio::clear()
    {
        if (ready())
            SDL_ClearQueuedAudio(device_);
    }

    void Audio::set_volume(int percent)
    {
        volume_ = std::clamp(percent, 0, 100);
        clear();
    }

    void Audio::set_muted(bool muted)
    {
        muted_ = muted;
        clear();
    }
}
