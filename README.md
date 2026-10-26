# CorvusCore

> Moteurs d'émulation GB, GBC et GBA en C++20 pour l'écosystème CorvusSoftware. Projet issu de RavenCore créé par GhostPunishR, sous **AGPL-3.0-only**.

[![CI](https://github.com/CorvusSoftware/CorvusCore/actions/workflows/ci.yml/badge.svg)](https://github.com/CorvusSoftware/CorvusCore/actions/workflows/ci.yml)

CorvusCore fournit les trois moteurs officiels **CorvusGB**, **CorvusGBC** et **CorvusGBA** (expérimental). Les frontends, notamment [CorvusRetro](https://github.com/CorvusSoftware/CorvusRetro), communiquent avec eux au moyen de [CorvusAPI](https://github.com/CorvusSoftware/CorvusAPI), une ABI C indépendante d'Android.

## Compiler et tester

Prérequis : CMake 3.22.1+, un compilateur C++20 et Git.

```sh
git clone --recurse-submodules https://github.com/CorvusSoftware/CorvusCore.git
cd CorvusCore
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCORVUSCORE_BUILD_CORVUSAPI=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Par défaut, `CORVUSCORE_BUILD_CORVUSAPI=OFF` construit les moteurs et le SDK. Avec `ON`, trois modules natifs sont également construits : `corvus_gb`, `corvus_gbc` et `corvus_gba`. Chacun exporte `corvus_get_api`.

## Interfaces

| Interface | Documentation |
| --- | --- |
| CorvusAPI (ABI C pour les frontends) | [Spécification publique](https://github.com/CorvusSoftware/CorvusAPI) |
| SDK C de CorvusCore | [`bridge.h`](sdk/include/corvuscore/bridge.h) |
| SDK C++ | [`sdk.hpp`](sdk/include/corvuscore/sdk.hpp) |
| Accès C++ aux moteurs | [`core.hpp`](cores/common/include/corvuscore/core.hpp) |

Installer le SDK et utiliser son paquet CMake :

```sh
cmake --install build --prefix /chemin/corvuscore
```

```cmake
find_package(CorvusCore 0.14 REQUIRED)
target_link_libraries(mon_app PRIVATE CorvusCore::corvuscore_c_api)
```

Le SDK prend en charge la vidéo, l'audio, les entrées, la batterie, les états instantanés, les codes de triche, les capteurs, les vibrations, le câble link local/distant et la lecture mémoire pour RetroAchievements. Les capacités exposées via CorvusAPI dépendent de son contrat public : le câble link n'y est pas encore disponible.

Voir [les modules CorvusAPI](docs/corvusapi.md), le [SDK](sdk/README.md), les [tests GB/GBC](cores/gb/tests/README.md) et les [tests GBA](cores/gba/tests/README.md).

## Tests, versions et licence

La CI exécute les tests des moteurs, des modules CorvusAPI et des suites de conformité publiques (Mooneye, Blargg, acid2, gba-tests et FuzzARM). Les ROMs de test publiques sont téléchargées séparément et vérifiées par SHA-256.

Version du code : **0.14.1**. Les anciennes [releases](https://github.com/CorvusSoftware/CorvusCore/releases) restent accessibles. Les prochaines archives concernent le SDK natif et les modules CorvusAPI. Voir le [CHANGELOG](CHANGELOG.md) et le [guide de contribution](CONTRIBUTING.md).

CorvusCore reste sous [AGPL-3.0-only](LICENSE). La spécification CorvusAPI est sous MIT. Les droits antérieurs et attributions figurent dans [NOTICE.md](NOTICE.md).

Aucune ROM ni aucun BIOS commercial n'est fourni. CorvusCore n'est affilié ni à Nintendo ni à RetroAchievements.

© 2026 CorvusSoftware.
