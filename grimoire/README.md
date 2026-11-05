# GRIMOIRE

> Ce que tu traces, le monde l'écrit.

Un monde d'enluminure vivante où l'on lance les sorts en les **dessinant**. Chaque glyphe tracé à l'encre d'or
(souris, stylet ou manette) devient un sort : la forme décide du sort, la taille et la justesse du trait décident de
sa force, et deux glyphes enchaînés au même endroit se combinent. On voit le glyphe de l'adversaire se former :
un duel se gagne en lisant l'autre et en répondant plus vite.

## Modes
- **L'Apprentie** (campagne solo) : Ysolde, apprentie de Maître Aldric, poursuit l'encre échappée d'un livre
  interdit. Dix chapitres — leçons, duels contre la Sorcière des Cendres, le Moine des Marées, l'Horloger, Aldric
  et la Reine d'Encre, expéditions sur les Deux Rives et la Bête du Fleuve. Un ou deux glyphes appris par chapitre,
  avec une leçon guidée.
- **Duel** : contre un maître (quatre niveaux) ou à deux sur un écran (souris contre manette). Trois couloirs,
  manches au choix.
- **Deux Rives** (coop) : deux mages avancent chacun sur une rive du fleuve. Seul, tu tiens les deux rives ; à deux
  (souris + manette, ou en ligne), chacun ne peut tracer que **sur la rive de l'autre**. Deux glyphes au même
  endroit, de deux mains différentes : la combinaison la plus forte.
- **En ligne** : duel ou Deux Rives, en lockstep (port UDP 47642, ouverture automatique par UPnP si la box le permet).
- **Le Grimoire** : chaque glyphe, sa forme animée, ses règles, et une page pour s'exercer avec le score du tracé.

## Les douze glyphes
Feu (zigzag), Eau (vague), Vent (spirale), Foudre (éclair), Terre (triangle), Bouclier (cercle), Mur (carré),
Étoile (étoile à cinq branches), Soin (croix), Glace (flocon), Sablier (huit), Sceau (losange).
Combinaisons : Feu + Vent, Eau + Foudre, Terre + Feu, Glace + Eau, Bouclier + Vent, Étoile + Soin.

## Commandes
| | Souris | Manette |
|---|---|---|
| Tracer | clic gauche maintenu | stick gauche pour la plume, gâchette droite ou A pour tracer |
| Effacer le tracé | clic droit | B |
| Changer de couloir | trait vertical, ou W/S (Z/S), ↑/↓ | croix ↑/↓ ou gâchettes hautes |
| Grimoire | Tab | Select |
| Pause | Échap | Start |

## Technique
Godot 4.4 (rendu Compatibilité), GDScript typé. Reconnaissance de tracé par nuage de points ($P) avec un
départage par les angles vifs, multi-traits, insensible au sens et à l'ordre. Simulations de duel et de rives en
entiers à 60 Hz, identiques sur chaque machine : en ligne, seuls les sorts lancés voyagent. Musique modale et effets
synthétisés hors ligne.

Tests : `godot --headless --path . -s res://tests/<test>.gd` — `recog.gd` (précision de la reconnaissance),
`ai_duel.gd` (équilibre des maîtres), `rives_bot.gd` (expéditions), `input_draw.gd` (tracé souris de bout en bout),
`net_test.gd -- host` + `-- join` (lockstep identique).
