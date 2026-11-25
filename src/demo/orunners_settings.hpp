#pragma once

#include "ps5_storage.hpp"

#include <cstdint>
#include <cstring>

namespace orunners
{
    /*
     * ORUNNERS_SETTINGS_V1
     *
     * Explicit byte encoding avoids structure padding,
     * compiler ABI changes and byte-order dependencies.
     */

    struct Settings
    {
        int wide = 1;
        bool fps = false;

        int dead_zone = 10;
        int sensitivity = 5;
        int steering_speed = 5;

        int volume = 5;
        int vibration = 50;

        bool both_radios = false;
    };

    inline constexpr char settings_name[] =
        "outrunners-settings.bin";

    inline constexpr unsigned settings_size = 17;

    inline uint32_t settings_checksum(
        const uint8_t* bytes,
        unsigned count)
    {
        uint32_t value = 2166136261u;

        for (unsigned i = 0; i < count; ++i)
        {
            value ^= bytes[i];
            value *= 16777619u;
        }

        return value;
    }

    inline bool valid_settings(
        const Settings& s)
    {
        return
            s.wide >= 0 &&
            s.wide <= 4 &&

            s.dead_zone >= 0 &&
            s.dead_zone <= 50 &&
            s.dead_zone % 5 == 0 &&

            s.sensitivity >= 1 &&
            s.sensitivity <= 9 &&

            s.steering_speed >= 1 &&
            s.steering_speed <= 9 &&

            s.volume >= 0 &&
            s.volume <= 10 &&

            s.vibration >= 0 &&
            s.vibration <= 100 &&
            s.vibration % 10 == 0;
    }

    inline void encode_settings(
        const Settings& s,
        uint8_t* data)
    {
        std::memcpy(data, "ORPS", 4);

        data[4] = 1;

        data[5] = static_cast<uint8_t>(s.wide);
        data[6] = static_cast<uint8_t>(s.fps);

        data[7] =
            static_cast<uint8_t>(s.dead_zone);

        data[8] =
            static_cast<uint8_t>(s.sensitivity);

        data[9] =
            static_cast<uint8_t>(s.steering_speed);

        data[10] =
            static_cast<uint8_t>(s.volume);

        data[11] =
            static_cast<uint8_t>(s.vibration);

        data[12] =
            static_cast<uint8_t>(s.both_radios);

        const uint32_t checksum =
            settings_checksum(data, 13);

        for (unsigned i = 0; i < 4; ++i)
        {
            data[13 + i] =
                static_cast<uint8_t>(
                    checksum >> (i * 8));
        }
    }

    inline bool decode_settings(
        const uint8_t* data,
        Settings& output)
    {
        if (std::memcmp(data, "ORPS", 4) != 0 ||
            data[4] != 1 ||
            data[6] > 1 ||
            data[12] > 1)
        {
            return false;
        }

        uint32_t expected = 0;

        for (unsigned i = 0; i < 4; ++i)
        {
            expected |=
                static_cast<uint32_t>(
                    data[13 + i]) << (i * 8);
        }

        if (settings_checksum(data, 13) != expected)
            return false;

        Settings s;

        s.wide = data[5];
        s.fps = data[6] != 0;
        s.dead_zone = data[7];
        s.sensitivity = data[8];
        s.steering_speed = data[9];
        s.volume = data[10];
        s.vibration = data[11];
        s.both_radios = data[12] != 0;

        if (!valid_settings(s))
            return false;

        output = s;

        return true;
    }

    inline bool load_settings(Settings& output)
    {
        uint8_t data[settings_size]{};

        if (!ps5::storage::read(
                settings_name,
                data,
                sizeof(data)))
        {
            return false;
        }

        return decode_settings(data, output);
    }

    inline bool save_settings(
        const Settings& settings)
    {
        if (!valid_settings(settings))
            return false;

        uint8_t data[settings_size]{};

        encode_settings(settings, data);

        if (!ps5::storage::write(
                settings_name,
                data,
                sizeof(data)))
        {
            return false;
        }

        uint8_t verified[settings_size]{};

        return ps5::storage::read(
                   settings_name,
                   verified,
                   sizeof(verified)) &&
               std::memcmp(
                   verified,
                   data,
                   sizeof(data)) == 0;
    }
}
