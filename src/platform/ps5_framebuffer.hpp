#pragma once

#include <SDL.h>
#include <cstddef>
#include <sys/types.h>

namespace ps5
{
    class Framebuffer
    {
    public:
        Framebuffer() = default;

        ~Framebuffer();

        Framebuffer(const Framebuffer&) = delete;

        Framebuffer& operator=(
            const Framebuffer&) = delete;

        bool create(
            int width,
            int height,
            Uint32 format
        );

        void release();

        SDL_Surface* surface() const
        {
            return surface_;
        }

        bool blit_to(
            SDL_Surface* destination,
            bool stretch = false
        ) const;

    private:
        SDL_Surface* surface_ = nullptr;

        void* memory_ = nullptr;

        off_t physical_ = 0;

        std::size_t allocation_size_ = 0;
    };
}
