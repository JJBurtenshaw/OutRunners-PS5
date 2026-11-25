#include "ps5_framebuffer.hpp"

#include <sys/mman.h>
#include <cstring>

extern "C" int sceKernelAllocateMainDirectMemory(
    size_t length,
    size_t alignment,
    int memoryType,
    off_t* physical
);

extern "C" int sceKernelMapDirectMemory(
    void** address,
    size_t length,
    int protection,
    int flags,
    off_t physical,
    size_t alignment
);

extern "C" int sceKernelReleaseDirectMemory(
    off_t physical,
    size_t length
);

namespace ps5
{
    Framebuffer::~Framebuffer()
    {
        release();
    }

    bool Framebuffer::create(
        int width,
        int height,
        Uint32 format)
    {
        if (width <= 0 || height <= 0 ||
            width > 1920 || height > 1080)
        {
            SDL_SetError(
                "Unsupported framebuffer dimensions");
            return false;
        }

        if (SDL_BYTESPERPIXEL(format) != 4)
        {
            SDL_SetError(
                "Framebuffer requires 32-bit pixels");
            return false;
        }

        // Match the cached direct-memory parameters
        // used by the verified native PS5 renderer.

        constexpr size_t alignment = 0x20000;

        const size_t required =
            static_cast<size_t>(width) *
            static_cast<size_t>(height) * 4;

        const size_t allocation =
            (required + alignment - 1) &
            ~(alignment - 1);

        off_t physical = 0;

        if (sceKernelAllocateMainDirectMemory(
                allocation,
                alignment,
                12,
                &physical) != 0)
        {
            SDL_SetError(
                "Cached framebuffer allocation failed");

            return false;
        }

        void* memory = nullptr;

        if (sceKernelMapDirectMemory(
                &memory,
                allocation,
                0x33,
                0,
                physical,
                alignment) != 0 ||
            !memory)
        {
            sceKernelReleaseDirectMemory(
                physical, allocation);

            SDL_SetError(
                "Cached framebuffer mapping failed");

            return false;
        }

        std::memset(memory, 0, required);

        SDL_Surface* next =
            SDL_CreateRGBSurfaceWithFormatFrom(
                memory,
                width,
                height,
                32,
                width * 4,
                format
            );

        if (!next)
        {
            munmap(memory, allocation);

            sceKernelReleaseDirectMemory(
                physical, allocation);

            return false;
        }

        if (SDL_SetSurfaceBlendMode(
                next, SDL_BLENDMODE_NONE) != 0)
        {
            SDL_FreeSurface(next);
            munmap(memory, allocation);

            sceKernelReleaseDirectMemory(
                physical, allocation);

            return false;
        }

        // Replace the previous buffer only after
        // its replacement has been created.

        release();

        surface_ = next;
        memory_ = memory;
        physical_ = physical;
        allocation_size_ = allocation;

        return true;
    }

    void Framebuffer::release()
    {
        if (surface_)
        {
            SDL_FreeSurface(surface_);
            surface_ = nullptr;
        }

        if (memory_)
        {
            munmap(memory_, allocation_size_);

            sceKernelReleaseDirectMemory(
                physical_, allocation_size_);

            memory_ = nullptr;
        }

        physical_ = 0;
        allocation_size_ = 0;
    }

    bool Framebuffer::blit_to(
        SDL_Surface* destination,
        bool stretch) const
    {
        if (!surface_ || !destination)
            return false;

        SDL_Rect target{
            0, 0,
            destination->w,
            destination->h
        };

        if (!stretch)
        {
            const long long scaled_width =
                static_cast<long long>(surface_->w) *
                destination->h / surface_->h;

            if (scaled_width <= destination->w)
            {
                target.w =
                    static_cast<int>(scaled_width);
            }
            else
            {
                target.h =
                    static_cast<int>(
                        static_cast<long long>(
                            surface_->h) *
                        destination->w /
                        surface_->w);
            }

            target.x =
                (destination->w - target.w) / 2;

            target.y =
                (destination->h - target.h) / 2;
        }

        // Clear letterboxing/pillarboxing if required.

        if (target.w != destination->w ||
            target.h != destination->h)
        {
            if (SDL_FillRect(
                    destination,
                    nullptr,
                    SDL_MapRGB(
                        destination->format,
                        0, 0, 0)) != 0)
                return false;
        }

        return SDL_BlitScaled(
            surface_,
            nullptr,
            destination,
            &target
        ) == 0;
    }
}
