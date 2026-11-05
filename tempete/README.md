# TEMPÊTE

> Le ciel ne prévient qu'une fois.

Un jeu de chasseurs d'orages. Une équipe de 1 à 4 traverse les Grandes Plaines à bord de « la Bailleuse », un
pick-up blindé, pour se placer au plus près de supercellules simulées : prendre la photo de sa vie, poser des
sondes sur la trajectoire d'une tornade, et rentrer vivant. Les orages sont un système physique lisible : radar de
réflectivité (le « crochet » qui annonce la tornade), radar de vitesses (le couplet de rotation), rafales, grêle,
foudre, tornades EF0 à EF5 qui serpentent.

## Modes
- **La Saison** (solo) : huit journées d'avril à juin, du risque marginal au haut risque. Briefing de l'équipe
  (June la pilote, Eli le météorologue, Margot la photographe, Teo l'ingénieur des sondes), Eli à la radio qui lit
  le radar et trace une route d'interception, un rival (Dutch Callahan) sur certaines journées, le garage entre les
  chasses (blindage, moteur, sondes, téléobjectif).
- **Chasse libre** : une journée générée au risque choisi ; seul, **à deux sur un écran** (la manette conduit, la
  souris photographie dans une fenêtre à part, pose les sondes et trace l'itinéraire sur le radar), ou contre
  l'équipe rivale.
- **En ligne** : équipage à deux (l'hôte conduit, l'invité photographie, pose les sondes, trace la route) ou course
  aux clichés (un pick-up chacun, le meilleur dossier gagne). Lockstep, port UDP 47643 (UPnP automatique si possible).

## Le dossier
Pellicule de 24 vues ; le meilleur cliché de chaque catégorie compte (tornade, structure, foudre, grêle), noté selon
le sujet, le cadrage, la distance et la pluie qui le cache. Les sondes rapportent des données si la tornade passe
dessus. La grêle et les débris abîment le pick-up ; sous la tornade, c'est l'épave.

## Commandes
| | Clavier / souris | Manette |
|---|---|---|
| Conduire | ZQSD / WASD ou flèches | gâchettes (accélérer / freiner), stick gauche |
| Appareil photo | clic droit ou C | LB |
| Déclencher / zoom | clic gauche, molette | RB, gâchettes |
| Poser une sonde (à l'arrêt) | P ou Espace | Y |
| Radar vitesses / réflectivité | R ou Tab | Select |
| Caméra vers l'orage | L ou clic molette | clic stick droit |
| Itinéraire | clic sur le radar | — |
| Pause | Échap | Start |

## Technique
Godot 4.4 (Forward+). Simulation entière à 60 Hz (millimètres, angles en 1/4096 de tour, trigonométrie et CORDIC en
entiers) : identique sur Windows et Linux, réseau en lockstep — même les photos sont notées par la simulation.
Carte de 12 × 12 km générée (routes en damier, boue sous la pluie, rivière et ponts, villes, fermes), nuages,
rideaux de pluie et tornades par shaders, pluie et grêle en particules, musique et sons synthétisés hors ligne.

Tests : `godot --headless --path . -s res://tests/<test>.gd` — `ai_chase.gd` (une journée jouée par l'IA),
`trig_test.gd` (trigonométrie entière), `net_test.gd -- host` + `-- join` (lockstep identique).
