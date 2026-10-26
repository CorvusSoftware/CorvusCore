# Organisation de l'émulation Game Boy Color

Le moteur GB de **CorvusCore** contient encore une implémentation commune des
modes matériels DMG et CGB, sélectionnés notamment par l'en-tête de cartouche.
`cores/gbc/` expose le moteur couleur et héberge les composants spécifiques
déjà extraits, sans dupliquer le pipeline matériel commun.

Les zones **APU, CPU, cartouche, DMA et mémoire** demeurent implémentées dans
les composants existants tant qu'une extraction séparée n'a pas été validée.
Il n'est pas nécessaire de maintenir des répertoires vides pour ces futurs
chantiers. Une extraction devra préserver les cycles et réussir les tests de
parité GB/GBC, tout en évitant les dépendances cycliques CMake.

La documentation technique du PPU couleur est conservée dans
`ppu/README.md`.
