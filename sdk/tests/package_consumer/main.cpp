// Inclut chaque en-tête public installé et appelle les deux interfaces.
#include <corvuscore/bridge.h>
#include <corvuscore/core.hpp>
#include <corvuscore/gb/core.hpp>
#include <corvuscore/gba/core.hpp>
#include <corvuscore/gbc/core.hpp>
#include <corvuscore/sdk.hpp>

#include <cstdio>

int main() {
    using corvuscore::sdk::System;
    for (const auto system : {System::game_boy, System::game_boy_color, System::game_boy_advance}) {
        if (!corvuscore::sdk::make_core(system)) return 1;
    }
    if (corvuscore::gba::make_core()->console() != corvuscore::Console::game_boy_advance) return 1;

    for (const int system : {CORVUSCORE_GB, CORVUSCORE_GBC, CORVUSCORE_GBA}) {
        auto* handle = corvuscore_create(system);
        if (handle == nullptr) {
            std::fprintf(stderr, "corvuscore_create : %s\n", corvuscore_last_error());
            return 1;
        }
        corvuscore_destroy(handle);
    }
    std::puts("paquet CorvusCore consommé avec succès");
    return 0;
}
