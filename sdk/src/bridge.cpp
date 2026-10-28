// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only

#include <corvuscore/bridge.h>
#include <corvuscore/core.hpp>
#include <corvuscore/gb/core.hpp>
#include <corvuscore/gbc/core.hpp>
#include <corvuscore/remote_link.hpp>

#include <array>
#include <algorithm>
#include <exception>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef CORVUSCORE_VERSION
#error "CORVUSCORE_VERSION comes from the CMake project version"
#endif

struct corvuscore_bridge {
    std::unique_ptr<corvuscore::Core> core;
    int system = -1;  // valeur de corvuscore_system donnée à corvuscore_create
    std::vector<std::int32_t> framebuffer;
    bool loaded = false;
    // État sérialisé par corvuscore_state_size : un frontend appelle presque
    // toujours save_state juste après, machine inchangée. Toute fonction qui
    // peut modifier la machine doit appeler machine_may_change().
    std::vector<std::uint8_t> state_cache;
    bool state_cache_valid = false;
    corvuscore_link* link = nullptr;
    corvuscore_remote_link* remote = nullptr;
};

struct corvuscore_link {
    std::vector<corvuscore_bridge*> handles;
    std::unique_ptr<corvuscore::LinkCable> cable;
};

struct corvuscore_remote_link {
    corvuscore_bridge* handle = nullptr;
    std::unique_ptr<corvuscore::RemoteLink> link;
};

namespace {
thread_local std::string error_message;

void fail(const char* message) noexcept {
    try { error_message = message; }
    catch (...) {}
}

template<typename F>
int attempt(F&& fn) noexcept {
    try {
        error_message.clear();
        fn();
        return 1;
    } catch (const std::exception& ex) {
        fail(ex.what());
    } catch (...) {
        fail("Unhandled native core error");
    }
    return 0;
}

void machine_may_change(corvuscore_bridge* handle) noexcept {
    if (handle) handle->state_cache_valid = false;
}

void require_gba(const corvuscore_bridge* handle) {
    if (!handle || !handle->core) throw std::invalid_argument("Null handle");
    if (handle->core->console() != corvuscore::Console::game_boy_advance) throw std::invalid_argument("Not a GBA handle");
}

// Console d'une valeur de corvuscore_system, pour décoder ses codes de triche.
std::optional<corvuscore::Console> console_of(int system) noexcept {
    switch (system) {
    case CORVUSCORE_GB: case CORVUSCORE_GBC: case CORVUSCORE_GBC_ON_GBA: return corvuscore::Console::game_boy;
    case CORVUSCORE_GBA: return corvuscore::Console::game_boy_advance;
    default: return std::nullopt;
    }
}

void require_loaded(const corvuscore_bridge* handle) {
    if (!handle || !handle->core || !handle->loaded)
        throw std::runtime_error("No ROM loaded");
}

constexpr std::array<std::uint32_t, 4> dmg_palette = {
    0xFFE0F8D0u, 0xFF88C070u, 0xFF346856u, 0xFF081820u
};

void copy_frame(const corvuscore_bridge& handle, uint32_t* argb) noexcept {
    const bool indexed = handle.core->framebuffer_format() == corvuscore::FramebufferFormat::indexed_4;
    for (size_t i = 0; i < handle.framebuffer.size(); ++i) {
        const auto pixel = static_cast<uint32_t>(handle.framebuffer[i]);
        argb[i] = indexed ? dmg_palette[pixel & 3u] : (0xFF000000u | (pixel & 0x00FFFFFFu));
    }
}

// Débranche le câble de toutes ses consoles ; la structure reste à libérer.
void unplug(corvuscore_link& link) noexcept {
    link.cable.reset();
    for (auto* handle : link.handles)
        if (handle) handle->link = nullptr;
    link.handles.clear();
}
} // namespace

extern "C" {

int corvuscore_api_version(void) { return CORVUSCORE_API_VERSION; }
const char* corvuscore_version(void) { return CORVUSCORE_VERSION; }

corvuscore_bridge* corvuscore_create(int system) {
    corvuscore_bridge* result = nullptr;
    attempt([&] {
        auto handle = std::make_unique<corvuscore_bridge>();
        switch (system) {
        case CORVUSCORE_GB:
            handle->core = corvuscore::gb::make_core(corvuscore::gb::HardwareModel::dmg);
            break;
        case CORVUSCORE_GBC:
            handle->core = corvuscore::gbc::make_core();
            break;
        case CORVUSCORE_GBA:
            handle->core = corvuscore::make_gba_core(std::nullopt);
            break;
        case CORVUSCORE_GBC_ON_GBA:
            handle->core = corvuscore::gb::make_core(corvuscore::gb::HardwareModel::agb);
            break;
        default:
            throw std::invalid_argument("Invalid system");
        }
        handle->framebuffer.resize(handle->core->video_spec().pixel_count());
        handle->system = system;
        result = handle.release();
    });
    return result;
}
void corvuscore_destroy(corvuscore_bridge* handle) {
    if (handle && handle->link) unplug(*handle->link);
    if (handle && handle->remote) {
        static_cast<void>(handle->core->connect_remote_link(nullptr, 0));
        handle->remote->handle = nullptr;
    }
    delete handle;
}
const char* corvuscore_last_error(void) { return error_message.c_str(); }

int corvuscore_load(corvuscore_bridge* handle, const uint8_t* rom, size_t rom_size,
                      const uint8_t* battery, size_t battery_size) {
    machine_may_change(handle);
    return attempt([&] {
        if (!handle || !handle->core) throw std::invalid_argument("Null handle");
        if (!rom || !rom_size || (battery_size && !battery))
            throw std::invalid_argument("Invalid ROM or battery data");
        handle->loaded = false;
        handle->core->load_rom({rom, rom_size}, {battery, battery_size});
        handle->loaded = true;
    });
}
int corvuscore_reset(corvuscore_bridge* handle) {
    machine_may_change(handle);
    return attempt([&] { require_loaded(handle); handle->core->reset(); });
}
int corvuscore_reset_group(corvuscore_bridge* const* handles, size_t count) {
    return attempt([&] {
        if (!handles || count == 0 || count > 4) throw std::invalid_argument("Invalid reset group");
        for (size_t index = 0; index < count; ++index) {
            require_loaded(handles[index]);
            for (size_t previous = 0; previous < index; ++previous)
                if (handles[index] == handles[previous]) throw std::invalid_argument("Duplicate reset handle");
        }
        std::array<std::unique_ptr<corvuscore::PreparedReset>, 4> plans;
        for (size_t index = 0; index < count; ++index) plans[index] = handles[index]->core->prepare_reset();
        for (size_t index = 0; index < count; ++index) {
            plans[index]->commit();
            machine_may_change(handles[index]);
        }
    });
}
int corvuscore_set_button(corvuscore_bridge* handle, int button, int pressed) {
    machine_may_change(handle);
    return attempt([&] {
        require_loaded(handle);
        if (button < CORVUSCORE_UP || button > CORVUSCORE_R)
            throw std::invalid_argument("Invalid button");
        handle->core->set_button(static_cast<corvuscore::Button>(button), pressed != 0);
    });
}
int corvuscore_video_width(const corvuscore_bridge* handle) {
    return handle && handle->core ? handle->core->video_spec().width : 0;
}
int corvuscore_video_height(const corvuscore_bridge* handle) {
    return handle && handle->core ? handle->core->video_spec().height : 0;
}
double corvuscore_video_fps(const corvuscore_bridge* handle) {
    return handle && handle->core ? handle->core->video_spec().refresh_rate_hz : 0;
}
int corvuscore_audio_rate(const corvuscore_bridge* handle) {
    return handle && handle->core ? handle->core->audio_spec().sample_rate_hz : 0;
}
int corvuscore_run_frame(corvuscore_bridge* handle, uint32_t* argb, size_t pixel_capacity) {
    machine_may_change(handle);
    return attempt([&] {
        require_loaded(handle);
        if (!argb || pixel_capacity < handle->framebuffer.size())
            throw std::invalid_argument("Insufficient video buffer");
        if (handle->link) throw std::logic_error("Linked handle: use corvuscore_link_run_frame");
        handle->core->run_frame(handle->framebuffer, true);
        copy_frame(*handle, argb);
    });
}
size_t corvuscore_read_audio(corvuscore_bridge* handle,
                              int16_t* output, size_t sample_capacity) {
    machine_may_change(handle);
    size_t count = 0;
    attempt([&] {
        require_loaded(handle);
        if (sample_capacity && !output) throw std::invalid_argument("Null audio buffer");
        count = handle->core->read_audio({output, sample_capacity});
    });
    return count;
}
size_t corvuscore_read_achievement_memory(const corvuscore_bridge* handle,
                                           uint32_t address, uint8_t* output,
                                           size_t capacity) {
    size_t read = 0;
    attempt([&] {
        require_loaded(handle);
        if (capacity > 0x58000 || (capacity && !output))
            throw std::invalid_argument("Invalid achievement memory buffer");
        if (!capacity) return;
        read = handle->core->read_achievement_memory(address, {output, capacity});
        if (read > capacity) throw std::runtime_error("Invalid achievement read count");
    });
    return read;
}

size_t corvuscore_state_size(corvuscore_bridge* handle) {
    size_t count = 0;
    attempt([&] {
        require_loaded(handle);
        handle->state_cache = handle->core->save_state();
        handle->state_cache_valid = true;
        count = handle->state_cache.size();
    });
    return count;
}
int corvuscore_save_state(corvuscore_bridge* handle, uint8_t* output, size_t capacity) {
    return attempt([&] {
        require_loaded(handle);
        if (!handle->state_cache_valid) {
            handle->state_cache = handle->core->save_state();
            handle->state_cache_valid = true;
        }
        const auto& state = handle->state_cache;
        if (!output || capacity < state.size()) throw std::invalid_argument("Insufficient state buffer");
        std::copy(state.begin(), state.end(), output);
    });
}
int corvuscore_load_state(corvuscore_bridge* handle, const uint8_t* state, size_t size) {
    machine_may_change(handle);
    return attempt([&] {
        require_loaded(handle);
        if (!state || !size) throw std::invalid_argument("Invalid save state");
        handle->core->load_state({state, size});
    });
}
size_t corvuscore_battery_size(corvuscore_bridge* handle) {
    machine_may_change(handle);
    size_t count = 0;
    attempt([&] {
        require_loaded(handle);
        const auto battery = handle->core->snapshot_battery_ram();
        count = battery ? battery->data.size() : 0;
    });
    return count;
}
int corvuscore_copy_battery(corvuscore_bridge* handle, uint8_t* output, size_t capacity) {
    return corvuscore_copy_battery_generation(handle, output, capacity, nullptr);
}
int corvuscore_battery_dirty(const corvuscore_bridge* handle) {
    return handle && handle->core && handle->loaded && handle->core->battery_ram_dirty() ? 1 : 0;
}
int corvuscore_copy_battery_generation(corvuscore_bridge* handle, uint8_t* output,
                                        size_t capacity, uint64_t* generation) {
    machine_may_change(handle);
    return attempt([&] {
        require_loaded(handle);
        const auto battery = handle->core->snapshot_battery_ram();
        if (!battery) throw std::runtime_error("No battery RAM");
        if (!output || capacity < battery->data.size())
            throw std::invalid_argument("Insufficient battery buffer");
        std::copy(battery->data.begin(), battery->data.end(), output);
        if (generation) *generation = battery->generation;
    });
}
int corvuscore_acknowledge_battery(corvuscore_bridge* handle, uint64_t generation) {
    machine_may_change(handle);
    return attempt([&] {
        require_loaded(handle);
        handle->core->acknowledge_battery_ram_saved(generation);
    });
}
int corvuscore_set_gba_save_type(corvuscore_bridge* handle, int save_type) {
    return attempt([&] {
        require_gba(handle);
        static constexpr std::array<std::optional<corvuscore::GbaSaveType>, 7> types{
            std::nullopt, corvuscore::GbaSaveType::none, corvuscore::GbaSaveType::sram,
            corvuscore::GbaSaveType::flash_64k, corvuscore::GbaSaveType::flash_128k,
            corvuscore::GbaSaveType::eeprom_512, corvuscore::GbaSaveType::eeprom_8k};
        if (save_type < 0 || save_type >= static_cast<int>(types.size())) throw std::invalid_argument("Invalid GBA save type");
        handle->core->set_gba_forced_save_type(types[static_cast<std::size_t>(save_type)]);
    });
}
int corvuscore_set_gba_rtc(corvuscore_bridge* handle, int rtc) {
    return attempt([&] {
        require_gba(handle);
        if (rtc < CORVUSCORE_GBA_RTC_AUTO || rtc > CORVUSCORE_GBA_RTC_ON) throw std::invalid_argument("Invalid GBA RTC mode");
        handle->core->set_gba_forced_rtc(rtc == CORVUSCORE_GBA_RTC_AUTO ? std::nullopt
                                                                         : std::optional<bool>{rtc == CORVUSCORE_GBA_RTC_ON});
    });
}
int corvuscore_set_gba_bios(corvuscore_bridge* handle, const uint8_t* bios, size_t size, int skip_intro) {
    return attempt([&] {
        require_gba(handle);
        if (size && !bios) throw std::invalid_argument("Null GBA BIOS");
        handle->core->set_gba_bios({bios, size}, skip_intro != 0);
    });
}
int corvuscore_rumble_active(const corvuscore_bridge* handle) {
    return handle && handle->core && handle->loaded && handle->core->rumble_active() ? 1 : 0;
}
int corvuscore_sensors(const corvuscore_bridge* handle) {
    if (!handle || !handle->core || !handle->loaded) return 0;
    const auto sensors = handle->core->cartridge_sensors();
    return (sensors.solar ? CORVUSCORE_SENSOR_SOLAR : 0) | (sensors.rotation ? CORVUSCORE_SENSOR_ROTATION : 0) |
           (sensors.tilt ? CORVUSCORE_SENSOR_TILT : 0);
}
int corvuscore_set_solar_light(corvuscore_bridge* handle, int level) {
    if (!handle || !handle->core) return 0;
    machine_may_change(handle);
    handle->core->set_solar_light(std::clamp(level, 0, 255));
    return 1;
}
int corvuscore_set_rotation(corvuscore_bridge* handle, int speed) {
    if (!handle || !handle->core) return 0;
    machine_may_change(handle);
    handle->core->set_rotation_speed(std::clamp(speed, -32768, 32767));
    return 1;
}
int corvuscore_set_tilt(corvuscore_bridge* handle, int x, int y) {
    if (!handle || !handle->core) return 0;
    machine_may_change(handle);
    x = std::clamp(x, -32768, 32767);
    y = std::clamp(y, -32768, 32767);
    if (handle->core->console() == corvuscore::Console::game_boy_advance) {
        handle->core->set_gba_tilt(x, y);
    } else {
        // MBC7 : 81D0h au repos, environ 70h par g ; X baisse quand le côté
        // droit descend, Y quand le haut s'éloigne (selon les conventions du moteur).
        constexpr int units_per_g = 0x70;
        handle->core->set_game_boy_acceleration(-x * units_per_g / 0x7fff, -y * units_per_g / 0x7fff);
    }
    return 1;
}
corvuscore_link* corvuscore_link_create(corvuscore_bridge* const* handles, size_t count) {
    corvuscore_link* result = nullptr;
    attempt([&] {
        if (!handles || count < 2) throw std::invalid_argument("A link cable needs at least two handles");
        std::vector<corvuscore::Core*> cores;
        for (size_t i = 0; i < count; ++i) {
            if (!handles[i] || !handles[i]->core) throw std::invalid_argument("Null handle");
            if (handles[i]->link || handles[i]->remote) throw std::invalid_argument("Handle already linked");
            cores.push_back(handles[i]->core.get());
        }
        auto link = std::make_unique<corvuscore_link>();
        link->cable = corvuscore::connect_link_cable(cores);
        link->handles.assign(handles, handles + count);
        for (auto* handle : link->handles) {
            machine_may_change(handle);
            handle->link = link.get();
        }
        result = link.release();
    });
    return result;
}
void corvuscore_link_destroy(corvuscore_link* link) {
    if (!link) return;
    for (auto* handle : link->handles) machine_may_change(handle);
    unplug(*link);
    delete link;
}
int corvuscore_link_run_frame(corvuscore_link* link, uint32_t* const* argb, size_t pixel_capacity) {
    return attempt([&] {
        if (!link || !link->cable) throw std::logic_error("Link cable unplugged");
        if (!argb) throw std::invalid_argument("Insufficient video buffer");
        std::vector<std::span<std::int32_t>> frames;
        for (size_t i = 0; i < link->handles.size(); ++i) {
            auto* handle = link->handles[i];
            require_loaded(handle);
            if (!argb[i] || pixel_capacity < handle->framebuffer.size())
                throw std::invalid_argument("Insufficient video buffer");
            machine_may_change(handle);
            frames.emplace_back(handle->framebuffer);
        }
        link->cable->run_frame(frames, true);
        for (size_t i = 0; i < link->handles.size(); ++i) copy_frame(*link->handles[i], argb[i]);
    });
}
corvuscore_remote_link* corvuscore_remote_link_create(corvuscore_bridge* handle, int player,
                                                  corvuscore_remote_send send, void* user) {
    corvuscore_remote_link* result = nullptr;
    attempt([&] {
        if (!handle || !handle->core) throw std::invalid_argument("Null handle");
        if (!send) throw std::invalid_argument("Null send callback");
        if (handle->link || handle->remote) throw std::invalid_argument("Handle already linked");
        if (player < 0 || player > 1) throw std::invalid_argument("Invalid player");
        const auto family = handle->core->console() == corvuscore::Console::game_boy_advance
            ? corvuscore::RemoteLink::Family::game_boy_advance : corvuscore::RemoteLink::Family::game_boy;
        auto remote = std::make_unique<corvuscore_remote_link>();
        remote->link = std::make_unique<corvuscore::RemoteLink>(family, [send, user](std::span<const std::uint8_t> bytes) {
            send(user, bytes.data(), bytes.size());
        });
        if (!handle->core->connect_remote_link(remote->link.get(), player))
            throw std::invalid_argument("This console takes no remote link cable");
        machine_may_change(handle);
        remote->handle = handle;
        handle->remote = remote.get();
        result = remote.release();
    });
    return result;
}
void corvuscore_remote_link_destroy(corvuscore_remote_link* link) {
    if (!link) return;
    link->link->set_connected(false);
    if (auto* handle = link->handle) {
        machine_may_change(handle);
        static_cast<void>(handle->core->connect_remote_link(nullptr, 0));
        handle->remote = nullptr;
    }
    delete link;
}
int corvuscore_remote_link_receive(corvuscore_remote_link* link, const uint8_t* message, size_t size) {
    if (!link || !message) return 0;
    try {
        return link->link->receive(std::span<const std::uint8_t>(message, size)) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}
void corvuscore_remote_link_set_connected(corvuscore_remote_link* link, int connected) {
    if (!link) return;
    try {
        link->link->set_connected(connected != 0);
    } catch (...) {
    }
}
int corvuscore_remote_link_state(const corvuscore_remote_link* link) {
    if (!link) return CORVUSCORE_REMOTE_UNPLUGGED;
    try {
        switch (link->link->state()) {
        case corvuscore::RemoteLink::State::unplugged: return CORVUSCORE_REMOTE_UNPLUGGED;
        case corvuscore::RemoteLink::State::waiting: return CORVUSCORE_REMOTE_WAITING;
        case corvuscore::RemoteLink::State::plugged: return CORVUSCORE_REMOTE_PLUGGED;
        case corvuscore::RemoteLink::State::incompatible: return CORVUSCORE_REMOTE_INCOMPATIBLE;
        }
    } catch (...) {
    }
    return CORVUSCORE_REMOTE_UNPLUGGED;
}
int corvuscore_remote_link_service(corvuscore_remote_link* link) {
    return attempt([&] {
        if (!link || !link->handle) throw std::invalid_argument("Remote link cable without console");
        if (!link->handle->loaded) return;
        machine_may_change(link->handle);
        link->handle->core->service_remote_link();
    });
}
int corvuscore_set_gbc_colors(corvuscore_bridge* handle, int colors) {
    if (!handle || !handle->core || (handle->system != CORVUSCORE_GBC && handle->system != CORVUSCORE_GBC_ON_GBA)) return 0;
    // Les valeurs C suivent l'énumération C++.
    static_assert(CORVUSCORE_GBC_COLORS_GRAY == static_cast<int>(corvuscore::GbcCompatibilityColors::grayscale));
    static_assert(CORVUSCORE_GBC_COLORS_UP == static_cast<int>(corvuscore::GbcCompatibilityColors::up));
    static_assert(CORVUSCORE_GBC_COLORS_B_RIGHT == static_cast<int>(corvuscore::GbcCompatibilityColors::b_right));
    if (colors < CORVUSCORE_GBC_COLORS_AUTO || colors > CORVUSCORE_GBC_COLORS_B_RIGHT) return 0;
    machine_may_change(handle);
    handle->core->set_gbc_compatibility_colors(static_cast<corvuscore::GbcCompatibilityColors>(colors));
    return 1;
}
int corvuscore_check_cheat(int system, const char* code) {
    return attempt([&] {
        const auto console = console_of(system);
        if (!console) throw std::invalid_argument("Invalid system");
        if (!code) throw std::invalid_argument("Null cheat code");
        static_cast<void>(corvuscore::parse_cheat(*console, code));
    });
}
int corvuscore_set_cheats(corvuscore_bridge* handle, const char* const* codes, size_t count) {
    return attempt([&] {
        if (!handle || !handle->core) throw std::invalid_argument("Null handle");
        if (count > 0 && !codes) throw std::invalid_argument("Null cheat list");
        std::vector<corvuscore::Cheat> cheats;
        cheats.reserve(count);
        for (size_t index = 0; index < count; ++index) {
            if (!codes[index]) throw std::invalid_argument("Null cheat code");
            try {
                cheats.push_back(corvuscore::parse_cheat(handle->core->console(), codes[index]));
            } catch (const corvuscore::CheatError& error) {
                throw std::runtime_error("code de triche " + std::to_string(index + 1) + " : " + error.what());
            }
        }
        handle->core->set_cheats(cheats);
    });
}
int corvuscore_link_wanted(const corvuscore_bridge* handle) {
    return handle && handle->core && handle->loaded && handle->core->link_wanted() ? 1 : 0;
}
} // extern "C"
