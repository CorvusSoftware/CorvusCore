// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only
#include <corvusapi.h>
#include <corvuscore/bridge.h>
#include <corvuscore/core.hpp>
#include <corvuscore/gb/core.hpp>
#include <synthetic_roms.hpp>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

namespace {
bool measuring = false;
size_t allocations = 0;
size_t fail_at = SIZE_MAX;
void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(1); }
}
void arm(size_t index = SIZE_MAX) { allocations = 0; fail_at = index; measuring = true; }
size_t disarm() { measuring = false; fail_at = SIZE_MAX; return allocations; }
uint32_t first_pixel = 0;
void video(void*, const uint32_t* pixels, uint32_t, uint32_t, size_t) { first_pixel = pixels[0]; }
std::vector<uint8_t> snapshot(corvuscore_bridge* handle) {
    std::vector<uint8_t> bytes(corvuscore_state_size(handle));
    check(!bytes.empty() && corvuscore_save_state(handle, bytes.data(), bytes.size()), "SDK snapshot");
    return bytes;
}
using Handle = std::unique_ptr<corvuscore_bridge, decltype(&corvuscore_destroy)>;
void group_failure(int system) {
    auto rom = system == CORVUSCORE_GBA ? corvuscore::testing::minimal_gba_rom()
                                     : corvuscore::testing::minimal_game_boy_rom();
    if (system == CORVUSCORE_GBA) std::memcpy(rom.data() + 0x100, "SRAM_V", 6);
    else {
        rom[0x147] = 3; rom[0x149] = 2;
        const uint8_t program[]{0x3e,0x0a,0xea,0,0,0x3e,0x5a,0xea,0,0xa0,0x18,0xfe};
        rom[0x101] = 0x50;
        std::memcpy(rom.data() + 0x150, program, sizeof(program));
    }
    Handle first(corvuscore_create(system), corvuscore_destroy), second(corvuscore_create(system), corvuscore_destroy);
    check(first && second && corvuscore_load(first.get(),rom.data(),rom.size(),nullptr,0) &&
          corvuscore_load(second.get(),rom.data(),rom.size(),nullptr,0), "reset pair loaded");
    corvuscore_bridge* handles[]{first.get(), second.get()};
    std::unique_ptr<corvuscore_link, decltype(&corvuscore_link_destroy)> cable(
        corvuscore_link_create(handles,2), corvuscore_link_destroy);
    check(cable != nullptr, "reset cable connected");
    std::vector<uint32_t> pixels(system == CORVUSCORE_GBA ? 240*160 : 160*144), other(pixels.size());
    uint32_t* frames[]{pixels.data(),other.data()};
    check(corvuscore_link_run_frame(cable.get(), frames, pixels.size()), "pair advanced");
    arm(); const int one = corvuscore_reset_group(handles,1); const size_t first_allocations = disarm();
    check(one && first_allocations > 0, "measure first prepare allocations");
    const auto before_first = snapshot(first.get()), before_second = snapshot(second.get());
    std::array<uint64_t,2> generations{};
    for (size_t slot=0;slot<2;++slot) {
        std::vector<uint8_t> battery(corvuscore_battery_size(handles[slot]));
        check(corvuscore_battery_dirty(handles[slot]) &&
              corvuscore_copy_battery_generation(handles[slot],battery.data(),battery.size(),&generations[slot]),
              "dirty battery before injected failure");
    }
    // The fixed-size SDK plan array allocates nothing: this is precisely the
    // second core's first prepare allocation, after P1's complete plan exists.
    arm(first_allocations); const int result = corvuscore_reset_group(handles,2); const size_t failed = disarm();
    check(!result && failed == first_allocations+1, "second prepare allocation failed");
    check(snapshot(first.get()) == before_first && snapshot(second.get()) == before_second,
          "failed group reset preserves both complete machines");
    for (size_t slot=0;slot<2;++slot) {
        uint64_t generation=0; std::vector<uint8_t> battery(corvuscore_battery_size(handles[slot]));
        check(corvuscore_copy_battery_generation(handles[slot],battery.data(),battery.size(),&generation) &&
              generation==generations[slot] && corvuscore_battery_dirty(handles[slot]),
              "failed group reset preserves dirty generations");
    }
    check(corvuscore_reset_group(handles,2) && corvuscore_link_run_frame(cable.get(),frames,pixels.size()),
          "paired reset retry succeeds and cable stays usable");
}
void adapter_failure() {
    corvus_core_api base{}; base.struct_size=sizeof(base);
    corvus_options_api options{}; options.struct_size=sizeof(options);
    check(corvus_get_api(0,&base) && corvus_get_options_api(1,&options), "adapter negotiated");
    corvus_host host{}; host.struct_size=sizeof(host); host.video_frame=video;
    void* core=base.create(&host);
    auto rom = CORVUS_ADAPTER_SYSTEM == CORVUSCORE_GBA ? corvuscore::testing::minimal_gba_rom()
                                                     : corvuscore::testing::minimal_game_boy_rom();
    if constexpr (CORVUS_ADAPTER_SYSTEM == CORVUSCORE_GBA) std::memcpy(rom.data()+0x100,"SRAM_V",6);
    corvus_game game{}; game.struct_size=sizeof(game); game.rom_data=rom.data(); game.rom_size=rom.size();
    check(core && base.load_game(core,&game) && base.run_frame(core), "adapter loaded");
    if constexpr (CORVUS_ADAPTER_SYSTEM == CORVUSCORE_GBC) {
        check(options.set_value(core,"gbc.colors",1) && base.run_frame(core) && first_pixel==0xffffffff,
              "immediate grayscale applied before reset");
        arm(0); const int result=base.reset(core); disarm();
        check(!result && options.get_value(core,"gbc.colors")==1 && base.run_frame(core) &&
              first_pixel==0xffffffff, "failed reset preserves immediate palette selection and output");
        base.destroy(core);
        return;
    }
    std::vector<uint8_t> before(base.serialize_size(core));
    check(base.serialize(core,before.data(),before.size()), "adapter snapshot before failure");
    check(options.set_value(core,"gba.save_type",4) && options.set_value(core,"gba.rtc",2), "pending GBA selections");
    arm(0); const int result=base.reset(core); disarm();
    check(!result && options.get_value(core,"gba.save_type")==4 && options.get_value(core,"gba.rtc")==2,
          "failed reset retains selected pending settings");
    std::vector<uint8_t> after(base.serialize_size(core));
    check(base.serialize(core,after.data(),after.size()) && after==before && base.battery_size(core)==32*1024,
          "failed reset retains old machine hardware and state");
    check(base.unserialize(core,before.data(),before.size()), "SDK forced fields restored after failure");
    check(base.reset(core) && base.battery_size(core)==128*1024+16,
          "retry applies pending GBA flash and RTC hardware");
    base.destroy(core);
}
}
// Only this test executable replaces the allocator. Fail once, then permit error
// reporting and cleanup; no fault hooks enter the SDK or shipped modules.
void* operator new(size_t size) {
    if (measuring && allocations++ == fail_at) { fail_at=SIZE_MAX; throw std::bad_alloc(); }
    if (void* memory=std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void* operator new(size_t size, const std::nothrow_t&) noexcept {
    try { return ::operator new(size); } catch (...) { return nullptr; }
}
void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    try { return ::operator new[](size); } catch (...) { return nullptr; }
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, size_t) noexcept { std::free(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
int main() {
    group_failure(CORVUSCORE_GB); group_failure(CORVUSCORE_GBC); group_failure(CORVUSCORE_GBA);
    adapter_failure(); std::puts("PASS transactional reset and pending-setting allocation failures");
}
