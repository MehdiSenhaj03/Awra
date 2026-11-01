# VOLTA

**Brise le Silence.** Jeu d'action en arène, en 3D temps réel, où chaque tir joue la musique.

Le Grand Silence a figé l'univers. Tu es Volta, la dernière note jamais jouée. Six Chefs d'orchestre tiennent la partition immobile ; il faut les vaincre, mouvement après mouvement, pour que la musique reprenne… jusqu'à la dernière mesure.

## Jouer

```bash
npm install
npm run dev        # http://localhost:5173
npm run desktop    # version Electron
npm run package    # build Linux (release/linux-unpacked)
npm test           # tests (mécaniques, 24 niveaux joués par un bot)
```

## Commandes (par défaut, modifiables)

| Action | Clavier / souris | Manette |
|---|---|---|
| Se déplacer | ZQSD / WASD, flèches | stick gauche |
| Viser | souris | stick droit |
| Tirer (maintenir) / salve parfaite (sur le temps) | clic gauche, J | RT / RB |
| Ruée (invulnérable) | Espace, Maj | A / LB |
| Pulsation (renvoi) | clic droit, K | LT / X |
| Surcharge | E, Q | Y / B |
| Pause | Échap, P | Start |

## Ce qui fait le jeu

- **Le rythme est la mécanique** : maintenir le tir joue une rythmique stable ; appuyer pile sur le temps déclenche la salve PARFAITE de chaque arme. Ruée, Pulsation et Surcharge ont aussi leur version « sur le temps ». L'anneau qui se referme autour de Volta donne le temps même sans le son.
- **Style** : rangs de D à VOLTA (×8 au score), qui chutent de deux crans à chaque coup reçu.
- **Profondeur** : 5 armes au rythme différent (doubles-croches, croches, charge par temps, chaîne, missiles), 16 Harmoniques à combiner dans 2 à 4 emplacements, améliorations d'armes, notes D→S puis V (VOLTA : S sans être touché et au rang maximal).
- **Contenu** : 6 secteurs, 24 mouvements dont 6 combats de Chefs d'orchestre à 3 phases, 13 types de Muets, 18 mémoires cachées (Échos dorés), 2 fins, mode Encore sans fin.
- **Rendu** : three.js + postprocessing, sol de verre noir à réflexions temps réel, obsidienne à contours néon (fresnel), milliers d'éclats instanciés, bloom HDR, aberration chromatique, ondes de choc, hit-stop.
- **Audio** : musique entièrement générative synchronisée à l'horloge du jeu ; les actions du joueur jouent des notes de l'accord en cours.

## Structure

- `src/game` — simulation déterministe à 60 Hz (aucune dépendance de rendu) : monde, joueur, armes, ennemis, Chefs (`bosses/`), niveaux, méta-progression.
- `src/render` — scène three.js, sol, modèles procéduraux, effets.
- `src/engine` — entrées, audio, synthés, musique.
- `src/ui` — écrans DOM (titre, partition, préparation, résultats, options, codex, cinématiques).
- `tests` — bot de validation (tous les niveaux sont terminables, chaque boss passe ses 3 phases) et tests de mécaniques.
- `art/ref` — images de référence des illustrations ; `public/img` — illustrations intégrées.

Illustrations réalisées avec l'aide d'outils de génération d'images, puis triées et validées une à une. Polices sous licence SIL OFL.
