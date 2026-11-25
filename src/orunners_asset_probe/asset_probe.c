/*
 * OutRunners PS5 native asset loading probe.
 *
 * Uses upstream assets_load_pack_bytes().
 * The pack remains in caller-owned direct memory
 * for the lifetime of the application.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "pack_expected.h"
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

/* ORUNNERS_NATIVE_ROM_INSTALL_V1 */

extern const char *game_assets_extract(
    const char *source,
    const char *folder);

extern const char *assets_load(
    const char *folder);

extern const char *assets_pack_write(
    const char *path);

extern void orunners_rom_screen(
    int stage,
    unsigned long detail);


extern void orunners_visual_checkpoint(int stage);

extern const char *assets_load_pack_bytes(
    const uint8_t *bytes,
    size_t size);

extern size_t assets_piece_count(void);
extern size_t assets_data_bytes(void);

extern int sceKernelAllocateMainDirectMemory(
    size_t length,
    size_t alignment,
    int memory_type,
    off_t *physical);

extern int sceKernelMapDirectMemory(
    void **address,
    size_t length,
    int protection,
    int flags,
    off_t physical,
    size_t alignment);

extern int sceKernelReleaseDirectMemory(
    off_t physical,
    size_t length);

#define PACK_PATH "/app0/assets/outrunners.assets"
#define ROM_ZIP_PATH "/app0/orunners.zip"
#define CACHE_PATH "/download0/outrunners.assets"
#define CACHE_TEMP "/download0/outrunners.assets.tmp"
#define ROM_ZIP_LIMIT (128u * 1024u * 1024u)

static const char *selected_pack = PACK_PATH;


#define ALIGNMENT 0x20000u
#define READ_CHUNK (256u * 1024u)

static void *pack_memory;
static off_t pack_physical;
static size_t pack_allocation;

static int result;

static int fail(int code)
{
    orunners_visual_checkpoint(code);

    result = -1;

    return result;
}

static void release_unloaded_pack(void)
{
    if (pack_memory)
    {
        munmap(pack_memory, pack_allocation);

        sceKernelReleaseDirectMemory(
            pack_physical,
            pack_allocation);

        pack_memory = NULL;
    }
}

static uint32_t checksum(
    const uint8_t *bytes,
    size_t size)
{
    uint32_t value = 2166136261u;

    for (size_t i = 0; i < size; ++i)
        value = (value ^ bytes[i]) * 16777619u;

    return value;
}


/*
 * The original extractor accepts MAME's World ROM
 * set and verifies its individual ROM CRC32 values.
 *
 * All generated data remains on the user's console.
 */

static int file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");

    if (!file)
        return 0;

    fclose(file);
    return 1;
}

static long file_size(const char *path)
{
    FILE *file = fopen(path, "rb");
    long size = -1;

    if (!file)
        return -1;

    if (fseek(file, 0, SEEK_END) == 0)
        size = ftell(file);

    fclose(file);
    return size;
}

/*
 * Validate the complete generated asset pack
 * without allocating another 31 MB buffer.
 */
/* ORUNNERS_NATIVE_CACHE_V1: exact size/hash pairs, not independent allowlists. */
static int accepted_pack(size_t size, uint32_t hash)
{
    return (size == OR_PACK_SIZE && hash == OR_PACK_FNV32) ||
           (size == 31016320u && hash == UINT32_C(0xc243daf4));
}

static int verify_pack_file(
    const char *path,
    uint32_t *actual_hash)
{
    static uint8_t buffer[65536];

    FILE *file = fopen(path, "rb");

    uint32_t hash = 2166136261u;
    size_t total = 0;
    size_t got;
    int valid = 1;

    if (actual_hash)
        *actual_hash = 0;

    if (!file)
        return 0;

    while ((got = fread(
                buffer, 1,
                sizeof(buffer), file)) != 0)
    {
        if (total == 0 &&
            (got < 4 ||
             memcmp(buffer, "ORAP", 4) != 0))
        {
            valid = 0;
            break;
        }

        for (size_t i = 0; i < got; ++i)
            hash =
                (hash ^ buffer[i]) *
                16777619u;

        total += got;

        if (total > OR_PACK_SIZE)
        {
            valid = 0;
            break;
        }
    }

    if (ferror(file))
        valid = 0;

    fclose(file);

    if (actual_hash)
        *actual_hash = hash;

    printf(
        "ROM pack verification: "
        "%zu bytes, FNV32 %08X\n",
        total, (unsigned)hash);

    return valid &&
        accepted_pack(total, hash);
}

/*
 * Only called on a staging directory created
 * exclusively by this installer.
 *
 * Never follows symbolic links.
 */
/* ORUNNERS_STAGING_CLEANUP_V1: only installer-owned, known paths. */
#define OR_STAGE_PATH "/download0/orunners-install"
static const char *const or_stage_files[] = {
    "data/layout.json",
    "data/names.txt",
    "data/pieces.txt",
    "sound/data/layout.json",
    "sound/data/names.txt",
    "sound/data/pieces.txt",
    "sound/samples/headers.txt",
    "sound/samples/pcm_001811.wav",
    "sound/samples/pcm_280000.wav",
    "sound/samples/samples.txt",
    "sprites/bank_0.json",
    "sprites/bank_0.png",
    "sprites/bank_1.json",
    "sprites/bank_1.png",
    "sprites/bank_2.json",
    "sprites/bank_2.png",
    "sprites/bank_3.json",
    "sprites/bank_3.png",
    "sprites/sheets.txt",
    "tilesets/tiles_0000.png",
    "tilesets/tiles_0800.png",
    "tilesets/tiles_1000.png",
    "tilesets/tiles_1800.png",
    "tilesets/tiles_2000.png",
    "tilesets/tiles_2800.png",
    "tilesets/tiles_3000.png",
    "tilesets/tiles_3800.png",
    "tilesets/tiles_4000.png",
    "tilesets/tiles_4800.png",
    "tilesets/tiles_5000.png",
    "tilesets/tiles_5800.png",
    "tilesets/tiles_6000.png",
    "tilesets/tiles_6800.png",
    "tilesets/tiles_7000.png",
    "tilesets/tiles_7800.png",
    "tilesets/tilesets.txt",
    "version.txt",
};
static const char *const or_stage_dirs[] = {
    "sound/samples", "sound/data", "sound", "data", "sprites", "tilesets"
};

static int or_stage_remove_one(const char *path, int directory)
{
    int result = directory ? rmdir(path) : unlink(path);
    if (result == 0 || errno == ENOENT) return 0;
    int error = errno;
    printf("OR_STAGE cleanup failed: %s errno=%d\n", path, error);
    return error;
}

static int remove_staging(const char *path)
{
    char child[512];
    int saved = errno, first_error = 0, error;
    size_t i;
    if (!path || strcmp(path, OR_STAGE_PATH) != 0) {
        errno = EINVAL;
        return 0;
    }
    for (i = 0; i < sizeof(or_stage_files)/sizeof(or_stage_files[0]); ++i) {
        snprintf(child, sizeof(child), "%s/%s", path, or_stage_files[i]);
        error = or_stage_remove_one(child, 0);
        if (error && !first_error) first_error = error;
    }
    for (i = 0; i < sizeof(or_stage_dirs)/sizeof(or_stage_dirs[0]); ++i) {
        snprintf(child, sizeof(child), "%s/%s", path, or_stage_dirs[i]);
        error = or_stage_remove_one(child, 1);
        if (error && !first_error) first_error = error;
    }
    error = or_stage_remove_one(path, 1);
    if (error && !first_error) first_error = error;
    if (first_error) {
        printf("OR_STAGE cleanup incomplete: errno=%d\n", first_error);
        errno = first_error;
        return 0;
    }
    printf("OR_STAGE cleanup complete.\n");
    errno = saved;
    return 1;
}


/* ORUNNERS_ROM_DIAGNOSTICS_V1 */

char orunners_rom_error_detail[512] = {0};

static void record_rom_error(
    const char *message,
    long zip_size,
    int error_number)
{
    snprintf(
        orunners_rom_error_detail,
        sizeof(orunners_rom_error_detail),
        "%s",
        message ? message : "Unknown error");

    FILE *log = fopen(
        "/download0/outrunners-rom-install.log",
        "w");

    if (log)
    {
        fprintf(log,
            "OutRunners PS5 native ROM installer\n"
            "Stage: game_assets_extract\n"
            "ZIP path: %s\n"
            "ZIP size: %ld\n"
            "Extractor error: %s\n"
            "errno snapshot: %d (%s)\n",
            ROM_ZIP_PATH,
            zip_size,
            orunners_rom_error_detail,
            error_number,
            strerror(error_number));

        fclose(log);
    }
}

extern int or_install_heap_begin(void);

static int prepare_rom_cache(void)
{
    if (!remove_staging(OR_STAGE_PATH))
    {
        int saved_errno = errno;
        record_rom_error("STAGING CLEANUP FAILED", 0, saved_errno);
        orunners_rom_screen(93, 0);
        return -1;
    }

    char folder[256];
    uint32_t hash = 0;

    const long size =
        file_size(ROM_ZIP_PATH);

    if (size <= 0 ||
        (unsigned long)size > ROM_ZIP_LIMIT)
    {
        orunners_rom_screen(91, 0);
        return -1;
    }

    /*
     * Create an exclusive staging directory.
     * An interrupted previous installation
     * is never used as an input.
     */

    int created = 0;

    for (unsigned attempt = 0;
         attempt < 100; ++attempt)
    {
        snprintf(folder, sizeof(folder), "%s", OR_STAGE_PATH);

        if (mkdir(folder, 0700) == 0)
        {
            created = 1;
            break;
        }

        if (errno != EEXIST)
            break;
    }

    if (!created)
    {
        orunners_rom_screen(92, 0);
        return -1;
    }

    orunners_rom_screen(1, 0);

    const char *problem =
        game_assets_extract(
            ROM_ZIP_PATH, folder);

    if (problem)
    {
        int extraction_errno = errno;

        record_rom_error(
            problem,
            size,
            extraction_errno);

        printf(
            "ROM extraction failed: %s\n",
            problem);

        remove_staging(folder);

        orunners_rom_screen(93, 0);
        return -1;
    }

    orunners_rom_screen(2, 0);

    if (!or_install_heap_begin())
    {
        record_rom_error("PRIVATE HEAP ALLOCATION OR MAP FAILED", size, errno);
        remove_staging(folder);
        orunners_rom_screen(93, 0);
        return -1;
    }
    errno = 0;
    problem = assets_load(folder);

    if (problem)
    {
        int saved_errno = errno;
        char detail[512];
        snprintf(detail, sizeof(detail), "ASSETS_LOAD: %s",
                 *problem ? problem : "NO ERROR DETAIL");
        record_rom_error(detail, size, saved_errno);
        printf("%s\n", detail);

        remove_staging(folder);

        orunners_rom_screen(93, 0);
        return -1;
    }

    orunners_rom_screen(3, 0);

    remove(CACHE_TEMP);

    problem =
        assets_pack_write(CACHE_TEMP);

    if (problem)
    {
        int saved_errno = errno;
        char detail[512];
        snprintf(detail, sizeof(detail), "PACK_WRITE: %s",
                 *problem ? problem : "NO ERROR DETAIL");
        record_rom_error(detail, size, saved_errno);
        printf("%s\n", detail);

        remove(CACHE_TEMP);
        remove_staging(folder);

        orunners_rom_screen(93, 0);
        return -1;
    }

    orunners_rom_screen(4, 0);

    if (!verify_pack_file(
            CACHE_TEMP, &hash))
    {
        /*
         * For this first proof of concept,
         * do not accept a different pack.
         *
         * Keep the candidate on the console
         * for local diagnostic comparison.
         */

        int saved_errno = errno;
        char detail[160];
        snprintf(detail, sizeof(detail),
                 "GENERATED ASSET CACHE FAILED INTEGRITY CHECK (%08X)",
                 (unsigned)hash);
        record_rom_error(detail, size, saved_errno);
        remove(CACHE_TEMP);
        remove_staging(folder);
        orunners_rom_screen(93, 0);
        return -1;
    }

    /*
     * Publish the validated pack atomically.
     */

    if (rename(
            CACHE_TEMP,
            CACHE_PATH) != 0)
    {
        remove_staging(folder);

        orunners_rom_screen(95, 0);
        return -1;
    }

    remove_staging(folder);

    printf(
        "Native ROM preparation complete.\n");

    /* S18V-R1: the validated folder-loaded assets remain resident.
     * The installer arena is retained until process termination.
     * Do not invoke the pack loader over initialized global state. */
    printf("Verified cache; continuing with resident assets.\n");
    return 2;
}

static int select_or_prepare_pack(void)
{
    /* A valid cache remains usable even if a leftover stage cannot be removed.
       prepare_rom_cache checks again and refuses extraction in that case. */
    (void)remove_staging(OR_STAGE_PATH);


    /*
     * A validated cache takes precedence.
     */

    if (verify_pack_file(CACHE_PATH, NULL))
    {
        selected_pack = CACHE_PATH;
        printf("Using native prepared cache.\n");
        return 1;
    }

    /*
     * If a ZIP is supplied, exercise the
     * native installer even on development
     * consoles with legacy assets installed.
     */

    if (file_exists(ROM_ZIP_PATH))
    {
        if (verify_pack_file(CACHE_TEMP, NULL))
        {
            if (rename(CACHE_TEMP, CACHE_PATH) != 0)
            {
                orunners_rom_screen(95, 0);
                return -1;
            }
            if (!verify_pack_file(CACHE_PATH, NULL))
            {
                orunners_rom_screen(96, 0);
                return -1;
            }
            selected_pack = CACHE_PATH;
            printf("Using verified native candidate.\n");
            return 1;
        }
        return prepare_rom_cache();
    }

    /*
     * Preserve the existing installation
     * workflow for earlier builds.
     */

    if (verify_pack_file(PACK_PATH, NULL))
    {
        selected_pack = PACK_PATH;
        printf("Using legacy prepared assets.\n");
        return 1;
    }

    orunners_rom_screen(90, 0);

    return -1;
}

int orunners_asset_probe(void)
{
    if (result)
        return result;

    orunners_visual_checkpoint(10);

    int preparation = select_or_prepare_pack();

    if (preparation == 2)
    {
        size_t pieces = assets_piece_count();
        size_t data_bytes = assets_data_bytes();
        if (!pieces || !data_bytes || data_bytes > 31016320u)
            return fail(118);
        printf("Resident assets: %zu pieces, %zu data bytes\n",
               pieces, data_bytes);
        orunners_visual_checkpoint(9);
        orunners_rom_screen(5, 0);
        result = 1;
        return result;
    }

    if (preparation != 1)
        return preparation;

    FILE *input = fopen(selected_pack, "rb");

    if (!input)
        return fail(110);

    orunners_visual_checkpoint(11);

    if (fseek(input, 0, SEEK_END) != 0)
    {
        fclose(input);
        return fail(111);
    }

    long size = ftell(input);

    if ((size != (long)OR_PACK_SIZE && size != 31016320L) ||
        fseek(input, 0, SEEK_SET) != 0)
    {
        fclose(input);
        return fail(112);
    }

    pack_allocation =
        ((size_t)size + ALIGNMENT - 1) &
        ~(size_t)(ALIGNMENT - 1);

    orunners_visual_checkpoint(12);

    if (sceKernelAllocateMainDirectMemory(
            pack_allocation,
            ALIGNMENT,
            12,
            &pack_physical) != 0)
    {
        fclose(input);
        return fail(113);
    }

    orunners_visual_checkpoint(13);

    if (sceKernelMapDirectMemory(
            &pack_memory,
            pack_allocation,
            0x33,
            0,
            pack_physical,
            ALIGNMENT) != 0 ||
        !pack_memory)
    {
        sceKernelReleaseDirectMemory(
            pack_physical,
            pack_allocation);

        fclose(input);

        return fail(114);
    }

    orunners_visual_checkpoint(14);

    uint8_t *bytes = pack_memory;

    size_t total = 0;

    while (total < (size_t)size)
    {
        size_t remaining = (size_t)size - total;

        size_t amount =
            remaining > READ_CHUNK
                ? READ_CHUNK
                : remaining;

        if (fread(bytes + total, 1, amount, input)
                != amount)
        {
            fclose(input);
            release_unloaded_pack();

            return fail(115);
        }

        total += amount;
    }

    fclose(input);

    orunners_visual_checkpoint(15);

    if (memcmp(bytes, "ORAP", 4) != 0 ||
        !accepted_pack(total, checksum(bytes, total)))
    {
        release_unloaded_pack();

        return fail(116);
    }

    orunners_visual_checkpoint(16);

    /*
     * The upstream loader keeps pointers into this
     * memory. Do not release it after this call,
     * including on a partially completed load.
     */

    const char *problem =
        assets_load_pack_bytes(bytes, total);

    if (problem)
    {
        printf("OutRunners asset error: %s\n", problem);

        return fail(117);
    }

    orunners_visual_checkpoint(17);

    size_t pieces = assets_piece_count();
    size_t data_bytes = assets_data_bytes();

    if (!pieces ||
        !data_bytes ||
        data_bytes > total)
    {
        return fail(118);
    }

    printf(
        "OutRunners assets loaded: "
        "%zu pieces, %zu data bytes\n",
        pieces,
        data_bytes);

    /*
     * Keep the pack mapped. Process termination
     * releases its address space.
     */

    orunners_visual_checkpoint(9);

    result = 1;

    return result;
}
