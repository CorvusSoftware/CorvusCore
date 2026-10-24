// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only

#ifndef CORVUSCORE_BRIDGE_H
#define CORVUSCORE_BRIDGE_H

/* Portable C ABI for Android JNI, Swift and other frontends.
 * No C++ types or platform-specific headers cross this boundary.
 * All calls for a given handle must be serialized by the caller.
 * Copyright (c) RavenEmu contributors (original authors).
 * Copyright (c) 2026 CorvusSoftware.
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct corvuscore_bridge corvuscore_bridge;

/* Version of this C API, bumped on every breaking change (0 = pre-release).
 * Compare corvuscore_api_version() with this macro before any other call:
 * a mismatch means the header and the linked library disagree. */
#define CORVUSCORE_API_VERSION 0
int corvuscore_api_version(void);
/* Version of the CorvusCore library, for example "0.4.1". */
const char* corvuscore_version(void);

enum corvuscore_system {
    CORVUSCORE_GB = 0,
    CORVUSCORE_GBC = 1,
    CORVUSCORE_GBA = 2,
    /* The Game Boy Color inside a Game Boy Advance: it runs GBC and GB games
     * like CORVUSCORE_GBC, but starts them with B = 1 (B + 1 for a GB game), so
     * that games which detect a GBA unlock their extras (Zelda Oracle,
     * Shantae...). */
    CORVUSCORE_GBC_ON_GBA = 3
};

enum corvuscore_button {
    CORVUSCORE_UP = 0,
    CORVUSCORE_DOWN = 1,
    CORVUSCORE_LEFT = 2,
    CORVUSCORE_RIGHT = 3,
    CORVUSCORE_A = 4,
    CORVUSCORE_B = 5,
    CORVUSCORE_START = 6,
    CORVUSCORE_SELECT = 7,
    CORVUSCORE_L = 8,
    CORVUSCORE_R = 9
};

/* Returns NULL on error. The error message is thread-local. */
corvuscore_bridge* corvuscore_create(int system);
void corvuscore_destroy(corvuscore_bridge* handle);
const char* corvuscore_last_error(void);

/* ROM/battery bytes are copied by the engine. Returns 1 on success. */
int corvuscore_load(corvuscore_bridge* handle,
                     const uint8_t* rom, size_t rom_size,
                     const uint8_t* battery, size_t battery_size);
int corvuscore_reset(corvuscore_bridge* handle);
int corvuscore_set_button(corvuscore_bridge* handle, int button, int pressed);

/* Dimensions and audio format remain stable for the loaded system.
 * run_frame writes width*height pixels as ARGB8888 (0xAARRGGBB).
 * It is the caller's job to schedule frames at fps and play the audio. */
int corvuscore_video_width(const corvuscore_bridge* handle);
int corvuscore_video_height(const corvuscore_bridge* handle);
double corvuscore_video_fps(const corvuscore_bridge* handle);
int corvuscore_audio_rate(const corvuscore_bridge* handle);
int corvuscore_run_frame(corvuscore_bridge* handle,
                         uint32_t* argb, size_t pixel_capacity);
/* Returns number of interleaved int16 samples copied (not stereo frames). */
size_t corvuscore_read_audio(corvuscore_bridge* handle,
                             int16_t* output, size_t sample_capacity);

/* Read-only RetroAchievements address space. Returns the number of bytes
 * really available, never clocks the emulation bus or changes state.
 * Unmapped addresses return short reads; frontend/SDK must respect count.
 * Call serially on the same handle as run_frame/load_state. */
size_t corvuscore_read_achievement_memory(const corvuscore_bridge* handle,
                                           uint32_t address, uint8_t* output,
                                           size_t capacity);

/* Caller owns buffers. Check size, allocate and then copy.
 * Save states can change size between frames and between core versions. */
size_t corvuscore_state_size(corvuscore_bridge* handle);
int corvuscore_save_state(corvuscore_bridge* handle, uint8_t* output, size_t capacity);
int corvuscore_load_state(corvuscore_bridge* handle,
                          const uint8_t* state, size_t size);
size_t corvuscore_battery_size(corvuscore_bridge* handle);
int corvuscore_copy_battery(corvuscore_bridge* handle,
                            uint8_t* output, size_t capacity);

/* Battery persistence without needless writes: when corvuscore_battery_dirty
 * returns 1, copy the battery with its generation, store it, then acknowledge
 * that generation. Writes made by the game after the copy keep it dirty. */
int corvuscore_battery_dirty(const corvuscore_bridge* handle);
int corvuscore_copy_battery_generation(corvuscore_bridge* handle,
                                        uint8_t* output, size_t capacity,
                                        uint64_t* generation);
int corvuscore_acknowledge_battery(corvuscore_bridge* handle, uint64_t generation);

/* GBA cartridge hardware, for games whose detection fails. Each setting takes
 * effect at the next corvuscore_load or corvuscore_reset. Returns 1 on
 * success, 0 for a GB/GBC handle or an unknown value. */
enum corvuscore_gba_save_type {
    CORVUSCORE_GBA_SAVE_AUTO = 0,
    CORVUSCORE_GBA_SAVE_NONE = 1,
    CORVUSCORE_GBA_SAVE_SRAM = 2,
    CORVUSCORE_GBA_SAVE_FLASH_64K = 3,
    CORVUSCORE_GBA_SAVE_FLASH_128K = 4,
    CORVUSCORE_GBA_SAVE_EEPROM_512 = 5,
    CORVUSCORE_GBA_SAVE_EEPROM_8K = 6
};
int corvuscore_set_gba_save_type(corvuscore_bridge* handle, int save_type);

enum corvuscore_gba_rtc {
    CORVUSCORE_GBA_RTC_AUTO = 0,
    CORVUSCORE_GBA_RTC_OFF = 1,
    CORVUSCORE_GBA_RTC_ON = 2
};
int corvuscore_set_gba_rtc(corvuscore_bridge* handle, int rtc);

/* GBA BIOS dumped from a console (16384 bytes, copied) instead of CorvusCore's
 * own, from the next corvuscore_load or corvuscore_reset; size 0 goes back to
 * CorvusCore's. The real BIOS plays its boot animation unless skip_intro is
 * nonzero. A state only loads with the BIOS it was taken with, and keeps the
 * BIOS in place. Returns 1, or 0 for a GB/GBC handle or another size. */
int corvuscore_set_gba_bios(corvuscore_bridge* handle, const uint8_t* bios, size_t size, int skip_intro);

/* 1 while the cartridge's rumble motor runs (GB MBC5 Rumble, GBA Drill Dozer
 * and WarioWare: Twisted!). Sample it after each corvuscore_run_frame. */
int corvuscore_rumble_active(const corvuscore_bridge* handle);

/* Sensors of the loaded game's cartridge, as bit flags, so that a frontend
 * can feed them from the device's own sensors or from its controls. */
enum corvuscore_sensor {
    CORVUSCORE_SENSOR_SOLAR = 1,    /* Boktai 1 to 3 */
    CORVUSCORE_SENSOR_ROTATION = 2, /* WarioWare: Twisted! */
    CORVUSCORE_SENSOR_TILT = 4      /* Yoshi Topsy-Turvy, Koro Koro Puzzle, GB MBC7 */
};
int corvuscore_sensors(const corvuscore_bridge* handle);

/* Sensor readings. They are kept across corvuscore_load, corvuscore_reset and
 * state loads, and ignored when the game has no such sensor; out-of-range
 * values are clamped. Each returns 1, or 0 for a null handle. */
/* Light on the solar sensor: 0 (darkness) to 255 (full sun). */
int corvuscore_set_solar_light(corvuscore_bridge* handle, int level);
/* Rotation around the screen's axis: -32768..32767, positive clockwise as
 * the player sees the screen. */
int corvuscore_set_rotation(corvuscore_bridge* handle, int speed);
/* Tilt: -32768..32767 per axis for -1 g..1 g. x > 0 when the right side of
 * the console goes down, y > 0 when its top edge moves away from the player. */
int corvuscore_set_tilt(corvuscore_bridge* handle, int x, int y);

/* Colors of Game Boy games on a GBC handle (CORVUSCORE_GBC, CORVUSCORE_GBC_ON_GBA)
 * started without boot ROM. AUTO gives the colors the GBC boot ROM picks for
 * the game from its title (Pokemon Red in red...), and its default ones to a
 * game it does not know; GRAY, four greys (those of B + Left). The others are
 * the button combinations held during the boot ROM's logo. Applied at once,
 * and kept across corvuscore_load, corvuscore_reset and state loads. Returns 1,
 * or 0 for another handle or an unknown value. */
enum corvuscore_gbc_colors {
    CORVUSCORE_GBC_COLORS_AUTO = 0,
    CORVUSCORE_GBC_COLORS_GRAY = 1,
    CORVUSCORE_GBC_COLORS_UP = 2,       /* brown */
    CORVUSCORE_GBC_COLORS_A_UP = 3,     /* red */
    CORVUSCORE_GBC_COLORS_B_UP = 4,     /* sepia */
    CORVUSCORE_GBC_COLORS_LEFT = 5,     /* blue */
    CORVUSCORE_GBC_COLORS_A_LEFT = 6,   /* blue-violet */
    CORVUSCORE_GBC_COLORS_B_LEFT = 7,   /* grey */
    CORVUSCORE_GBC_COLORS_DOWN = 8,     /* pastel */
    CORVUSCORE_GBC_COLORS_A_DOWN = 9,   /* yellow and red */
    CORVUSCORE_GBC_COLORS_B_DOWN = 10,  /* yellow and brown */
    CORVUSCORE_GBC_COLORS_RIGHT = 11,   /* green and red */
    CORVUSCORE_GBC_COLORS_A_RIGHT = 12, /* green and blue, an unknown game's */
    CORVUSCORE_GBC_COLORS_B_RIGHT = 13  /* negative */
};
int corvuscore_set_gbc_colors(corvuscore_bridge* handle, int colors);

/* Cheat codes. An entry is one cheat of a cheat menu: one or more lines
 * separated by spaces, line breaks or '+'.
 * - GB/GBC: Game Genie (ABC-DEF-GHI or ABC-DEF) and GameShark (8 digits,
 *   like 010238CD), mixed as needed.
 * - GBA: unencrypted Codebreaker (XAAAAAAA YYYY), and GameShark v1/v2 or
 *   Pro Action Replay v3 encrypted as the device shows them
 *   (XXXXXXXX YYYYYYYY), told apart by their decryption.
 * GBA ROM patches, DEADFACE seed changes and encrypted Codebreaker codes are
 * refused. Refusal reasons are in French, meant for the player. */
/* 1 if `code` is a usable cheat for `system` (enum corvuscore_system), else 0
 * with the reason in corvuscore_last_error(). Needs no handle. */
int corvuscore_check_cheat(int system, const char* code);
/* Replaces the active cheats from the next frame; count 0 removes them all.
 * They survive corvuscore_load, corvuscore_reset and state loads, without being
 * stored in states. Returns 0 if an entry is refused: the previous list
 * stays, and corvuscore_last_error() names the entry (from 1) and the reason. */
int corvuscore_set_cheats(corvuscore_bridge* handle, const char* const* codes, size_t count);

/* Link cable between handles of this process, for example two games side by
 * side on one device. Link 2 GB/GBC handles (a GB and a GBC can be mixed) or
 * 2 to 4 GBA handles; the first GBA is the parent of multiplayer games. Each
 * handle keeps its ROM, saves, states and audio: load them as usual.
 * While linked, advance the handles with corvuscore_link_run_frame only:
 * corvuscore_run_frame fails on them. Destroying one of the handles unplugs
 * the cable for all of them; corvuscore_link_destroy must still be called.
 * Returns NULL on error (see corvuscore_last_error). */
typedef struct corvuscore_link corvuscore_link;
corvuscore_link* corvuscore_link_create(corvuscore_bridge* const* handles, size_t count);
void corvuscore_link_destroy(corvuscore_link* link);
/* One frame of every linked handle, run together. argb[i] receives the frame
 * of handles[i], in the format of corvuscore_run_frame; each buffer holds at
 * least pixel_capacity pixels. Returns 1 on success. */
int corvuscore_link_run_frame(corvuscore_link* link, uint32_t* const* argb, size_t pixel_capacity);

/* Link cable to a console on another device: another phone, a PC...
 * The host carries the messages between the two CorvusCore instances, in
 * order and without loss (a TCP connection, for example): `send` receives
 * each message for the other side, and each message from it goes to
 * corvuscore_remote_link_receive. Then the host plugs the cable in.
 * GB/GBC: either console may clock a transfer. GBA: two players; `player` 0
 * is the parent of multiplayer games, the other device takes 1.
 * The console that clocks a transfer waits for the other's reply in real
 * time, inside corvuscore_run_frame: emulated time does not move, so the game
 * sees no delay. The other answers from its own corvuscore_run_frame, or
 * between frames from corvuscore_remote_link_service.
 * Threads: `send` is called from the thread running the handle and from the
 * one plugging the cable in; it must be thread-safe and must not block for
 * long. corvuscore_remote_link_receive and corvuscore_remote_link_set_connected
 * may be called from any thread; the other calls follow the handle's rule.
 * A handle joined by corvuscore_link_create takes no remote cable, and the
 * reverse. Returns NULL on error (see corvuscore_last_error). */
typedef struct corvuscore_remote_link corvuscore_remote_link;
typedef void (*corvuscore_remote_send)(void* user, const uint8_t* message, size_t size);
corvuscore_remote_link* corvuscore_remote_link_create(corvuscore_bridge* handle, int player,
                                                  corvuscore_remote_send send, void* user);
/* Unplugs and frees the cable: the handle plays alone again. Not while the
 * handle runs a frame: unplug first, a waiting handle then returns at once. */
void corvuscore_remote_link_destroy(corvuscore_remote_link* link);
/* A message from the other device. Returns 0 when it is malformed: it is
 * then ignored. */
int corvuscore_remote_link_receive(corvuscore_remote_link* link, const uint8_t* message, size_t size);
/* Plugs the cable in (1) or pulls it out (0). Plugged in, the console
 * introduces itself to the other one; the cable carries transfers once the
 * other has done the same. Pulled out, a transfer in progress ends as
 * without a cable. */
void corvuscore_remote_link_set_connected(corvuscore_remote_link* link, int connected);
enum corvuscore_remote_state {
    CORVUSCORE_REMOTE_UNPLUGGED = 0,
    CORVUSCORE_REMOTE_WAITING = 1,      /* plugged in, the other has not introduced itself yet */
    CORVUSCORE_REMOTE_PLUGGED = 2,
    CORVUSCORE_REMOTE_INCOMPATIBLE = 3  /* another protocol version or console family */
};
int corvuscore_remote_link_state(const corvuscore_remote_link* link);
/* Between two frames: answers the transfers the other console clocked in
 * the meantime, without waiting for the next frame. Returns 1 on success. */
int corvuscore_remote_link_service(corvuscore_remote_link* link);
/* 1 while the game uses its link port: a transfer asked for or awaited in
 * the last three seconds, or a GBA in multiplayer mode. A partner is welcome. */
int corvuscore_link_wanted(const corvuscore_bridge* handle);

#ifdef __cplusplus
}
#endif
#endif
