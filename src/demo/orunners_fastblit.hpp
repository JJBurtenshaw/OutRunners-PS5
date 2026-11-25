#pragma once

#include <SDL.h>
#include <cstdint>
#include <cstring>

/*
 * OutRunners PS5 FastBlit V1.
 *
 * Nearest-neighbour scaling directly into a
 * 32-bit destination surface.
 *
 * Preserves the existing native framebuffer
 * implementation as the fallback renderer.
 */

namespace orunners
{
    class FastBlit
    {
    private:

        static constexpr int max_width = 1920;
        static constexpr int max_height = 1080;
        static constexpr int max_source_width = 496;

        int source_width_ = 0;
        int source_height_ = 0;

        // ORUNNERS_FASTBLIT_ASPECT_V2
        bool arcade_aspect_ = false;

        int output_width_ = 0;
        int output_height_ = 0;

        Uint32 output_format_ = 0;

        int target_x_ = 0;
        int target_y_ = 0;
        int target_width_ = 0;
        int target_height_ = 0;

        int xmap_[max_width]{};
        int ymap_[max_height]{};

        alignas(64) Uint32 expanded_[max_width]{};
        alignas(64) Uint32 converted_[max_source_width]{};
        alignas(64) Uint32 black_row_[max_width]{};

        Uint32 red_[256]{};
        Uint32 green_[256]{};
        Uint32 blue_[256]{};

        Uint32 alpha_ = 0;

        bool rgb_order_ = false;
        bool bgr_order_ = false;

        bool configure(
            SDL_Surface* output,
            int width,
            int height,
            bool arcade_aspect)
        {
            if (!output ||
                !output->format ||
                output->w <= 0 ||
                output->h <= 0 ||
                output->w > max_width ||
                output->h > max_height ||
                width <= 0 ||
                width > max_source_width ||
                height <= 0 ||
                output->format->BytesPerPixel != 4 ||
                output->pitch < output->w * 4)
                return false;

            SDL_PixelFormat* format = output->format;

            if (format->Rloss != 0 ||
                format->Gloss != 0 ||
                format->Bloss != 0)
                return false;

            if (source_width_ == width &&
                source_height_ == height &&
                arcade_aspect_ == arcade_aspect &&
                output_width_ == output->w &&
                output_height_ == output->h &&
                output_format_ == format->format)
                return true;

            source_width_ = width;
            source_height_ = height;
            arcade_aspect_ = arcade_aspect;

            output_width_ = output->w;
            output_height_ = output->h;

            output_format_ = format->format;

            target_width_ = output_width_;
            target_height_ = output_height_;

            const int aspect_width =
                arcade_aspect ? 4 : width;

            const int aspect_height =
                arcade_aspect ? 3 : height;

            const long long scaled_width =
                static_cast<long long>(aspect_width) *
                output_height_ / aspect_height;

            if (scaled_width <= output_width_)
            {
                target_width_ =
                    static_cast<int>(scaled_width);
            }
            else
            {
                target_height_ =
                    static_cast<int>(
                        static_cast<long long>(aspect_height) *
                        output_width_ / aspect_width);
            }

            if (target_width_ <= 0 ||
                target_height_ <= 0)
                return false;

            target_x_ =
                (output_width_ - target_width_) / 2;

            target_y_ =
                (output_height_ - target_height_) / 2;

            for (int x = 0; x < target_width_; ++x)
            {
                xmap_[x] =
                    static_cast<int>(
                        (static_cast<long long>(2*x+1) *
                         width) /
                        (2LL * target_width_));
            }

            for (int y = 0; y < target_height_; ++y)
            {
                ymap_[y] =
                    static_cast<int>(
                        (static_cast<long long>(2*y+1) *
                         height) /
                        (2LL * target_height_));
            }

            alpha_ = format->Amask;

            for (unsigned i = 0; i < 256; ++i)
            {
                red_[i] =
                    (i << format->Rshift) &
                    format->Rmask;

                green_[i] =
                    (i << format->Gshift) &
                    format->Gmask;

                blue_[i] =
                    (i << format->Bshift) &
                    format->Bmask;
            }

            rgb_order_ =
                format->Rmask == 0x00ff0000u &&
                format->Gmask == 0x0000ff00u &&
                format->Bmask == 0x000000ffu;

            bgr_order_ =
                format->Rmask == 0x000000ffu &&
                format->Gmask == 0x0000ff00u &&
                format->Bmask == 0x00ff0000u;

            const Uint32 black =
                SDL_MapRGB(format, 0, 0, 0);

            for (int x = 0; x < output_width_; ++x)
                black_row_[x] = black;

            /*
             * Side borders remain black in the
             * expanded row. Only the image region
             * is replaced while rendering.
             */

            std::memcpy(
                expanded_,
                black_row_,
                static_cast<size_t>(output_width_) * 4);

            return true;
        }

        void expand(const uint32_t* source)
        {
            /*
             * Convert each original source pixel
             * once rather than converting every
             * enlarged destination pixel.
             */

            if (rgb_order_)
            {
                for (int x = 0; x < source_width_; ++x)
                {
                    converted_[x] =
                        (source[x] & 0x00ffffffu) |
                        alpha_;
                }
            }
            else if (bgr_order_)
            {
                for (int x = 0; x < source_width_; ++x)
                {
                    const Uint32 p = source[x];

                    converted_[x] =
                        ((p & 0x00ff0000u) >> 16) |
                         (p & 0x0000ff00u) |
                        ((p & 0x000000ffu) << 16) |
                        alpha_;
                }
            }
            else
            {
                for (int x = 0; x < source_width_; ++x)
                {
                    const Uint32 p = source[x];

                    converted_[x] =
                        red_[(p >> 16) & 255] |
                        green_[(p >> 8) & 255] |
                        blue_[p & 255] |
                        alpha_;
                }
            }

            Uint32* destination =
                expanded_ + target_x_;

            for (int x = 0; x < target_width_; ++x)
            {
                destination[x] =
                    converted_[xmap_[x]];
            }
        }

    public:

        bool draw(
            SDL_Surface* output,
            const uint32_t* pixels,
            int width,
            int height,
            int stride,
            bool arcade_aspect = false)
        {
            if (!pixels ||
                stride < width ||
                !configure(output, width, height, arcade_aspect))
                return false;

            const bool lock = SDL_MUSTLOCK(output);

            if (lock && SDL_LockSurface(output) != 0)
                return false;

            if (!output->pixels)
            {
                if (lock)
                    SDL_UnlockSurface(output);

                return false;
            }

            auto* destination =
                static_cast<Uint8*>(output->pixels);

            const size_t row_bytes =
                static_cast<size_t>(output_width_) * 4;

            int previous_source_y = -1;

            for (int y = 0; y < output_height_; ++y)
            {
                Uint8* row =
                    destination +
                    static_cast<size_t>(y) *
                    output->pitch;

                const int relative_y =
                    y - target_y_;

                if (relative_y < 0 ||
                    relative_y >= target_height_)
                {
                    std::memcpy(
                        row,
                        black_row_,
                        row_bytes);

                    continue;
                }

                const int source_y =
                    ymap_[relative_y];

                /*
                 * Reuse the horizontally expanded
                 * row for repeated source scanlines.
                 */

                if (source_y != previous_source_y)
                {
                    expand(
                        pixels +
                        static_cast<size_t>(source_y) *
                        stride);

                    previous_source_y = source_y;
                }

                std::memcpy(
                    row,
                    expanded_,
                    row_bytes);
            }

            if (lock)
                SDL_UnlockSurface(output);

            return true;
        }
    };
}
