#!/usr/bin/env bash
# CorvusCore - CorvusSoftware
# Copyright (C) 2026 CorvusSoftware
# SPDX-License-Identifier: AGPL-3.0-only
set -euo pipefail

# Detect executable/build dependencies on removed third-party frontend APIs.
# Historical release notes are documentation and must not be rejected.
retired_dir="lib""retro"
retired_app="retro""arch"
retired_option="CORVUSCORE_BUILD_LIB""RETRO"

if [ -n "$(git ls-files "$retired_dir/*")" ]; then
  echo "Removed frontend adapter directory has been reintroduced" >&2
  exit 1
fi

# Scan source, CMake and workflows only, not docs or license attributions.
for term in "$retired_dir" "$retired_app" "$retired_option"; do
  if git grep --ignore-case -n -I -F "$term" -- \
    '*.c' '*.h' '*.cpp' '*.hpp' '*.cmake' '*CMakeLists.txt' \
    '.github/workflows/*.yml' '.github/workflows/*.yaml' \
    ':!tools/check_external_frontend_refs.sh'; then
    echo "Obsolete third-party frontend dependency found" >&2
    exit 1
  fi
done
