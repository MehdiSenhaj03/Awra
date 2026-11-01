# VOLTA — document de conception

## Pitch
Action d'arène en 3D vue plongeante, explosive et lisible, où la musique est le système de jeu : tout tombe sur la grille du tempo, et jouer en rythme rend plus fort, plus rapide, plus riche.

## Boucle
Partition (choix du mouvement) → Préparation (arme, Harmoniques, atelier) → Mouvement (3 parties avec points de reprise, ou Chef d'orchestre) → Résultats (note D…V, Notes gagnées) → déblocages.

## Systèmes
- **Horloge** : battement calculé à partir des images de simulation (jamais ralenti par le hit-stop ni la Surcharge). Fenêtres : PARFAIT ±70 ms, BIEN ±120 ms ; calibrage de latence dans les options.
- **Armes** : Prisme (doubles-croches, accent sur le temps, salve de 5), Accord (croches, gerbe), Lance (charge par temps, relâcher sur le temps), Arc (chaîne), Essaim (missiles sur le temps). Trois niveaux chacune.
- **Ruée** : 2 charges, invulnérable ; sur le temps, la charge est rendue et une onde blesse autour.
- **Pulsation** : renvoie les projectiles proches vers les ennemis (dégâts ×), plus large et étourdissante sur le temps ; ratée, elle coûte un temps de recharge plus long.
- **Surcharge** : jauge de Résonance (éliminations, parfaits, frôlements, renvois) ; 16 temps de monde ralenti, tirs multipliés, score ×2.
- **Style** : rang D→VOLTA (×1 à ×8), baisse lente sans action, −2 rangs par coup reçu.
- **Harmoniques** (16) : Sillage, Tenuto, Forte, Aimant, Reprise, Frôlement, Trille, Ampleur, Point d'orgue, Harmonie, Allegro, Contrepoint, Temps fort, Sourdine, Virtuose, Métronome.

## Progression
| Secteur | Tempo | Nouveautés | Chef | Récompense |
|---|---|---|---|---|
| 1. Le Seuil | 128 | déplacement, tir en rythme, ruée, Pulsation (après 1.1) | Ostinato | Surcharge, Accord |
| 2. Le Jardin de Verre | 136 | Miroirs, Échos | Crescendo | Lance, 3e emplacement |
| 3. La Fonderie d'Échos | 140 | Cloches, Remparts, Tisseuses | Fugue (jumeaux) | Arc, mode Encore |
| 4. L'Océan de Mercure | 118 | Yeux, Chœurs | Rubato (tempo variable) | Essaim, 4e emplacement |
| 5. La Ville Muette | 150 | tout, ruelles étroites | Staccato | — |
| 6. Le Cœur du Silence | 160 | tout, élites | Fermata (la musique disparaît) | les fins |

## Chefs d'orchestre
Trois phases chacun, attaques déclenchées sur la grille rythmique, respiration d'une mesure entre les phases, effondrement final.
- **Ostinato** : boucles répétées, anneaux qui s'alignent une mesure sur deux (cœur exposé), anneau à trou mouvant.
- **Crescendo** : volées qui grossissent à chaque mesure, cristaux qui éclatent s'ils mûrissent, rideau à trou en phase 3.
- **Fugue** : deux jumeaux ; le second rejoue la mesure du premier en miroir ; chaîne d'or en fusion qui balaie ; le survivant joue en solo.
- **Rubato** : serpent de chrome, plongées et résurgences, tempo ralenti puis accéléré selon la phase.
- **Staccato** : sauts entre ancres, rafales courtes, grilles de lasers annoncées une mesure à l'avance, leurres.
- **Fermata** : attraction, notes tenues jusqu'à la mesure, silence total en phase 2 (le joueur joue seul la musique), puis le chant de tous les Chefs.

## Durée estimée
Premier parcours : 3 à 5 h (24 mouvements de 1 à 3 min, recommencements, boss). Maîtrise : notes S/V sur 24 mouvements, 18 mémoires, Harmoniques et améliorations (≈ 25 000 Notes), mode Encore sans fin. Estimation non mesurée sur des joueurs réels.
