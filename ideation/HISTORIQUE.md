# Historique d'idéation — cinq jeux

> Journal de travail tenu en autonomie. Chaque idée est d'abord proposée seule, critiquée, polie ;
> puis toutes les combinaisons sont examinées ; les fusions retenues consomment leurs idées sources et
> de nouvelles idées viennent compléter la liste jusqu'à cinq ; on recommence jusqu'à stabilité.

## 0. Diagnostic : « il manque un truc »

Avant d'inventer, il faut nommer ce qui manque aux sept jeux précédents (Contrebande, Grille, Refusé,
Phare, Carillon, Vael, VOLTA). Relecture honnête :

| Constat | Conséquence |
|---|---|
| Tous sont **solo**. | Personne n'appelle un ami pour y jouer. Zéro bouche-à-oreille, zéro moment « regarde ça ». |
| Tous sont **pilotés par le contenu** (niveaux, histoires, cartes écrites à la main). | Le joueur consomme ce que j'ai prévu ; une fois vu, c'est fini. |
| Leur verbe central existe déjà ailleurs (tirer en rythme, deviner des mots, sauter-taper). | On les décrit par comparaison (« un X mais avec Y »), ce qui est l'inverse d'un argument de vente. |
| Aucun ne fabrique d'**histoires de joueurs** : des situations que personne n'a écrites. | Pas de clip, pas d'anecdote racontée le lendemain. |
| Le *skill cap* existe (VOLTA) mais il est **solitaire** : on progresse contre un chiffre. | Progresser face à un humain, ou avec lui, motive dix fois plus. |

**Ce qui manque, en une phrase : un jeu qui crée des moments entre des personnes.**
Critères retenus pour juger chaque idée (note sur 5) :

- **H** — Accroche : se pitche en une phrase sans « mais en mieux ».
- **N** — Nouveauté du verbe central.
- **S** — Moments partagés (coop, versus, ou asynchrone) et potentiel de clips.
- **P** — Profondeur : marge de progression visible dès la première heure.
- **R** — Rejouabilité systémique (pas seulement du contenu).
- **F** — Faisabilité de très haute qualité avec mes outils (Godot, simulation déterministe, images
  peintes générées, 3D procédurale stylisée, audio synthétisé). Point faible connu : l'animation de
  personnages réalistes (critique de Vael). Points forts : systèmes, simulation déterministe, rendu néon.

Contraintes réalistes pour le multijoueur : pas de serveur. Donc (1) coop locale sur un écran avec
manettes, (2) en ligne pair-à-pair (ENet, ouverture de port automatique par UPnP, IP directe ou réseau
local), en *lockstep* déterministe sur les entrées — ma simulation 60 Hz déterministe le permet,
(3) asynchrone par **codes partageables** (un niveau, un défi, un fantôme tient dans une chaîne de texte).

---

## Tour 1 — cinq idées, une par une

### Idée 1 — COLOSSE (un seul corps, quatre esprits)
**Pitch.** Quatre joueurs pilotent *un seul* colosse de pierre : bras gauche, bras droit, jambes, cœur.
Ensemble ils affrontent d'autres colosses dans des villes qui s'effondrent.
- Solo : un « mode chef d'orchestre » où l'on passe d'un membre à l'autre au ralenti.
- Coop : chaque joueur tient un membre ; il faut se coordonner à la voix.
- Versus : colosse contre colosse, deux équipes de deux.

**Premier jet → critique.** Le syndrome *Octodad* : drôle vingt minutes, puis l'incontrôlable fatigue.
La physique des membres est illisible et le réseau d'une physique de *ragdoll* est un enfer.
Le solo est un pis-aller.

**Polissage.** Remplacer la physique libre par des **verbes de timing** : chaque membre a quatre actions
fortes (saisir, frapper, parer, ancrer) et une jauge d'**équilibre partagée**. Les combos naissent de la
synchronisation (bras A saisit + jambes ancrent + bras B frappe dans la fenêtre = projection).
C'est lisible, profond, et réseau-compatible (états discrets).

**Reste fragile.** Pas de *but* au-delà du combat ; le solo n'est pas naturel ; la rejouabilité repose
sur les adversaires.
**Notes :** H4 · N4 · S5 · P3 · R2 · F3.

### Idée 2 — SIGNAL (chacun voit une autre réalité)
**Pitch.** Une station orbitale hantée. Chaque joueur perçoit une couche différente du monde : l'un voit
les présences, l'autre la chaleur, le troisième entend les sons comme des formes, le quatrième voit le
plan de la station. Personne ne peut survivre seul ; il faut se parler.

**Premier jet → critique.** C'est *Keep Talking and Nobody Explodes* appliqué à l'horreur : formidable
en groupe, **mort en solo**. Et l'horreur se périme vite une fois les mécanismes compris.

**Polissage.** Le solo : un « résonateur » qui bascule entre les couches avec un coût (le temps passe,
la couche précédente s'estompe en mémoire, en surimpression fantôme). Les couches deviennent des
**systèmes** (la chaleur se propage, les sons rebondissent) plutôt que des énigmes écrites, et la station
est générée.

**Reste fragile.** Le cœur reste la communication, qui ne se rejoue pas ; l'horreur est un genre saturé.
**Notes :** H5 · N4 · S5 · P2 · R3 · F3.

### Idée 3 — ARCHITECTE (un contre tous, en direct)
**Pitch.** Un joueur est l'Architecte : il construit et réécrit le donjon *pendant* que les autres le
traversent, avec un deck de pièges, de salles et de monstres. Les héros sont en action pure.
- Solo : héros contre une IA architecte, ou l'inverse.
- Multi : de 1 contre 1 à 1 contre 4.

**Premier jet → critique.** *Evolve*, *Dead by Daylight*, *Dungeon Keeper* : le 1 contre N vit et meurt
par l'équilibrage, et exige un public en ligne permanent. Sans serveur ni communauté, il meurt.

**Polissage.** Rendre le duel **asynchrone** : l'Architecte conçoit un donjon, l'exporte en code, l'ami le
tente plus tard. On garde aussi un mode direct pour les soirées.

**Reste fragile.** Un donjon d'action générique ; le créatif se perd si l'outil d'édition est pauvre.
**Notes :** H4 · N3 · S4 · P4 · R4 · F4.

### Idée 4 — BOUCLE (ton équipe, c'est toi d'avant)
**Pitch.** Tu as 40 secondes. Quand elles s'écoulent, tout recommence et ta tentative précédente rejoue
à côté de toi, à l'identique. Au bout de six boucles, tu es une escouade de six versions de toi-même,
chacune exécutant le plan d'une boucle passée.

**Premier jet → critique.** *Quantum League* a fait ce concept en compétitif et s'est éteint : illisible,
trop de choses à suivre. *The Sequence*, *Cursed to Golf*… le concept est connu des curieux mais
jamais rendu grand public.

**Polissage.** Lisibilité absolue : vue de dessus, chaque écho a sa couleur et sa trajectoire future
dessinée au sol ; on peut **rembobiner** et réenregistrer une seule boucle. La difficulté vient des
**paradoxes** : un écho dont le plan est contrarié (une porte fermée par un autre écho) se désynchronise
et devient une menace.

**Reste fragile.** Il manque *pourquoi* on boucle et *contre qui* : les niveaux sont des énigmes écrites,
donc finissables.
**Notes :** H5 · N4 · S2 · P5 · R2 · F5.

### Idée 5 — CARAVANE (traverser un monde mourant)
**Pitch.** Un équipage de 1 à 4 personnes conduit une caravane-forteresse à travers un continent qui
se meurt. Le jour, on gère et on améliore ; à la nuit, les tempêtes et les bêtes attaquent.

**Premier jet → critique.** *Unrailed*, *FTL*, *Mortal Engines*, *Last Train Home*… bon genre, mais
la caravane est un décor.

**Polissage.** Les rôles d'équipage sont physiques : on court réparer, charger, viser. La carte se
choisit comme dans *FTL*. Le solo passe par un équipage d'IA à qui l'on donne des ordres.

**Reste fragile.** Quelle est la *star* du jeu ? Pas la caravane. Rien ne distingue la cinquième minute de
la cinquantième.
**Notes :** H3 · N2 · S4 · P3 · R4 · F4.

---

## Tour 1 — combinaisons (les 26 sous-ensembles de 2 à 5 idées)

| Combinaison | Verdict | Raison |
|---|---|---|
| 1+2 | ~ | Un colosse aux sens partagés (un joueur est ses yeux) : bon *ingrédient*, pas un jeu. |
| 1+3 | ✗ | Un joueur pilote un colosse-boss contre quatre : c'est *Evolve*, déjà mort. |
| 1+4 | ✗ | Boucles d'un colosse : illisible au carré. |
| **1+5** | **✓✓** | **La caravane EST le colosse.** L'équipage vit sur le dos d'un titan qui marche. Le colosse gagne un but et une durée de vie ; la caravane gagne une star. |
| 2+3 | ✗ | Architecte qui joue avec les perceptions : trop méta. |
| **2+4** | ~ | Chaque boucle, une autre perception ; tes échos agissent selon ce qu'ils voyaient. Brillant sur le papier, charge cognitive extrême. Écarté. |
| 2+5 | ✓ | Un membre d'équipage voit les menaces invisibles : excellent rôle pour 1+5. |
| **3+4** | **✓✓** | **L'Architecte conçoit un coffre ; le voleur a N boucles pour le percer.** Le duel asynchrone devient un *puzzle-duel* : concevoir, partager un code, voir son ami échouer puis comprendre. BOUCLE gagne un adversaire humain inépuisable ; ARCHITECTE gagne un verbe unique. |
| 3+5 | ✗ | Un architecte pose les embuscades de la caravane : marginal. |
| 4+5 | ✗ | Aucun lien naturel. |
| **1+2+5** | **✓✓** | Fusion de 1+5 enrichie du rôle « Vigie » venu de 2. |
| 3+4+2, 3+4+1, 3+4+5 | ✗ | Chaque ajout dilue la clarté de 3+4. |
| 1+2+3, 1+2+4, 1+3+4, 1+3+5, 1+4+5, 2+3+5, 2+4+5, 3+4+5 | ✗ | Pas de mécanique commune ; collage. |
| les 5 quadruplets et le quintuplet | ✗ | Collages sans cœur. |

**Fusions retenues.**
- **A = 1+2+5 → TITAN.** Un équipage de 1 à 4 vit sur le dos d'un titan qui marche à travers un monde
  mourant : pilote (jambes et cap), artilleurs (harpons, canons), mécanicien (chaleur, réparations),
  vigie (voit ce que les autres ne voient pas).
  - Le combat de colosse contre colosse devient le climax.
  - Le solo : on change de poste à volonté, et les autres postes sont tenus par une IA à qui l'on donne
    des ordres.
- **B = 3+4 → CASSE.** Concevoir des coffres, les partager par code ; percer ceux des autres avec ses
  propres boucles temporelles.

Il reste 2 idées, il en faut 3 nouvelles.

---

## Tour 2 — trois nouvelles idées

### Idée 6 — MASCARADE (se cacher dans une foule)
**Pitch.** Un bal masqué de deux cents invités, tous contrôlés par l'IA, sauf un ou plusieurs joueurs.
- L'Espion doit accomplir des tâches discrètes (voler un pli, empoisonner un verre, retrouver un
  contact) en *se comportant comme un invité*.
- Le Chasseur observe depuis la galerie et n'a qu'une seule balle.

**Critique.** *SpyParty* a prouvé la profondeur infinie de ce face-à-face (des joueurs y jouent depuis
dix ans), mais son rendu austère et son accès élitiste l'ont gardé confidentiel.
*Hidden in Plain Sight* et le mode *Wanted* d'*Assassin's Creed* ont prouvé le plaisir en groupe.
Le solo est le point faible.

**Polissage.**
- Solo : une campagne d'espion contre des chasseurs IA qui *apprennent* (leur soupçon se fonde sur des
  écarts statistiques réels : vitesse, trajets, regards).
- Versus 1 contre 1 en ligne et sur un seul écran : le chasseur tourne le dos à l'écran pendant que
  l'espion choisit son masque.
- Fête de 3 à 8 joueurs : plusieurs espions, plusieurs chasseurs.

La foule est un système (conversations, danses, buffet), donc chaque partie est différente.
Le *skill cap* se trouve dans la lecture humaine et l'art de mentir.
**Notes :** H5 · N4 · S5 · P5 · R5 · F4.

### Idée 7 — GRIMOIRE (dessiner ses sorts)
**Pitch.** Duels de mages où l'on lance les sorts en les **dessinant** (souris, stylet ou stick).
- La forme décide du sort ; la taille, la vitesse et la précision du trait décident de sa puissance.
- Combiner deux glyphes à la suite crée un troisième effet.

**Critique.** *Okami*, *Magicka*, *Scribblenauts*, *Arx Fatalis*… le dessin de sorts est connu mais
n'a jamais été le cœur d'un jeu de duel compétitif. Risques : la reconnaissance de geste injuste, et
la manette, moins précise.

**Polissage.**
- Une reconnaissance de forme déterministe et tolérante (algorithme $1/$P, transparent : on voit
  le score de chaque trait).
- Un alphabet court (douze glyphes) mais une grammaire combinatoire.
- Des duels 1 contre 1, une coop 2 contre boss, et une campagne solo d'apprenti.

**Reste fragile.** Pas de vrais « moments de groupe » au-delà du duel.
**Notes :** H4 · N5 · S4 · P5 · R4 · F4.

### Idée 8 — HÉRITAGE (une dynastie, une vie par partie)
**Pitch.** Chaque partie est la vie entière d'un héritier. Ses actes changent le monde, que la génération
suivante retrouve transformé. Ses enfants héritent de ses traits.

**Critique.** *Rogue Legacy*, *Crusader Kings*, *Wildermyth* : très bonne structure, mais une
*structure* sans verbe ; et le multijoueur, des familles d'amis qui s'allient, est confus.
**Notes :** H3 · N3 · S2 · P3 · R5 · F3.

---

## Tour 2 — combinaisons (A TITAN, B CASSE, 6, 7, 8)

| Combinaison | Verdict | Raison |
|---|---|---|
| **A+8** | **✓✓** | **Le voyage du titan dure des générations.** L'équipage vieillit, transmet ses postes ; le titan, lui, se souvient (cicatrices, mémoire de route, rancune contre les titans sauvages). L'héritage donne au titan un temps long et une raison de rejouer ; le titan donne à l'héritage son verbe. |
| B+6 | ~ | Un casse dans un bal masqué : tentant. Mais CASSE est asynchrone et réfléchi, MASCARADE direct et psychologique. La fusion tuerait les deux rythmes. Écarté ; CASSE pourra proposer des gardes et des invités comme pièces de construction. |
| B+7 | ✗ | Dessiner pour crocheter : gadget. |
| 6+7 | ✗ | Des mages cachés dans un bal : contradictoire (un sort se voit). |
| A+6, A+7, B+8, 6+8, 7+8 | ✗ | Pas de mécanique commune. |
| triplets et plus | ✗ | Collages. |

**Fusion retenue.** **A' = A+8 → TITAN** :
- une traversée générationnelle ;
- une campagne qui couvre plusieurs vies ;
- une coop où les amis rejoignent ou quittent l'équipage entre les étapes sans casser l'histoire,
  puisque les postes se transmettent.

Il reste 4 idées (A', B, 6, 7), il en faut une nouvelle.

---

## Tour 3 — une nouvelle idée

### Idée 9 — MIROIR (deux mondes, deux joueurs)
**Pitch.** Une aventure coopérative à deux : l'un vit dans le monde du jour, l'autre dans son reflet de
nuit. Les mêmes lieux, mais ce qui est mur chez l'un est vide chez l'autre, ce qui brûle chez l'un gèle
chez l'autre. On ne se voit qu'à travers les miroirs.
- Solo : une main par personnage, comme *Brothers*.

**Critique.** *It Takes Two* et *Split Fiction* prouvent l'énorme marché de la coop narrative à deux.
Mais tout repose sur des énigmes et des niveaux écrits à la main, donc sur la quantité de contenu.
**Notes :** H4 · N3 · S5 · P2 · R1 · F2.

## Tour 3 — combinaisons (A', B, 6, 7, 9)

| Combinaison | Verdict | Raison |
|---|---|---|
| 9+B | ~ | Deux voleurs dans deux versions du même coffre (jour/nuit), chacun avec ses boucles. Riche, mais CASSE est déjà complet ; double charge cognitive. Écarté. |
| **9+7** | **✓** | **Deux mages, deux mondes : on dessine dans son monde un glyphe qui agit dans l'autre.** Le dessin devient un langage *entre* joueurs : je trace un pont chez toi, tu traces un feu chez moi. GRIMOIRE gagne ses moments de groupe (sa faiblesse) ; MIROIR gagne un verbe systémique qui rejoue (sa faiblesse : le contenu écrit). |
| 9+A', 9+6 | ✗ | Sans lien. |
| triplets et plus | ✗ | Collages. |

**Fusion retenue.** **C = 7+9 → GRIMOIRE.**
- Duels de glyphes 1 contre 1.
- Mode « Deux Rives » : coop à deux, une rive chacun ; les sorts tracés chez l'un agissent chez
  l'autre, sur des expéditions générées.
- Solo : une campagne d'apprenti où l'on tient les deux rives tour à tour.

Il reste 4 idées (A' TITAN, B CASSE, 6 MASCARADE, C GRIMOIRE), il en faut une nouvelle.

---

## Tour 4 — une nouvelle idée

### Idée 10 — RUCHE (un royaume que des amis gouvernent ensemble)
Une cité-ruche, deux à quatre reines rivales. Rejeté avant polissage : la stratégie en temps réel
multijoueur demande une IA et un équilibrage énormes pour une accroche faible (H2).

### Idée 11 — TEMPÊTE (les chasseurs d'orage)
**Pitch.** Une équipe de 1 à 4 chasseurs d'orages traverse des plaines en véhicule pour se placer au
cœur de tempêtes simulées : tornades, grêle, foudre. Il faut y lâcher des sondes, prendre des clichés
et en ressortir vivant. Les tempêtes sont un **système physique** (pression, vent, rotation) jamais
deux fois pareil.

**Critique.** *Content Warning* et *Lethal Company* ont prouvé le jeu d'équipe « chaos + preuve à
ramener ». *Twisted Metal*, *BeamNG*… ici, la star est un phénomène naturel, ce qui est rare.
Risque : la physique de véhicule en réseau.

**Polissage.**
- Véhicule arcade déterministe et vue de dessus inclinée, donc réseau en lockstep.
- La tempête est une simulation lisible, avec des isobares au sol pour qui sait les lire : c'est le
  *skill cap*.
- Rôles : conducteur, navigateur (lit le radar), photographe, lanceur de sondes.
- Le solo : conduite et radar, avec un copilote IA.

**Notes :** H5 · N5 · S5 · P4 · R5 · F3.

## Tour 4 — combinaisons (A', B, 6, C, 11)

| Combinaison | Verdict | Raison |
|---|---|---|
| **11+A'** | ~→✗ | Un titan qui traverse les tempêtes : le titan *subit* déjà le climat ; doublon de fantasme (« voyager en équipage dans un monde hostile »). On ne les fusionne pas : on les **différencie**. TITAN = lent, épique, générationnel, combat de géants ; TEMPÊTE = court, nerveux, sessions de 20 minutes, chasse. |
| 11+B | ✗ | Sans lien. |
| 11+6 | ✗ | Sans lien. |
| 11+C | ✗ | Sans lien. |
| Tous les triplets et plus | ✗ | Aucun noyau commun. |
| Re-vérification A'+B, A'+6, A'+C, B+6, B+C, 6+C | ✗ | Déjà étudiées ; aucune n'améliore les deux parties. |

**Aucune fusion : la liste est stable.** Les cinq idées sont différentes par le verbe
(piloter ensemble, concevoir et percer, mentir, dessiner, chasser un phénomène), par le rythme
(générationnel, asynchrone, duel tendu, duel rapide, session de chaos) et par la forme sociale.

---

## Liste finale (stable après 4 tours)

| # | Titre | Verbe | Solo | Coop | Versus | H | N | S | P | R | F |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | **CASSE** | concevoir un coffre / le percer avec ses boucles | campagne de 40 coffres | voleurs à deux (boucles partagées) | duel asynchrone par code, duel en direct | 5 | 5 | 5 | 5 | 5 | 5 |
| 2 | **MASCARADE** | se fondre dans une foule / démasquer | campagne d'espion vs IA | — | 1v1 en ligne et sur un écran, fête jusqu'à 8 | 5 | 4 | 5 | 5 | 5 | 4 |
| 3 | **GRIMOIRE** | dessiner des sorts | campagne d'apprenti | Deux Rives (chacun un monde) | duels 1v1 | 4 | 5 | 5 | 5 | 4 | 4 |
| 4 | **TEMPÊTE** | chasser des orages simulés | conduite + copilote IA | équipe de 4 rôles | course aux clichés | 5 | 5 | 5 | 4 | 5 | 3 |
| 5 | **TITAN** | vivre sur le dos d'un géant en marche | postes à volonté + équipage IA | équipage de 4 | combat de titans à deux équipes | 5 | 5 | 5 | 4 | 5 | 3 |

**Ordre de développement** (qualité d'abord, du plus sûr au plus ambitieux) :
1. **CASSE**. Ma simulation déterministe est exactement ce que demandent les boucles ; les codes
   partageables donnent du multijoueur réel sans serveur. On y construit aussi le socle réseau
   (lockstep ENet + UPnP + réseau local + coop sur un écran) réutilisé par tous les suivants.
2. **MASCARADE**.
3. **GRIMOIRE**.
4. **TEMPÊTE**.
5. **TITAN**.

Chaque jeu est terminé, testé (bots, captures, réseau entre deux instances), puis exporté en zips
Linux et Windows avant de passer au suivant.

---

## Journal de développement

- **CASSE** — terminé : moteur à boucles, 22 coffres de campagne, atelier, coop et duel en ligne, audio, zips
  Windows/Linux envoyés.
- **MASCARADE** — terminé : foule simulée (cent invités, routines, valse), IA espion et IA tireuse équilibrées
  par simulation (≈50/50 à niveau égal, difficulté croissante sur les 9 nuits), rendu 3D (trois lieux,
  invités en MultiMesh avec masques peints), Saison à double point de vue (briefings, cinématiques), Contrat en
  ligne (lockstep vérifié : même état final sur deux instances), Bal des Assassins à 2-4 sur un écran,
  valse en trois arrangements + couche de tension, zips Windows/Linux envoyés.
  Écart assumé avec la fiche : la fête est à 2-4 joueurs (un écran, clavier ×2 + manettes) plutôt que 8.
