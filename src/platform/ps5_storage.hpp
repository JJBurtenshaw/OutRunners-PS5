#pragma once

#include <cstddef>

namespace ps5
{
    namespace storage
    {
        constexpr const char* asset_root = "/app0/";

        constexpr const char* save_root = "/download0/";

        bool read(
            const char* filename,
            void* data,
            std::size_t bytes
        );

        bool write(
            const char* filename,
            const void* data,
            std::size_t bytes
        );
    }
}
