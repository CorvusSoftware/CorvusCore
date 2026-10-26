// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only

#include <corvus/api.h>
#include <corvus/achievements.h>
#include <corvuscore/bridge.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#ifndef CORVUS_ADAPTER_SYSTEM
#error "CORVUS_ADAPTER_SYSTEM must be set to a system identifier"
#endif

namespace {
struct Instance {
    corvus_host host{};
    corvuscore_bridge* bridge = nullptr;
    bool loaded = false;
    std::vector<uint32_t> pixels;
    std::array<int16_t, 4096> audio{};

    ~Instance() { corvuscore_destroy(bridge); }
};

constexpr const char* ids[] = {"corvus.gb", "corvus.gbc", "corvus.gba"};
constexpr const char* names[] = {"CorvusGB", "CorvusGBC", "CorvusGBA"};
constexpr const char* systems[] = {"Game Boy", "Game Boy Color", "Game Boy Advance"};
constexpr const char* extensions[] = {"gb", "gb|gbc", "gba"};

int get_info(corvus_core_info* out) {
    if (!out || out->struct_size < sizeof(corvus_core_info)) return 0;
    *out = {sizeof(corvus_core_info), ids[CORVUS_ADAPTER_SYSTEM],
            names[CORVUS_ADAPTER_SYSTEM], corvuscore_version(),
            systems[CORVUS_ADAPTER_SYSTEM], extensions[CORVUS_ADAPTER_SYSTEM], 1};
    return 1;
}

void* create(const corvus_host* host) {
    if (!host || host->struct_size < sizeof(corvus_host) || !host->video_frame) return nullptr;
    auto instance = std::unique_ptr<Instance>(new (std::nothrow) Instance());
    if (!instance) return nullptr;
    instance->host = *host;
    instance->bridge = corvuscore_create(CORVUS_ADAPTER_SYSTEM);
    return instance->bridge ? instance.release() : nullptr;
}

void destroy(void* core) { delete static_cast<Instance*>(core); }

int load_game(void* core, const corvus_game* game) {
    auto* instance = static_cast<Instance*>(core);
    if (!instance || !game || game->struct_size < sizeof(corvus_game) ||
        !game->rom_data || !game->rom_size ||
        (!game->battery_data && game->battery_size) || instance->loaded) return 0;
    if (!instance->bridge)
        instance->bridge = corvuscore_create(CORVUS_ADAPTER_SYSTEM);
    if (!instance->bridge ||
        !corvuscore_load(instance->bridge, game->rom_data, game->rom_size,
                         game->battery_data, game->battery_size)) return 0;
    const int width = corvuscore_video_width(instance->bridge);
    const int height = corvuscore_video_height(instance->bridge);
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) return 0;
    try {
        instance->pixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
    } catch (...) {
        return 0;
    }
    instance->loaded = true;
    return 1;
}

void unload_game(void* core) {
    auto* instance = static_cast<Instance*>(core);
    if (!instance) return;
    corvuscore_destroy(instance->bridge);
    instance->bridge = nullptr;
    instance->loaded = false;
    instance->pixels.clear();
}

int get_av_info(void* core, corvus_av_info* out) {
    auto* instance = static_cast<Instance*>(core);
    if (!instance || !instance->loaded || !out ||
        out->struct_size < sizeof(corvus_av_info)) return 0;
    const auto* bridge = instance->bridge;
    *out = {sizeof(corvus_av_info),
            static_cast<uint32_t>(corvuscore_video_width(bridge)),
            static_cast<uint32_t>(corvuscore_video_height(bridge)),
            corvuscore_video_fps(bridge),
            static_cast<uint32_t>(corvuscore_audio_rate(bridge))};
    return 1;
}

void set_button(void* core, uint32_t player, uint32_t button, int pressed) {
    auto* instance = static_cast<Instance*>(core);
    if (!instance || !instance->loaded || player != 0) return;
    constexpr std::array<int, 16> mapping = {0, 1, 2, 3, 4, 5, -1, -1,
                                               6, 7, 8, 9, -1, -1, -1, -1};
    if (button < mapping.size() && mapping[button] >= 0)
        corvuscore_set_button(instance->bridge, mapping[button], pressed != 0);
}

int run_frame(void* core) {
    auto* instance = static_cast<Instance*>(core);
    if (!instance || !instance->loaded || instance->pixels.empty()) return 0;
    if (!corvuscore_run_frame(instance->bridge, instance->pixels.data(),
                               instance->pixels.size())) return 0;
    const auto width = static_cast<uint32_t>(corvuscore_video_width(instance->bridge));
    const auto height = static_cast<uint32_t>(corvuscore_video_height(instance->bridge));
    instance->host.video_frame(instance->host.userdata, instance->pixels.data(), width, height, width);
    const size_t samples = corvuscore_read_audio(instance->bridge, instance->audio.data(),
                                                  instance->audio.size());
    if (samples > instance->audio.size() || samples % 2 != 0) return 0;
    if (samples && instance->host.audio_frames)
        instance->host.audio_frames(instance->host.userdata, instance->audio.data(), samples / 2);
    return 1;
}

int reset(void* core) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->loaded) ? corvuscore_reset(instance->bridge) : 0;
}

const char* last_error(void*) { return corvuscore_last_error(); }

size_t serialize_size(void* core) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->loaded) ? corvuscore_state_size(instance->bridge) : 0;
}

int serialize(void* core, uint8_t* data, size_t capacity) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->loaded)
               ? corvuscore_save_state(instance->bridge, data, capacity) : 0;
}

int unserialize(void* core, const uint8_t* data, size_t size) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->loaded)
               ? corvuscore_load_state(instance->bridge, data, size) : 0;
}

size_t battery_size(void* core) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->loaded) ? corvuscore_battery_size(instance->bridge) : 0;
}

int battery_dirty(void* core) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->loaded) ? corvuscore_battery_dirty(instance->bridge) : 0;
}

int battery_copy(void* core, uint8_t* data, size_t capacity, uint64_t* generation) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->loaded)
               ? corvuscore_copy_battery_generation(instance->bridge, data, capacity, generation) : 0;
}

int battery_acknowledge(void* core, uint64_t generation) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->loaded)
               ? corvuscore_acknowledge_battery(instance->bridge, generation) : 0;
}
size_t read_achievement_memory(void* core, uint32_t address, uint8_t* output,
                                size_t capacity) {
    const auto* instance = static_cast<const Instance*>(core);
    if (!instance || !instance->loaded || !output || !capacity) return 0;
    const uint64_t remaining = (uint64_t{1} << 32) - address;
    if (static_cast<uint64_t>(capacity) > remaining)
        capacity = static_cast<size_t>(remaining);
    return corvuscore_read_achievement_memory(instance->bridge, address, output, capacity);
}

}  // namespace

extern "C" CORVUS_API_EXPORT int corvus_get_api(uint32_t requested_version,
                                                   corvus_core_api* out) {
    if (!out || requested_version != CORVUS_API_VERSION ||
        out->struct_size < sizeof(corvus_core_api)) return 0;
    *out = {sizeof(corvus_core_api), CORVUS_API_VERSION, get_info, create, destroy,
            load_game, unload_game, get_av_info, set_button, run_frame,
            reset, last_error, serialize_size, serialize, unserialize,
            battery_size, battery_dirty, battery_copy, battery_acknowledge};
    return 1;
}

extern "C" CORVUS_API_EXPORT int corvus_get_achievements_api(
    uint32_t requested_version, corvus_achievements_api* out) {
    if (!out || requested_version != CORVUS_ACHIEVEMENTS_API_VERSION ||
        out->struct_size < sizeof(corvus_achievements_api)) return 0;
    constexpr uint32_t consoles[] = {CORVUS_ACHIEVEMENT_GAME_BOY,
                                     CORVUS_ACHIEVEMENT_GAME_BOY_COLOR,
                                     CORVUS_ACHIEVEMENT_GAME_BOY_ADVANCE};
    *out = {sizeof(corvus_achievements_api), CORVUS_ACHIEVEMENTS_API_VERSION,
            consoles[CORVUS_ADAPTER_SYSTEM], read_achievement_memory};
    return 1;
}
