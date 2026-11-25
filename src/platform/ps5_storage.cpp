#include "ps5_storage.hpp"

#include <cstdio>
#include <cstring>

namespace
{
    // Restrict storage operations to simple filenames.
    // Do not allow absolute paths or directory traversal.

    bool valid_filename(const char* name)
    {
        if (!name || !*name)
            return false;

        if (std::strlen(name) > 96)
            return false;

        if (name[0] == '.')
            return false;

        for (const char* p = name; *p; ++p)
        {
            const char c = *p;

            const bool valid =
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '_' || c == '-' || c == '.';

            if (!valid)
                return false;
        }

        return true;
    }

    bool make_path(
        char* path,
        std::size_t capacity,
        const char* name,
        bool temporary)
    {
        if (!valid_filename(name))
            return false;

        const int length = std::snprintf(
            path,
            capacity,
            "%s%s%s",
            ps5::storage::save_root,
            name,
            temporary ? ".tmp" : ""
        );

        return length > 0 &&
            static_cast<std::size_t>(length) < capacity;
    }
}

namespace ps5
{
    namespace storage
    {
        bool read(
            const char* filename,
            void* data,
            std::size_t bytes)
        {
            if (!data || bytes == 0)
                return false;

            char path[160]{};

            if (!make_path(
                    path, sizeof(path),
                    filename, false))
                return false;

            FILE* file = std::fopen(path, "rb");

            if (!file)
                return false;

            const bool complete =
                std::fread(
                    data, 1, bytes, file
                ) == bytes;

            // Reject truncated or oversized files.

            const bool exact =
                complete &&
                std::fgetc(file) == EOF &&
                !std::ferror(file);

            const bool closed =
                std::fclose(file) == 0;

            return exact && closed;
        }

        bool write(
            const char* filename,
            const void* data,
            std::size_t bytes)
        {
            if (!data || bytes == 0)
                return false;

            char path[160]{};
            char temporary[160]{};

            if (!make_path(
                    path, sizeof(path),
                    filename, false))
                return false;

            if (!make_path(
                    temporary, sizeof(temporary),
                    filename, true))
                return false;

            // Write to a temporary file first.
            // Keep the previous save until the
            // replacement has been written.

            FILE* file = std::fopen(
                temporary, "wb");

            if (!file)
                return false;

            const bool complete =
                std::fwrite(
                    data, 1, bytes, file
                ) == bytes;

            const bool flushed =
                std::fflush(file) == 0;

            const bool closed =
                std::fclose(file) == 0;

            if (!complete || !flushed || !closed)
            {
                std::remove(temporary);
                return false;
            }

            if (std::rename(temporary, path) != 0)
            {
                std::remove(temporary);
                return false;
            }

            return true;
        }
    }
}
