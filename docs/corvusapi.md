# Adaptateurs CorvusAPI

Copyright (C) 2026 CorvusSoftware. SPDX-License-Identifier: AGPL-3.0-only.

En-tête public : `#include <corvusapi.h>` (API de base et extension optionnelle).

Option CMake : `-DCORVUSCORE_BUILD_CORVUSAPI=ON`. Initialiser le sous-module
`third_party/CorvusAPI` avant compilation. Trois modules sont produits :
`libcorvus_gb.so`, `libcorvus_gbc.so` et `libcorvus_gba.so` (selon plateforme).
Chaque module expose `corvus_get_api` et adapte le SDK C natif de CorvusCore.

La distribution des modules Android reste sous le contrôle de CorvusRetro.
Le SDK C de CorvusCore demeure disponible pour les intégrations directes.
Les identifiants des boutons sont traduits explicitement ; les sauvegardes
batterie conservent l'acquittement par génération.

L'ABI CorvusAPI est encore expérimentale (version 0). L'adaptateur CorvusCore
reste sous sa licence AGPL-3.0-only. Seule l'interface CorvusAPI est sous MIT.

## Extension mémoire RetroAchievements

Depuis 0.14.1, chaque module expose aussi `corvus_get_achievements_api`, décrit
par le contrat public MIT `achievements.h` version 1. Il fournit une copie
en lecture seule de l'espace mémoire RetroAchievements, avec les identifiants
Game Boy (4), Game Boy Color (6) et Game Boy Advance (5). Le handle est exactement celui
retourné par `corvus_core_api.create` : aucun handle SDK privé ne traverse cette ABI.

Les lectures ont lieu entre deux appels de frame, sur le même thread que le cœur.
Elles n'avancent pas l'émulation, ne modifient pas le jeu et respectent les lectures
courtes aux adresses non exposées. Le symbole et la version de l'API de base restent
inchangés. Les anciens frontends continuent de jouer sans utiliser cette extension.
Le frontend assure la connexion au service, l'identification du jeu et l'évaluation
des conditions ; le cœur ne contacte aucun serveur et ne conserve aucun identifiant.

## Options des moteurs

Les modules exposent `corvus_get_options_api` version 1, défini dans l’en-tête
public MIT `corvusapi.h`. Les clés et les identifiants de valeurs sont stables ;
les libellés décrivent les choix affichés. Les sélections appartiennent à chaque
instance et survivent à `unload_game`. Une clé ou un index inconnu est refusé sans
changer la sélection précédente. Aucun setter ne produit de callback audio/vidéo.

| Module | Clé | Valeurs (défaut en premier) | Application |
| --- | --- | --- | --- |
| GB | `gb.palette` | `green`, `gray`, `amber` | Image suivante |
| GBC | `gbc.colors` | `auto`, `gray`, puis les douze combinaisons de touches du SDK | Image suivante, jeux GB uniquement |
| GBA | `gba.save_type` | `auto`, `none`, `sram`, `flash_64k`, `flash_128k`, `eeprom_512`, `eeprom_8k` | Prochain chargement ou reset |
| GBA | `gba.rtc` | `auto`, `off`, `on` | Prochain chargement ou reset |
| Tous | `link.audio` | `player1`, `player2`, `mix` | Image suivante, partie à deux joueurs |

Les réglages GBA portent `CORVUS_OPTION_RESTART_REQUIRED`. `get_value` renvoie
le choix sélectionné, même s’il attend le prochain reset. Les paramètres en attente
ne changent pas la restauration d’un état de la partie courante. Imposer un autre
type de sauvegarde modifie le matériel de cartouche ; conserver les sauvegardes
avant ce changement. Aucun chemin de BIOS ni modèle matériel supplémentaire n’est
exposé par cette extension.

## Deux joueurs sur le même appareil

`corvus_get_subsystem_api` version 1 décrit `corvus.link.two_player`, une session
avec deux contenus obligatoires du même module. Charger les deux `corvus_game`
sur une instance créée mais sans jeu ; un chargement échoué laisse cette instance
réutilisable. Un chargement sur une instance déjà occupée est refusé sans remplacer
la partie. Les octets ROM/batterie sont copiés avant le retour de l’appel.

Les appels de base pilotent ensuite toute la session : `set_button` reçoit le
joueur 0 ou 1, `run_frame` avance le câble une fois et `reset` réinitialise les deux
consoles en conservant leurs batteries. Un seul callback vidéo présente les écrans
côte à côte : 320 × 144 en GB/GBC, 480 × 160 en GBA. Le son reste du PCM16 stéréo à
32768 Hz. Les deux files audio sont toujours consommées ; `mix` calcule la moyenne
avec un intermédiaire 32 bits, en complétant par du silence les échantillons absents.
Le choix sonore ne change pas les parties en solo.

Chaque contenu conserve sa propre batterie. Utiliser les fonctions batterie de
l’extension avec l’index de contenu 0 ou 1, puis acquitter sa génération seulement
après une écriture durable. Les fonctions batterie de base annoncent une absence
pour les sessions à deux contenus ; elles continuent de fonctionner en solo.
`capabilities` vaut zéro sans jeu, `CORVUS_SESSION_RESET | CORVUS_SESSION_STATES`
en solo et seulement `CORVUS_SESSION_RESET` à deux joueurs. Les états groupés ne
sont pas implémentés : taille zéro et refus de serialize/unserialize pendant le
link. L’extension mémoire des succès retourne également zéro pendant le link.

`unload_game` ferme le câble et les deux contenus ; il peut être répété. Détruire
une instance ferme également son câble avant ses consoles. Les appels d’une même
instance restent sérialisés sur le thread propriétaire, entre les images.

Cette extension fournit le câble local, sans réseau. Le SDK supporte aussi des
combinaisons GB/GBC et jusqu’à quatre GBA, mais ces variantes ne sont pas annoncées
par les modules actuels. Sur GBA, le câble avance par tranches de 1024 cycles : les
transferts multijoueurs sont synchronisés, tandis qu’un transfert normal plus court
qu’une tranche lit le partenaire à sa position courante. La compatibilité de tous
les jeux commerciaux n’est pas garantie ; les tests emploient des programmes
synthétiques originaux pour vérifier les échanges GB/GBC et GBA.
