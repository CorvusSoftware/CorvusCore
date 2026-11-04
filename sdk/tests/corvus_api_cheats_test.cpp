// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only
#include <corvusapi.h>
#include <synthetic_roms.hpp>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <vector>

namespace {
void check(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(1); }
}
unsigned callbacks = 0;
void video(void*, const uint32_t*, uint32_t, uint32_t, size_t) { ++callbacks; }
void audio(void*, const int16_t*, size_t) { ++callbacks; }
struct Module {
    void* library;
    corvus_core_api base{};
    corvus_cheats_api cheats{};
    corvus_subsystem_api link{};
    explicit Module(const char* path) : library(dlopen(path, RTLD_NOW | RTLD_LOCAL)) {
        check(library, "module opens");
        const auto get_base=reinterpret_cast<corvus_get_api_fn>(dlsym(library,CORVUS_CORE_ENTRYPOINT));
        const auto get_cheats=reinterpret_cast<corvus_get_cheats_api_fn>(dlsym(library,CORVUS_CHEATS_ENTRYPOINT));
        const auto get_link=reinterpret_cast<corvus_get_subsystem_api_fn>(dlsym(library,CORVUS_SUBSYSTEM_ENTRYPOINT));
        check(get_base && get_cheats && get_link, "entrypoints present");
        struct Extended { corvus_cheats_api api; std::array<unsigned char,32> tail; } out;
        std::memset(&out,0x5a,sizeof(out)); out.api.struct_size=sizeof(out);
        check(!get_cheats(2,&out.api) && !get_cheats(1,nullptr), "bad negotiation rejected");
        Extended before=out;
        check(!get_cheats(2,&out.api) && std::memcmp(&before,&out,sizeof(out))==0,"version failure writes nothing");
        out.api.struct_size=sizeof(out.api)-1; before=out;
        check(!get_cheats(1,&out.api) && std::memcmp(&before,&out,sizeof(out))==0,"short failure writes nothing");
        out.api.struct_size=sizeof(out);
        check(get_cheats(1,&out.api) && out.api.api_version==1 && out.api.struct_size==sizeof(out.api) &&
              out.tail==before.tail,"bounded negotiation preserves host tail");
        cheats=out.api; base.struct_size=sizeof(base); link.struct_size=sizeof(link);
        check(get_base(0,&base) && get_link(1,&link) && cheats.capabilities && cheats.validate && cheats.replace,
              "complete optional API negotiated");
    }
    ~Module() { dlclose(library); }
};
std::vector<uint8_t> fixture(unsigned system) {
    if (system==2) {
        auto rom=corvuscore::testing::minimal_gba_rom();
        // Repeatedly copy EWRAM[0] to SRAM[0], entirely original ARM fixture.
        const uint32_t words[]{0xe59f000c,0xe59f100c,0xe5d02000,0xe5c12000,0xeafffffc,0x02000000,0x0e000000};
        for(size_t i=0;i<std::size(words);++i)
            for(unsigned byte=0;byte<4;++byte) rom[4*i+byte]=static_cast<uint8_t>(words[i]>>(8*byte));
        std::memcpy(rom.data()+0x100,"SRAM_V",6);
        return rom;
    }
    auto rom=corvuscore::testing::minimal_game_boy_rom();
    rom[0x101]=0x50; rom[0x147]=3; rom[0x149]=2;
    // Enable cartridge RAM, then repeatedly copy WRAM[C010] to SRAM[A000].
    const uint8_t code[]{0x3e,0x0a,0xea,0,0,0xfa,0x10,0xc0,0xea,0,0xa0,0x18,0xf8};
    std::memcpy(rom.data()+0x150,code,sizeof(code)); return rom;
}
corvus_game game(const std::vector<uint8_t>& rom) {
    corvus_game value{}; value.struct_size=sizeof(value); value.rom_data=rom.data(); value.rom_size=rom.size(); return value;
}
uint8_t battery(Module& m,void* core,uint32_t slot) {
    std::vector<uint8_t> data(m.link.battery_size(core,slot)); uint64_t generation=0;
    check(!data.empty() && m.link.battery_copy(core,slot,data.data(),data.size(),&generation),"battery readable");
    return data[0];
}
void advance(Module& m,void* core) {
    // Cheats apply before a frame; the original fixture then copies the result.
    check(m.base.run_frame(core) && m.base.run_frame(core),"two frames advance");
}
void test(const char* path,unsigned system) {
    Module m(path); corvus_host host{}; host.struct_size=sizeof(host); host.video_frame=video; host.audio_frames=audio;
    void* core=m.base.create(&host); check(core,"instance created");
    const char* aa=system==2?"32000000 00AA":"01AA10C0";
    const char* bb=system==2?"32000000 00BB":"01BB10C0";
    check(!m.cheats.capabilities(nullptr,0) && !m.cheats.capabilities(core,0) &&
          !m.cheats.validate(core,0,aa) && !m.cheats.replace(core,0,nullptr,0),"unloaded extension unavailable");
    auto rom=fixture(system); auto content=game(rom); check(m.base.load_game(core,&content),"solo loaded");
    const uint32_t expected=system==2 ? CORVUS_CHEAT_FORMAT_GBA_CODEBREAKER_RAW|CORVUS_CHEAT_FORMAT_GBA_GAMESHARK_V1_V2|
        CORVUS_CHEAT_FORMAT_GBA_ACTION_REPLAY_V3 : CORVUS_CHEAT_FORMAT_GB_GAME_GENIE|CORVUS_CHEAT_FORMAT_GB_GAME_SHARK;
    check(m.cheats.capabilities(core,0)==expected && !m.cheats.capabilities(core,1),"exact loaded family capabilities");
    advance(m,core); std::vector<uint8_t> state(m.base.serialize_size(core));
    check(!state.empty() && m.base.serialize(core,state.data(),state.size()),"pre-cheat snapshot");
    const unsigned before=callbacks;
    check(m.cheats.validate(core,0,aa) && battery(m,core,0)==0 && callbacks==before,"validation is pure");
    check(m.cheats.replace(core,0,&aa,1),"active list installed"); advance(m,core);
    check(battery(m,core,0)==0xaa,"cheat changes real machine");
    const char* invalid[]{bb,"no code"};
    check(!m.cheats.replace(core,0,invalid,2) && m.base.last_error(core)[0],"second invalid code rejected with reason");
    std::string exactly(aa); exactly.resize(CORVUS_CHEATS_MAX_CODE_BYTES,' ');
    check(m.cheats.validate(core,0,exactly.c_str()),"4096 text bytes exclude NUL");
    const std::string too_long(CORVUS_CHEATS_MAX_CODE_BYTES+1,'A');
    const char* invalid_utf8="\xc0\xaf";
    const char* no_code=nullptr;
    check(!m.cheats.validate(core,0,too_long.c_str()) && !m.cheats.validate(core,0,invalid_utf8) &&
          !m.cheats.validate(core,0,nullptr) && !m.cheats.validate(core,1,aa) &&
          !m.cheats.replace(core,0,&aa,129) && !m.cheats.replace(core,0,nullptr,1) &&
          !m.cheats.replace(core,0,&no_code,1) && !m.cheats.replace(core,2,&aa,1),"invalid inputs bounded");
    if(system==2) {
        const char* slide="42000000 0001+0000FFFF 0000";
        check(m.cheats.validate(core,0,slide),"individual slide within budget");
        const char* excessive[]{slide,slide};
        check(!m.cheats.replace(core,0,excessive,2),"aggregate slide budget enforced");
    }
    check(m.base.reset(core),"reset after refusals"); advance(m,core);
    check(battery(m,core,0)==0xaa,"all refusals preserve active list and reset retains it");
    std::string borrowed(bb); const char* code=borrowed.c_str();
    check(m.cheats.replace(core,0,&code,1),"replacement copied"); borrowed.assign(borrowed.size(),'!');
    check(m.base.unserialize(core,state.data(),state.size()),"restore old machine"); advance(m,core);
    check(battery(m,core,0)==0xbb,"current list survives state restore and borrowed input expiry");
    std::array<const char*,128> maximum{}; maximum.fill(aa);
    check(m.cheats.replace(core,0,maximum.data(),maximum.size()),"maximum entry count accepted");
    check(m.cheats.replace(core,0,nullptr,0) && m.base.reset(core),"clear list and reset"); advance(m,core);
    check(battery(m,core,0)==0,"clear stops future writes");
    check(m.cheats.replace(core,0,&aa,1),"re-enable before unload"); m.base.unload_game(core);
    check(!m.cheats.capabilities(core,0) && !m.cheats.replace(core,0,&aa,1),"unload disables extension");
    check(m.base.load_game(core,&content),"content reload"); advance(m,core);
    check(battery(m,core,0)==0,"new content has empty list"); m.base.unload_game(core);
    std::array<corvus_game,2> pair{content,content};
    check(m.link.load(core,"corvus.link.two_player",pair.data(),pair.size()),"pair loaded");
    check(m.cheats.capabilities(core,1)==expected && !m.cheats.capabilities(core,2) &&
          m.cheats.replace(core,0,&aa,1) && m.cheats.replace(core,1,&bb,1),"independent slot lists");
    advance(m,core); check(battery(m,core,0)==0xaa && battery(m,core,1)==0xbb,"both players cheat independently");
    check(!m.cheats.replace(core,1,invalid,2) && m.base.reset(core),"failed second-player transaction");
    advance(m,core); check(battery(m,core,0)==0xaa && battery(m,core,1)==0xbb,"refusal and pair reset preserve both lists");
    check(m.cheats.replace(core,1,nullptr,0) && m.base.reset(core),"clear only second player"); advance(m,core);
    check(battery(m,core,0)==0xaa && battery(m,core,1)==0,"clear/reset isolation");
    m.base.destroy(core);
}
}
int main(int argc,char** argv) {
    check(argc==4,"three module paths");
    for(unsigned system=0;system<3;++system) test(argv[system+1],system);
    std::puts("PASS cheats negotiation, limits, lifecycle and real solo/link effects");
}
