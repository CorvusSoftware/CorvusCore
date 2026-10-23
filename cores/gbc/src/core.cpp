#include <corvuscore/gbc/core.hpp>
#include <corvuscore/gb/core.hpp>

namespace corvuscore::gbc {
std::unique_ptr<Core> make_core(std::span<const std::uint8_t> boot_rom) {
    return gb::make_core(gb::HardwareModel::cgb, boot_rom);
}
} // namespace corvuscore::gbc
