# Instructions de développement — CorvusCore

Ce fichier s'applique à l'ensemble du dépôt. Avant toute modification, lire le README, le guide de contribution et les documents de la partie concernée. Préférer l'état courant des sources et de CMake aux exemples anciens du journal des versions.

## Mission et limites d'architecture

CorvusCore maintient les moteurs d'émulation **Game Boy, Game Boy Color et Game Boy Advance** de CorvusSoftware, écrits en **C++20**. Le projet doit rester utilisable indépendamment de toute interface utilisateur et de toute plateforme applicative.

- `cores/common/` : contrats et outils communs aux moteurs.
- `cores/gb/`, `cores/gbc/`, `cores/gba/` : implémentations matérielles et tests par console.
- `sdk/` : SDK C/C++ propre à CorvusCore, pont C, adaptateurs de modules et tests.
- `third_party/CorvusAPI/` : sous-module du contrat public CorvusAPI (MIT).
- `docs/` : documentation technique ; `.github/workflows/` : validation et publication.

**Séparer strictement les responsabilités :**
- Les moteurs ne dépendent pas d'Android, de Java/JNI, d'un frontend, d'un moteur tiers ou d'une interface graphique.
- Les frontends communiquent avec les modules via **CorvusAPI**, sauf intégration directe volontaire du SDK par un consommateur externe.
- Ne pas réintroduire d'ancien adaptateur de frontend tiers, de bibliothèque d'interface concurrente ni de dépendance à un client particulier. Conserver le rôle de CorvusAPI comme frontière d'interopérabilité.
- Ne pas modifier silencieusement l'ABI C, les symboles exportés, les identifiants de systèmes/cœurs ou les formats de sauvegarde. Toute évolution du contrat public doit d'abord être définie et versionnée dans **CorvusAPI**.
- La version de base de CorvusAPI est encore expérimentale ; ne pas promettre une stabilité binaire inexistante. Les capacités non couvertes par l'ABI de base doivent passer par une extension publique négociée et testée, et non par un accès privé au SDK.
- Préserver l'indépendance entre les trois moteurs et leur partage de code justifié.

## Règles de code

- Employer **C++20**, respecter le style du répertoire modifié et les avertissements stricts configurés par CMake.
- Favoriser les modifications minimales, lisibles et testables. Ne pas faire de renommage ou de refonte sans rapport avec la demande.
- Vérifier les limites des buffers, tailles, alignements, conversions numériques, erreurs et durées de vie : ROMs, états et données batterie sont non fiables.
- Préserver le déterminisme des cycles, les timings CPU/PPU/APU/DMA, les interruptions et l'ordre des événements.
- Respecter les contrats existants de sauvegarde : pas de perte de données, écriture durable côté hôte, copie et acquittement des générations de batterie.
- Pour les modules CorvusAPI, conserver le point d'entrée `corvus_get_api`, les tailles de structures, la négociation de version, les callbacks et leurs règles de durée de vie. L'extension de lecture mémoire des succès doit rester optionnelle et sans effet de bord.
- Ne pas ajouter de ROMs commerciales, de BIOS propriétaires, de secrets ou de données privées au dépôt.
- Préserver toutes les licences et attributions existantes ; les sources originales de CorvusCore relèvent de **AGPL-3.0-only**, tandis que le contrat CorvusAPI reste sous **MIT** dans son dépôt.

## Construire et valider

Depuis la racine du dépôt :

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCORVUSCORE_BUILD_CORVUSAPI=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
bash tools/check_external_frontend_refs.sh
```

Pour travailler seulement sur les moteurs et le SDK, la configuration peut omettre `-DCORVUSCORE_BUILD_CORVUSAPI=ON`.

Les suites de conformité publiques se téléchargent **séparément** avec leurs scripts et contrôles d'empreintes :

```sh
cores/gb/tests/fetch_test_roms.sh build/gb-test-roms
cores/gba/tests/fetch_test_roms.sh build/gba-test-roms
cmake -S . -B build -DBUILD_TESTING=ON -DCORVUSCORE_BUILD_CORVUSAPI=ON \
  -DCORVUSCORE_GB_TEST_ROMS=build/gb-test-roms \
  -DCORVUSCORE_GBA_TEST_ROMS=build/gba-test-roms
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

- Toute correction matérielle ou de compatibilité doit être accompagnée d'un **test de régression** adapté, ou d'une explication précise de son absence.
- Les modifications du SDK/CorvusAPI doivent couvrir création, chargement, frames audio/vidéo, boutons, états et batterie selon les capacités concernées.
- Faire passer les tests synthétiques et les tests du système modifié ; utiliser **ASan/UBSan** lorsque la modification touche à la mémoire ou à la sérialisation.
- Ne jamais affirmer qu'un test, une compilation ou une vérification sur appareil a réussi sans l'avoir réellement exécuté. Signaler les limites de validation.
- Garder la compatibilité Linux, Windows, macOS et Android visée par les workflows ; ne pas introduire d'API dépendante d'une plateforme dans les moteurs.

## Organisation du travail

1. Lire les fichiers pertinents et repérer les appels entre moteurs, SDK et CorvusAPI avant de coder.
2. Décrire l'effet attendu sur les APIs, le comportement matériel, les sauvegardes et les frontends.
3. Modifier la couche propriétaire du comportement ; éviter les raccourcis qui contournent CorvusAPI.
4. Ajouter ou actualiser les tests puis exécuter les vérifications adaptées.
5. Mettre à jour la documentation et la section « Non publié » de `CHANGELOG.md` pour les changements notables.
6. Présenter un résumé factuel des fichiers modifiés, des tests lancés et des limites restantes dans la pull request.

Les tags et les artefacts de release doivent correspondre à un commit qualifié. Les versions courantes distribuent les moteurs, le SDK et les modules CorvusAPI ; ne pas recréer de pipeline lié à un ancien frontend externe.

Pour les conventions de contribution, consulter aussi `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md` et `NOTICE.md`.
