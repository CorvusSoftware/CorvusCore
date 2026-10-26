// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only
#include <corvusapi.h>
#include <corvus/achievements.h>
#include <synthetic_roms.hpp>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vector>

namespace {
unsigned video_frames;
void video(void*, const uint32_t*, uint32_t, uint32_t, size_t) { ++video_frames; }
void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(1); }
}
void module(const char* path, unsigned system) {
    void* library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    check(library != nullptr, "module opens");
    auto get_api = reinterpret_cast<corvus_get_api_fn>(dlsym(library, CORVUS_CORE_ENTRYPOINT));
    auto get_memory = reinterpret_cast<corvus_get_achievements_api_fn>(
        dlsym(library, CORVUS_ACHIEVEMENTS_ENTRYPOINT));
    check(get_api && get_memory, "base and optional extension entrypoints");
    corvus_core_api api{};
    api.struct_size = sizeof(api);
    check(get_api(0, &api) && api.api_version == 0 && api.create && api.run_frame,
          "unchanged base ABI0 remains usable");
    corvus_core_info info{};
    info.struct_size = sizeof(info);
    check(api.get_info(&info) && std::strcmp(info.version, CORVUSCORE_VERSION) == 0,
          "module metadata matches the provider project version");
    corvus_achievements_api memory{};
    memory.struct_size = sizeof(memory);
    check(!get_memory(2, &memory) && !get_memory(1, nullptr), "version and NULL negotiation rejected");
    memory.struct_size = sizeof(memory) - 1;
    memory.console_id = 0xfeed;
    check(!get_memory(1, &memory) && memory.console_id == 0xfeed, "small output not overwritten");
    memory.struct_size = sizeof(memory);
    check(get_memory(1, &memory) && memory.api_version == 1 && memory.read_memory,
          "optional extension negotiated");
    constexpr std::array<uint32_t, 3> ids{4, 6, 5};
    check(memory.console_id == ids[system], "declared achievement address space");
    corvus_host host{};
    host.struct_size = sizeof(host);
    host.video_frame = video;
    void* core = api.create(&host);
    check(core != nullptr, "base API creates the extension handle");
    uint8_t output[32]{};
    check(memory.read_memory(core, 0, output, sizeof(output)) == 0, "unloaded core memory unavailable");
    auto rom = system == 2 ? corvuscore::testing::minimal_gba_rom()
                           : corvuscore::testing::minimal_game_boy_rom();
    corvus_game game{};
    game.struct_size = sizeof(game);
    game.rom_data = rom.data();
    game.rom_size = rom.size();
    check(api.load_game(core, &game), "original synthetic game loaded");
    const uint32_t start = system == 2 ? 0 : 0x100;
    check(memory.read_memory(core, start, output, sizeof(output)) == sizeof(output), "valid memory bytes returned");
    if (system != 2) check(output[0] == rom[0x100], "Game Boy ROM address has the expected bytes");
    check(memory.read_memory(nullptr, 0, output, 1) == 0 &&
              memory.read_memory(core, 0, nullptr, 1) == 0 &&
              memory.read_memory(core, 0, output, 0) == 0, "invalid memory buffers rejected");
    check(memory.read_memory(core, UINT32_MAX, output, sizeof(output)) == 0,
          "out-of-range address cannot wrap into valid memory");
    if (system != 2)
        check(memory.read_memory(core, 0xfe9c, output, 8) == 4, "unmapped GB hole returns a short prefix");
    else
        check(memory.read_memory(core, 0x47ffc, output, 8) == 4,
              "GBA RAM boundary without save memory returns a short prefix");
    check(video_frames == 0, "inspection did not advance a frame");
    check(api.run_frame(core), "base frame still runs after inspection");
    check(video_frames == 1, "frame callback remains intact");
    api.unload_game(core);
    check(memory.read_memory(core, start, output, 1) == 0, "unload revokes memory availability");
    api.destroy(core);
    dlclose(library);
    video_frames = 0;
}
}

int main(int argc, char** argv) {
    check(argc == 4, "three module paths supplied");
    for (unsigned index = 0; index < 3; ++index) module(argv[index + 1], index);
    std::puts("PASS optional achievement memory ABI, real GB/GBC/GBA modules and safe short reads");
}
