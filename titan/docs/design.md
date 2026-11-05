# TITAN — document de conception

> « Elle marche. Nous vivons sur elle. Un jour, nos enfants verront la Source. »

## Pitch
Un équipage nomade vit sur le dos d'Ama, une titane de pierre et de chair qui traverse la Mer de Cendres vers une
source légendaire. Sur son dos : un village, une forge-cœur à entretenir, deux tourelles à harpons, une tour de
vigie, et à sa tête le harnais du pilote. Le voyage dure trois générations : l'équipage vieillit et transmet ses
postes. Parfois, un titan sauvage surgit, et c'est un combat de colosses.

## Les postes
| Poste | Où | Ce qu'on y fait |
|---|---|---|
| Pilote | la tête | cap (éviter gouffres et éboulis), allure (halte, marche, grand pas), s'ancrer ; en combat : frapper, parer, saisir, s'ancrer |
| Harponneurs ×2 | les épaules | viser et tirer : charognards ailés, pillards sur leurs voiliers des sables, épaves à ramener (provisions), points faibles d'un titan sauvage |
| Mécano | la forge-cœur | attiser (chaleur, au prix de provisions) et purger (pression) : rester dans la bande verte ; éteindre les feux sur le pont |
| Vigie | la tour | voir loin (dangers annoncés plus tôt, tempêtes de cendres traversables), révéler les points faibles d'un titan sauvage |

Les personnages marchent sur le pont pour changer de poste. **Seul**, on prend n'importe quel poste à volonté (touches
1 à 5) et l'équipage IA tient les autres, avec des ordres simples. **À plusieurs** (2 sur un écran, ou 2 en ligne),
chacun son personnage.

## Le voyage
Neuf étapes en trois générations, sur une carte : à chaque nœud, choisir la route (ruines : épaves et charognards ;
forêt pétrifiée : geysers et chaleur ; tempête : raccourci dangereux ; oasis : repos et eau). La troisième étape de
chaque génération finit sur un titan sauvage. Entre les générations, des années passent : les aînés transmettent
leur poste à un enfant qui hérite d'un trait. La titane garde ses cicatrices.

## Le combat de colosses
Verbes de timing, jauge d'**équilibre** partagée : frapper (la chaleur du cœur décide de la force), parer (pendant
l'élan adverse), saisir (le titan sauvage est tenu : ses points faibles sont exposés), s'ancrer (contre une saisie
ou un choc). Une saisie + un harpon dans un point faible révélé par la vigie = coup dévastateur. Un titan à terre
perd son équilibre... et le combat.

## Modes
- **La Longue Marche** (campagne, solo ou à deux) : neuf étapes, trois générations, la Source au bout.
- **Duel de titans** : 1 contre 1 (contre l'IA, à deux sur un écran, ou en ligne).

## Technique
Simulation en entiers à 60 Hz (centimètres, angles en 1/4096 de tour, trigonométrie entière) : lockstep en ligne.
Rendu 3D : la titane modelée par morceaux (corps, pattes qui marchent, tête, bras), son village, la Mer de Cendres qui
défile, les créatures, les effets. Musique et sons synthétisés.
