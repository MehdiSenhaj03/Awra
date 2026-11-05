# MASCARADE

> Cent masques. Un seul ment. Une seule balle.

Venise, carnaval de 1890. Au Palais Vermeil, un espion — le Corbeau — se cache parmi une centaine d'invités
masqués et doit accomplir des missions discrètes. Dans la galerie, la Lynx guette la foule avec un fusil à
lunette et une seule balle. Toute la partie tient dans une question : *qui bouge comme quelqu'un qui a quelque
chose à faire ?*

## Modes
- **La Saison** (solo) : neuf soirées dans trois lieux (le Salon Vermeil, la Serre aux Orchidées, le Foyer de
  l'Opéra). On joue tour à tour l'espion contre une tireuse IA, et la tireuse contre un espion IA, qui
  deviennent plus fins de nuit en nuit. Récit à double point de vue, briefings de la Dogesse, cinématiques.
- **Le Contrat** (1 contre 1 en ligne) : l'un est l'espion, l'autre la tireuse ; les rôles s'inversent à chaque
  manche. L'hôte choisit le lieu ; le port s'ouvre automatiquement (UPnP) quand la box le permet (port UDP 47640).
- **Le Bal des Assassins** (2 à 4 joueurs, un seul écran) : chaque joueur est un invité parmi cent. Repère les
  autres joueurs à leur façon de bouger et poignarde-les ; poignarder un innocent te démasque.

## Commandes
| | Clavier / souris | Manette |
|---|---|---|
| Espion : marcher | WASD (ZQSD sur AZERTY) / flèches | stick gauche |
| Espion : se fondre / accomplir (maintenir) | E / Espace | A |
| Espion : presser le pas (ça se remarque) | Maj | RB |
| Tireuse : viser | souris | stick droit |
| Tireuse : épauler (lunette) | clic droit / Q | LB |
| Tireuse : tirer (une balle, lunette épaulée) | clic gauche / F | A |
| Tireuse : marquer un suspect | clic molette / R | Y |
| Pause | Échap | Start |

Bal des Assassins : joueur 1 = WASD, E se fondre, Maj presser, Q poignarder ; joueur 2 = flèches, Entrée, Ctrl,
`/` ou `.` ; joueurs 3-4 = manettes (A se fondre, X poignarder, RB presser). Toutes les touches se changent dans
Options › Commandes.

## Conseils
- Espion : les invités ne s'arrêtent qu'aux endroits intéressants (buffet, tableaux, statues, conversations,
  valse, fenêtres), ne reviennent pas sur leurs pas et ne courent jamais. Fais tes missions quand le point
  rouge regarde ailleurs ; une valse ou un groupe couvre bien un geste.
- Tireuse : chaque mission accomplie fait vaciller une bougie à l'endroit — tu sais *où* et *quand*, pas *qui*.
  Marque les suspects, croise les indices, et ne tire que lorsque tu es sûre : abattre un innocent fait perdre.

## Technique
Godot 4.4, GDScript typé. Simulation 60 Hz en entiers (positions en 1/256 de case, hasard xorshift à graine
fixe) : identique sur chaque machine, donc réseau en lockstep (seules les entrées voyagent). Foule par champs de
distance vers chaque point d'intérêt, évitement local par grille. Rendu 3D stylisé (invités en MultiMesh, masques
d'un atlas peint), musique (valse en ré mineur en trois arrangements + couche de tension synchronisée) et effets
synthétisés hors ligne.

Tests : `godot --headless --path . -s res://tests/<test>.gd` — `ai_match.gd` (équilibre IA), `season_balance.gd`
(les neuf nuits), `party_test.gd` (règles du bal), `net_test.gd -- host` + `-- join` (lockstep identique).
