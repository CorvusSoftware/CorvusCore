// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only
#include <corvusapi.h>
#include <achievements.h>
#include <synthetic_roms.hpp>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vector>

namespace {
void check(bool value, const char *message) {
    if (!value) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(1);
    }
}
struct Output {
    unsigned frames = 0;
    uint32_t width = 0, height = 0;
    std::vector<uint32_t> pixels;
    std::vector<int16_t> audio;
};
void video(void *userdata, const uint32_t *pixels, uint32_t width, uint32_t height, size_t stride) {
    auto &out = *static_cast<Output *>(userdata);
    check(stride == width, "packed composite stride");
    ++out.frames;
    out.width = width;
    out.height = height;
    out.pixels.assign(pixels, pixels + static_cast<size_t>(width) * height);
}
void audio(void *userdata, const int16_t *samples, size_t frames) {
    check(frames > 0 && frames <= 2048, "bounded stereo frame count");
    static_cast<Output *>(userdata)->audio.assign(samples, samples + frames * 2);
}
corvus_game game(const std::vector<uint8_t> &rom) {
    corvus_game result{};
    result.struct_size = sizeof(result);
    result.rom_data = rom.data();
    result.rom_size = rom.size();
    return result;
}
std::vector<uint8_t> gb_link_rom(uint8_t value, bool master) {
    auto rom = corvuscore::testing::minimal_game_boy_rom();
    rom[0x101] = 0x50;
    rom[0x147] = 3;
    rom[0x149] = 2;
    // Distinct square tones exercise real stereo routing and mixing.
    std::vector<uint8_t> program{0x3e,  static_cast<uint8_t>(master ? 0xff : 0),
                                 0xe0,  0x47,
                                 0x3e,  0x80,
                                 0xe0,  0x26,
                                 0x3e,  0x77,
                                 0xe0,  0x24,
                                 0x3e,  0x11,
                                 0xe0,  0x25,
                                 0x3e,  0x80,
                                 0xe0,  0x11,
                                 0x3e,  0xf0,
                                 0xe0,  0x12,
                                 0x3e,  value,
                                 0xe0,  0x13,
                                 0x3e,  0x87,
                                 0xe0,  0x14,
                                 0x3e,  0x0a,
                                 0xea,  0,
                                 0,     0x3e,
                                 value, 0xe0,
                                 1};
    // Let the slave reach SC before the master's first divider edge.
    if (master)
        program.insert(program.end(), {0x06, 0x40, 0x05, 0x20, 0xfd});
    const uint8_t transfer[]{0x3e, static_cast<uint8_t>(master ? 0x81 : 0x80),
                             0xe0, 2,
                             0xf0, 2,
                             0xe6, 0x80,
                             0x20, 0xfa,
                             0xf0, 1,
                             0xea, 0,
                             0xa0, 0x3e,
                             0x10, 0xe0,
                             0,    0xf0,
                             0,    0xe6,
                             0x0f, 0xea,
                             1,    0xa0,
                             0x18, 0xf3};
    program.insert(program.end(), std::begin(transfer), std::end(transfer));
    std::memcpy(rom.data() + 0x150, program.data(), program.size());
    return rom;
}

std::vector<uint8_t> gba_link_rom() {
    // Original SDK multiplayer fixture, persisting low bytes directly to SRAM.
    const uint32_t code[]{0xe59f208c, 0xe3a01000, 0xe1c211b4, 0xe3a01a02, 0xe3811003, 0xe1c210b8, 0xe1d230b8,
                          0xe3130004, 0x03a040aa, 0x13a040bb, 0xe1c240ba, 0x1a000008, 0xe1d230b8, 0xe3130008,
                          0x0afffffc, 0xe3833080, 0xe1c230b8, 0xe1d230b8, 0xe3130080, 0x1afffffc, 0xea000005,
                          0xe1d230b8, 0xe3130080, 0x0afffffc, 0xe1d230b8, 0xe3130080, 0x1afffffc, 0xe3a0540e,
                          0xe1d260b0, 0xe1c560b0, 0xe1d260b2, 0xe1c560b2, 0xe1d260b8, 0xe1c560b4, 0xe1d260b4,
                          0xe1c560b6, 0xeafffffe, 0x04000120};
    auto rom = corvuscore::testing::minimal_gba_rom();
    for (size_t word = 0; word < std::size(code); ++word)
        for (unsigned byte = 0; byte < 4; ++byte)
            rom[word * 4 + byte] = static_cast<uint8_t>(code[word] >> (byte * 8));
    std::memcpy(rom.data() + 0x100, "SRAM_V", 6);
    return rom;
}
struct Module {
    void *library;
    corvus_core_api base{};
    corvus_options_api options{};
    corvus_subsystem_api subsystem{};
    corvus_achievements_api memory{};
    explicit Module(const char *path) : library(dlopen(path, RTLD_NOW | RTLD_LOCAL)) {
        check(library, "module opens");
        auto get_base = reinterpret_cast<corvus_get_api_fn>(dlsym(library, CORVUS_CORE_ENTRYPOINT));
        auto get_options =
            reinterpret_cast<corvus_get_options_api_fn>(dlsym(library, CORVUS_OPTIONS_ENTRYPOINT));
        auto get_subsystem =
            reinterpret_cast<corvus_get_subsystem_api_fn>(dlsym(library, CORVUS_SUBSYSTEM_ENTRYPOINT));
        auto get_memory =
            reinterpret_cast<corvus_get_achievements_api_fn>(dlsym(library, CORVUS_ACHIEVEMENTS_ENTRYPOINT));
        check(get_base && get_options && get_subsystem && get_memory, "all extension symbols exported");
        base.struct_size = sizeof(base);
        options.struct_size = sizeof(options);
        subsystem.struct_size = sizeof(subsystem);
        memory.struct_size = sizeof(memory);
        check(!get_options(2, &options) && !get_options(1, nullptr) && !get_subsystem(2, &subsystem) &&
                  !get_subsystem(1, nullptr),
              "unsupported negotiations rejected");
        options.struct_size = sizeof(options) - 1;
        options.api_version = 0xfeed;
        subsystem.struct_size = sizeof(subsystem) - 1;
        subsystem.api_version = 0xfeed;
        check(!get_options(1, &options) && options.api_version == 0xfeed && !get_subsystem(1, &subsystem) &&
                  subsystem.api_version == 0xfeed,
              "small negotiation preserves output");
        options.struct_size = sizeof(options);
        subsystem.struct_size = sizeof(subsystem);
        check(get_base(0, &base) && get_options(1, &options) && get_subsystem(1, &subsystem) &&
                  get_memory(1, &memory),
              "extensions negotiated");
    }
    ~Module() { dlclose(library); }
};
std::vector<uint8_t> battery(Module &module, void *core, uint32_t slot, uint64_t &generation) {
    std::vector<uint8_t> bytes(module.subsystem.battery_size(core, slot));
    check(!bytes.empty() &&
              module.subsystem.battery_copy(core, slot, bytes.data(), bytes.size(), &generation),
          "per-slot battery copied");
    return bytes;
}
void module_test(const char *path, unsigned system) {
    Module m(path);
    Output out;
    corvus_host host{sizeof(host), &out, video, audio, nullptr};
    void *core = m.base.create(&host);
    check(core, "base instance created");
    check(m.options.count(core) == (system == 2 ? 3u : 2u) && !m.options.count(nullptr), "real option count");
    for (uint32_t index = 0; index < m.options.count(core); ++index) {
        corvus_core_option option{};
        option.struct_size = sizeof(option);
        check(m.options.describe(core, index, &option) && option.value_count &&
                  option.default_index < option.value_count,
              "option metadata");
        check(m.options.get_value(core, option.key) == option.default_index, "default selected");
        check(!m.options.set_value(core, option.key, option.value_count) &&
                  m.options.get_value(core, option.key) == option.default_index,
              "invalid index atomic");
        for (uint32_t value = 0; value < option.value_count; ++value)
            check(m.options.set_value(core, option.key, value) &&
                      m.options.get_value(core, option.key) == value,
                  "all advertised choices accepted");
        check(m.options.set_value(core, option.key, option.default_index), "restore option default");
        option.struct_size = sizeof(option) - 1;
        option.key = "sentinel";
        check(!m.options.describe(core, index, &option) && std::strcmp(option.key, "sentinel") == 0,
              "small descriptor unchanged");
    }
    check(m.options.get_value(core, "missing") == UINT32_MAX && !m.options.set_value(core, "missing", 0),
          "unknown options rejected");
    corvus_subsystem_info subsystem{};
    subsystem.struct_size = sizeof(subsystem);
    check(m.subsystem.count() == 1 && m.subsystem.describe(0, &subsystem) && subsystem.num_players == 2 &&
              subsystem.game_count == 2,
          "two-player descriptor");
    check(!m.subsystem.describe(1, &subsystem) && !m.subsystem.capabilities(core), "unloaded capability");
    auto rom =
        system == 2 ? corvuscore::testing::minimal_gba_rom() : corvuscore::testing::minimal_game_boy_rom();
    auto solo = game(rom);
    const char *key = system == 0 ? "gb.palette" : system == 1 ? "gbc.colors" : "gba.save_type";
    check(m.options.set_value(core, key, system == 2 ? 4u : 1u), "select useful option before load");
    check(m.base.load_game(core, &solo), "solo loads");
    check(m.subsystem.capabilities(core) == (CORVUS_SESSION_RESET | CORVUS_SESSION_STATES),
          "solo capabilities");
    check(m.base.run_frame(core) && !out.audio.empty(), "solo AV");
    if (system == 0) {
        check(out.pixels[0] == 0xffffffff, "GB grayscale changes actual video");
        check(m.options.set_value(core, key, 0) && m.base.run_frame(core) && out.pixels[0] == 0xffe0f8d0,
              "GB default green image preserved");
        check(m.options.set_value(core, key, 2) && m.base.run_frame(core) && out.pixels[0] == 0xffffe0a0,
              "GB amber applies immediately");
        check(m.options.set_value(core, key, 1), "restore gray selection");
    }
    if (system == 1)
        check(out.pixels[0] == 0xffffffff, "GBC grayscale changes actual video");
    if (system == 2) {
        check(m.base.battery_size(core) == 128 * 1024, "forced GBA flash applied");
        check(m.options.set_value(core, key, 2) && m.base.battery_size(core) == 128 * 1024,
              "restart selection pending");
        check(m.base.reset(core) && m.base.battery_size(core) == 32 * 1024, "reset applies SRAM selection");
        const auto state_without_rtc = m.base.serialize_size(core);
        check(m.options.set_value(core, "gba.rtc", 2) && m.base.serialize_size(core) == state_without_rtc &&
                  m.base.reset(core) && m.base.serialize_size(core) > state_without_rtc,
              "RTC applies on reset");
        check(m.options.set_value(core, "gba.rtc", 0), "restore automatic RTC");
    }
    std::vector<uint8_t> state(m.base.serialize_size(core));
    check(!state.empty() && m.base.serialize(core, state.data(), state.size()) &&
              m.base.unserialize(core, state.data(), state.size()),
          "solo states unchanged");
    m.base.unload_game(core);
    m.base.unload_game(core);
    check(m.options.get_value(core, key) == (system == 2 ? 2u : 1u) && m.base.load_game(core, &solo),
          "selection survives unload/reload");
    m.base.unload_game(core);
    auto first = system == 2 ? gba_link_rom() : gb_link_rom(0xaa, true);
    auto second = system == 2 ? gba_link_rom() : gb_link_rom(0xbb, false);
    std::array<corvus_game, 2> games{game(first), game(second)};
    check(!m.subsystem.load(core, "missing", games.data(), 2) &&
              !m.subsystem.load(core, subsystem.id, games.data(), 1),
          "unknown subsystem and content count rejected");
    auto invalid = games;
    invalid[1].rom_size = 1;
    const unsigned before = out.frames;
    check(!m.subsystem.load(core, subsystem.id, invalid.data(), invalid.size()) &&
              !m.subsystem.capabilities(core) && out.frames == before,
          "failed second load rolls back without callback");
    check(m.subsystem.load(core, subsystem.id, games.data(), games.size()), "two-player load");
    check(!m.subsystem.load(core, subsystem.id, games.data(), games.size()) && !m.base.load_game(core, &solo),
          "already loaded session unchanged");
    corvus_av_info av{};
    av.struct_size = sizeof(av);
    check(m.base.get_av_info(core, &av) && av.width == (system == 2 ? 480u : 320u), "combined AV width");
    check(m.subsystem.capabilities(core) == CORVUS_SESSION_RESET && !m.base.serialize_size(core) &&
              !m.base.serialize(core, state.data(), state.size()) &&
              !m.base.unserialize(core, state.data(), state.size()),
          "paired states unavailable");
    check(!m.base.battery_size(core) && !m.base.battery_dirty(core) &&
              !m.base.battery_copy(core, state.data(), state.size(), nullptr) &&
              !m.base.battery_acknowledge(core, 0),
          "base paired battery unavailable");
    uint8_t untouched = 0x5a;
    uint64_t untouched_generation = 0x1234;
    check(!m.subsystem.battery_copy(core, 2, &untouched, 1, &untouched_generation) && untouched == 0x5a &&
              untouched_generation == 0x1234,
          "invalid battery slot unchanged");
    check(!m.memory.read_memory(core, 0, &untouched, 1), "linked achievements unavailable");
    for (int frame = 0; frame < 3; ++frame)
        check(m.base.run_frame(core), "linked frame");
    check(out.width == av.width && out.height == av.height && !out.audio.empty(),
          "paired video/audio delivered");
    if (system != 2) {
        check(out.pixels[0] != out.pixels[out.width / 2], "left and right consoles show distinct video");
        check(out.pixels[out.width - 1] == out.pixels[out.width / 2],
              "right screen occupies the complete row");
    }
    std::array<uint64_t, 2> generations{};
    for (uint32_t slot = 0; slot < 2; ++slot) {
        auto bytes = battery(m, core, slot, generations[slot]);
        const auto original = bytes;
        check(!m.subsystem.battery_copy(core, slot, bytes.data(), bytes.size(), nullptr) && bytes == original,
              "NULL generation rejected before writing a valid slot buffer");
        if (system == 2)
            check(bytes[0] == 0xaa && bytes[2] == 0xbb, "true GBA multiplayer transfer persisted");
        else
            check(bytes[0] == (slot == 0 ? 0xbb : 0xaa), "true GB serial transfer persisted");
        check(m.subsystem.battery_dirty(core, slot), "both batteries dirty");
    }
    if (system != 2) {
        m.base.set_button(core, 1, CORVUS_BUTTON_A, 1);
        check(m.base.run_frame(core), "P2 input frame");
        uint64_t generation = 0;
        auto p1 = battery(m, core, 0, generation);
        auto p2 = battery(m, core, 1, generation);
        check((p1[1] & 1) == 1 && (p2[1] & 1) == 0, "P2 input independent of P1");
        check(m.subsystem.battery_acknowledge(core, 1, generations[1]) && m.subsystem.battery_dirty(core, 1),
              "stale slot generation cannot clear newer write");
        m.base.set_button(core, 1, CORVUS_BUTTON_A, 0);
    }
    std::array<std::vector<int16_t>, 3> routed;
    for (uint32_t value = 0; value < 3; ++value) {
        check(m.options.set_value(core, "link.audio", value) && m.base.reset(core), "audio route selected");
        for (int frame = 0; frame < 3; ++frame)
            check(m.base.run_frame(core), "audio route frame");
        routed[value] = out.audio;
    }
    check(routed[2].size() == std::max(routed[0].size(), routed[1].size()), "mix retains all stereo frames");
    if (system != 2)
        check(routed[0] != routed[1], "distinct console tones remain distinct");
    for (size_t sample = 0; sample < routed[2].size(); ++sample) {
        const int32_t p1 = sample < routed[0].size() ? routed[0][sample] : 0;
        const int32_t p2 = sample < routed[1].size() ? routed[1][sample] : 0;
        check(routed[2][sample] == (p1 + p2) / 2, "real stereo PCM average without overflow");
    }
    check(m.base.reset(core), "reset both consoles");
    for (uint32_t slot = 0; slot < 2; ++slot) {
        uint64_t generation = 0;
        auto bytes = battery(m, core, slot, generation);
        check(m.subsystem.battery_dirty(core, slot) && generation >= generations[slot],
              "reset preserves dirty generation");
        check(bytes[0] == (system == 2 ? 0xaa
                           : slot == 0 ? 0xbb
                                       : 0xaa),
              "reset preserves each battery content");
        check(m.subsystem.battery_acknowledge(core, slot, generation) &&
                  !m.subsystem.battery_dirty(core, slot),
              "independent generation acknowledged");
    }
    m.base.unload_game(core);
    m.base.unload_game(core);
    if (system == 2) {
        auto input_rom = corvuscore::testing::minimal_gba_rom();
        const uint32_t input[]{0xe59f000c, 0xe59f100c, 0xe5d02000, 0xe5c12000,
                               0xeafffffc, 0x04000130, 0x0e000000};
        for (size_t word = 0; word < std::size(input); ++word)
            for (unsigned byte = 0; byte < 4; ++byte)
                input_rom[word * 4 + byte] = static_cast<uint8_t>(input[word] >> (byte * 8));
        std::array<corvus_game, 2> inputs{game(input_rom), game(input_rom)};
        check(m.subsystem.load(core, subsystem.id, inputs.data(), 2), "GBA input pair loads");
        m.base.set_button(core, 1, CORVUS_BUTTON_A, 1);
        check(m.base.run_frame(core), "GBA P2 input frame");
        uint64_t generation = 0;
        auto p1 = battery(m, core, 0, generation);
        auto p2 = battery(m, core, 1, generation);
        check((p1[0] & 1) == 1 && (p2[0] & 1) == 0, "GBA P2 input independent of P1");
        m.base.unload_game(core);
    }
    check(m.base.load_game(core, &solo) && m.base.run_frame(core), "solo usable after paired unload");
    m.base.destroy(core);
    core = m.base.create(&host);
    check(core && m.subsystem.load(core, subsystem.id, games.data(), 2), "fresh paired session");
    m.base.destroy(core); // destruction must unplug before either bridge dies
}
} // namespace
int main(int argc, char **argv) {
    check(argc == 4, "three module paths");
    for (unsigned system = 0; system < 3; ++system)
        module_test(argv[system + 1], system);
    std::puts("PASS real module options and two-player link sessions");
}
