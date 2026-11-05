# GRIMOIRE — document de conception

> « Ce que tu traces, le monde l'écrit. »

## Pitch
Un monde d'enluminure vivante. On lance les sorts en les **dessinant** : chaque glyphe tracé à l'encre d'or
(souris, stylet, ou stick) devient un sort. La forme décide du sort ; la taille et la justesse du trait
décident de sa force ; deux glyphes enchaînés au même endroit se combinent. On **voit** le glyphe de
l'adversaire se former : un duel se gagne en lisant l'autre et en répondant plus vite.

## Les trois couloirs
L'arène a trois couloirs (haut, milieu, bas). Le glyphe part dans le couloir où on le trace (hauteur de son
centre). Un trait vertical droit (« Pas ») déplace son mage d'un couloir vers le haut ou le bas.

## L'alphabet (12 glyphes)
| Glyphe | Forme | Sort | Règles |
|---|---|---|---|
| Feu | zigzag en M | trait de feu | 12 dégâts ; éteint par l'Eau ; brûle double les Murs |
| Eau | vague ~ | vague | 8 dégâts, repousse ; éteint le Feu ; gelée par la Glace en mur de glace |
| Vent | spirale | tourbillon | 5 dégâts ; renvoie les projectiles adverses de son couloir (pas la Terre) |
| Foudre | éclair ϟ | foudre | frappe tout le couloir après 0,3 s ; 14 dégâts ; arrêtée net par un Bouclier |
| Terre | triangle | pic de terre | lent, 18 dégâts ; brise Bouclier et Mur ; insensible au Vent |
| Bouclier | cercle | bouclier | absorbe 20 dégâts pendant 6 s dans son couloir |
| Mur | carré | mur | 30 PV, bloque tout dans son couloir |
| Étoile | étoile à 5 branches | pluie d'étoiles | 3 étoiles, une par couloir, 5 dégâts chacune |
| Soin | croix + (2 traits) | soin | +15 PV en 2 s |
| Glace | flocon ✳ (3 traits) | gel | ralentit le couloir adverse ; touché : encre ralentie 3 s |
| Sablier | huit ∞ | temps | les sorts adverses ralentissent de moitié 4 s |
| Sceau | losange ◇ | contre-sort | le prochain sort adverse lancé dans les 3 s se dissipe |

Puissance = taille (×0,8 à ×1,25) × justesse (×0,7 à ×1,15). Encre : 100, +9/s ; chaque glyphe coûte 12 à 35.

## Combinaisons (deux glyphes dans le même couloir en moins de 1,2 s)
Feu+Vent = tornade de feu · Eau+Foudre = orage (les trois couloirs) · Terre+Feu = magma ·
Glace+Eau = lance de glace (traverse les murs) · Bouclier+Vent = miroir (renvoie le prochain sort) ·
Étoile+Soin = constellation.
En coop, les combinaisons se font **à deux** : mon glyphe et le tien, au même endroit, au même moment.

## Modes
| Mode | Joueurs | Principe |
|---|---|---|
| L'Apprentie (campagne) | solo | 10 chapitres : leçons, duels contre les maîtres, expéditions sur les Deux Rives. Un ou deux glyphes nouveaux par chapitre. |
| Deux Rives | coop 2 (un écran : souris + manette, ou en ligne) | Deux mages avancent chacun sur une rive du fleuve. On ne peut lancer des sorts que **sur l'autre rive** : je protège ta marche, tu protèges la mienne. Obstacles (ronces = Feu, brume = Vent, gouffre = Mur, ténèbres = Étoile), vagues de créatures d'encre, Bête du Fleuve à la fin. |
| Duel | 1 contre 1 (IA, en ligne, ou un écran souris + manette) | Deux mages, trois couloirs, deux manches gagnantes. |

## Technique
Reconnaissance de tracé par nuage de points ($P) : multi-traits, insensible au sens et à l'ordre, déterministe,
score affiché. La simulation de duel et de rives est en entiers à 60 Hz : en ligne, seuls les sorts lancés
(glyphe, couloir, puissance) voyagent en lockstep ; les traits en cours sont envoyés à part pour qu'on les voie
se former.
