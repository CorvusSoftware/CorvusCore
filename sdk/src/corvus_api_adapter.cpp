// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only

#include <corvusapi.h>
#include <achievements.h>
#include <corvuscore/bridge.h>

#include <algorithm>
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
using Bridge = std::unique_ptr<corvuscore_bridge, decltype(&corvuscore_destroy)>;
struct Instance {
    corvus_host host{};
    std::array<Bridge, 2> bridges{Bridge(nullptr, corvuscore_destroy), Bridge(nullptr, corvuscore_destroy)};
    corvuscore_link* cable = nullptr;
    uint32_t players = 0;
    std::array<std::vector<uint32_t>, 2> pixels;
    std::vector<uint32_t> combined;
    std::array<std::array<int16_t, 4096>, 2> audio{};
    std::array<int16_t, 4096> mixed{};
    std::array<uint32_t, 3> selected{};
    std::array<uint32_t, 3> applied{};
    std::array<char, 256> error{};

    ~Instance() { corvuscore_link_destroy(cable); }
};

int reject(Instance* instance, const char* message) {
    if (instance) {
        const size_t size = std::min(std::strlen(message), instance->error.size() - 1);
        std::memcpy(instance->error.data(), message, size);
        instance->error[size] = 0;
    }
    return 0;
}

constexpr corvus_option_value palette_values[] = {
    {sizeof(corvus_option_value), "green", "Vert"},
    {sizeof(corvus_option_value), "gray", "Gris"},
    {sizeof(corvus_option_value), "amber", "Ambre"}};
constexpr corvus_option_value color_values[] = {
    {sizeof(corvus_option_value), "auto", "Automatique"},
    {sizeof(corvus_option_value), "gray", "Gris"},
    {sizeof(corvus_option_value), "up", "Brun"},
    {sizeof(corvus_option_value), "a_up", "Rouge"},
    {sizeof(corvus_option_value), "b_up", "Sépia"},
    {sizeof(corvus_option_value), "left", "Bleu"},
    {sizeof(corvus_option_value), "a_left", "Bleu violet"},
    {sizeof(corvus_option_value), "b_left", "Gris (B + Gauche)"},
    {sizeof(corvus_option_value), "down", "Pastel"},
    {sizeof(corvus_option_value), "a_down", "Jaune et rouge"},
    {sizeof(corvus_option_value), "b_down", "Jaune et brun"},
    {sizeof(corvus_option_value), "right", "Vert et rouge"},
    {sizeof(corvus_option_value), "a_right", "Vert et bleu"},
    {sizeof(corvus_option_value), "b_right", "Négatif"}};
constexpr corvus_option_value save_values[] = {
    {sizeof(corvus_option_value), "auto", "Automatique"},
    {sizeof(corvus_option_value), "none", "Aucune"},
    {sizeof(corvus_option_value), "sram", "SRAM"},
    {sizeof(corvus_option_value), "flash_64k", "Flash 64 Kio"},
    {sizeof(corvus_option_value), "flash_128k", "Flash 128 Kio"},
    {sizeof(corvus_option_value), "eeprom_512", "EEPROM 512 octets"},
    {sizeof(corvus_option_value), "eeprom_8k", "EEPROM 8 Kio"}};
constexpr corvus_option_value rtc_values[] = {
    {sizeof(corvus_option_value), "auto", "Automatique"},
    {sizeof(corvus_option_value), "off", "Désactivée"},
    {sizeof(corvus_option_value), "on", "Activée"}};
constexpr corvus_option_value audio_values[] = {
    {sizeof(corvus_option_value), "player1", "Joueur 1"},
    {sizeof(corvus_option_value), "player2", "Joueur 2"},
    {sizeof(corvus_option_value), "mix", "Les deux"}};

const auto& option_descriptors() {
    constexpr corvus_core_option audio_option{sizeof(corvus_core_option), "link.audio", "Son du câble link",
        "Console entendue pendant une partie locale à deux joueurs.", 0, 3, audio_values, 0};
    if constexpr (CORVUS_ADAPTER_SYSTEM == CORVUSCORE_GB) {
        [[maybe_unused]] static constexpr std::array options{
            corvus_core_option{sizeof(corvus_core_option), "gb.palette", "Palette Game Boy",
                "Couleurs de l’écran Game Boy.", 0, 3, palette_values, 0}, audio_option};
        return options;
    } else if constexpr (CORVUS_ADAPTER_SYSTEM == CORVUSCORE_GBC) {
        [[maybe_unused]] static constexpr std::array options{
            corvus_core_option{sizeof(corvus_core_option), "gbc.colors", "Couleurs des jeux Game Boy",
                "Palette de compatibilité GB ; sans effet sur les jeux couleur natifs.", 0, 14, color_values, 0}, audio_option};
        return options;
    } else {
        [[maybe_unused]] static constexpr std::array options{
            corvus_core_option{sizeof(corvus_core_option), "gba.save_type", "Type de sauvegarde",
                "Remplace la détection de la cartouche au prochain chargement ou reset.",
                CORVUS_OPTION_RESTART_REQUIRED, 7, save_values, 0},
            corvus_core_option{sizeof(corvus_core_option), "gba.rtc", "Horloge de cartouche",
                "Remplace la détection de l’horloge au prochain chargement ou reset.",
                CORVUS_OPTION_RESTART_REQUIRED, 3, rtc_values, 0}, audio_option};
        return options;
    }
}

bool apply_settings(corvuscore_bridge* bridge, const std::array<uint32_t, 3>& selected) {
    if constexpr (CORVUS_ADAPTER_SYSTEM == CORVUSCORE_GBC)
        return corvuscore_set_gbc_colors(bridge, static_cast<int>(selected[0])) != 0;
    if constexpr (CORVUS_ADAPTER_SYSTEM == CORVUSCORE_GBA)
        return corvuscore_set_gba_save_type(bridge, static_cast<int>(selected[0])) &&
               corvuscore_set_gba_rtc(bridge, static_cast<int>(selected[1]));
    return true;
}

uint32_t option_count(void* core) { return core ? static_cast<uint32_t>(option_descriptors().size()) : 0; }
int describe_option(void* core, uint32_t index, corvus_core_option* out) {
    const auto& options = option_descriptors();
    if (!core || !out || out->struct_size < sizeof(*out) || index >= options.size()) return 0;
    *out = options[index];
    return 1;
}
uint32_t get_option(void* core, const char* key) {
    const auto* instance = static_cast<Instance*>(core);
    if (!instance || !key) return UINT32_MAX;
    const auto& options = option_descriptors();
    for (size_t index = 0; index < options.size(); ++index)
        if (std::strcmp(key, options[index].key) == 0) return instance->selected[index];
    return UINT32_MAX;
}
int set_option(void* core, const char* key, uint32_t value) {
    auto* instance = static_cast<Instance*>(core);
    if (!instance || !key) return 0;
    const auto& options = option_descriptors();
    for (size_t index = 0; index < options.size(); ++index) {
        if (std::strcmp(key, options[index].key) != 0) continue;
        if (value >= options[index].value_count) return reject(instance, "Valeur d’option inconnue");
        // All accepted SDK values are validated above; setters cannot fail for these live handles.
        if constexpr (CORVUS_ADAPTER_SYSTEM == CORVUSCORE_GBC) {
            for (uint32_t player = 0; player < instance->players; ++player)
                if (index == 0 && !corvuscore_set_gbc_colors(instance->bridges[player].get(), static_cast<int>(value))) return 0;
        }
        instance->selected[index] = value;
        if (!(options[index].flags & CORVUS_OPTION_RESTART_REQUIRED))
            instance->applied[index] = value;
        instance->error[0] = 0;
        return 1;
    }
    return reject(instance, "Option inconnue");
}

constexpr const char* ids[] = {"corvus.gb", "corvus.gbc", "corvus.gba"};
constexpr const char* names[] = {"CorvusGB", "CorvusGBC", "CorvusGBA"};
constexpr const char* systems[] = {"Game Boy", "Game Boy Color", "Game Boy Advance"};
constexpr const char* extensions[] = {"gb", "gb|gbc", "gba"};

int get_info(corvus_core_info* out) {
    if (!out || out->struct_size < sizeof(corvus_core_info)) return 0;
    *out = {sizeof(corvus_core_info), ids[CORVUS_ADAPTER_SYSTEM],
            names[CORVUS_ADAPTER_SYSTEM], corvuscore_version(),
            systems[CORVUS_ADAPTER_SYSTEM], extensions[CORVUS_ADAPTER_SYSTEM], 2};
    return 1;
}

void* create(const corvus_host* host) {
    if (!host || host->struct_size < sizeof(corvus_host) || !host->video_frame) return nullptr;
    auto instance = std::unique_ptr<Instance>(new (std::nothrow) Instance());
    if (!instance) return nullptr;
    instance->host = *host;
    return instance.release();
}

void destroy(void* core) { delete static_cast<Instance*>(core); }

int load_games(Instance* instance, const corvus_game* games, size_t count) {
    if (!instance || !games || (count != 1 && count != 2) || instance->players)
        return reject(instance, "Le chargement nécessite une instance sans jeu");
    instance->error[0] = 0;
    try {
        std::array<Bridge, 2> bridges{Bridge(nullptr, corvuscore_destroy), Bridge(nullptr, corvuscore_destroy)};
        std::array<std::vector<uint32_t>, 2> pixels;
        for (size_t player = 0; player < count; ++player) {
            const auto& game = games[player];
            if (game.struct_size < sizeof(game) || !game.rom_data || !game.rom_size ||
                (!game.battery_data && game.battery_size)) return reject(instance, "Données de jeu invalides");
            bridges[player].reset(corvuscore_create(CORVUS_ADAPTER_SYSTEM));
            if (!bridges[player] || !apply_settings(bridges[player].get(), instance->selected) ||
                !corvuscore_load(bridges[player].get(), game.rom_data, game.rom_size,
                                 game.battery_data, game.battery_size)) return 0;
            const int width = corvuscore_video_width(bridges[player].get());
            const int height = corvuscore_video_height(bridges[player].get());
            if (width <= 0 || height <= 0 || width > 1024 || height > 1024) return 0;
            pixels[player].resize(static_cast<size_t>(width) * static_cast<size_t>(height));
        }
        std::vector<uint32_t> combined(pixels[0].size() * count);
        corvuscore_bridge* pair[] = {bridges[0].get(), bridges[1].get()};
        auto* cable = count == 2 ? corvuscore_link_create(pair, 2) : nullptr;
        if (count == 2 && !cable) return 0;
        instance->bridges = std::move(bridges);
        instance->pixels = std::move(pixels);
        instance->combined = std::move(combined);
        instance->cable = cable;
        instance->players = static_cast<uint32_t>(count);
        instance->applied = instance->selected;
        return 1;
    } catch (...) { return reject(instance, "Impossible d’allouer la session"); }
}

int load_game(void* core, const corvus_game* game) {
    return load_games(static_cast<Instance*>(core), game, 1);
}

void unload_game(void* core) {
    auto* instance = static_cast<Instance*>(core);
    if (!instance) return;
    corvuscore_link_destroy(instance->cable);
    instance->cable = nullptr;
    for (auto& bridge : instance->bridges) bridge.reset();
    for (auto& pixels : instance->pixels) pixels.clear();
    instance->players = 0;
    instance->combined.clear();
    instance->error[0] = 0;
}

int get_av_info(void* core, corvus_av_info* out) {
    auto* instance = static_cast<Instance*>(core);
    if (!instance || !instance->players || !out ||
        out->struct_size < sizeof(corvus_av_info)) return 0;
    const auto* bridge = instance->bridges[0].get();
    *out = {sizeof(corvus_av_info),
            static_cast<uint32_t>(corvuscore_video_width(bridge)) * instance->players,
            static_cast<uint32_t>(corvuscore_video_height(bridge)),
            corvuscore_video_fps(bridge),
            static_cast<uint32_t>(corvuscore_audio_rate(bridge))};
    return 1;
}

void set_button(void* core, uint32_t player, uint32_t button, int pressed) {
    auto* instance = static_cast<Instance*>(core);
    if (!instance || !instance->players || player >= instance->players) return;
    constexpr std::array<int, 16> mapping = {0, 1, 2, 3, 4, 5, -1, -1,
                                               6, 7, 8, 9, -1, -1, -1, -1};
    if (button < mapping.size() && mapping[button] >= 0)
        corvuscore_set_button(instance->bridges[player].get(), mapping[button], pressed != 0);
}

void apply_palette(Instance& instance, uint32_t player) {
    if constexpr (CORVUS_ADAPTER_SYSTEM != CORVUSCORE_GB) return;
    if (instance.selected[0] == 0) return;
    constexpr std::array<uint32_t, 4> original{0xffe0f8d0, 0xff88c070, 0xff346856, 0xff081820};
    constexpr std::array<uint32_t, 4> gray{0xffffffff, 0xffaaaaaa, 0xff555555, 0xff000000};
    constexpr std::array<uint32_t, 4> amber{0xffffe0a0, 0xffd0a050, 0xff805020, 0xff302010};
    const auto& target = instance.selected[0] == 1 ? gray : amber;
    for (auto& pixel : instance.pixels[player])
        for (size_t shade = 0; shade < original.size(); ++shade)
            if (pixel == original[shade]) { pixel = target[shade]; break; }
}

int run_frame(void* core) {
    auto* instance = static_cast<Instance*>(core);
    if (!instance || !instance->players) return 0;
    instance->error[0] = 0;
    uint32_t* frames[] = {instance->pixels[0].data(), instance->pixels[1].data()};
    if (instance->players == 2) {
        if (!corvuscore_link_run_frame(instance->cable, frames, instance->pixels[0].size())) return 0;
    } else if (!corvuscore_run_frame(instance->bridges[0].get(), frames[0], instance->pixels[0].size())) return 0;
    const auto width = static_cast<uint32_t>(corvuscore_video_width(instance->bridges[0].get()));
    const auto height = static_cast<uint32_t>(corvuscore_video_height(instance->bridges[0].get()));
    for (uint32_t player = 0; player < instance->players; ++player) {
        apply_palette(*instance, player);
        for (uint32_t row = 0; row < height; ++row)
            std::copy_n(instance->pixels[player].data() + static_cast<size_t>(row) * width, width,
                instance->combined.data() + static_cast<size_t>(row) * width * instance->players + player * width);
    }
    const auto output_width = width * instance->players;
    instance->host.video_frame(instance->host.userdata, instance->combined.data(), output_width, height, output_width);
    std::array<size_t, 2> count{};
    for (uint32_t player = 0; player < instance->players; ++player) {
        count[player] = corvuscore_read_audio(instance->bridges[player].get(), instance->audio[player].data(),
                                             instance->audio[player].size());
        if (count[player] > instance->audio[player].size() || count[player] % 2 != 0) return 0;
    }
    const uint32_t selection = instance->selected[option_descriptors().size() - 1];
    const int16_t* output = instance->audio[0].data();
    size_t samples = count[0];
    if (instance->players == 2 && selection == 1) {
        output = instance->audio[1].data(); samples = count[1];
    } else if (instance->players == 2 && selection == 2) {
        samples = std::max(count[0], count[1]);
        for (size_t sample = 0; sample < samples; ++sample) {
            const int32_t first = sample < count[0] ? instance->audio[0][sample] : 0;
            const int32_t second = sample < count[1] ? instance->audio[1][sample] : 0;
            instance->mixed[sample] = static_cast<int16_t>(std::clamp((first + second) / 2, int32_t{-32768}, int32_t{32767}));
        }
        output = instance->mixed.data();
    }
    if (samples && instance->host.audio_frames)
        instance->host.audio_frames(instance->host.userdata, output, samples / 2);
    return 1;
}

int reset(void* core) {
    auto* instance = static_cast<Instance*>(core);
    if (!instance || !instance->players) return 0;
    instance->error[0] = 0;
    corvuscore_bridge* handles[] = {instance->bridges[0].get(), instance->bridges[1].get()};
    bool configured = true;
    for (uint32_t player = 0; player < instance->players; ++player)
        configured = apply_settings(handles[player], instance->selected) && configured;
    if (!configured || !corvuscore_reset_group(handles, instance->players)) {
        // Restoring these validated scalar fields allocates nothing. Keep the
        // failure message before SDK setters clear their thread-local error.
        reject(instance, corvuscore_last_error());
        for (uint32_t player = 0; player < instance->players; ++player)
            static_cast<void>(apply_settings(handles[player], instance->applied));
        return 0;
    }
    instance->applied = instance->selected;
    return 1;
}

const char* last_error(void* core) {
    auto* instance = static_cast<Instance*>(core);
    return instance && instance->error[0] ? instance->error.data() : corvuscore_last_error();
}

size_t serialize_size(void* core) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->players == 1) ? corvuscore_state_size(instance->bridges[0].get()) : 0;
}

int serialize(void* core, uint8_t* data, size_t capacity) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->players == 1)
               ? corvuscore_save_state(instance->bridges[0].get(), data, capacity)
               : reject(instance, "Les états groupés du câble link ne sont pas pris en charge");
}

int unserialize(void* core, const uint8_t* data, size_t size) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->players == 1)
               ? corvuscore_load_state(instance->bridges[0].get(), data, size)
               : reject(instance, "Les états groupés du câble link ne sont pas pris en charge");
}

size_t battery_size(void* core) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->players == 1) ? corvuscore_battery_size(instance->bridges[0].get()) : 0;
}

int battery_dirty(void* core) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->players == 1) ? corvuscore_battery_dirty(instance->bridges[0].get()) : 0;
}

int battery_copy(void* core, uint8_t* data, size_t capacity, uint64_t* generation) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->players == 1)
               ? corvuscore_copy_battery_generation(instance->bridges[0].get(), data, capacity, generation) : 0;
}

int battery_acknowledge(void* core, uint64_t generation) {
    auto* instance = static_cast<Instance*>(core);
    return (instance && instance->players == 1)
               ? corvuscore_acknowledge_battery(instance->bridges[0].get(), generation) : 0;
}
size_t read_achievement_memory(void* core, uint32_t address, uint8_t* output,
                                size_t capacity) {
    const auto* instance = static_cast<const Instance*>(core);
    if (!instance || instance->players != 1 || !output || !capacity) return 0;
    const uint64_t remaining = (uint64_t{1} << 32) - address;
    if (static_cast<uint64_t>(capacity) > remaining)
        capacity = static_cast<size_t>(remaining);
    return corvuscore_read_achievement_memory(instance->bridges[0].get(), address, output, capacity);
}

uint32_t subsystem_count() { return 1; }
int describe_subsystem(uint32_t index, corvus_subsystem_info* out) {
    if (index != 0 || !out || out->struct_size < sizeof(*out)) return 0;
    static const corvus_subsystem_rom games[] = {
        {sizeof(corvus_subsystem_rom), "Joueur 1", extensions[CORVUS_ADAPTER_SYSTEM], 1},
        {sizeof(corvus_subsystem_rom), "Joueur 2", extensions[CORVUS_ADAPTER_SYSTEM], 1}};
    *out = {sizeof(*out), "corvus.link.two_player", "Deux joueurs (câble link)", 2, 2, games};
    return 1;
}
int load_subsystem(void* core, const char* id, const corvus_game* games, size_t count) {
    auto* instance = static_cast<Instance*>(core);
    if (!id || std::strcmp(id, "corvus.link.two_player") != 0 || count != 2)
        return reject(instance, "Sous-système ou nombre de jeux inconnu");
    return load_games(instance, games, count);
}
uint32_t session_capabilities(void* core) {
    const auto* instance = static_cast<const Instance*>(core);
    if (!instance || !instance->players) return 0;
    return CORVUS_SESSION_RESET | (instance->players == 1 ? CORVUS_SESSION_STATES : 0u);
}
corvuscore_bridge* slot_bridge(void* core, uint32_t slot) {
    auto* instance = static_cast<Instance*>(core);
    return instance && slot < instance->players ? instance->bridges[slot].get() : nullptr;
}
size_t slot_battery_size(void* core, uint32_t slot) {
    auto* bridge = slot_bridge(core, slot);
    return bridge ? corvuscore_battery_size(bridge) : 0;
}
int slot_battery_dirty(void* core, uint32_t slot) {
    return corvuscore_battery_dirty(slot_bridge(core, slot));
}
int slot_battery_copy(void* core, uint32_t slot, uint8_t* output, size_t capacity, uint64_t* generation) {
    auto* bridge = slot_bridge(core, slot);
    return bridge && generation ? corvuscore_copy_battery_generation(bridge, output, capacity, generation) : 0;
}
int slot_battery_acknowledge(void* core, uint32_t slot, uint64_t generation) {
    auto* bridge = slot_bridge(core, slot);
    return bridge ? corvuscore_acknowledge_battery(bridge, generation) : 0;
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

extern "C" CORVUS_API_EXPORT int corvus_get_options_api(uint32_t requested_version, corvus_options_api* out) {
    if (!out || requested_version != CORVUS_OPTIONS_API_VERSION || out->struct_size < sizeof(*out)) return 0;
    *out = {sizeof(*out), CORVUS_OPTIONS_API_VERSION, option_count, describe_option, get_option, set_option};
    return 1;
}

extern "C" CORVUS_API_EXPORT int corvus_get_subsystem_api(uint32_t requested_version, corvus_subsystem_api* out) {
    if (!out || requested_version != CORVUS_SUBSYSTEM_API_VERSION || out->struct_size < sizeof(*out)) return 0;
    *out = {sizeof(*out), CORVUS_SUBSYSTEM_API_VERSION, subsystem_count, describe_subsystem, load_subsystem,
            session_capabilities, slot_battery_size, slot_battery_dirty, slot_battery_copy, slot_battery_acknowledge};
    return 1;
}
