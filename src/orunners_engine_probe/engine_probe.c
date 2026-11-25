/*
 * OutRunners PS5 first engine execution diagnostic.
 *
 * Initialization and frame callback ordering follow:
 * upstream ports/headless/main.c.
 *
 * Single screen, no extra worker threads,
 * no input script and no network session.
 */

#include "s32/machine.h"
#include "s32/program.h"
#include "s32/video.h"
#include "s32/script.h"
#include "s32/workers.h"
#include "sound/chips.h"
#include "game/assets.h"
#include "console.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>



extern void orunners_visual_checkpoint(int stage);

extern void orunners_tasks_release_all(void);

/* ORUNNERS_LIVE_VIDEO_V1 */

extern int orunners_present_game_frame(
    const uint32_t *pixels,
    int width,
    int height,
    int stride);

static int presentation_status;

/*
 * ORUNNERS_MOTOR_BRIDGE_V1
 *
 * Original System 32 wheel-motor output:
 *
 * s32_board.outputs[player][0] & 0x10
 *
 * Same signal used by the upstream SDL2
 * forces.c implementation.
 */

extern void orunners_rumble_frame(
    int player,
    int motor,
    int strength);

static void update_controller_rumble(void)
{
    const bool two_players =
        console_both_screens();

    const int strength =
        console_window_settings.vibration;

    for (int player = 0; player < 2; ++player)
    {
        const bool active =
            player == 0 || two_players;

        const bool motor =
            active &&
            (s32_board.outputs[player][0] & 0x10) != 0;

        orunners_rumble_frame(
            player,
            motor ? 1 : 0,
            strength);
    }
}


/*
 * ORUNNERS_ARCADE_RECORDS_V1
 *
 * Original desktop persistence formats from
 * upstream ports/sdl2/main.c.
 *
 * EEPROM: 64 little-endian words.
 * Bookkeeping: 250 bytes at shared RAM 0x1f00,
 * encoded as little-endian halfwords.
 */

#define OR_RECORD_START 0x1f00
#define OR_RECORD_SIZE  0xfa

_Static_assert(
    S32_EEPROM_WORDS == 64,
    "Unexpected EEPROM format");

_Static_assert(
    sizeof(s32_board.shared_ram) >=
        OR_RECORD_START + OR_RECORD_SIZE,
    "Unexpected shared RAM size");

extern int orunners_record_read(
    const char *filename,
    void *data,
    size_t bytes);

extern int orunners_record_write(
    const char *filename,
    const void *data,
    size_t bytes);

static uint8_t record_ram_saved[OR_RECORD_SIZE];

static uint8_t record_eeprom_saved[
    S32_EEPROM_WORDS * 2];

static bool record_eeprom_exists;
static bool record_ram_exists;

static void encode_record_eeprom(uint8_t *bytes)
{
    for (int word = 0;
         word < S32_EEPROM_WORDS;
         ++word)
    {
        const uint16_t value =
            s32_board.eeprom.data[word];

        bytes[word * 2] =
            (uint8_t)value;

        bytes[word * 2 + 1] =
            (uint8_t)(value >> 8);
    }
}

static void encode_record_ram(uint8_t *bytes)
{
    const uint8_t *kept =
        &s32_board.shared_ram[OR_RECORD_START];

    for (int at = 0;
         at < OR_RECORD_SIZE;
         at += 2)
    {
        uint16_t value;

        memcpy(
            &value,
            kept + at,
            sizeof(value));

        bytes[at] = (uint8_t)value;

        bytes[at + 1] =
            (uint8_t)(value >> 8);
    }
}

static void orunners_records_load(void)
{
    uint8_t eeprom[S32_EEPROM_WORDS * 2];

    record_eeprom_exists =
        orunners_record_read(
            "outrunners.eeprom",
            eeprom,
            sizeof(eeprom)) != 0;

    if (record_eeprom_exists)
    {
        for (int word = 0;
             word < S32_EEPROM_WORDS;
             ++word)
        {
            s32_board.eeprom.data[word] =
                (uint16_t)(
                    eeprom[word * 2] |
                    (eeprom[word * 2 + 1] << 8));
        }

        s32_board.eeprom.dirty = false;
    }

    uint8_t bytes[OR_RECORD_SIZE];

    record_ram_exists =
        orunners_record_read(
            "outrunners.bookkeeping",
            bytes,
            sizeof(bytes)) != 0;

    if (record_ram_exists)
    {
        uint8_t *kept =
            &s32_board.shared_ram[
                OR_RECORD_START];

        for (int at = 0;
             at < OR_RECORD_SIZE;
             at += 2)
        {
            const uint16_t value =
                (uint16_t)(
                    bytes[at] |
                    (bytes[at + 1] << 8));

            memcpy(
                kept + at,
                &value,
                sizeof(value));
        }
    }

    /*
     * Keep a snapshot in the actual on-disk
     * encoding for later change detection.
     */

    encode_record_eeprom(record_eeprom_saved);
    encode_record_ram(record_ram_saved);
}

static void orunners_records_save(void)
{
    uint8_t eeprom[S32_EEPROM_WORDS * 2];

    encode_record_eeprom(eeprom);

    const bool eeprom_changed =
        memcmp(
            eeprom,
            record_eeprom_saved,
            sizeof(eeprom)) != 0;

    if (s32_board.eeprom.dirty ||
        eeprom_changed)
    {
        if (orunners_record_write(
                "outrunners.eeprom",
                eeprom,
                sizeof(eeprom)))
        {
            memcpy(
                record_eeprom_saved,
                eeprom,
                sizeof(eeprom));

            record_eeprom_exists = true;
            s32_board.eeprom.dirty = false;
        }
    }

    uint8_t bytes[OR_RECORD_SIZE];

    encode_record_ram(bytes);

    const bool bookkeeping_changed =
        memcmp(
            bytes,
            record_ram_saved,
            sizeof(bytes)) != 0;

    if (bookkeeping_changed)
    {
        if (orunners_record_write(
                "outrunners.bookkeeping",
                bytes,
                sizeof(bytes)))
        {
            memcpy(
                record_ram_saved,
                bytes,
                sizeof(bytes));

            record_ram_exists = true;
        }
    }
}



/* ORUNNERS_SETTINGS_BRIDGE_V1 */

extern void orunners_settings_start(void);

extern void orunners_settings_sync(void);

void orunners_settings_get(int32_t *values)
{
    const console_window *w =
        &console_window_settings;

    values[0] = w->wide;
    values[1] = w->fps ? 1 : 0;
    values[2] = w->dead_zone;
    values[3] = w->sensitivity;
    values[4] = w->steering_speed;
    values[5] = w->volume;
    values[6] = w->vibration;
    values[7] = w->both_radios ? 1 : 0;
}

void orunners_settings_set(
    const int32_t *values)
{
    console_window *w =
        &console_window_settings;

    w->wide = values[0];
    w->fps = values[1] != 0;
    w->dead_zone = values[2];
    w->sensitivity = values[3];
    w->steering_speed = values[4];
    w->volume = values[5];
    w->vibration = values[6];
    w->both_radios = values[7] != 0;

    w->changed = true;
}

void orunners_settings_fps(int enabled)
{
    console_window_settings.fps =
        enabled != 0;

    console_window_settings.changed = true;
}



/* ORUNNERS_TWO_SCREENS_V1 */

const uint32_t *orunners_second_frame(int *width)
{
    if (!console_both_screens())
    {
        *width = 0;
        return NULL;
    }

    return s32_video_frame(1, width);
}


/*
 * ORUNNERS_WIDESCREEN_V1
 *
 * Reuse the original console settings and
 * original extended horizontal rendering.
 */

const char *orunners_display_mode_label(void)
{
    switch (console_window_settings.wide)
    {
        case WIDE_OFF:
            return "DISPLAY MODE: ORIGINAL";

        case WIDE_16_10:
            return "DISPLAY MODE: 16:10";

        case WIDE_16_9:
            return "DISPLAY MODE: 16:9";

        case WIDE_20_9:
            return "DISPLAY MODE: 20:9";

        case WIDE_AUTO:
            return "DISPLAY MODE: AUTO";

        default:
            return "DISPLAY MODE: ORIGINAL";
    }
}

void orunners_cycle_display_mode(int direction)
{
    static const int modes[] = {
        WIDE_OFF,
        WIDE_16_10,
        WIDE_16_9,
        WIDE_20_9,
        WIDE_AUTO
    };

    const int count =
        (int)(sizeof(modes) / sizeof(modes[0]));

    int current = 0;

    for (int i = 0; i < count; ++i)
    {
        if (modes[i] == console_window_settings.wide)
        {
            current = i;
            break;
        }
    }

    current =
        (current +
         (direction < 0 ? -1 : 1) +
         count) % count;

    console_window_settings.wide = modes[current];
    console_window_settings.changed = true;
}

/*
 * Match the original SDL2 frontend's visible
 * picture widths, rather than the larger
 * intermediate engine framebuffer widths.
 */

int orunners_display_width(void)
{
    if (console_both_screens())
        return 320;

    switch (console_wide_in_effect())
    {
        case WIDE_16_10:
            return 358;

        case WIDE_16_9:
            return 398;

        case WIDE_20_9:
            return 496;

        default:
            return 320;
    }
}



/* ORUNNERS_AUDIO_MIX_V1 */

extern void orunners_audio_start(uint32_t rate);

extern void orunners_audio_submit(
    const int16_t *samples,
    size_t frames);



static int result;


static void on_irq_changed(bool asserted)
{
    program_irq_line(asserted);
}

static void on_log(const char *message)
{
    printf("[s32] %s\n", message);
}

/*
 * Match the upstream headless console's
 * idle-input and radio-button processing.
 */


/* ORUNNERS_GAME_INPUT_V1 */

extern void orunners_read_pad(
    int player,
    int32_t *stick_x,
    int32_t *stick_y,
    int32_t *accelerator,
    int32_t *brake,
    uint32_t *buttons);

/*
 * The input bridge's bit layout is kept separate
 * from the original board's active-low ports.
 */

enum
{
    PAD_UP       = 1u << 0,
    PAD_DOWN     = 1u << 1,
    PAD_LEFT     = 1u << 2,
    PAD_RIGHT    = 1u << 3,
    PAD_CROSS    = 1u << 4,
    PAD_CIRCLE   = 1u << 5,
    PAD_START    = 1u << 6,
    PAD_SHIFT_UP = 1u << 7,
    PAD_SHIFT_DN = 1u << 8,
    PAD_MUSIC    = 1u << 9
};

/*
 * Original upstream wheel and pedal ranges.
 */

enum
{
    WHEEL_LEFT   = 0x10,
    WHEEL_CENTRE = 0x80,
    WHEEL_RIGHT  = 0xf0,

    WHEEL_TURN   = 6,
    WHEEL_RETURN = 8,

    PEDAL_PRESS  = 0x20,
    PEDAL_LIFT   = 0x30
};

static uint32_t previous_actions;

/* ORUNNERS_DUAL_SEAT_INPUT_V1 */

static int digital_wheel[2] = {
    WHEEL_CENTRE, WHEEL_CENTRE
};

static int digital_gas[2];
static int digital_brake[2];

static int clamp_input(
    int value,
    int low,
    int high)
{
    return value < low
        ? low
        : value > high
            ? high
            : value;
}

/*
 * Adapted from upstream ports/sdl2/main.c:
 * digital steering acceleration and return.
 */

static int steer_digital(
    int position,
    bool left,
    bool right)
{
    int speed = clamp_input(
        console_window_settings.steering_speed,
        1,
        STEERING_LEVELS);

    int turn =
        WHEEL_TURN + speed - STEERING_NORMAL;

    int back =
        WHEEL_RETURN * turn / WHEEL_TURN;

    if (left != right)
        return clamp_input(
            position + (right ? turn : -turn),
            WHEEL_LEFT,
            WHEEL_RIGHT);

    if (position < WHEEL_CENTRE)
        return clamp_input(
            position + back,
            WHEEL_LEFT,
            WHEEL_CENTRE);

    return clamp_input(
        position - back,
        WHEEL_CENTRE,
        WHEEL_RIGHT);
}

/*
 * Original upstream steering sensitivity curves,
 * adapted for the DualSense's full stick travel.
 */

static int steer_analog(
    int stick,
    int fallback)
{
    static const double curves[STEERING_LEVELS] = {
        2.2, 1.9, 1.6, 1.3, 1.0,
        0.85, 0.72, 0.6, 0.5
    };

    int travel = stick < 0 ? -stick : stick;

    int dead_percent = clamp_input(
        console_window_settings.dead_zone,
        0,
        DEAD_ZONE_MOST);

    int sensitivity = clamp_input(
        console_window_settings.sensitivity,
        1,
        STEERING_LEVELS);

    int dead = dead_percent * 32767 / 100;

    if (travel <= dead)
        return fallback;

    double fraction =
        (double)(travel - dead) /
        (32767 - dead);

    if (fraction > 1.0)
        fraction = 1.0;

    double curved = pow(
        fraction,
        curves[sensitivity - 1]);

    int amount = (int)lround(
        curved *
        (WHEEL_RIGHT - WHEEL_CENTRE));

    return clamp_input(
        WHEEL_CENTRE +
        (stick < 0 ? -amount : amount),
        WHEEL_LEFT,
        WHEEL_RIGHT);
}

static int pedal_digital(
    int position,
    bool held)
{
    return clamp_input(
        position +
        (held ? PEDAL_PRESS : -PEDAL_LIFT),
        0,
        255);
}

static int pedal_analog(int travel)
{
    travel = clamp_input(travel, 0, 32767);

    return travel * 255 / 32767;
}

static void on_controls_update(void)
{
    int32_t stick_x[2] = {0, 0};
    int32_t stick_y[2] = {0, 0};
    int32_t gas_axis[2] = {0, 0};
    int32_t brake_axis[2] = {0, 0};

    uint32_t held[2] = {0, 0};

    for (int player = 0; player < 2; ++player)
    {
        orunners_read_pad(
            player,
            &stick_x[player],
            &stick_y[player],
            &gas_axis[player],
            &brake_axis[player],
            &held[player]);
    }

    s32_script_apply(NULL, s32_board.frame);

    /*
     * Original console menus accept actions
     * from either player's controller.
     */

    uint32_t actions = 0;

    for (int player = 0; player < 2; ++player)
    {
        uint32_t buttons = held[player];

        if ((buttons & PAD_UP) ||
            stick_y[player] < -16000)
            actions |= ACTION_BIT(ACTION_UP);

        if ((buttons & PAD_DOWN) ||
            stick_y[player] > 16000)
            actions |= ACTION_BIT(ACTION_DOWN);

        if ((buttons & PAD_LEFT) ||
            stick_x[player] < -16000)
            actions |= ACTION_BIT(ACTION_LEFT);

        if ((buttons & PAD_RIGHT) ||
            stick_x[player] > 16000)
            actions |= ACTION_BIT(ACTION_RIGHT);

        if (buttons & PAD_CROSS)
            actions |= ACTION_BIT(ACTION_ACCEPT);

        if (buttons & PAD_CIRCLE)
            actions |= ACTION_BIT(ACTION_BACK);

        if (buttons & PAD_START)
            actions |= ACTION_BIT(ACTION_START);
    }

    uint32_t pressed =
        actions & ~previous_actions;

    previous_actions = actions;

    if (console_can_pause())
        pressed &= ~ACTION_BIT(ACTION_START);

    console_actions(pressed);

    if (console_quit_requested())
    {
        program_stop();
        return;
    }

    const bool two_players =
        console_both_screens();

    s32_video_show_screens(
        two_players ? 3 : 1);

    s32_video_set_wide_side(
        console_wide_side());

    s32_inputs *input = &s32_board.input;

    /*
     * Exact upstream System 32 analog channel
     * assignments:
     *
     * Left seat:  wheel 0, gas 1, brake 2
     * Right seat: wheel 3, gas 6, brake 7
     */

    static const int wheel_channel[2] = {0, 3};
    static const int gas_channel[2]   = {1, 6};
    static const int brake_channel[2] = {2, 7};

    for (int player = 0; player < 2; ++player)
    {
        const uint32_t buttons = held[player];

        const bool active =
            player == 0 || two_players;

        int wheel = WHEEL_CENTRE;
        int gas = 0;
        int brake = 0;

        bool shift_up = false;
        bool shift_down = false;
        bool music = false;
        bool previous = false;
        bool next = false;

        bool start =
            (buttons & PAD_START) != 0;

        if (active)
        {
            digital_wheel[player] =
                steer_digital(
                    digital_wheel[player],
                    (buttons & PAD_LEFT) != 0,
                    (buttons & PAD_RIGHT) != 0);

            wheel = steer_analog(
                stick_x[player],
                digital_wheel[player]);

            digital_gas[player] =
                pedal_digital(
                    digital_gas[player],
                    (buttons & PAD_CROSS) != 0);

            digital_brake[player] =
                pedal_digital(
                    digital_brake[player],
                    (buttons & PAD_CIRCLE) != 0);

            gas = pedal_analog(gas_axis[player]);

            brake = pedal_analog(
                brake_axis[player]);

            if (digital_gas[player] > gas)
                gas = digital_gas[player];

            if (digital_brake[player] > brake)
                brake = digital_brake[player];

            shift_up =
                (buttons & PAD_SHIFT_UP) != 0;

            shift_down =
                (buttons & PAD_SHIFT_DN) != 0;

            music =
                (buttons & PAD_MUSIC) != 0;

            previous =
                (buttons & PAD_DOWN) != 0;

            next =
                (buttons & PAD_UP) != 0;
        }
        else
        {
            digital_wheel[player] = WHEEL_CENTRE;
            digital_gas[player] = 0;
            digital_brake[player] = 0;

            /*
             * Preserve upstream Classic mode's
             * right-seat challenger behaviour.
             */

            start =
                start &&
                player == 1 &&
                console_right_seat_joins();
        }

        if (console_can_pause())
            start = false;

        console_radio_buttons(
            player, &previous, &next);

        input->port_a[player] = (uint8_t)(
            0xff &
            ~(shift_up   ? 0x01 : 0) &
            ~(shift_down ? 0x02 : 0));

        input->port_b[player] = (uint8_t)(
            0xff &
            ~(music    ? 0x01 : 0) &
            ~(previous ? 0x02 : 0) &
            ~(next     ? 0x04 : 0));

        input->port_e[player] = (uint8_t)(
            0xff & ~(start ? 0x10 : 0));

        input->analog[wheel_channel[player]] =
            (uint8_t)wheel;

        input->analog[gas_channel[player]] =
            (uint8_t)gas;

        input->analog[brake_channel[player]] =
            (uint8_t)brake;
    }
}

static void on_frame_done(void)
{
    orunners_settings_sync();

    /*
     * Retain the verified sound advancement order.
     * Submit the original mixed stereo PCM.
     */

    if (s32_board.time_passes)
        s32_board.time_passes(s32_board.cycles);

    sound_chips_end_frame(s32_board.cycles);

    size_t audio_frames = 0;

    const int16_t *audio_samples =
        sound_chips_frame_mix(&audio_frames);

    orunners_audio_submit(
        audio_samples,
        audio_frames);


    update_controller_rumble();

    console_network_frame();

    int width = 0;

    const uint32_t *pixels =
        s32_video_frame(0, &width);

    presentation_status =
        orunners_present_game_frame(
            pixels,
            width,
            S32_VIDEO_HEIGHT,
            S32_VIDEO_MAX_WIDTH);

    if (presentation_status <= 0)
    {
        program_stop();
    }
}

int orunners_engine_probe(void)
{
    if (result)
        return result;

    orunners_visual_checkpoint(20);

    s32_workers_start(0);

    s32_board.program_at = data_byte_at;
    s32_board.irq_changed = on_irq_changed;
    s32_board.controls_update = on_controls_update;
    s32_board.log = on_log;
    s32_board.frame_timing = true;

    orunners_visual_checkpoint(21);

    s32_init(NULL);

    orunners_visual_checkpoint(22);

    s32_video_init_graphics(assets_graphics());
    s32_video_show_screens(1);

    orunners_visual_checkpoint(23);

    s32_board.sound_at = assets_sound_data;
    s32_board.sound_byte_at = assets_sound_byte;
    s32_board.pcm_at = assets_pcm_byte;

    sound_chips_attach();

    orunners_visual_checkpoint(24);

    sound_cpu_attach();

    orunners_visual_checkpoint(25);

    sound_chips_join_sides(true);

    orunners_audio_start(
        sound_chips_mix_rate());

    console_ghost_folder("/download0/");

    console_attach();

    console_set_screen_wide(WIDE_16_9);

    orunners_settings_start();

    orunners_visual_checkpoint(26);

    orunners_records_load();

    program_reset();
    program_frame_done = on_frame_done;

    orunners_visual_checkpoint(27);

    program_run();

    /*
     * Match the original desktop frontend:
     * persist board data after the game stops.
     *
     * Avoid replacing previously valid saves
     * when native presentation has failed.
     */

    if (presentation_status >= 0)
        orunners_records_save();

    /*
     * program_stop() returns here through the
     * hardware-verified cooperative scheduler.
     */

    program_frame_done = NULL;

    orunners_visual_checkpoint(36);

    printf(
        "Live engine stopped after %llu frames\\n",
        (unsigned long long)program_frames());

    /*
     * Game tasks will never be resumed after exit.
     */

    orunners_tasks_release_all();

    if (presentation_status < 0)
    {
        orunners_visual_checkpoint(130);
        return result = -1;
    }

    return result = 1;
}
