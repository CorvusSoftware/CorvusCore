# Adaptateurs CorvusAPI

Copyright (C) 2026 CorvusSoftware. SPDX-License-Identifier: AGPL-3.0-only.

Option CMake : `-DCORVUSCORE_BUILD_CORVUSAPI=ON`. Initialiser le sous-module
`third_party/CorvusAPI` avant compilation. Trois modules sont produits :
`libcorvus_gb.so`, `libcorvus_gbc.so` et `libcorvus_gba.so` (selon plateforme).
Chaque module expose `corvus_get_api` et adapte le SDK C natif de CorvusCore.

La distribution des modules Android reste sous le controle de CorvusRetro.
L'ancien SDK C CorvusCore et les adaptateurs libretro demeurent disponibles.
Les identifiants des boutons sont traduits explicitement ; les sauvegardes
batterie conservent l'acquittement par generation.

L'ABI CorvusAPI est encore experimentale (version 0). L'adaptateur CorvusCore
reste sous sa licence AGPL-3.0-only. Seule l'interface CorvusAPI est sous MIT.
