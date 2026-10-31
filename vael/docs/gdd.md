# Vael — document de conception

*L'étoile est tombée. On l'a enchaînée. Elle rêve encore.*

Action-exploration 2D en vue de côté, exigeant et spectaculaire. Peinture numérique, lumière dynamique, effets lumineux (bloom, ondes de choc, ralentis).

## Univers

Il y a mille ans, une étoile est tombée. Les premiers rois de Vael ne l'ont pas laissée mourir : ils ont bâti leur royaume **dans son cadavre** et l'ont **enchaînée** pour boire sa lumière. Pendant des siècles, Vael a brillé.

Aujourd'hui l'étoile s'éteint. Sa lumière pourrit en **Pénombre**, une encre violette qui ronge les créatures et les pierres. Les habitants sont devenus des coquilles. Les chaînes grincent.

Vous êtes **la Porte-Étoile** : un éclat du cœur de l'étoile, détaché d'elle, qui s'est donné un corps. Votre arme est la **Lame Liée**, forgée dans une côte de l'étoile, reliée à vous par un fil de lumière. Vous descendez jusqu'au Cœur pour comprendre ce que l'étoile veut, et décider de ce qu'il faut faire des chaînes.

Le récit se lit dans le monde : stèles, fresques, personnages, objets. Trois fins selon ce que vous aurez compris et choisi.

## Le cœur du jeu : la Lame Liée

Votre épée n'est pas seulement une arme de corps à corps.

- **Lancer** : la lame part en ligne droite (horizontale, en diagonale ou vers le haut). Elle se plante dans les murs, les ennemis et les cristaux-ancres.
- **Plantée dans un mur**, elle devient une **prise** : on peut se tenir dessus. Dès le début, on grimpe des parois en lançant la lame.
- **Rappel** : la lame revient vers vous et **transperce tout sur son chemin**. Placer les ennemis entre soi et la lame, c'est le cœur du combat avancé.
- **Plantée dans un ennemi**, elle le **marque** : le rappel l'arrache avec de gros dégâts.
- **Saut d'Étoile** (déblocage) : vous vous téléportez sur la lame. Contre un ennemi marqué : **Exécution**.
- **Lien** (déblocage) : la lame plantée dans un anneau astral vous tire jusqu'à lui.

Sans la lame en main, vos coups sont des frappes de paume plus faibles : chaque lancer est un choix.

## Combat

- Combo de 3 coups, rapide, annulable par saut, dash ou lancer. Coup vers le haut, coup vers le bas en l'air (rebond).
- **Esquive parfaite** : un dash qui traverse une attaque au dernier moment déclenche l'**Éclipse**, un ralenti de 1,5 s, monde désaturé, vous en couleur.
- **Lumen** : jauge remplie en frappant (beaucoup plus avec l'Éclipse et l'Exécution). Elle sert à **se soigner** (maintenir) ou à lancer des **Arts**.
- **Arts** (bouton Art + direction) :
  - neutre — **Comète** : la lame part chargée et explose ;
  - haut — **Ascension** : tourbillon ascendant ;
  - bas — **Chute d'Astre** : plongeon et onde de choc.
  Chacun a une version supérieure à trouver. Plus tard : **Constellation**, l'art ultime à jauge pleine (temps figé, sept coups qui traversent l'écran).
- Arrêts sur image, ondes de choc, aberration chromatique, flash : chaque coup doit se sentir.

## Progression

| Zone | Accès | Gardien | Récompense |
|---|---|---|---|
| 1. La Couronne | départ ; escalade à la lame | Le Gardien Brisé | Sillage (ruée) |
| 2. Les Jardins Suspendus | gouffre d'épines (Sillage) | La Mère-Racine | Griffes de Racine (saut mural) |
| 3. La Forge Éteinte | cheminée de roche polie (Griffes) | Le Forgeron Creux | Saut d'Étoile |
| 4. Les Archives Englouties | grille au sol (lame à travers, puis Saut d'Étoile) | Le Bibliothécaire Noyé | Second Souffle (double saut) |
| 5. Le Bastion des Chaînes | bloc poli trop haut (double saut) | Les Geôliers | Lien (anneaux astraux) |
| 6. Le Firmament Inversé | vide où pendent des anneaux (Lien) | L'Oiseau-Comète | Sillage de Comète |
| 7. La Pénombre | cristal noir (Sillage de Comète) | La Reine de Pénombre | Météore, porte du Cœur |
| 8. Le Cœur | sol ouvert par la Reine | L'Enchaîneur | le choix |

Optionnel : le Veilleur Gris (duel, encoche de Sceau), l'Épreuve du Cœur (Constellation), l'Étoile Rêveuse (gardien secret, vraie fin).

La roche polie et les bords des salles repoussent la lame : on ne peut pas tout escalader avec elle. Le Saut d'Étoile ne rejoint qu'une lame plantée.

## Contenu

- 63 salles (environ 140 écrans) dans 8 zones, toutes reliées et vérifiées par l'explorateur automatique.
- 10 gardiens à trois phases (anticipations visibles, vacillement après une rafale de coups, mort au ralenti).
- 19 types d'ennemis : soldats creux, acariens de cristal, lanternes, prêtres voilés, chevaliers de Pénombre, ronces, spores, mantes, ouvriers de braise, enclumiers, étincelles, lecteurs noyés, anguilles, gardiens de cellule (bouclier), corbeaux, astrelets, ombres-mères, sentinelles de cristal.
- 7 Arts : Comète, Grande Comète, Ascension, Firmament, Chute d'Astre, Météore, Constellation.
- 16 Sceaux à équiper dans des encoches (3 à 7).
- Objets : éclats de vie (4 = une vie de plus), fragments de fiole (3 = une fiole de Lumen), fer d'étoile (forge de la lame, 4 niveaux), 8 Chants de l'Étoile, cartes.
- 6 personnages : Orsa la Cartographe, Maître Vey, Sœur Lunelle, le Veilleur Gris, Pip, Murmure ; 4 marchands.
- 16 fresques de récit, 3 fins : le Nouveau Lien, la Libération, l'Aube (8 Chants + Pip retrouvé 4 fois).
- Mode Supernova (dégâts reçus doublés), 3 emplacements de sauvegarde, touches reconfigurables, manette, français et anglais.

## Durée

Estimation (pas une mesure) : 5 à 7 heures pour une première partie qui va au bout, 8 à 10 heures pour tout trouver. Un robot invulnérable qui frappe sans esquiver bat chaque gardien en 15 à 40 secondes ; un joueur qui doit esquiver met plusieurs minutes et plusieurs essais.
