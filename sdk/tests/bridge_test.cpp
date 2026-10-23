// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only

#include <corvuscore/bridge.h>
#include <synthetic_roms.hpp>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

using Handle = std::unique_ptr<corvuscore_bridge, decltype(&corvuscore_destroy)>;

/* The minimal GBA program stores 0x5A into SRAM once, then loops. */
int battery_tracking_test() {
    auto rom = corvuscore::testing::minimal_gba_rom();
    constexpr char marker[] = "SRAM_V";
    std::memcpy(rom.data() + 0x100, marker, sizeof marker - 1);
    Handle handle(corvuscore_create(CORVUSCORE_GBA), corvuscore_destroy);
    if (!handle || !corvuscore_load(handle.get(), rom.data(), rom.size(), nullptr, 0)) return 20;
    if (corvuscore_battery_dirty(handle.get())) return 21;

    std::vector<std::uint32_t> frame(240 * 160);
    if (!corvuscore_run_frame(handle.get(), frame.data(), frame.size())) return 22;
    if (!corvuscore_battery_dirty(handle.get())) return 23;

    std::vector<std::uint8_t> battery(corvuscore_battery_size(handle.get()));
    std::uint64_t generation = 0;
    if (battery.empty() ||
        !corvuscore_copy_battery_generation(handle.get(), battery.data(), battery.size(), &generation) ||
        battery[0] != 0x5a) return 24;
    // Acknowledging another generation must not hide the pending write.
    if (!corvuscore_acknowledge_battery(handle.get(), generation + 1) ||
        !corvuscore_battery_dirty(handle.get())) return 25;
    if (!corvuscore_acknowledge_battery(handle.get(), generation) ||
        corvuscore_battery_dirty(handle.get())) return 26;
    if (!corvuscore_run_frame(handle.get(), frame.data(), frame.size()) ||
        corvuscore_battery_dirty(handle.get())) return 27;

    if (corvuscore_battery_dirty(nullptr) || corvuscore_acknowledge_battery(nullptr, 0)) return 28;
    return 0;
}

/* state_size met l'état en cache pour save_state : toute modification de la
 * machine entre les deux doit le rendre caduc, sinon l'état serait périmé. */
int state_cache_test() {
    auto rom = corvuscore::testing::minimal_game_boy_rom();
    Handle handle(corvuscore_create(CORVUSCORE_GB), corvuscore_destroy);
    if (!handle || !corvuscore_load(handle.get(), rom.data(), rom.size(), nullptr, 0)) return 30;
    std::vector<std::uint32_t> frame(160 * 144);
    const auto snapshot = [&] {
        std::vector<std::uint8_t> state(corvuscore_state_size(handle.get()));
        if (!corvuscore_save_state(handle.get(), state.data(), state.size())) state.clear();
        return state;
    };
    const auto save_only = [&](std::size_t size) {
        std::vector<std::uint8_t> state(size);
        if (!corvuscore_save_state(handle.get(), state.data(), state.size())) state.clear();
        return state;
    };
    const auto first = snapshot();
    if (first.empty() || save_only(first.size()) != first) return 31;
    if (!corvuscore_run_frame(handle.get(), frame.data(), frame.size())) return 32;
    const auto after_frame = save_only(first.size());
    if (after_frame.empty() || after_frame == first) return 33;
    if (!corvuscore_set_button(handle.get(), CORVUSCORE_START, 1)) return 34;
    const auto after_button = save_only(first.size());
    if (after_button.empty() || after_button == after_frame) return 35;
    if (!corvuscore_load_state(handle.get(), first.data(), first.size()) ||
        save_only(first.size()) != first) return 36;
    return 0;
}

/* Réglages de cartouche GBA : refusés sur un autre système ou hors bornes,
 * appliqués au prochain chargement ou reset. */
int gba_cartridge_settings_test() {
    auto rom = corvuscore::testing::minimal_gba_rom();
    Handle game_boy(corvuscore_create(CORVUSCORE_GB), corvuscore_destroy);
    if (!game_boy || corvuscore_set_gba_save_type(game_boy.get(), CORVUSCORE_GBA_SAVE_SRAM) ||
        corvuscore_set_gba_rtc(game_boy.get(), CORVUSCORE_GBA_RTC_ON) ||
        std::string(corvuscore_last_error()).empty()) return 40;
    if (corvuscore_set_gba_save_type(nullptr, CORVUSCORE_GBA_SAVE_AUTO)) return 41;

    Handle handle(corvuscore_create(CORVUSCORE_GBA), corvuscore_destroy);
    if (!handle || corvuscore_set_gba_save_type(handle.get(), -1) ||
        corvuscore_set_gba_save_type(handle.get(), CORVUSCORE_GBA_SAVE_EEPROM_8K + 1) ||
        corvuscore_set_gba_rtc(handle.get(), CORVUSCORE_GBA_RTC_ON + 1)) return 42;
    if (!corvuscore_set_gba_save_type(handle.get(), CORVUSCORE_GBA_SAVE_FLASH_128K) ||
        !corvuscore_load(handle.get(), rom.data(), rom.size(), nullptr, 0) ||
        corvuscore_battery_size(handle.get()) != 128 * 1024) return 43;
    // Sans signature, la détection ne trouve aucune sauvegarde.
    if (!corvuscore_set_gba_save_type(handle.get(), CORVUSCORE_GBA_SAVE_AUTO) ||
        corvuscore_battery_size(handle.get()) != 128 * 1024 ||
        !corvuscore_reset(handle.get()) || corvuscore_battery_size(handle.get()) != 0) return 44;
    // L'horloge ajoute le port GPIO, donc son état, à la sauvegarde d'état.
    const auto without_rtc = corvuscore_state_size(handle.get());
    if (!corvuscore_set_gba_rtc(handle.get(), CORVUSCORE_GBA_RTC_ON) || !corvuscore_reset(handle.get()) ||
        corvuscore_state_size(handle.get()) <= without_rtc) return 45;
    if (!corvuscore_set_gba_rtc(handle.get(), CORVUSCORE_GBA_RTC_AUTO) || !corvuscore_reset(handle.get()) ||
        corvuscore_state_size(handle.get()) != without_rtc) return 46;
    return 0;
}

/* Drill Dozer (code V49) allume son moteur par la broche 3 du GPIO. */
int gba_rumble_test() {
    std::vector<std::uint8_t> rom(0x200);
    // LDR r0, =0x080000C4 ; MOV r1, #8 ; STRH r1, [r0, #2] ; STRH r1, [r0] ; B .
    const std::uint32_t program[]{0xe59f000cU, 0xe3a01008U, 0xe1c010b2U, 0xe1c010b0U, 0xeafffffeU, 0x080000c4U};
    for (std::size_t index = 0; index < std::size(program); ++index)
        for (unsigned byte = 0; byte < 4; ++byte)
            rom[index * 4U + byte] = static_cast<std::uint8_t>(program[index] >> (byte * 8U));
    rom[0xb2] = 0x96;
    std::vector<std::uint32_t> frame(240 * 160);
    for (const bool motor : {false, true}) {
        std::memcpy(rom.data() + 0xac, motor ? "V49E" : "AAAE", 4);
        Handle handle(corvuscore_create(CORVUSCORE_GBA), corvuscore_destroy);
        if (!handle || !corvuscore_load(handle.get(), rom.data(), rom.size(), nullptr, 0) ||
            corvuscore_rumble_active(handle.get())) return 50;
        if (!corvuscore_run_frame(handle.get(), frame.data(), frame.size()) ||
            corvuscore_rumble_active(handle.get()) != (motor ? 1 : 0)) return 51;
    }
    return corvuscore_rumble_active(nullptr) ? 52 : 0;
}

/* Capteurs : reconnus par le jeu, et fournis avant même le chargement. Le
 * programme lance une mesure d'inclinaison (Yoshi, KYG) puis recopie l'octet
 * bas de X en IWRAM. */
int sensors_test() {
    auto rom = corvuscore::testing::minimal_gba_rom();
    const std::uint32_t program[]{0xe59f001cU, 0xe3a01055U, 0xe5c01000U, 0xe3a010aaU, 0xe5c01100U, 0xe59f200cU,
                                  0xe5d01200U, 0xe5c21000U, 0xeafffffcU, 0x0e008000U, 0x03000000U};
    for (std::size_t index = 0; index < std::size(program); ++index)
        for (unsigned byte = 0; byte < 4; ++byte)
            rom[index * 4U + byte] = static_cast<std::uint8_t>(program[index] >> (byte * 8U));
    struct Case { const char* code; int sensors; };
    for (const auto& item : {Case{"U3IE", CORVUSCORE_SENSOR_SOLAR}, Case{"RZWE", CORVUSCORE_SENSOR_ROTATION},
                             Case{"AAAE", 0}, Case{"KYGE", CORVUSCORE_SENSOR_TILT}}) {
        std::memcpy(rom.data() + 0xac, item.code, 4);
        Handle handle(corvuscore_create(CORVUSCORE_GBA), corvuscore_destroy);
        if (!handle || corvuscore_sensors(handle.get()) != 0) return 60;
        // Hors limites : ramené à la plage, jamais refusé.
        if (!corvuscore_set_solar_light(handle.get(), 999) || !corvuscore_set_rotation(handle.get(), -99999) ||
            !corvuscore_set_tilt(handle.get(), 40000, 0)) return 61;
        if (!corvuscore_load(handle.get(), rom.data(), rom.size(), nullptr, 0) ||
            corvuscore_sensors(handle.get()) != item.sensors) return 62;
    }
    // Dernier cas, Yoshi : inclinaison maximale à droite, 392h + DFh = 471h.
    Handle yoshi(corvuscore_create(CORVUSCORE_GBA), corvuscore_destroy);
    std::vector<std::uint32_t> frame(240 * 160);
    std::uint8_t copied = 0;
    if (!yoshi || !corvuscore_set_tilt(yoshi.get(), 32767, 0) ||
        !corvuscore_load(yoshi.get(), rom.data(), rom.size(), nullptr, 0) ||
        !corvuscore_run_frame(yoshi.get(), frame.data(), frame.size()) ||
        corvuscore_read_achievement_memory(yoshi.get(), 0, &copied, 1) != 1 || copied != 0x71) return 63;

    // Accéléromètre MBC7 des cartouches Game Boy.
    auto game_boy_rom = corvuscore::testing::minimal_game_boy_rom();
    game_boy_rom[0x147] = 0x22;
    Handle kirby(corvuscore_create(CORVUSCORE_GBC), corvuscore_destroy);
    if (!kirby || !corvuscore_load(kirby.get(), game_boy_rom.data(), game_boy_rom.size(), nullptr, 0) ||
        corvuscore_sensors(kirby.get()) != CORVUSCORE_SENSOR_TILT || !corvuscore_set_tilt(kirby.get(), -32768, 32767))
        return 64;
    return corvuscore_sensors(nullptr) || corvuscore_set_solar_light(nullptr, 0) ||
        corvuscore_set_rotation(nullptr, 0) || corvuscore_set_tilt(nullptr, 0, 0) ? 65 : 0;
}

} // namespace

/* Deux GBA reliées : le parent lance un transfert multijoueur (programme
 * assemblé avec llvm-mc, détaillé dans gba_core_test.cpp) et chaque console
 * range SIOMULTI0 et SIOMULTI1 au début de l'IWRAM. */
int link_cable_test() {
    const std::uint32_t code[] = {
        0xe59f208c, 0xe3a01000, 0xe1c211b4, 0xe3a01a02, 0xe3811003, 0xe1c210b8, 0xe1d230b8, 0xe3130004,
        0x03a04caa, 0x13a04cbb, 0xe1c240ba, 0x1a000008, 0xe1d230b8, 0xe3130008, 0x0afffffc, 0xe3833080,
        0xe1c230b8, 0xe1d230b8, 0xe3130080, 0x1afffffc, 0xea000005, 0xe1d230b8, 0xe3130080, 0x0afffffc,
        0xe1d230b8, 0xe3130080, 0x1afffffc, 0xe3a05403, 0xe1d260b0, 0xe1c560b0, 0xe1d260b2, 0xe1c560b2,
        0xe1d260b8, 0xe1c560b4, 0xe1d260b4, 0xe1c560b6, 0xeafffffe, 0x04000120};
    auto rom = corvuscore::testing::minimal_gba_rom();
    std::memcpy(rom.data(), code, sizeof code);  // petit-boutiste, comme la GBA
    Handle parent(corvuscore_create(CORVUSCORE_GBA), corvuscore_destroy);
    Handle child(corvuscore_create(CORVUSCORE_GBA), corvuscore_destroy);
    if (!parent || !child || !corvuscore_load(parent.get(), rom.data(), rom.size(), nullptr, 0) ||
        !corvuscore_load(child.get(), rom.data(), rom.size(), nullptr, 0)) return 60;
    corvuscore_bridge* const pair[] = {parent.get(), child.get()};
    std::unique_ptr<corvuscore_link, decltype(&corvuscore_link_destroy)> link(corvuscore_link_create(pair, 2), corvuscore_link_destroy);
    if (!link) return 61;
    if (corvuscore_link_create(pair, 2)) return 62;  // déjà reliées
    std::vector<std::uint32_t> first(240 * 160), second(240 * 160);
    std::uint32_t* const frames[] = {first.data(), second.data()};
    if (corvuscore_run_frame(parent.get(), first.data(), first.size())) return 63;  // reliée : passe par le câble
    if (corvuscore_link_run_frame(link.get(), frames, first.size() - 1)) return 64;
    for (int frame = 0; frame < 3; ++frame)
        if (!corvuscore_link_run_frame(link.get(), frames, first.size())) return 65;
    for (auto* handle : pair) {
        std::uint8_t received[4]{};
        if (corvuscore_read_achievement_memory(handle, 0, received, sizeof received) != sizeof received) return 66;
        if (received[0] != 0x00 || received[1] != 0xaa || received[2] != 0x00 || received[3] != 0xbb) return 67;
    }
    if ((first[0] & 0xFF000000u) != 0xFF000000u) return 68;

    // Une GBA ne se relie pas à une Game Boy ; deux Game Boy, si.
    auto gb_rom = corvuscore::testing::minimal_game_boy_rom();
    Handle game_boy(corvuscore_create(CORVUSCORE_GB), corvuscore_destroy);
    Handle color(corvuscore_create(CORVUSCORE_GBC), corvuscore_destroy);
    if (!game_boy || !color || !corvuscore_load(game_boy.get(), gb_rom.data(), gb_rom.size(), nullptr, 0) ||
        !corvuscore_load(color.get(), gb_rom.data(), gb_rom.size(), nullptr, 0)) return 69;
    corvuscore_bridge* const mixed[] = {game_boy.get(), parent.get()};
    if (corvuscore_link_create(mixed, 2)) return 70;
    corvuscore_bridge* const game_boys[] = {game_boy.get(), color.get()};
    std::unique_ptr<corvuscore_link, decltype(&corvuscore_link_destroy)> gb_link(corvuscore_link_create(game_boys, 2), corvuscore_link_destroy);
    if (!gb_link) return 71;
    std::vector<std::uint32_t> gb_first(160 * 144), gb_second(160 * 144);
    std::uint32_t* const gb_frames[] = {gb_first.data(), gb_second.data()};
    if (!corvuscore_link_run_frame(gb_link.get(), gb_frames, gb_first.size())) return 72;

    // Détruire une console débranche le câble ; l'autre repart seule.
    child.reset();
    if (corvuscore_link_run_frame(link.get(), frames, first.size())) return 73;
    if (!corvuscore_run_frame(parent.get(), first.data(), first.size())) return 74;
    if (corvuscore_link_create(nullptr, 2) || corvuscore_link_run_frame(nullptr, frames, first.size())) return 75;
    corvuscore_link_destroy(nullptr);
    return 0;
}

int main() {
    if (corvuscore_api_version() != CORVUSCORE_API_VERSION ||
        std::string(corvuscore_version()) != CORVUSCORE_VERSION) {
        std::cerr << "C API or library version mismatch\n";
        return 1;
    }
    if (const int failure = state_cache_test()) {
        std::cerr << "State cache failed (" << failure << "): " << corvuscore_last_error() << "\n";
        return failure;
    }
    if (const int failure = battery_tracking_test()) {
        std::cerr << "Battery tracking failed (" << failure << "): " << corvuscore_last_error() << "\n";
        return failure;
    }
    if (const int failure = gba_cartridge_settings_test()) {
        std::cerr << "GBA cartridge settings failed (" << failure << "): " << corvuscore_last_error() << "\n";
        return failure;
    }
    if (const int failure = gba_rumble_test()) {
        std::cerr << "GBA rumble failed (" << failure << "): " << corvuscore_last_error() << "\n";
        return failure;
    }
    if (const int failure = link_cable_test()) {
        std::cerr << "Link cable failed (" << failure << "): " << corvuscore_last_error() << "\n";
        return failure;
    }
    if (const int failure = sensors_test()) {
        std::cerr << "Cartridge sensors failed (" << failure << "): " << corvuscore_last_error() << "\n";
        return failure;
    }
    if (corvuscore_create(99)) {
        std::cerr << "Invalid platform was accepted\n";
        return 1;
    }
    for (int system = CORVUSCORE_GB; system <= CORVUSCORE_GBA; ++system) {
        Handle handle(corvuscore_create(system), corvuscore_destroy);
        if (!handle) return 2;
        auto rom = system == CORVUSCORE_GBA
            ? corvuscore::testing::minimal_gba_rom()
            : corvuscore::testing::minimal_game_boy_rom();
        if (system == CORVUSCORE_GBC) rom[0x143] = 0x80;
        if (!corvuscore_load(handle.get(), rom.data(), rom.size(), nullptr, 0)) {
            std::cerr << corvuscore_last_error() << "\n";
            return 3;
        }
        const auto width = corvuscore_video_width(handle.get());
        const auto height = corvuscore_video_height(handle.get());
        if (width != (system == CORVUSCORE_GBA ? 240 : 160) ||
            height != (system == CORVUSCORE_GBA ? 160 : 144)) return 4;
        // Unmapped/OOB reads must report a short length. Each read is
        // observational: it cannot advance a frame or clock a mapper.
        std::uint8_t achievement[16]{};
        if (corvuscore_read_achievement_memory(
                handle.get(), 0xFFFFFFFFu, achievement, sizeof achievement) != 0)
            return 41;
        if (corvuscore_read_achievement_memory(
                handle.get(), 0x0000u, achievement, sizeof achievement) != sizeof achievement)
            return 42;
        if (corvuscore_read_achievement_memory(
                handle.get(), 0x0000u, nullptr, sizeof achievement) != 0)
            return 43;
        std::vector<std::uint32_t> frame(static_cast<size_t>(width * height));
        if (corvuscore_run_frame(handle.get(), frame.data(), frame.size() - 1)) return 5;
        if (!corvuscore_run_frame(handle.get(), frame.data(), frame.size())) {
            std::cerr << corvuscore_last_error() << "\n"; return 6;
        }
        if ((frame[0] & 0xFF000000u) != 0xFF000000u) return 7;
        if (!corvuscore_set_button(handle.get(), CORVUSCORE_A, 1)) return 8;
        if (!corvuscore_set_button(handle.get(), CORVUSCORE_A, 0)) return 9;
        std::vector<std::int16_t> audio(4096);
        corvuscore_read_audio(handle.get(), audio.data(), audio.size());
        if (corvuscore_audio_rate(handle.get()) <= 0) return 10;
        const auto size = corvuscore_state_size(handle.get());
        if (size == 0) return 11;
        std::vector<std::uint8_t> state(size);
        if (!corvuscore_save_state(handle.get(), state.data(), state.size())) return 12;
        if (!corvuscore_load_state(handle.get(), state.data(), state.size())) {
            std::cerr << corvuscore_last_error() << "\n"; return 13;
        }
        if (!corvuscore_reset(handle.get())) return 14;
    }
    std::cout << "Portable bridge GB/GBC/GBA smoke tests OK\n";
}
