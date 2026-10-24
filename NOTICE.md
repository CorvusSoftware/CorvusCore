# CorvusCore - Provenance, droits d'auteur et licences

**© 2026 CorvusSoftware.** CorvusCore est la nouvelle identité du projet RavenCore créé par **GhostPunishR**. Le code reste distribué sous **GNU AGPL-3.0-only** (`LICENSE`). Les contributions antérieures et les droits de leurs auteurs sont conservés ; la nouvelle identité ne supprime aucune attribution. Les identifiants publics (namespace, API C, CMake, Libretro) ont été renommés de `ravenemu`/`RavenCore` vers `corvuscore`/`CorvusCore` et nécessitent une adaptation des intégrations existantes.

**Source du projet avant renommage :** https://github.com/GhostPunishR/RavenCore

**Exception tierce :** `libretro/include/libretro.h` conserve la licence MIT et les avis de copyright originaux de l'équipe RetroArch.

---

# Provenance et licences

- **Cœurs GB/GBC/GBA** : code de [GhostPunishR/RavenEmu](https://github.com/GhostPunishR/RavenEmu), importé depuis le commit [312e4af](https://github.com/GhostPunishR/RavenEmu/commit/312e4af765e2005e1f21713f4caf79e96c6f4da5), sous **AGPL-3.0** (voir `LICENSE`), puis modifié dans ce dépôt : validation des états instantanés, correctifs audio et de compilation. L'historique Git de `cores/` détaille chaque changement. Les moteurs restent les cœurs maison RavenEmu ; aucune source d'un cœur tiers n'est substituée.
- **Interface Libretro** : fichier `libretro/include/libretro.h` récupéré depuis [libretro-common@a3194c18ba65ef89734e1898a2f61403c255d368](https://github.com/libretro/libretro-common/blob/a3194c18ba65ef89734e1898a2f61403c255d368/include/libretro.h). Copyright (C) 2010–2024 The RetroArch team, distribué selon les termes MIT figurant en tête de ce fichier.
- **Palettes de compatibilité de la GBC** : les tables de `cores/gb/src/ppu/compatibility_palettes.hpp` reprennent les données de [The Cutting Room Floor, « Notes:Game Boy Color Bootstrap ROM »](https://tcrf.net/Notes:Game_Boy_Color_Bootstrap_ROM) (révision 2183057), sous licence [Creative Commons Attribution 3.0 Unported](https://creativecommons.org/licenses/by/3.0/). Modification : couleurs converties du RGB 24 bits au RGB555 de la console. Le choix des palettes selon le titre suit Pan Docs, « Compatibility palettes ». Attribution maintenue lors du portage depuis RavenCore.
- **Adaptateurs, exemples et configuration de ce dépôt** : AGPL-3.0, sauf indication contraire.

L'API Libretro est indépendante de RetroArch. L'inclusion de son en-tête ne signifie pas que ce projet est approuvé, référencé ou distribué officiellement par RetroArch ou RetroAchievements.

Les ROMs, firmwares et BIOS commerciaux ne sont pas distribués avec ce dépôt.
