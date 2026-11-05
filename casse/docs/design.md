# CASSE — document de conception

> « Tu as 40 secondes. Puis tout recommence, et celle que tu étais t'aide. »

## Pitch
Odile Varenne a volé une montre qui rembobine le temps de 40 secondes. À chaque boucle, ses versions
précédentes rejouent leurs gestes exacts à côté d'elle : ses **échos**. Un coffre se perce en équipe —
et l'équipe, c'est elle. On conçoit aussi ses propres coffres, on les partage par **code**, et on regarde
ses amis s'y casser les dents (puis on reçoit leur **code de replay**).

## Les trois façons de jouer
| Mode | Contenu |
|---|---|
| Solo — Campagne | 5 quartiers × 6 coffres, histoire racontée en dialogues peints. Médailles par coffre (échos utilisés, temps). |
| Solo — Coffre du jour | Un coffre généré par la date ; même coffre pour tout le monde ; code de score comparable. |
| Atelier (Architecte) | Éditeur complet ; on ne peut publier un coffre qu'après l'avoir percé soi-même (preuve de faisabilité, record de l'auteur affiché). Export en code `CASSE1:…`. |
| Coop à deux | Chaque boucle, deux échos jouent en même temps (même écran avec deux manettes ou clavier+manette, ou en ligne). |
| Duel en ligne | Les deux joueurs construisent un coffre en 6 minutes avec le même budget, échangent, et font la course pour percer celui de l'autre ; progression adverse visible en direct. |

## Règles de la boucle
- Durée L (24/32/40/48 s selon le coffre), simulation fixe 60 Hz, **entièrement en entiers** (positions en
  1/256 de case, angles en 1/256 de tour, table de cosinus figée) → déterministe sur toutes les machines,
  ce qui permet les replays, la coop en ligne en *lockstep* et les codes de score vérifiables.
- Chaque boucle : le monde repart de son état initial, les échos passés rejouent leurs entrées, le joueur
  joue l'écho suivant. Budget d'échos fixé par le coffre.
- Échec d'une boucle : un garde ou une caméra repère un écho (jauge de détection 0,3 s) → ALARME.
  On peut alors **rembobiner** à n'importe quel instant de la boucle courante et reprendre de là.
- **Paradoxe** : un écho dont le monde a changé (porte fermée, carte déjà prise) continue d'exécuter ses
  gestes à l'aveugle. Il est signalé (contour rouge, écart avec sa trajectoire d'origine) et peut se faire
  repérer. On peut réenregistrer n'importe quel écho passé.
- Victoire : un écho porte le butin jusqu'à la sortie.
- Ligne du temps : on peut prévisualiser (« scruter ») n'importe quel instant de la boucle avec tous les
  échos, car la simulation est déterministe.

## Pièces
Mur, sol, verre (bruyant), sortie · Porte simple · Porte à canal (plaque, bouton, levier, terminal) ·
Porte à badge (4 couleurs, badges ramassables et transmissibles) · Porte-forte à N leviers simultanés ·
Verrou temporel (ouvert entre T1 et T2) · Plaque de pression · Bouton (impulsion de D s) · Levier ·
Terminal (piratage 1 s : coupe caméras/lasers du canal 10 s) · Laser (canal) · Interrupteur de lumière
(zone sombre : vision des gardes réduite) · Coffre (2 s de crochetage) · Garde (ronde, cône de vision,
enquête au bruit, assommable par derrière, donne l'alerte s'il voit un collègue à terre) · Caméra
(balayage) · Pièce de monnaie (leurre sonore, une par écho).

## Musique
Chaque écho ajoute un instrument au morceau : l'écho 1 joue l'horloge et la contrebasse, le 2 la batterie
aux balais, le 3 le Rhodes, le 4 le vibraphone, le 5 les cuivres, le 6 la voix. Un coffre bien percé
sonne comme un orchestre complet.

## Direction artistique
Noir art déco rétro-futuriste, ville de Valmont la nuit. Jeu en 3D stylisée vue de dessus : marbre noir
réfléchissant, murs à liserés de laiton, néons, cônes de vision dessinés au sol, échos translucides de
couleurs néon avec leurs trajectoires. Illustrations peintes (portraits, décors de menus, cinématiques).
