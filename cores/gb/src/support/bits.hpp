#pragma once

#include "corvuscore/core.hpp"
#include "corvuscore/binary_io.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace corvuscore::cgb {

using detail::BinaryReader;
using detail::BinaryWriter;

constexpr int byte(int value) noexcept { return value & 0xff; }
constexpr int word(int value) noexcept { return value & 0xffff; }
using RomImage = std::shared_ptr<const std::vector<std::uint8_t>>;

} // namespace corvuscore::cgb
