# MASCARADE — document de conception

> « Cent masques. Un seul ment. Une seule balle. »

## Pitch
Venise, carnaval de 1890, le Palais Vermeil. Un espion — le Corbeau — se cache parmi une centaine d'invités
masqués et doit accomplir des missions discrètes (échanger une lettre, verser un poison, glisser un mot à un
contact…). Dans la galerie, la Lynx guette la foule avec un fusil à lunette et **une seule balle**. Tout le jeu
tient dans une question : *qui bouge comme une personne qui a quelque chose à faire ?*

## Modes
| Mode | Joueurs | Principe |
|---|---|---|
| La Saison (campagne) | solo | 9 soirées dans 3 lieux ; on joue tour à tour l'espion (contre une tireuse IA qui apprend à lire la foule) et la tireuse (contre un espion IA). Récit à double point de vue. |
| Le Contrat | 1 contre 1 en ligne | L'un est l'espion, l'autre la tireuse ; on échange les rôles à la manche suivante. |
| Le Bal des Assassins | 2 à 4 sur un écran | Chaque joueur est un invité et chasse un autre joueur sans savoir lequel des cent masques c'est. Poignarder un innocent vous démasque. |

## Règles du Contrat
- Durée : 3 min 30. L'espion doit accomplir 3 missions parmi 4 proposées.
- Chaque mission se fait à un endroit précis, en maintenant une action 1 à 3 s, avec un **tic** visible
  (geste de la main, posture). Quand une mission est accomplie, la tireuse en est **prévenue** (une bougie
  vacille à l'endroit) : elle sait *où* et *quand*, pas *qui*.
- La tireuse vise avec un point laser que tout le monde voit, peut **marquer** des suspects (marques visibles
  d'elle seule) et zoomer à la lunette. Une seule balle : abattre l'espion gagne, abattre un innocent perd.
- Les invités IA suivent des routines (buffet, tableaux, conversations, valse, balcons) ; l'espion peut faire
  les mêmes gestes pour se fondre.

## Profondeur
- Côté espion : imiter le rythme de la foule (on ne s'arrête qu'aux points d'intérêt, on ne revient pas sur
  ses pas, on n'accélère jamais), choisir l'ordre des missions, se servir des groupes et de la valse pour
  couvrir un tic, faire ses missions quand le laser regarde ailleurs.
- Côté tireuse : croiser les notifications de missions avec qui était là, surveiller les comportements
  anormaux, gérer le temps et le laser (le pointer, c'est aussi prévenir l'espion).

## Technique
Simulation 60 Hz en entiers (positions en 1/256 de case, hasard xorshift graine fixe) : identique sur chaque
machine, donc réseau en lockstep (seules les entrées voyagent). Foule par champs de distance précalculés vers
chaque point d'intérêt, évitement local par grille spatiale. Rendu 3D stylisé, invités instanciés (MultiMesh).
