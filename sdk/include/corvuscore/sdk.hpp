// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <corvuscore/core.hpp>
#include <corvuscore/gb/core.hpp>
#include <corvuscore/gbc/core.hpp>
#include <memory>

namespace corvuscore::sdk {

/** Header-only convenience API over CorvusCore's native C++20 core factories. */
enum class System {
    game_boy,
    game_boy_color,
    game_boy_advance,
    game_boy_color_on_gba,  ///< la GBC d'une Game Boy Advance (gb::HardwareModel::agb)
};

[[nodiscard]] inline std::unique_ptr<Core> make_core(System system) {
    switch (system) {
    case System::game_boy:         return gb::make_core(gb::HardwareModel::dmg);
    case System::game_boy_color:   return gbc::make_core();
    case System::game_boy_advance: return make_gba_core(std::nullopt);
    case System::game_boy_color_on_gba: return gb::make_core(gb::HardwareModel::agb);
    }
    return {};
}

} // namespace corvuscore::sdk
