# TEMPÊTE — document de conception

> « Le ciel ne prévient qu'une fois. »

## Pitch
Une équipe de chasseurs d'orages (1 à 4) traverse les Grandes Plaines à bord de « la Bailleuse », un pick-up
blindé, pour se placer au plus près de supercellules simulées : prendre la photo de sa vie, poser des sondes sur
la trajectoire d'une tornade, et rentrer vivant. Les orages sont un **système physique lisible** : un radar
(réflectivité et vitesses), une rotation qui se forme, un « crochet » qui annonce la tornade. Savoir lire le
ciel, c'est le *skill cap*.

## Une chasse (≈ 9 minutes)
- Carte de 12 × 12 km générée (graine) : routes en damier (goudron rapide, terre qui devient boue sous la pluie),
  rivières et ponts, villes, fermes.
- 2 ou 3 orages naissent au sud-ouest et traversent la carte vers le nord-est. Chacun vit : développement, maturité
  (fenêtres tornadiques), dissipation. Noyau de pluie, cœur de grêle, rafales descendantes, tornade (EF0 à EF5,
  trajectoire qui serpente).
- **Photos** : pellicule de 24 vues. Mode appareil (vue du toit, zoom). Le meilleur cliché de chaque catégorie
  compte : tornade, structure (nuage-mur, vaisseau-mère), foudre, grêle. Le score dépend du sujet, du cadrage, de la
  distance, de la visibilité.
- **Sondes** : 3 « tortues » à poser à l'arrêt ; une tornade qui passe dessus = données (score selon la force et la
  précision).
- **Survie** : la grêle et les débris abîment le véhicule ; une tornade dessus = épave, chasse terminée.

## Modes
| Mode | Joueurs | Principe |
|---|---|---|
| La Saison (campagne) | solo | 8 journées d'avril à juin, du risque « marginal » au « haut risque » ; briefing météo, copilote IA (Eli) qui commente le radar et propose une route d'interception ; garage entre les chasses (blindage, moteur, radar, sondes, objectif). Une équipe rivale sur certaines journées. |
| Équipage | coop 2 à 4 (en ligne) ou 2 sur un écran (manette + souris) | Un véhicule, des rôles : pilote, photographe, navigateur (pose l'itinéraire sur le radar), ingénieur des sondes. |
| Course aux clichés | 1 contre 1 en ligne, ou contre l'équipe rivale IA | Deux véhicules dans le même orage ; le meilleur dossier l'emporte. |

## Technique
Simulation en entiers à 60 Hz (positions en millimètres, angles en 1/4096 de tour, table trigonométrique calculée
en entiers) : identique sur Windows et Linux, d'où un réseau en lockstep (seules les commandes voyagent, y compris les
photos : le cadrage est noté par la simulation). Rendu 3D (Forward+) : terrain, routes, fermes, nuages et tornades
par shaders, pluie et grêle en particules.
