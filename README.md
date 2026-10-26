# CorvusCore

**Moteurs d'émulation Game Boy, Game Boy Color et Game Boy Advance, écrits en C++20.**

CorvusCore est la couche d'émulation native de [CorvusSoftware](https://github.com/CorvusSoftware). Elle fonctionne indépendamment de toute interface graphique et peut être intégrée dans un frontend grâce à **[CorvusAPI](https://github.com/CorvusSoftware/CorvusAPI)** ou directement avec son SDK C/C++.

[![CI](https://github.com/CorvusSoftware/CorvusCore/actions/workflows/ci.yml/badge.svg)](https://github.com/CorvusSoftware/CorvusCore/actions/workflows/ci.yml)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)
[![Licence AGPL-3.0](https://img.shields.io/badge/Licence-AGPL--3.0-663399?style=plastic&logo=gnu&logoColor=white)](LICENSE)

[**Démarrer**](#démarrage-rapide) · [**Moteurs**](#moteurs) · [**Interfaces**](#interfaces-et-architecture) · [**Tests**](#tests-et-conformité) · [**Documentation**](#documentation)

---

## Moteurs

| Moteur | Console | Description |
| :--- | :--- | :--- |
| **CorvusGB** | Game Boy | Émulation DMG, cartouches et contrôleurs mémoire, audio, vidéo et périphériques. |
| **CorvusGBC** | Game Boy Color | Modes couleur CGB et compatibilité GB, avec composants matériels partagés avec CorvusGB. |
| **CorvusGBA** | Game Boy Advance | Émulation ARM/Thumb et du matériel GBA ; **compatibilité encore expérimentale**. |

Les trois moteurs prennent en charge le chargement de jeux, le rendu, l'audio et les entrées. Le SDK natif expose également, selon le matériel et le moteur, les **états de sauvegarde**, la **batterie**, les **codes de triche**, les **capteurs**, la **vibration** et le **câble link local ou distant**.

Les fonctionnalités disponibles dans un frontend via CorvusAPI dépendent de son contrat public ; toutes les fonctions avancées du SDK ne sont pas encore exposées par cette ABI.

## Démarrage rapide

**Prérequis :** Git, **CMake 3.22.1 ou plus** et un compilateur compatible **C++20**.

```sh
git clone --recurse-submodules https://github.com/CorvusSoftware/CorvusCore.git
cd CorvusCore

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DCORVUSCORE_BUILD_CORVUSAPI=ON

cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Cette configuration construit les moteurs, le SDK et les trois modules CorvusAPI. Pour ne construire **que les moteurs et le SDK**, omettez `-DCORVUSCORE_BUILD_CORVUSAPI=ON` (désactivé par défaut).

Si vous avez cloné le dépôt sans ses sous-modules :

```sh
git submodule update --init --recursive
```

## Interfaces et architecture

CorvusCore **n'est pas une application** : il ne fournit ni menu, ni lecteur de bibliothèque de jeux, ni frontend Android. Cette séparation permet d'utiliser les mêmes moteurs avec plusieurs applications.

```text
Frontend (CorvusRetro ou application tierce)
                    │
           ABI C : CorvusAPI
                    │
     ┌──────────────┼──────────────┐
     ▼              ▼              ▼
  corvus_gb      corvus_gbc      corvus_gba
     │              │              │
     └──────────────┼──────────────┘
                    ▼
         Moteurs CorvusCore (C++20)
                    │
            SDK natif C / C++
```

| Vous développez… | Interface recommandée | Référence |
| :--- | :--- | :--- |
| Un frontend capable de charger des modules | **CorvusAPI**, ABI C indépendante des moteurs | [Spécification CorvusAPI](https://github.com/CorvusSoftware/CorvusAPI) |
| Une application intégrant directement les moteurs | **SDK C de CorvusCore** | [`bridge.h`](sdk/include/corvuscore/bridge.h) |
| Un outil ou projet C++ natif | **SDK C++** | [`sdk.hpp`](sdk/include/corvuscore/sdk.hpp) |
| Une intégration C++ bas niveau | **Interface des moteurs** | [`core.hpp`](cores/common/include/corvuscore/core.hpp) |

### Modules CorvusAPI

Avec `CORVUSCORE_BUILD_CORVUSAPI=ON`, le projet produit trois bibliothèques natives : **`corvus_gb`**, **`corvus_gbc`** et **`corvus_gba`** (avec le préfixe et l'extension propres à la plateforme).

Chaque module exporte `corvus_get_api` et utilise l'en-tête public **`<corvusapi.h>`**. Les modules actuels proposent aussi l'extension facultative de lecture mémoire pour les succès. Le frontend reste responsable de l'affichage, de l'audio système, des fichiers et de la distribution des modules.

→ [Fonctionnement des adaptateurs](docs/corvusapi.md)

### Installer le SDK C/C++

```sh
cmake --install build --config Release --prefix /chemin/installation
```

Dans un projet CMake consommateur, renseignez `CMAKE_PREFIX_PATH` vers ce préfixe puis utilisez :

```cmake
find_package(CorvusCore 0.14 CONFIG REQUIRED)
target_link_libraries(mon_application PRIVATE CorvusCore::corvuscore_c_api)
```

Pour l'interface C++, utilisez la cible `CorvusCore::corvuscore`. **Le SDK natif CorvusCore et l'ABI CorvusAPI sont deux interfaces distinctes** ; cette dernière est destinée aux modules chargeables par un frontend.

→ [Guide complet du SDK](sdk/README.md)

## Tests et conformité

Les tests unitaires et synthétiques s'exécutent avec **CTest** sans ROM commerciale. Des suites de conformité publiques peuvent être téléchargées séparément, avec vérification des empreintes SHA-256.

```sh
# ROMs de test publiques (facultatif)
cores/gb/tests/fetch_test_roms.sh build/gb-test-roms
cores/gba/tests/fetch_test_roms.sh build/gba-test-roms

cmake -S . -B build -DBUILD_TESTING=ON \
  -DCORVUSCORE_BUILD_CORVUSAPI=ON \
  -DCORVUSCORE_GB_TEST_ROMS=build/gb-test-roms \
  -DCORVUSCORE_GBA_TEST_ROMS=build/gba-test-roms

cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

La CI vérifie les moteurs et leurs interfaces sur **Linux, Windows et macOS**, ainsi que la compilation des modules **Android ARM64 et ARMv7**. Elle comprend également des tests sous **ASan/UBSan**. Le passage de ces tests ne garantit pas que tous les jeux ou toutes les configurations matérielles fonctionnent parfaitement.

[Tests GB/GBC](cores/gb/tests/README.md) · [Tests GBA](cores/gba/tests/README.md) · [Exécutions CI](https://github.com/CorvusSoftware/CorvusCore/actions/workflows/ci.yml)

## Documentation

| Sujet | Guide |
| :--- | :--- |
| API C/C++, sauvegardes, capteurs et câble link | [SDK CorvusCore](sdk/README.md) |
| Modules natifs et extension mémoire | [Intégration CorvusAPI](docs/corvusapi.md) |
| Câble link entre deux appareils | [Protocole de liaison distante](docs/remote_link.md) |
| Lecture mémoire pour les succès | [RetroAchievements](docs/retroachievements.md) |
| Historique des modifications | [CHANGELOG.md](CHANGELOG.md) |
| Proposer une correction | [CONTRIBUTING.md](CONTRIBUTING.md) |

## Versions et distribution

La version définie dans le CMake du dépôt est **0.14.1**. Les versions antérieures et les archives publiées sont disponibles dans les [releases GitHub](https://github.com/CorvusSoftware/CorvusCore/releases).

Les workflows de publication prévoient le SDK et les modules natifs pour Linux, Windows, macOS et Android. La distribution des cœurs dans **[CorvusRetro](https://github.com/CorvusSoftware/CorvusRetro)** reste gérée par cette application.

## Licence et crédits

CorvusCore est distribué sous **[AGPL-3.0-only](LICENSE)**. Le contrat public **[CorvusAPI](https://github.com/CorvusSoftware/CorvusAPI)** est sous **MIT** ; il conserve sa propre licence et son propre dépôt.

Le projet est issu de **RavenCore**, créé par **GhostPunishR**. Les droits et contributions antérieurs sont conservés dans [NOTICE.md](NOTICE.md) et [COPYRIGHT.md](COPYRIGHT.md).

Aucune ROM ni aucun BIOS commercial n'est fourni. CorvusCore n'est affilié ni à Nintendo ni à RetroAchievements.

**© 2026 CorvusSoftware**
