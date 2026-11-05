# TITAN

> Elle marche. Nous vivons sur elle. Un jour, nos enfants verront la Source.

Un équipage nomade vit sur le dos d'Ama, une titane de pierre et de mousse qui traverse la Mer de Cendres vers une
source légendaire. Sur son dos, un village : le harnais du pilote à la nuque, deux tourelles à harpons sur les
épaules, la forge-cœur au milieu, la tour de vigie, des cabanes, des voiles. Le voyage dure trois générations :
l'équipage vieillit et transmet ses postes. Parfois, un titan sauvage surgit, et c'est un combat de colosses.

## Les postes (touches 1 à 5)
| Poste | Rôle |
|---|---|
| **1. Pilote** (la tête) | obliquer pour éviter les gouffres, l'allure (Halte, Marche, Grand pas), s'ancrer avant un éboulement ; en combat : frapper, parer, saisir, s'ancrer |
| **2 et 3. Harpons** (les épaules) | abattre les charognards, couler les voiliers pillards, hisser les épaves (provisions), percer les points faibles d'un titan sauvage |
| **4. Forge-cœur** | attiser (chaleur, au prix de provisions) et purger (pression) : sans chaleur Ama ralentit, trop de pression et le cœur éclate |
| **5. Vigie** | voir plus loin (les dangers sont annoncés plus tôt), traverser les tempêtes de cendres, révéler les points faibles en combat |

Seul, on saute d'un poste à l'autre ; l'équipage tient les autres. Sans poste, on marche sur le pont pour éteindre
les feux et repousser les pillards qui grimpent par les pattes.

## Modes
- **La Longue Marche** (seul ou à deux sur un écran) : neuf étapes en trois générations. À chaque carrefour, choisir
  la terre (Mer de Cendres, Ruines englouties, Forêt pétrifiée, Tempête noire, Oasis) ; la dernière étape de chaque
  génération finit face à un titan sauvage (le Bélier, la Fournaise, le Roi des Cendres). Ama garde ses cicatrices ;
  entre deux générations, chaque héritier garde le don de son aîné ou apporte le sien.
- **Étape libre** : une terre, une difficulté, un titan au bout ou non ; seul ou à deux (écran partagé).
- **Duel de titans** : contre l'IA (trois tempéraments) ou à deux sur un écran.
- **En ligne** : une expédition à deux sur Ama (chacun son personnage) ou un duel. Lockstep, port UDP 47644 (UPnP).

## Commandes
| | Joueur 1 (clavier + souris) | Joueur 2 (clavier) | Manette |
|---|---|---|---|
| Bouger / obliquer / allure | ZQSD / WASD | flèches | stick gauche |
| Action (frapper, tirer, attiser, s'ancrer) | Espace ou clic gauche | Entrée | A / gâchette droite |
| Action secondaire (parer, purger) | Maj ou clic droit | / | B / gâchette gauche |
| Saisir | F | . | X |
| Postes | 1 à 5 | 6 à 0 | croix (haut, gauche, droite, bas) et Y |
| Viser (harpon) | souris | flèches | stick droit |
| Vue d'ensemble | Tab | — | Select |
| Pause | Échap | | Start |

## Technique
Godot 4.4 (Forward+). Simulation entière à 60 Hz (centimètres, angles en 1/4096 de tour, trigonométrie entière) :
identique sur Windows et Linux, réseau en lockstep. Les titans sont modelés et animés en code (marche à pas
plantés par cinématique inverse, poses de combat), le village, les terres, les dangers et les créatures aussi.
Musique (mode hijaz) et sons synthétisés hors ligne (`tools/audio-render.*`), textes dans `tools/texts.py`.

Tests : `godot --headless --path . -s res://tests/<test>.gd` — `ai_leg.gd -- <terre> <niveau> <boss 0/1> <graine>`
(une étape jouée par l'équipage IA), `ai_duel.gd -- <niveau> <n>` (combats de colosses), `net_test.gd -- host` +
`-- join` (lockstep identique).
