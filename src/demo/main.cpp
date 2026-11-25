/*
 * PS5 Native SDL2 Starter - reference demo
 *
 * Demonstrates native software rendering, controller
 * input, audio, persistent settings and diagnostics.
 *
 * Keep PS5-specific platform workarounds in the
 * platform layer rather than duplicating them in
 * individual games.
 *
 * See PS5-DEVELOPMENT-GUIDE.md and
 * docs/PS5-IMPLEMENTATION-NOTES.md.
 */

#include "ps5_platform.hpp"
#include "ps5_framebuffer.hpp"
#include "ps5_audio.hpp"
#include "ps5_storage.hpp"
#include "ps5_frame_timer.hpp"
#include "ps5_debug_text.hpp"
#include "controller_diagnostics.hpp"
#include "orunners_fastblit.hpp"
#include "orunners_audio.hpp"
#include "orunners_input.hpp"
#include "orunners_rumble.hpp"
#include "orunners_settings.hpp"

/* ORUNNERS_TASK_PROBE_V1 */
extern "C" int orunners_task_probe(void);
/* ORUNNERS_ASSET_PROBE_V1 */
extern "C" int orunners_asset_probe(void);
/* ORUNNERS_ENGINE_PROBE_V1 */
extern "C" int orunners_engine_probe(void);

extern "C" void orunners_settings_get(int32_t* values);

extern "C" void orunners_settings_set(
    const int32_t* values);

extern "C" void orunners_settings_fps(int enabled);


/* ORUNNERS_PAUSE_MENU_V2 */

extern "C" bool console_can_pause(void);
extern "C" bool console_can_restart(void);
extern "C" void console_restart_game(void);
extern "C" void console_quit_game(void);

/* ORUNNERS_DISPLAY_MENU_V1 */

extern "C" const char* orunners_display_mode_label(void);

extern "C" void orunners_cycle_display_mode(int direction);

extern "C" int orunners_display_width(void);

/* ORUNNERS_SPLIT_SCREEN_V1 */

extern "C" bool console_both_screens(void);

extern "C" const uint32_t*
orunners_second_frame(int* width);





static int or_task_result = 0;

/* ORUNNERS_VISUAL_CONTEXT_V1 */

extern "C" void orunners_visual_checkpoint(int stage)
{

    // ORUNNERS_DIRECT_START_V1
    if (stage < 90)
        return;

    SDL_Surface *surface = ps5::screen();

    if (!surface)
        return;

    char label[8];

    SDL_snprintf(label, sizeof(label), "%02d", stage);

    const bool error = stage >= 90;
    const bool passed = stage == 9;

    Uint32 background = SDL_MapRGB(
        surface->format,
        error ? 160 : passed ? 25 : 170,
        error ? 25  : passed ? 120 : 100,
        error ? 25  : passed ? 45 : 15);

    Uint32 foreground = SDL_MapRGB(
        surface->format, 255, 255, 255);

    SDL_FillRect(surface, nullptr, background);

    ps5::debug::text(
        surface,
        surface->w / 2 - 120,
        surface->h / 2 - 80,
        label,
        20,
        foreground);

    ps5::present();

    // ORUNNERS_FAST_CHECKPOINTS_V1
    SDL_Delay(
        stage >= 90 ? 3000 :
        stage == 9  ? 1000 :
        stage <= 6  ? 250  :
        stage >= 30 && stage <= 35 ? 0 :
        stage >= 10 ? 250  :
                      500);
}



#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace
{
    SDL_GameController* controller = nullptr;

    // ORUNNERS_DUAL_CONTROLLER_V1

    SDL_GameController* controller_two = nullptr;

    // ORUNNERS_NATIVE_RUMBLE_V1

    orunners::Rumble controller_rumble[2];

    void stop_all_rumble()
    {
        controller_rumble[0].stop();
        controller_rumble[1].stop();
    }



    ps5::Framebuffer framebuffer;

    // ORUNNERS_NATIVE_VIDEO_V1
    ps5::Framebuffer game_framebuffer;

    // ORUNNERS_FASTBLIT_V1
    orunners::FastBlit game_fastblit;

    orunners::FastBlit split_fastblit;

    bool split_screen_active = false;


    bool fastblit_active = false;
    bool renderer_reported = false;


    bool game_exit_requested = false;

    // ORUNNERS_PAUSE_MENU_V1

    bool pause_requested = false;

    uint32_t pause_input_suppression[2] = {};


    Uint64 game_next_frame = 0;
    Uint64 game_frequency = 0;

    // ORUNNERS_PROFILING_V1

    struct GameProfile
    {
        Uint64 engine = 0;
        Uint64 input = 0;
        Uint64 convert = 0;
        Uint64 scale = 0;
        Uint64 wait = 0;
        Uint64 present = 0;
        Uint64 total = 0;

        unsigned frames = 0;
    };

    GameProfile profile_sum{};
    GameProfile profile_average{};

    Uint64 profile_previous_end = 0;
    Uint64 profile_window_start = 0;

    Uint64 profile_frequency = 0;

    unsigned profile_fps10 = 0;

    Uint64 ticks()
    {
        return SDL_GetPerformanceCounter();
    }

    unsigned milliseconds100(Uint64 value)
    {
        if (!profile_frequency)
            return 0;

        return static_cast<unsigned>(
            value * 100000 / profile_frequency);
    }

    void profile_record(
        const GameProfile& sample,
        Uint64 end)
    {
        profile_sum.engine += sample.engine;
        profile_sum.input += sample.input;
        profile_sum.convert += sample.convert;
        profile_sum.scale += sample.scale;
        profile_sum.wait += sample.wait;
        profile_sum.present += sample.present;
        profile_sum.total += sample.total;
        ++profile_sum.frames;

        if (!profile_window_start)
            profile_window_start = end;

        const Uint64 elapsed =
            end - profile_window_start;

        if (elapsed < profile_frequency)
            return;

        const unsigned n = profile_sum.frames;

        if (n)
        {
            profile_average.engine =
                profile_sum.engine / n;

            profile_average.input =
                profile_sum.input / n;

            profile_average.convert =
                profile_sum.convert / n;

            profile_average.scale =
                profile_sum.scale / n;

            profile_average.wait =
                profile_sum.wait / n;

            profile_average.present =
                profile_sum.present / n;

            profile_average.total =
                profile_sum.total / n;

            profile_average.frames = n;

            profile_fps10 = static_cast<unsigned>(
                10 * (Uint64)n *
                profile_frequency / elapsed);
        }

        profile_sum = GameProfile{};
        profile_window_start = end;
    }

    void draw_game_profile(SDL_Surface* output);


    ps5::Audio audio;

    // ORUNNERS_NATIVE_AUDIO_V1
    orunners::AudioBridge game_audio;

    bool game_audio_failed = false;

    ps5::FrameTimer frame_timer;

    bool show_fps = false;

    // ORUNNERS_NATIVE_SETTINGS_V1

    orunners::Settings native_settings;

    bool native_settings_ready = false;
    bool native_settings_dirty = false;

    int applied_game_volume = 5;

    Uint64 settings_last_change = 0;
    Uint64 settings_last_attempt = 0;

    uint8_t settings_snapshot[
        orunners::settings_size]{};

    void save_native_settings(bool force);
    void toggle_game_fps();


    // ORUNNERS_PROFILE_ORDER_FIX_V1
    void draw_game_profile(SDL_Surface* output)
    {
        if (!show_fps)
            return;

        /*
         * ORUNNERS_COMPACT_FPS_V1
         *
         * Normal display: compact FPS counter.
         *
         * Hold L3 on either attached controller
         * to temporarily reveal development
         * diagnostics.
         *
         * All profiling counters remain active.
         */

        const bool detailed =
            (controller &&
             SDL_GameControllerGetAttached(controller) &&
             SDL_GameControllerGetButton(
                 controller,
                 SDL_CONTROLLER_BUTTON_LEFTSTICK)) ||

            (controller_two &&
             SDL_GameControllerGetAttached(controller_two) &&
             SDL_GameControllerGetButton(
                 controller_two,
                 SDL_CONTROLLER_BUTTON_LEFTSTICK));

        if (!detailed)
        {
            const int panel_w = 152;
            const int panel_h = 40;

            const int x =
                output->w - panel_w - 16;

            const int y = 16;

            SDL_Rect panel{
                x, y, panel_w, panel_h
            };

            SDL_FillRect(
                output,
                &panel,
                SDL_MapRGB(output->format, 0, 0, 0));

            char label[32]{};

            if (profile_fps10)
            {
                std::snprintf(
                    label,
                    sizeof(label),
                    "FPS %u.%u",
                    profile_fps10 / 10,
                    profile_fps10 % 10);
            }
            else
            {
                std::snprintf(
                    label,
                    sizeof(label),
                    "FPS ...");
            }

            ps5::debug::text(
                output,
                x + 12,
                y + 12,
                label,
                3,
                SDL_MapRGB(
                    output->format,
                    255, 255, 255));

            return;
        }

        const int x = output->w - 400 - 16;
        const int y = 16;

        SDL_Rect panel{x, y, 400, 812};

        SDL_FillRect(
            output,
            &panel,
            SDL_MapRGB(output->format, 0, 0, 0));

        const Uint32 white =
            SDL_MapRGB(
                output->format,
                255, 255, 255);

        char label[64]{};

        std::snprintf(
            label,
            sizeof(label),
            fastblit_active
                ? "FPS %u.%u FAST"
                : "FPS %u.%u SDL",
            profile_fps10 / 10,
            profile_fps10 % 10);

        ps5::debug::text(
            output, x + 12, y + 10,
            label, 3, white);

        const char* names[] = {
            "FRAME",
            "ENGINE",
            "INPUT",
            "CONVERT",
            "SCALE",
            "WAIT",
            "PRESENT"
        };

        const Uint64 values[] = {
            profile_average.total,
            profile_average.engine,
            profile_average.input,
            profile_average.convert,
            profile_average.scale,
            profile_average.wait,
            profile_average.present
        };

        for (int i = 0; i < 7; ++i)
        {
            unsigned ms =
                milliseconds100(values[i]);

            std::snprintf(
                label,
                sizeof(label),
                "%s %u.%02u",
                names[i],
                ms / 100,
                ms % 100);

            ps5::debug::text(
                output,
                x + 12,
                y + 42 + i * 30,
                label,
                3,
                white);
        }
    

        const char* audio_labels[] = {
            "RATE",
            "QUEUE",
            "PEAK",
            "UNDERRUN",
            "OVERRUN",
            "ERROR",
            "RECOVER",
            "QMIN",
            "QMAX"
        };

        const unsigned audio_values[] = {
            game_audio.source_rate(),
            game_audio.queued_ms(),
            game_audio.peak(),
            game_audio.underruns(),
            game_audio.overruns(),
            game_audio.errors(),
            game_audio.recoveries(),
            game_audio.minimum_ms(),
            game_audio.maximum_ms()
        };

        for (int i = 0; i < 9; ++i)
        {
            std::snprintf(
                label,
                sizeof(label),
                "%s %u",
                audio_labels[i],
                audio_values[i]);

            ps5::debug::text(
                output,
                x + 12,
                y + 260 + i * 27,
                label,
                3,
                white);
        }

        // ORUNNERS_AUDIO_DIAGNOSTICS_V2

        ps5::debug::text(
            output,
            x + 12,
            y + 510,
            game_audio.buffering()
                ? "BUFFERING"
                : "PLAYING",
            3,
            white);

        if (game_audio_failed)
        {
            ps5::debug::text(
                output,
                x + 12,
                y + 537,
                "AUDIO FAILED",
                3,
                white);
        }

    

        // ORUNNERS_RUMBLE_DIAGNOSTICS_V1

        SDL_Rect separator{
            x + 12,
            y + 566,
            376,
            2
        };

        SDL_FillRect(
            output,
            &separator,
            SDL_MapRGB(
                output->format,
                90, 90, 90));

        const int strength =
            native_settings.vibration;

        std::snprintf(
            label,
            sizeof(label),
            "RUMBLE LEVEL %d",
            strength);

        ps5::debug::text(
            output,
            x + 12,
            y + 580,
            label,
            3,
            white);

        for (int player = 0; player < 2; ++player)
        {
            const auto& rumble =
                controller_rumble[player];

            const int top =
                y + 610 + player * 96;

            const char* status =
                !rumble.attached()
                    ? "NO PAD"
                    : !rumble.supported()
                        ? "UNSUPPORTED"
                        : rumble.running()
                            ? "ACTIVE"
                            : "READY";

            std::snprintf(
                label,
                sizeof(label),
                "P%d %s",
                player + 1,
                status);

            ps5::debug::text(
                output,
                x + 12,
                top,
                label,
                3,
                white);

            std::snprintf(
                label,
                sizeof(label),
                "MOTOR %u",
                rumble.motor_frames());

            ps5::debug::text(
                output,
                x + 12,
                top + 22,
                label,
                3,
                white);

            std::snprintf(
                label,
                sizeof(label),
                "OK %u",
                rumble.accepted());

            ps5::debug::text(
                output,
                x + 12,
                top + 44,
                label,
                3,
                white);

            std::snprintf(
                label,
                sizeof(label),
                "ERROR %u",
                rumble.errors());

            ps5::debug::text(
                output,
                x + 12,
                top + 66,
                label,
                3,
                white);
        }

    }

    bool audio_attempted = false;

    // Keep only the current session's log, capped at 64 KiB.
    void trace(const char* message)
    {
        static bool started = false;
        constexpr long limit = 64 * 1024;
        if (!message) return;
        FILE* file = std::fopen(
            "/download0/outrunners.log", started ? "a" : "w");
        if (!file) return;
        started = true;
        if (std::fseek(file, 0, SEEK_END) == 0)
        {
            const long size = std::ftell(file);
            if (size >= 0 && size < limit)
            {
                const int room = static_cast<int>(limit - size - 1);
                std::fprintf(file, "%.*s\n", std::min(room, 1024), message);
            }
        }
        std::fclose(file);
    }

    // ----------------------------------------
    // Controller
    // ----------------------------------------


    SDL_GameController* player_controller(int player)
    {
        return player == 0
            ? controller
            : player == 1
                ? controller_two
                : nullptr;
    }

    SDL_JoystickID controller_id(
        SDL_GameController* pad)
    {
        if (!pad)
            return -1;

        SDL_Joystick* joystick =
            SDL_GameControllerGetJoystick(pad);

        if (!joystick)
            return -1;

        return SDL_JoystickInstanceID(joystick);
    }

    int controller_player(SDL_JoystickID id)
    {
        if (id < 0)
            return -1;

        if (controller_id(controller) == id)
            return 0;

        if (controller_id(controller_two) == id)
            return 1;

        return -1;
    }

    void open_controller()
    {
        SDL_GameController** slots[] = {
            &controller,
            &controller_two
        };

        // Release disconnected devices without
        // shifting the other player to a new seat.

        for (int player = 0; player < 2; ++player)
        {
            SDL_GameController*& pad =
                *slots[player];

            if (pad &&
                !SDL_GameControllerGetAttached(pad))
            {
                controller_rumble[player].detach();

                SDL_GameControllerClose(pad);
                pad = nullptr;

                pause_input_suppression[player] = 0;

                trace(
                    player == 0
                        ? "PLAYER 1 DISCONNECTED"
                        : "PLAYER 2 DISCONNECTED");
            }
        }

        if (controller && controller_two)
            return;

        for (int i = 0; i < SDL_NumJoysticks(); ++i)
        {
            if (!SDL_IsGameController(i))
                continue;

            const SDL_JoystickID id =
                SDL_JoystickGetDeviceInstanceID(i);

            if (id < 0 ||
                controller_player(id) >= 0)
                continue;

            int player = !controller
                ? 0
                : !controller_two
                    ? 1
                    : -1;

            if (player < 0)
                break;

            SDL_GameController* pad =
                SDL_GameControllerOpen(i);

            if (!pad)
                continue;

            *slots[player] = pad;

            controller_rumble[player].attach(pad);

            trace(
                controller_rumble[player].supported()
                    ? "CONTROLLER RUMBLE AVAILABLE"
                    : "CONTROLLER RUMBLE UNAVAILABLE");

            // Suppress buttons already held when
            // a controller attaches.

            pause_input_suppression[player] =
                orunners::read_pad(pad).buttons;

            trace(
                player == 0
                    ? "PLAYER 1 CONNECTED"
                    : "PLAYER 2 CONNECTED");
        }
    }

    // ----------------------------------------
    // Audio
    // ----------------------------------------

    bool ensure_audio()
    {
        if (audio.ready())
            return true;

        if (audio_attempted)
            return false;

        audio_attempted = true;

        trace("AUDIO INIT BEGIN");

        if (!audio.initialize())
        {
            trace("AUDIO INIT FAILED");
            trace(SDL_GetError());
            return false;
        }

        trace("AUDIO INIT OK");

        return true;
    }

    /*
     * Save only when the encoded settings change.
     * Debounce writes so menu navigation does not
     * cause repeated filesystem operations.
     */

    void save_native_settings(bool force)
    {
        if (!native_settings_ready ||
            !native_settings_dirty)
            return;

        const Uint64 now = SDL_GetTicks64();

        if (!force)
        {
            if (now - settings_last_change < 1500)
                return;

            if (now - settings_last_attempt < 5000)
                return;
        }

        settings_last_attempt = now;

        if (orunners::save_settings(native_settings))
        {
            native_settings_dirty = false;

            trace("OUTRUNNERS SETTINGS SAVED");
        }
        else
        {
            trace("OUTRUNNERS SETTINGS SAVE FAILED");
        }
    }

    void toggle_game_fps()
    {
        show_fps = !show_fps;

        if (native_settings_ready)
            orunners_settings_fps(show_fps ? 1 : 0);
    }

    // ----------------------------------------
    // Shutdown
    // ----------------------------------------

    [[noreturn]] void finish()
    {
        stop_all_rumble();

        controller_rumble[0].detach();
        controller_rumble[1].detach();

        save_native_settings(true);

        game_audio.shutdown();
        audio.shutdown();

        if (controller)
        {
            SDL_GameControllerClose(controller);
            controller = nullptr;
        }

        if (controller_two)
        {
            SDL_GameControllerClose(controller_two);
            controller_two = nullptr;
        }

        game_framebuffer.release();
        framebuffer.release();

        ps5::shutdown();
        ps5::exit_to_dashboard();
    }
}


/*
 * ORUNNERS_NATIVE_VIDEO_V1
 *
 * Live engine presentation through the verified
 * cached direct-memory SDL2 framebuffer.
 */




/*
 * ORUNNERS_RECORD_STORAGE_V1
 *
 * C interface to the verified native PS5 storage
 * implementation.
 *
 * Restrict access to the two original arcade
 * record filenames and their exact file sizes.
 */

static bool valid_record_file(
    const char* filename,
    size_t bytes)
{
    if (!filename)
        return false;

    if (std::strcmp(
            filename,
            "outrunners.eeprom") == 0)
        return bytes == 128;

    if (std::strcmp(
            filename,
            "outrunners.bookkeeping") == 0)
        return bytes == 250;

    return false;
}

extern "C" int orunners_record_read(
    const char* filename,
    void* data,
    size_t bytes)
{
    if (!data ||
        !valid_record_file(filename, bytes))
        return 0;

    const bool loaded =
        ps5::storage::read(
            filename,
            data,
            bytes);

    trace(
        loaded
            ? "ARCADE RECORD FILE LOADED"
            : "ARCADE RECORD FILE MISSING OR INVALID SIZE");

    trace(filename);

    return loaded ? 1 : 0;
}

extern "C" int orunners_record_write(
    const char* filename,
    const void* data,
    size_t bytes)
{
    if (!data ||
        !valid_record_file(filename, bytes))
        return 0;

    if (!ps5::storage::write(
            filename,
            data,
            bytes))
    {
        trace("ARCADE RECORD SAVE FAILED");
        trace(filename);

        return 0;
    }

    uint8_t verified[250]{};

    const bool valid =
        ps5::storage::read(
            filename,
            verified,
            bytes) &&
        std::memcmp(
            data,
            verified,
            bytes) == 0;

    trace(
        valid
            ? "ARCADE RECORD SAVE VERIFIED"
            : "ARCADE RECORD READBACK FAILED");

    trace(filename);

    return valid ? 1 : 0;
}


extern "C" void orunners_rumble_frame(
    int player,
    int motor,
    int strength)
{
    if (player < 0 || player >= 2)
        return;

    controller_rumble[player].update(
        motor != 0,
        strength);
}

extern "C" void orunners_settings_start(void)
{
    orunners::Settings saved;

    const bool loaded =
        orunners::load_settings(saved);

    native_settings = saved;

    const int32_t values[8] = {
        saved.wide,
        saved.fps ? 1 : 0,
        saved.dead_zone,
        saved.sensitivity,
        saved.steering_speed,
        saved.volume,
        saved.vibration,
        saved.both_radios ? 1 : 0
    };

    orunners_settings_set(values);

    show_fps = saved.fps;

    native_settings_ready = true;
    native_settings_dirty = !loaded;

    settings_last_change = SDL_GetTicks64();

    applied_game_volume = saved.volume;

    orunners::encode_settings(
        native_settings,
        settings_snapshot);

    trace(
        loaded
            ? "OUTRUNNERS SETTINGS LOADED"
            : "OUTRUNNERS DEFAULT SETTINGS");

    /*
     * Apply the initial volume before gameplay.
     * Later changes are applied to incoming PCM
     * rather than clearing the native audio queue.
     */

    save_native_settings(true);
}

extern "C" void orunners_settings_sync(void)
{
    if (!native_settings_ready)
        return;

    int32_t values[8]{};

    orunners_settings_get(values);

    orunners::Settings current;

    current.wide = values[0];
    current.fps = values[1] != 0;
    current.dead_zone = values[2];
    current.sensitivity = values[3];
    current.steering_speed = values[4];
    current.volume = values[5];
    current.vibration = values[6];
    current.both_radios = values[7] != 0;

    if (!orunners::valid_settings(current))
        return;

    show_fps = current.fps;
    applied_game_volume = current.volume;

    uint8_t encoded[orunners::settings_size]{};

    orunners::encode_settings(current, encoded);

    if (std::memcmp(
            encoded,
            settings_snapshot,
            sizeof(encoded)) != 0)
    {
        native_settings = current;

        std::memcpy(
            settings_snapshot,
            encoded,
            sizeof(encoded));

        native_settings_dirty = true;

        settings_last_change = SDL_GetTicks64();
    }

    save_native_settings(false);
}

extern "C" void orunners_audio_start(uint32_t rate)
{
    trace("OUTRUNNERS AUDIO START");

    if (!ensure_audio())
    {
        game_audio_failed = true;
        return;
    }

    audio.set_volume(80);
    audio.set_muted(false);

    if (!game_audio.initialize(audio, rate))
    {
        trace("AUDIO STREAM INIT FAILED");
        trace(SDL_GetError());

        game_audio_failed = true;

        game_audio.shutdown();
        audio.shutdown();

        return;
    }

    trace("OUTRUNNERS AUDIO READY");
}

extern "C" void orunners_audio_submit(
    const int16_t* samples,
    size_t frames)
{

    const int16_t* volume_samples = samples;

    if (applied_game_volume != 5 &&
        samples &&
        frames <= 4096)
    {
        static int16_t adjusted[8192];

        for (size_t i = 0; i < frames * 2; ++i)
        {
            const int value =
                static_cast<int>(samples[i]) *
                applied_game_volume / 5;

            adjusted[i] =
                static_cast<int16_t>(
                    std::clamp(value, -32768, 32767));
        }

        volume_samples = adjusted;
    }


    if (game_audio_failed)
        return;

    if (!game_audio.ready())
        return;

    if (!game_audio.submit(volume_samples, frames))
    {
        trace("OUTRUNNERS AUDIO SUBMIT FAILED");
        trace(SDL_GetError());

        /*
         * Keep graphics and engine running
         * even if audio playback fails.
         */

        game_audio_failed = true;

        game_audio.shutdown();
        audio.shutdown();
    }
}


/* ORUNNERS_NATIVE_INPUT_V1 */

extern "C" void orunners_read_pad(
    int player,
    int32_t* stick_x,
    int32_t* stick_y,
    int32_t* accelerator,
    int32_t* brake,
    uint32_t* buttons)
{
    orunners::PadState state{};

    if (player >= 0 && player < 2)
    {
        state = orunners::read_pad(
            player_controller(player));

        pause_input_suppression[player] &=
            state.buttons;

        state.buttons &=
            ~pause_input_suppression[player];
    }

    *stick_x = state.stick_x;
    *stick_y = state.stick_y;
    *accelerator = state.accelerator;
    *brake = state.brake;
    *buttons = state.buttons;
}


/*
 * ORUNNERS_PAUSE_MENU_V1
 *
 * A blocking native menu inside the existing
 * game frame callback.
 *
 * No additional game frames are executed while
 * this menu is active.
 */


enum PauseItem
{
    PAUSE_RESUME,
    PAUSE_RESTART,
    PAUSE_ATTRACT,
    PAUSE_DISPLAY,
    PAUSE_FPS,
    PAUSE_EXIT,
    PAUSE_ITEMS
};

static bool pause_item_enabled(int item)
{
    switch (item)
    {
        case PAUSE_RESTART:
            return console_can_pause() &&
                   console_can_restart();

        case PAUSE_ATTRACT:
            return console_can_pause();

        default:
            return true;
    }
}

static int pause_next_item(
    int selected,
    int direction)
{
    do
    {
        selected =
            (selected + direction + PAUSE_ITEMS) %
            PAUSE_ITEMS;
    }
    while (!pause_item_enabled(selected));

    return selected;
}

static void draw_pause_menu(
    SDL_Surface* output,
    int selected)
{
    const int panel_w = 800;
    const int panel_h = 650;

    const int x = (output->w - panel_w) / 2;
    const int y = (output->h - panel_h) / 2;

    const Uint32 background =
        SDL_MapRGB(output->format, 18, 24, 34);

    const Uint32 highlight =
        SDL_MapRGB(output->format, 55, 79, 114);

    const Uint32 white =
        SDL_MapRGB(output->format, 255, 255, 255);

    const Uint32 grey =
        SDL_MapRGB(output->format, 160, 173, 190);

    SDL_Rect panel{
        x, y, panel_w, panel_h
    };

    SDL_FillRect(output, &panel, background);

    const auto centered_text =
        [output, x](
            const char* text,
            int top,
            int scale,
            Uint32 color)
        {
            const int text_width =
                static_cast<int>(std::strlen(text)) *
                4 * scale;

            ps5::debug::text(
                output,
                x + (panel_w - text_width) / 2,
                top,
                text,
                scale,
                color);
        };

    centered_text("PAUSED", y + 25, 8, white);

    SDL_Rect divider{
        x + 32, y + 92,
        panel_w - 64, 2
    };

    SDL_FillRect(output, &divider, grey);

    const char* items[] = {
        "RESUME",
        "RESTART RACE",
        "RETURN TO ATTRACT MODE",
        orunners_display_mode_label(),
        show_fps
            ? "FPS DISPLAY: ON"
            : "FPS DISPLAY: OFF",
        "EXIT TO DASHBOARD"
    };

    for (int i = 0; i < PAUSE_ITEMS; ++i)
    {
        const int row_y =
            y + 116 + i * 70;

        if (i == selected)
        {
            SDL_Rect active{
                x + 24,
                row_y,
                panel_w - 48,
                58
            };

            SDL_FillRect(
                output,
                &active,
                highlight);
        }

        centered_text(
            items[i],
            row_y + 16,
            5,
            pause_item_enabled(i)
                ? white
                : grey);
    }

    centered_text(
        "DPAD NAVIGATE  CROSS SELECT",
        y + 562,
        3,
        grey);

    centered_text(
        "LEFT RIGHT CHANGE DISPLAY",
        y + 590,
        3,
        grey);

    centered_text(
        "CIRCLE OR TOUCHPAD RESUME",
        y + 617,
        3,
        grey);
}


/*
 * ORUNNERS_SPLIT_SCREEN_V1
 *
 * Two non-owning views of the existing display
 * surface. The views are never freed or presented
 * independently.
 *
 * The original display surface is locked once,
 * and its actual pixel format and pitch are used.
 */

static bool draw_split_screen(
    SDL_Surface* output,
    const uint32_t* left_pixels,
    int left_width,
    const uint32_t* right_pixels,
    int right_width,
    int height,
    int stride)
{
    if (!output ||
        !output->format ||
        !left_pixels ||
        !right_pixels ||
        height != 224 ||
        stride != 496 ||
        left_width <= 0 ||
        right_width <= 0 ||
        left_width > stride ||
        right_width > stride ||
        output->format->BytesPerPixel != 4 ||
        output->w <= 0 ||
        output->h <= 0 ||
        output->w > 1920 ||
        output->h > 1080 ||
        (output->w & 1) ||
        output->pitch < output->w * 4)
    {
        return false;
    }

    // Crop any transitional wide frames to the
    // original 320-pixel cabinet view.

    if (left_width > 320)
    {
        left_pixels += (left_width - 320) / 2;
        left_width = 320;
    }

    if (right_width > 320)
    {
        right_pixels += (right_width - 320) / 2;
        right_width = 320;
    }

    const bool locked = SDL_MUSTLOCK(output);

    if (locked && SDL_LockSurface(output) != 0)
        return false;

    if (!output->pixels)
    {
        if (locked)
            SDL_UnlockSurface(output);

        return false;
    }

    const int half_width = output->w / 2;

    /*
     * FastBlit accesses only the surface dimensions,
     * pixel pointer, format, pitch and lock flags.
     *
     * Both views refer to the same underlying native
     * display memory. Each writes only its own half.
     */

    SDL_Surface left = *output;
    SDL_Surface right = *output;

    left.flags = 0;
    right.flags = 0;

    left.w = half_width;
    right.w = half_width;

    left.clip_rect = {
        0, 0, half_width, output->h
    };

    right.clip_rect = left.clip_rect;

    right.pixels =
        static_cast<Uint8*>(output->pixels) +
        half_width * 4;

    const bool left_ok = split_fastblit.draw(
        &left,
        left_pixels,
        left_width,
        height,
        stride,
        true);

    const bool right_ok = split_fastblit.draw(
        &right,
        right_pixels,
        right_width,
        height,
        stride,
        true);

    if (locked)
        SDL_UnlockSurface(output);

    return left_ok && right_ok;
}


/*
 * ORUNNERS_CONFIRM_DIALOG_V1
 *
 * Use the existing diagnostic font and native
 * surface drawing routines. No additional assets
 * or framebuffer allocations are required.
 */

static void draw_pause_confirmation(
    SDL_Surface* output,
    int action,
    bool yes)
{
    const char* heading = "CONFIRM";
    const char* message = "CONTINUE?";

    switch (action)
    {
        case PAUSE_RESTART:

            heading = "RESTART RACE";
            message = "RESTART THE CURRENT RACE?";
            break;

        case PAUSE_ATTRACT:

            heading = "RETURN TO ATTRACT";
            message = "END THE CURRENT GAME?";
            break;

        case PAUSE_EXIT:

            heading = "EXIT TO DASHBOARD";
            message = "EXIT OUTRUNNERS?";
            break;

        default:
            break;
    }

    const int panel_w = 800;
    const int panel_h = 350;

    const int x = (output->w - panel_w) / 2;
    const int y = (output->h - panel_h) / 2;

    const Uint32 background =
        SDL_MapRGB(output->format, 18, 24, 34);

    const Uint32 highlight =
        SDL_MapRGB(output->format, 55, 79, 114);

    const Uint32 inactive =
        SDL_MapRGB(output->format, 31, 40, 54);

    const Uint32 white =
        SDL_MapRGB(output->format, 255, 255, 255);

    const Uint32 grey =
        SDL_MapRGB(output->format, 160, 173, 190);

    SDL_Rect panel{
        x, y, panel_w, panel_h
    };

    SDL_FillRect(output, &panel, background);

    const auto center =
        [output](
            const char* text,
            int center_x,
            int top,
            int scale,
            Uint32 color)
        {
            const int text_width =
                static_cast<int>(std::strlen(text)) *
                4 * scale;

            ps5::debug::text(
                output,
                center_x - text_width / 2,
                top,
                text,
                scale,
                color);
        };

    center(
        heading,
        x + panel_w / 2,
        y + 26,
        6,
        white);

    SDL_Rect divider{
        x + 32,
        y + 80,
        panel_w - 64,
        2
    };

    SDL_FillRect(output, &divider, grey);

    center(
        message,
        x + panel_w / 2,
        y + 111,
        5,
        white);

    const int button_w = 300;
    const int button_h = 70;

    // ORUNNERS_CONFIRM_ORDER_V2

    SDL_Rect no_button{
        x + 425,
        y + 180,
        button_w,
        button_h
    };

    SDL_Rect yes_button{
        x + 75,
        y + 180,
        button_w,
        button_h
    };

    SDL_FillRect(
        output,
        &no_button,
        yes ? inactive : highlight);

    SDL_FillRect(
        output,
        &yes_button,
        yes ? highlight : inactive);

    center(
        "NO",
        no_button.x + button_w / 2,
        no_button.y + 20,
        6,
        yes ? grey : white);

    center(
        "YES",
        yes_button.x + button_w / 2,
        yes_button.y + 20,
        6,
        yes ? white : grey);

    center(
        "DPAD SELECT  CROSS CONFIRM",
        x + panel_w / 2,
        y + 278,
        3,
        grey);

    center(
        "CIRCLE OR TOUCHPAD CANCEL",
        x + panel_w / 2,
        y + 309,
        3,
        grey);
}

static int run_pause_menu(
    const uint32_t* pixels,
    int width,
    int height,
    int stride,
    const uint32_t* right_pixels,
    int right_width)
{
    trace("OUTRUNNERS PAUSED");

    stop_all_rumble();

    /*
     * Preserve existing queued PCM and resampler
     * state. Do not clear either buffer.
     */

    if (game_audio.ready())
        audio.set_paused(true);

    int selected = PAUSE_RESUME;

    // ORUNNERS_CONFIRM_DIALOG_V1

    int confirmation = -1;
    bool confirm_yes = false;

    bool resume = false;
    bool exit_requested = false;

    while (!resume && !exit_requested)
    {
        ps5::update_input();

        SDL_Event event{};

        while (ps5::next_event(event))
        {
            if (event.type == SDL_QUIT)
            {
                exit_requested = true;
                break;
            }

            if (event.type == SDL_CONTROLLERDEVICEADDED)
            {
                open_controller();
            }

            if (event.type == SDL_CONTROLLERDEVICEREMOVED)
            open_controller();

            if (event.type != SDL_CONTROLLERBUTTONDOWN)
                continue;

            if (controller_player(event.cbutton.which) < 0)
                continue;

            if (confirmation >= 0)
            {
                switch (event.cbutton.button)
                {
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:

                        confirm_yes = true;
                        break;

                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:

                        confirm_yes = false;
                        break;

                    case SDL_CONTROLLER_BUTTON_B:
                    case SDL_CONTROLLER_BUTTON_TOUCHPAD:

                        confirmation = -1;
                        confirm_yes = false;
                        break;

                    case SDL_CONTROLLER_BUTTON_A:
                    {
                        const int action = confirmation;

                        confirmation = -1;

                        if (!confirm_yes)
                            break;

                        confirm_yes = false;

                        if (!pause_item_enabled(action))
                            break;

                        switch (action)
                        {
                            case PAUSE_RESTART:

                                trace("CONFIRMED RESTART");

                                console_restart_game();

                                resume = true;
                                break;

                            case PAUSE_ATTRACT:

                                trace("CONFIRMED RETURN TO ATTRACT");

                                console_quit_game();

                                resume = true;
                                break;

                            case PAUSE_EXIT:

                                trace("CONFIRMED DASHBOARD EXIT");

                                exit_requested = true;
                                break;

                            default:
                                break;
                        }

                        break;
                    }

                    default:
                        break;
                }

                if (resume || exit_requested)
                    break;

                // A modal button event must never
                // also activate the normal menu.
                continue;
            }

            switch (event.cbutton.button)
            {
                case SDL_CONTROLLER_BUTTON_TOUCHPAD:
                case SDL_CONTROLLER_BUTTON_B:

                    resume = true;
                    break;

                case SDL_CONTROLLER_BUTTON_DPAD_UP:

                    selected =
                        pause_next_item(
                            selected, -1);

                    break;

                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:

                    selected =
                        pause_next_item(
                            selected, 1);

                    break;

                case SDL_CONTROLLER_BUTTON_RIGHTSTICK:

                    toggle_game_fps();
                    break;

                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:

                    if (selected == PAUSE_DISPLAY)
                    {
                        orunners_cycle_display_mode(
                            event.cbutton.button ==
                            SDL_CONTROLLER_BUTTON_DPAD_LEFT
                                ? -1 : 1);
                    }
                    else if (selected == PAUSE_FPS)
                    {
                        toggle_game_fps();
                    }

                    break;

                case SDL_CONTROLLER_BUTTON_A:

                    if (!pause_item_enabled(selected))
                        break;

                    switch (selected)
                    {
                        case PAUSE_RESUME:

                            resume = true;
                            break;

                        case PAUSE_RESTART:

                            confirmation = PAUSE_RESTART;
                            confirm_yes = false;
                            break;

                        case PAUSE_ATTRACT:

                            confirmation = PAUSE_ATTRACT;
                            confirm_yes = false;
                            break;

                        case PAUSE_DISPLAY:

                            orunners_cycle_display_mode(1);
                            break;

                        case PAUSE_FPS:

                            toggle_game_fps();
                            break;

                        case PAUSE_EXIT:

                            confirmation = PAUSE_EXIT;
                            confirm_yes = false;
                            break;

                        default:
                            break;
                    }

                    break;



                default:
                    break;
            }

            if (resume || exit_requested)
                break;
        }

        if (resume || exit_requested)
            break;

        SDL_Surface* output = ps5::screen();

        if (!output)
            return -1;

        /*
         * Redraw the same frozen source frame.
         * No game state or video generation advances.
         */

        const bool two_players = right_pixels != nullptr;

        bool drawn = two_players
            ? draw_split_screen(
                output,
                pixels,
                width,
                right_pixels,
                right_width,
                height,
                stride)
            : game_fastblit.draw(
                output,
                pixels,
                width,
                height,
                stride);

        if (!drawn)
        {
            if (two_players)
                return -1;

            if (!game_framebuffer.surface() ||
                !game_framebuffer.blit_to(output, false))
                return -1;
        }

        if (confirmation >= 0)
        {
            draw_pause_confirmation(
                output,
                confirmation,
                confirm_yes);
        }
        else
        {
            draw_pause_menu(output, selected);
        }

        if (!ps5::present())
            return -1;

        SDL_Delay(8);
    }

    orunners_settings_sync();
    save_native_settings(true);

    if (exit_requested)
    {
        game_exit_requested = true;

        trace("PAUSE MENU DASHBOARD EXIT");

        /*
         * Leave audio paused until the existing
         * native shutdown routine closes it.
         */

        return 0;
    }

    /*
     * Suppress buttons held during menu dismissal
     * until the player releases them.
     */

    for (int player = 0; player < 2; ++player)
    {
        pause_input_suppression[player] =
            orunners::read_pad(
                player_controller(player)).buttons;
    }

    /*
     * Only resume audio if the original audio
     * bridge was already playing.
     *
     * If it was prebuffering, allow its existing
     * queue management to resume playback naturally.
     */

    if (game_audio.ready() &&
        !game_audio.buffering())
    {
        audio.set_paused(false);
    }

    /*
     * Reset wall-clock deadlines and profiling.
     * Paused time must not count as engine time.
     */

    game_frequency = 0;
    game_next_frame = 0;

    profile_previous_end = 0;
    profile_window_start = 0;

    profile_sum = GameProfile{};
    profile_average = GameProfile{};

    profile_fps10 = 0;

    frame_timer.reset();

    trace("OUTRUNNERS RESUMED");

    return 1;
}

extern "C" int orunners_present_game_frame(
    const uint32_t* pixels,
    int width,
    int height,
    int stride)
{
    if (!profile_frequency)
        profile_frequency =
            SDL_GetPerformanceFrequency();

    if (!profile_frequency)
        return -1;

    const Uint64 callback_start = ticks();

    GameProfile sample{};

    const bool have_previous =
        profile_previous_end != 0;

    if (have_previous)
    {
        sample.engine =
            callback_start -
            profile_previous_end;
    }

    if (!pixels ||
        width <= 0 ||
        width > stride ||
        stride != 496 ||
        height != 224)
    {
        return -1;
    }


    /*
     * ORUNNERS_WIDE_PRESENTATION_V1
     *
     * Match the original SDL2 frontend:
     *
     * 320 -> 320
     * 384 -> 358
     * 416 -> 398
     * 496 -> 496
     *
     * Preserve the original 496-pixel row stride.
     * Do not stretch the intermediate framebuffer.
     */

    split_screen_active = console_both_screens();

    int right_width = 0;

    const uint32_t* right_pixels =
        split_screen_active
            ? orunners_second_frame(&right_width)
            : nullptr;

    if (split_screen_active && !right_pixels)
        return -1;

    const int cell_width = orunners_display_width();

    if (cell_width <= 0 || cell_width > stride)
        return -1;

    if (width > cell_width)
    {
        /*
         * Crop the same number of pixels from
         * both sides of the original game image.
         */

        pixels += (width - cell_width) / 2;

        width = cell_width;
    }
    else if (width < cell_width)
    {
        /*
         * Some original game screens may remain
         * narrower than the selected widescreen
         * mode. Match the upstream black padding.
         *
         * Static storage avoids large stack or
         * conventional heap allocations.
         */

        alignas(64)
        static uint32_t padded[496 * 224];

        const int offset =
            (cell_width - width) / 2;

        for (int y = 0; y < height; ++y)
        {
            uint32_t* row = padded + y * 496;

            std::memset(
                row,
                0,
                static_cast<size_t>(cell_width) * 4);

            std::memcpy(
                row + offset,
                pixels + y * stride,
                static_cast<size_t>(width) * 4);
        }

        pixels = padded;
        width = cell_width;
    }

    SDL_Surface* output = ps5::screen();

    if (!output)
        return -1;

    // Use the existing native controller update path.
    // Do not call SDL_PollEvent or SDL_PumpEvents.

    const Uint64 input_start = ticks();

    ps5::update_input();

    SDL_Event event{};

    while (ps5::next_event(event))
    {
        if (event.type == SDL_QUIT)
        {
            game_exit_requested = true;
            return 0;
        }

        if (event.type == SDL_CONTROLLERDEVICEADDED)
        {
            open_controller();
        }

        if (event.type == SDL_CONTROLLERDEVICEREMOVED)
            open_controller();

        if (event.type != SDL_CONTROLLERBUTTONDOWN)
            continue;

        if (controller_player(event.cbutton.which) < 0)
            continue;

        if (event.cbutton.button ==
            SDL_CONTROLLER_BUTTON_TOUCHPAD)
        {
            pause_requested = true;
        }

        if (event.cbutton.button ==
            SDL_CONTROLLER_BUTTON_RIGHTSTICK)
        {
            toggle_game_fps();
        }
    }

    sample.input = ticks() - input_start;

    const Uint64 fastblit_start = ticks();

    fastblit_active =
        split_screen_active
            ? draw_split_screen(
                output,
                pixels,
                width,
                right_pixels,
                right_width,
                height,
                stride)
            : game_fastblit.draw(
                output,
                pixels,
                width,
                height,
                stride);

    if (split_screen_active && !fastblit_active)
    {
        trace("SPLIT SCREEN PRESENTATION FAILED");
        return -1;
    }

    if (fastblit_active)
    {
        sample.convert = 0;
        sample.scale = ticks() - fastblit_start;

        if (!renderer_reported)
        {
            trace("OUTRUNNERS FASTBLIT ACTIVE");
            renderer_reported = true;
        }
    }
    else
    {
        if (!renderer_reported)
        {
            trace("OUTRUNNERS SDL FALLBACK ACTIVE");
            renderer_reported = true;
        }

        const Uint64 convert_start = ticks();

        SDL_Surface* source =
            game_framebuffer.surface();

        /*
         * Recreate the source framebuffer only when
         * the engine changes its actual frame width.
         */

        if (!source ||
            source->w != width ||
            source->h != height)
        {
            if (!game_framebuffer.create(
                    width,
                    height,
                    SDL_PIXELFORMAT_ARGB8888))
            {
                trace("GAME FRAMEBUFFER CREATE FAILED");
                return -1;
            }

            source = game_framebuffer.surface();
        }

        /*
         * The engine's source format is 0x00RRGGBB.
         * Use SDL's XRGB8888-compatible RGB888 source
         * format and convert to opaque ARGB8888.
         *
         * Source pitch remains 496 pixels regardless
         * of visible frame width.
         */

        bool locked = false;

        if (SDL_MUSTLOCK(source))
        {
            if (SDL_LockSurface(source) != 0)
                return -1;

            locked = true;
        }

        const int converted = SDL_ConvertPixels(
            width,
            height,
            SDL_PIXELFORMAT_RGB888,
            pixels,
            stride * (int)sizeof(uint32_t),
            source->format->format,
            source->pixels,
            source->pitch);

        if (locked)
            SDL_UnlockSurface(source);

        if (converted != 0)
        {
            trace("GAME PIXEL CONVERSION FAILED");
            trace(SDL_GetError());
            return -1;
        }

        sample.convert = ticks() - convert_start;

        const Uint64 scale_start = ticks();

        if (!game_framebuffer.blit_to(output, false))
        {
            trace("GAME FRAMEBUFFER BLIT FAILED");
            trace(SDL_GetError());
            return -1;
        }

        /*
         * Board-rate pacing. Match the upstream
         * desktop frontend's 60 Hz deadline approach.
         */

        sample.scale = ticks() - scale_start;
    }

    const Uint64 wait_start = ticks();

    if (!game_frequency)
    {
        game_frequency =
            SDL_GetPerformanceFrequency();

        if (!game_frequency)
            return -1;

        game_next_frame =
            SDL_GetPerformanceCounter();

        frame_timer.reset();
    }

    game_next_frame += game_frequency / 60;

    Uint64 now = SDL_GetPerformanceCounter();

    if (now > game_next_frame + game_frequency / 4)
        game_next_frame = now;

    while ((now = SDL_GetPerformanceCounter())
                < game_next_frame)
    {
        const Uint64 left =
            (game_next_frame - now) *
            1000 / game_frequency;

        if (left > 2)
            SDL_Delay((Uint32)(left - 1));
    }

    sample.wait = ticks() - wait_start;

    draw_game_profile(output);

    const Uint64 present_start = ticks();

    if (!ps5::present())
    {
        trace("GAME PRESENT FAILED");
        trace(SDL_GetError());
        return -1;
    }

    const Uint64 present_end = ticks();

    sample.present =
        present_end - present_start;

    if (have_previous)
    {
        sample.total =
            present_end - profile_previous_end;

        profile_record(sample, present_end);
    }

    profile_previous_end = present_end;

    frame_timer.frame_presented();

    if (pause_requested)
    {
        pause_requested = false;

        return run_pause_menu(
            pixels,
            width,
            height,
            stride,
            right_pixels,
            right_width);
    }

    return 1;
}

// ============================================
// Main application
// ============================================


/* ORUNNERS_ROM_SCREEN_V1 */
/* ORUNNERS_ROM_ERROR_DISPLAY_V1 */

extern "C" {
    extern char orunners_rom_error_detail[512];
}

extern "C" void orunners_rom_screen(
    int stage,
    unsigned long /* detail */)
{
    SDL_Surface* output = ps5::screen();

    if (!output)
        return;

    const Uint32 background =
        SDL_MapRGB(output->format, 12, 21, 38);

    const Uint32 white =
        SDL_MapRGB(output->format, 255, 255, 255);

    const Uint32 gold =
        SDL_MapRGB(output->format, 255, 192, 55);

    const Uint32 grey =
        SDL_MapRGB(output->format, 156, 177, 200);

    SDL_FillRect(output, nullptr, background);

    const auto center =
        [output](const char* text,
                 int top,
                 int scale,
                 Uint32 colour)
        {
            const int width =
                static_cast<int>(
                    std::strlen(text)) *
                4 * scale;

            ps5::debug::text(
                output,
                (output->w - width) / 2,
                top,
                text,
                scale,
                colour);
        };

    center(
        "OUTRUNNERS PS5",
        output->h / 7,
        12,
        gold);

    const char* title = "";
    const char* line1 = "";
    const char* line2 = "";
    const char* line3 = "";

    switch (stage)
    {
        case 1:
            title = "PREPARING GAME DATA";
            line1 = "STEP 1 OF 4 - CHECKING ROMS";
            line2 = "FIRST TIME INSTALLATION";
            break;

        case 2:
            title = "PREPARING GAME DATA";
            line1 = "STEP 2 OF 4 - PROCESSING GAME DATA";
            break;

        case 3:
            title = "PREPARING GAME DATA";
            line1 = "STEP 3 OF 4 - SAVING GAME DATA";
            break;

        case 4:
            title = "PREPARING GAME DATA";
            line1 = "STEP 4 OF 4 - VERIFYING GAME DATA";
            break;

        case 5:
            title = "INSTALLATION COMPLETE";
            line1 = "GAME DATA VERIFIED";
            line2 = "STARTING OUTRUNNERS";
            break;

        case 90:
            title = "ROM SET REQUIRED";
            line1 = "COPY YOUR ORUNNERS.ZIP";
            line2 = "INTO THE OUTRUNNERS APP FOLDER";
            line3 = "USE THE MAME WORLD ROM SET";
            break;

        case 91:
            title = "INVALID ZIP FILE";
            line1 = "CHECK YOUR ORUNNERS.ZIP";
            line2 = "USE THE MAME WORLD ROM SET";
            break;

        case 92:
            title = "STORAGE ERROR";
            line1 = "CANNOT CREATE INSTALLATION FOLDER";
            break;

        case 93:
            title = "ROM INSTALLATION FAILED";
            line1 = "CHECK THE ERROR BELOW";
            break;

        case 94:
            title = "ASSET PROCESSING FAILED";
            line1 = "GAME DATA COULD NOT BE LOADED";
            break;

        case 95:
            title = "CACHE WRITE FAILED";
            line1 = "CHECK AVAILABLE STORAGE";
            break;

        case 96:
            title = "CACHE VERIFICATION FAILED";
            line1 = "GENERATED DATA DOES NOT MATCH";
            line2 = "EXPECTED ASSET PACK";
            break;

        default:
            title = "INSTALLATION ERROR";
            break;
    }

    const int middle = output->h / 2;

    center(title, middle - 155, 8, white);
    center(line1, middle - 55, 5, grey);
    center(line2, middle + 5, 5, grey);
    center(line3, middle + 65, 5, grey);

    if (stage == 93)
    {
        /*
         * Convert to uppercase for the existing
         * native bitmap font and wrap long errors.
         */

        char message[512] = {};

        SDL_snprintf(
            message, sizeof(message),
            "%s", orunners_rom_error_detail);

        for (char* c = message; *c; ++c)
        {
            if (*c >= 'a' && *c <= 'z')
                *c = *c - 'a' + 'A';

            if (*c == '\n' || *c == '\r')
                *c = ' ';
        }

        const int scale = 4;
        int columns = (output->w - 80) / (4 * scale);

        if (columns > 56)
            columns = 56;

        if (columns < 12)
            columns = 12;

        const char* cursor = message;

        for (int row = 0;
             row < 5 && *cursor;
             ++row)
        {
            char line[64] = {};
            int count = 0;

            while (cursor[count] &&
                   count < columns)
                ++count;

            for (int i = 0; i < count; ++i)
                line[i] = cursor[i];

            center(
                line,
                middle + row * 32,
                scale,
                white);

            cursor += count;
        }
    }

    if (stage >= 1 && stage <= 5)
    {
        SDL_Rect track{
            (output->w - 720) / 2,
            middle + 140,
            720, 22
        };

        SDL_FillRect(output, &track, grey);

        SDL_Rect progress = track;
        progress.w = track.w * std::min(stage - 1, 4) / 4;

        SDL_FillRect(output, &progress, gold);

        center(
            stage == 5 ? "READY TO PLAY" : "DO NOT CLOSE THE APPLICATION",
            middle + 200,
            4,
            grey);
    }
    else
    {
        center(
            "CROSS OR CIRCLE TO EXIT",
            middle + 230,
            5,
            gold);
    }

    ps5::present();

    if (stage >= 1 && stage <= 5)
        return;

    /*
     * Keep the instructions visible until the
     * user dismisses them.
     */

    while (true)
    {
        ps5::update_input();

        SDL_Event event;

        while (ps5::next_event(event))
        {
            if (event.type == SDL_QUIT)
                return;

            if (event.type ==
                    SDL_CONTROLLERBUTTONDOWN)
            {
                const auto button =
                    event.cbutton.button;

                if (button == SDL_CONTROLLER_BUTTON_A ||
                    button == SDL_CONTROLLER_BUTTON_B)
                    return;
            }
        }

        SDL_Delay(20);
    }
}


int main(int, char**)
{
    trace("MAIN ENTERED");

    if (!ps5::initialize())
    {
        trace("VIDEO INIT FAILED");
        trace(SDL_GetError());
        finish();
    }

    SDL_Surface* output = ps5::screen();

    SDL_FillRect(
        output,
        nullptr,
        SDL_MapRGB(output->format, 0, 0, 0));

    if (!ps5::present())
    {
        trace("FIRST PRESENT FAILED");
        finish();
    }

    if (!ps5::initialize_input())
    {
        trace("CONTROLLER INIT FAILED");
        trace(SDL_GetError());
        finish();
    }

    open_controller();



    if (!frame_timer.initialize())
    {
        trace("FRAME TIMER INIT FAILED");
        finish();
    }

    game_frequency = 0;
    game_next_frame = 0;
    game_exit_requested = false;

    // Keep the performance overlay off at launch.
    // R3 enables it during attract mode.

    show_fps = false;

    trace("OUTRUNNERS DIRECT START");

    or_task_result = orunners_asset_probe();

    if (or_task_result == 2)
    {
        trace("ROM INSTALLATION COMPLETE");
        finish();
    }

    if (or_task_result <= 0)
    {
        trace("ASSET LOADING FAILED");
        finish();
    }

    trace("ASSETS LOADED");

    or_task_result = orunners_engine_probe();

    if (or_task_result <= 0)
        trace("ENGINE EXECUTION FAILED");
    else
        trace("GAME EXIT REQUESTED");

    finish();
}
