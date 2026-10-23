// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <corvuscore/core.hpp>

namespace corvuscore::gba {
inline std::unique_ptr<Core> make_core() { return make_gba_core(std::nullopt); }
}
