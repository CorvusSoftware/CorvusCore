// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace corvuscore::detail {

[[nodiscard]] std::array<std::uint8_t, 32> sha256(std::span<const std::uint8_t> data);

} // namespace corvuscore::detail
