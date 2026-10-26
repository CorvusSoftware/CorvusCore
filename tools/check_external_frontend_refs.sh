#!/usr/bin/env bash
# CorvusCore - CorvusSoftware
# Copyright (C) 2026 CorvusSoftware
# SPDX-License-Identifier: AGPL-3.0-only
set -euo pipefail
for term in "lib""retro" "retro""arch"; do
  if git grep --ignore-case -n -I "$term" -- ':!tools/check_external_frontend_refs.sh'; then
    echo "Obsolete external frontend reference found" >&2
    exit 1
  fi
done
