#pragma once

#include <SDL.h>
#include <cstdint>
#include <cstddef>

#include "ps5_audio.hpp"

/*
 * OutRunners PS5 audio bridge.
 *
 * Original engine PCM -> continuous SDL resampling
 * -> verified native 48 kHz queued audio.
 *
 * No changes to the original sound engine.
 * No per-frame allocation in application code.
 */

namespace orunners
{
    class AudioBridge
    {
    private:

        SDL_AudioStream* stream_ = nullptr;

        ps5::Audio* output_ = nullptr;

        int source_rate_ = 0;

        // ORUNNERS_AUDIO_BUFFER_V2

        static constexpr unsigned target_frames = 4608;
        static constexpr unsigned recovery_frames = 512;
        static constexpr unsigned maximum_frames = 12000;

        Sint16 converted_[2048]{};
        unsigned recoveries_ = 0;

        unsigned queue_min_ = maximum_frames;
        unsigned queue_max_ = 0;

        bool buffering_ = true;

        unsigned underruns_ = 0;
        unsigned overruns_ = 0;
        unsigned errors_ = 0;

        unsigned peak_ = 0;
        unsigned peak_hold_ = 0;

        bool started_ = false;

    public:

        bool initialize(
            ps5::Audio& output,
            int source_rate)
        {
            if (stream_)
                return true;

            if (!output.ready() || source_rate <= 0)
                return false;

            output_ = &output;
            source_rate_ = source_rate;

            stream_ = SDL_NewAudioStream(
                AUDIO_S16SYS,
                2,
                source_rate_,
                AUDIO_S16SYS,
                2,
                output.sample_rate());

            if (!stream_)
                return false;

            output_->set_paused(true);
            output_->clear();

            underruns_ = 0;
            overruns_ = 0;
            errors_ = 0;
            recoveries_ = 0;

            queue_min_ = maximum_frames;
            queue_max_ = 0;

            peak_ = 0;
            peak_hold_ = 0;

            buffering_ = true;
            started_ = false;

            return true;
        }

        void shutdown()
        {
            if (stream_)
            {
                SDL_FreeAudioStream(stream_);
                stream_ = nullptr;
            }

            if (output_)
            {
                output_->set_paused(true);
                output_->clear();
            }

            output_ = nullptr;
            started_ = false;
            buffering_ = true;
        }

        bool ready() const
        {
            return stream_ &&
                   output_ &&
                   output_->ready();
        }

        unsigned source_rate() const
        {
            return static_cast<unsigned>(source_rate_);
        }

        unsigned queued_frames() const
        {
            return output_
                ? output_->queued_frames()
                : 0;
        }

        unsigned queued_ms() const
        {
            return queued_frames() * 1000 / 48000;
        }

        unsigned underruns() const
        {
            return underruns_;
        }

        unsigned recoveries() const
        {
            return recoveries_;
        }

        unsigned minimum_ms() const
        {
            if (!started_)
                return 0;

            return queue_min_ * 1000 / 48000;
        }

        unsigned maximum_ms() const
        {
            return queue_max_ * 1000 / 48000;
        }

        bool buffering() const
        {
            return buffering_;
        }

        unsigned overruns() const
        {
            return overruns_;
        }

        unsigned errors() const
        {
            return errors_;
        }

        unsigned peak() const
        {
            return peak_hold_;
        }

        bool submit(
            const int16_t* samples,
            size_t frames)
        {
            if (!ready())
                return false;

            if (!frames)
                return true;

            if (!samples || frames > 8192)
            {
                ++errors_;
                return false;
            }

            if (started_ && !buffering_)
            {
                const unsigned pending = queued_frames();

                if (pending < queue_min_)
                    queue_min_ = pending;

                if (pending > queue_max_)
                    queue_max_ = pending;

                if (pending == 0)
                    ++underruns_;

                if (pending <= recovery_frames)
                {
                    output_->set_paused(true);

                    buffering_ = true;

                    ++recoveries_;
                }
            }

            /*
             * Measure generated PCM independently
             * of the user's playback volume.
             */

            peak_ = 0;

            for (size_t i = 0; i < frames * 2; ++i)
            {
                const int value = samples[i];

                const unsigned amplitude =
                    static_cast<unsigned>(
                        value < 0 ? -value : value);

                if (amplitude > peak_)
                    peak_ = amplitude;
            }

            if (peak_ > peak_hold_)
                peak_hold_ = peak_;
            else
                peak_hold_ = peak_hold_ * 63 / 64;

            /*
             * Preserve the SDL stream between frames
             * so resampling phase is continuous.
             */

            if (SDL_AudioStreamPut(
                    stream_,
                    samples,
                    static_cast<int>(
                        frames * 2 * sizeof(int16_t))) != 0)
            {
                ++errors_;
                return false;
            }

            while (true)
            {
                const int available =
                    SDL_AudioStreamAvailable(stream_);

                if (available < 0)
                {
                    ++errors_;
                    return false;
                }

                if (available < 4)
                    break;

                int amount = available;

                if (amount >
                    static_cast<int>(sizeof(converted_)))
                {
                    amount = sizeof(converted_);
                }

                amount -= amount % 4;

                const int received =
                    SDL_AudioStreamGet(
                        stream_,
                        converted_,
                        amount);

                if (received <= 0 || received % 4)
                {
                    ++errors_;
                    return false;
                }

                const unsigned count =
                    static_cast<unsigned>(received / 4);

                const unsigned pending =
                    queued_frames();

                /*
                 * Never permit unbounded queue growth.
                 * Count discarded chunks rather than
                 * blocking the main game loop.
                 */

                if (pending > maximum_frames ||
                    count > maximum_frames - pending)
                {
                    ++overruns_;
                    continue;
                }

                if (!output_->queue(converted_, count))
                {
                    ++errors_;
                    return false;
                }
            }

            const unsigned pending = queued_frames();

            if (buffering_ && pending >= target_frames)
            {
                if (!started_)
                {
                    queue_min_ = pending;
                    queue_max_ = pending;
                }

                started_ = true;
                buffering_ = false;

                output_->set_paused(false);
            }

            if (started_ && !buffering_)
            {
                if (pending < queue_min_)
                    queue_min_ = pending;

                if (pending > queue_max_)
                    queue_max_ = pending;
            }

            return true;
        }
    };
}
