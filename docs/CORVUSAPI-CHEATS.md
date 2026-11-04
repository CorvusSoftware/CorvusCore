# Codes de triche des modules CorvusAPI

Les trois modules GB, GBC et GBA exportent l’extension optionnelle version 1 `corvus_get_cheats_api`, définie dans le sous-module [CorvusAPI](../third_party/CorvusAPI/docs/CHEATS.md). Le handle reste celui de `corvus_core_api`, ABI de base 0. Le contrat ne dépend d’aucun frontend.

## Découverte et formats

Le frontend recherche le symbole sans exiger sa présence sur les anciens modules. Il initialise `struct_size`, négocie la version 1 et vérifie les trois pointeurs. Une version inconnue, une sortie nulle ou trop petite échoue sans écrire ; une sortie plus grande conserve sa partie finale.

`capabilities(core, player)` renvoie zéro sans jeu chargé ou pour un joueur absent. Le joueur 0 désigne la session solo ; le sous-système `corvus.link.two_player` fournit les joueurs 0 et 1.

| Module | Familles annoncées |
| --- | --- |
| GB et GBC | `CORVUS_CHEAT_FORMAT_GB_GAME_GENIE`, `CORVUS_CHEAT_FORMAT_GB_GAME_SHARK` |
| GBA | `CORVUS_CHEAT_FORMAT_GBA_CODEBREAKER_RAW`, `CORVUS_CHEAT_FORMAT_GBA_GAMESHARK_V1_V2`, `CORVUS_CHEAT_FORMAT_GBA_ACTION_REPLAY_V3` |

La validation reste l’autorité : tous les opcodes d’une famille ne sont pas acceptés. Les lignes d’une entrée sont séparées par des espaces, retours à la ligne ou `+`. Les patches de ROM GBA, le changement de graines `DEADFACE` et les codes Codebreaker chiffrés restent refusés. Les écritures GBA ciblent les régions mémoire 0x02 à 0x07, pas directement la sauvegarde 0x0E. Aucun nouveau décodeur provenant d’un émulateur tiers n’est ajouté.

## Validation et remplacement

`validate(core, player, code)` décode une entrée sans changer la machine, la liste, les sauvegardes ou émettre des callbacks. Il permet aussi de vérifier les entrées désactivées d’un menu. `replace(core, player, codes, count)` reçoit uniquement les entrées activées, dans l’ordre d’exécution. Noms, activation et fichiers restent la responsabilité du frontend.

Les chaînes UTF-8 sont non vides, terminées par NUL et limitées à 4096 octets hors NUL. Une liste contient au plus 128 entrées. Tous les paramètres sont empruntés pendant l’appel, puis décodés et copiés. Une entrée invalide, un dépassement ou une allocation impossible conserve entièrement l’ancienne liste et la machine. Le motif est disponible dans `last_error(core)`, en français. Aucune transaction entre deux joueurs n’est promise : chaque appel ne touche qu’un joueur.

Les limites de travail du fournisseur sont également vérifiées avant tout échange : au plus 1024 opérations décodées et 65536 accès mémoire développés par liste. Une écriture répétée compte chaque répétition ; addition, OU/ET et écriture indirecte comptent deux accès, test et patch ROM un, saut zéro. Pour les patches Game Genie, cette borne compte les descripteurs, appliqués aux lectures de ROM ; elle n’est pas une mesure temporelle du processeur. Les tests conditionnels ne diminuent pas l’estimation. `validate` vérifie ces limites pour une entrée ; plusieurs entrées individuellement valides peuvent dépasser la borne agrégée de `replace`. Ces bornes empêchent notamment une série GBA de développer des centaines de milliers d’écritures par image. Elles ne changent aucun timing CPU, PPU, APU ou câble.

Le SDK C/C++ applique les mêmes limites de texte, nombre et opérations, à travers `parse_cheat`, `validate_cheat_budget` et `Core::set_cheats`. Le SDK direct ne valide pas le format UTF-8 séparément : les décodeurs n’acceptent que leur syntaxe ASCII. Les vecteurs temporaires et la copie des patches du bus GB sont prêts avant les échanges sans allocation.

## Cycle de vie

Les effets commencent à l’image suivante ; les patches Game Genie affectent les lectures futures. `replace(core, player, NULL, 0)` retire uniquement les codes de ce joueur. Retirer un code n’annule pas les écritures déjà faites : le jeu ou un reset doit restaurer la mémoire.

Les listes actives survivent au reset et au chargement d’un état ; elles ne sont pas sérialisées dans cet état. Elles disparaissent au déchargement et lors de toute nouvelle session, car l’adaptateur public détruit ses bridges. Une session link garde une liste par machine et le reset groupé les conserve. Les états solo gardent leur format ; le sous-système link ne fournit toujours pas d’états. Le SDK direct conserve historiquement les codes entre ses propres chargements, ce qui est distinct du cycle du module.

Tous les appels restent synchrones, sur le thread propriétaire du handle, en dehors des callbacks AV. Le module ne lit ni n’écrit de fichier de triche et ne définit aucune politique de succès ou de compte utilisateur.

## Vérification

Le test dynamique `corvuscore_cheats_api` charge les trois vrais modules et utilise uniquement des ROMs synthétiques originales : lecture mémoire copiée vers la batterie, limites, négociation, callbacks, reset, restauration d’état, déchargement et isolation des deux joueurs. `corvuscore_cheats` vérifie les bornes d’opérations et séries développées ; les deux exécutables de transaction injectent chaque panne d’allocation du remplacement GB/GBA et contrôlent la liste conservée avant et après reset.
