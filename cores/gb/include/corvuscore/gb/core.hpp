#pragma once
#include <corvuscore/core.hpp>
#include <corvuscore/gb/hardware_mode.hpp>

namespace corvuscore::gb {
/** Fabrique GB/GBC explicite. Le mode automatique conserve le contrat JNI. */
[[nodiscard]] std::unique_ptr<Core> make_core(
    HardwareModel model = HardwareModel::automatic,
    std::span<const std::uint8_t> boot_rom = {}
);
}
