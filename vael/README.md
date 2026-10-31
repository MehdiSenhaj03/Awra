# Vael

Action-exploration 2D en vue de côté (TypeScript, PixiJS/WebGL, Vite, Electron).

*L'étoile est tombée. On l'a enchaînée. Elle rêve encore.*

## Lancer

```bash
npm install
npm run dev
```

Build web : `npm run build` (dossier `dist/`). Version de bureau : `npm run desktop`. Paquet Linux : `npm run package`.

Démarrage direct dans une salle (développement) : `?room=c_nef&ab=dash,walljump`.

## Tests

```bash
npx vitest run
```

- `tests/world.test.ts` : salles sans chevauchement, chaque sortie mène à une ouverture, aucune sortie murée.
- `tests/mechanics.test.ts` : hauteurs de saut, lame plantée comme prise, escalade d'une falaise trop haute, rappel qui transperce, esquive parfaite.
- `tests/bosses.test.ts` : chaque gardien peut être battu par un robot et passe par ses trois phases.
- `tests/reach.test.ts` : explorateur du royaume avec la vraie physique du héros : toutes les salles, tous les objets, tous les gardiens et le cœur sont atteignables dans l'ordre prévu.

## Arborescence

- `src/engine` : boucle, entrées, physique, son procédural.
- `src/game` : héros, Lame Liée, scène, ennemis, gardiens, Arts, Sceaux, sauvegardes.
- `src/world` : format des salles et les 63 salles des 8 zones.
- `src/render` : rendu WebGL (terrain par shader, parallaxe, lumières, bloom, ondes de choc, particules).
- `src/ui` : titre, HUD, menus, carte, inventaire, boutiques, cinématiques, fins.
- `tools/` : découpe des planches d'animation (`sheet.py`), import des images (`import_img.py`), manifeste.
- `art/ref/` : images de référence données au générateur pour garder le même héros et le même style.

## Illustrations

Les illustrations ont été produites avec un générateur d'images, à partir de demandes détaillées et d'images de référence, puis vérifiées, découpées et alignées par les outils de `tools/`. Steam demande de le déclarer.
