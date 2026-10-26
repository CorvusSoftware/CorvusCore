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

## Extension mémoire RetroAchievements

Depuis0.14.1, chaque module expose aussi `corvus_get_achievements_api`, décrit
par le contrat public MIT `corvus/achievements.h` version1. Il fournit une copie
en lecture seule de l'espace mémoire RetroAchievements, avec les identifiants
Game Boy4, Game Boy Color6 et Game Boy Advance5. Le handle est exactement celui
retourné par `corvus_core_api.create` : aucun handle SDK privé ne traverse cette ABI.

Les lectures ont lieu entre deux appels de frame, sur le même thread que le cœur.
Elles n'avancent pas l'émulation, ne modifient pas le jeu et respectent les lectures
courtes aux adresses non exposées. Le symbole et la version de l'API de base restent
inchangés. Les anciens frontends continuent de jouer sans utiliser cette extension.
Le frontend assure la connexion au service, l'identification du jeu et l'évaluation
des conditions ; le cœur ne contacte aucun serveur et ne conserve aucun identifiant.
