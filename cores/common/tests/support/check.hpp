// CorvusCore - CorvusSoftware
// Copyright (C) 2026 CorvusSoftware
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace corvuscore::testing {

inline void check(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string{message});
}

template <typename Exception, typename Function>
void expect_failure(Function&& function, std::string_view message) {
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(std::string{message});
}

} // namespace corvuscore::testing
