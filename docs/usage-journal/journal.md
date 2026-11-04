<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Journal d’utilisation d’Awra

Les entrées des 17 et 18 août ci-dessous reconstruisent rétrospectivement les
enseignements conservés de la conception d’Awra, des passes successives du
Showcase et du port de l’application de référence. Leur ordre est significatif, mais leur heure
n’avait pas été consignée à l’époque.

# 2026-08-17

## Heure non consignée — reconstruction — Awra — Fondation technique

Awra est parti sur une bibliothèque C17/GObject, GTK 4.22+, Meson et GIR. Ce
choix devait produire un contrat ABI naturel pour GTK, GtkBuilder, Blueprint,
C, Vala, Python/GI et de futurs bindings Rust. Les objets Wayland, le
coordinateur d’effets et les détails KWin devaient rester privés.

Le modèle initial séparait le contexte par display, les préférences de style,
les tokens, les matériaux immuables et les surfaces. `Solid`, `Translucent` et
`Frosted` exprimaient une intention ; un Frosted sans capacité native devait
devenir opaque plutôt que rester transparent sans blur.

## Heure non consignée — reconstruction — Awra — Limites honnêtes du blur

Le protocole `ext-background-effect-v1` permet de demander un effet pour une
région, mais pas de choisir son rayon, sa saturation ou son algorithme. Ces
paramètres appartiennent au compositeur. Awra devait donc promettre une
activation, une région et un fallback cohérent, jamais un résultat identique
entre tous les compositeurs.

Les popovers possèdent leur propre surface native et doivent recevoir leur
propre demande d’effet. Une surface imbriquée dans la même `GdkSurface` ne peut
pas créer un vrai backdrop-filter entre widgets frères.

## Heure non consignée — reconstruction — Showcase — Passer d’une galerie à une application

Le premier Showcase présentait beaucoup de composants mais ne répondait pas
encore à la question « à quoi ressemble une vraie application Awra ? ». La
sidebar a donc été transformée en navigation réelle vers Overview, Surfaces,
Controls, Inputs, Navigation, Overlays, Motion, Layout, Platform et
Diagnostics. Des patterns d’application complets ont été ajoutés en plus de la
galerie de composants.

La règle importante était que Showcase ne devait jamais recevoir de CSS local
ou appeler une API privée : tout problème générique devait être corrigé dans
Awra.

## Heure non consignée — reconstruction — Awra — Isolation visuelle et mode Light

Les contrôles GTK comportementaux laissaient initialement filtrer certains
styles du thème système et des providers USER, notamment dans les inputs,
switches, sliders, dropdowns et sous-widgets de scroll. L’isolation a été
reconstruite au niveau du provider Awra, sans supprimer les fonts, le scaling,
l’IME, l’accessibilité, le contraste ou les préférences système utiles.

Le mode Light manquait aussi de contraste. Les tokens de texte, contrôles,
états selected/pressed/disabled et focus ont été renforcés et couverts par des
seuils de contraste. Dark est resté la référence artistique principale.

## Heure non consignée — reconstruction — Références visuelles — Une grammaire moins « boxed »

Les captures de référence ont montré que le Showcase était trop composé de
rectangles bordés indépendants. La hiérarchie devait davantage venir de la
matière, de la profondeur, du tint, de l’espace, des highlights et de quelques
séparateurs utiles.

La sidebar ne devait plus présenter chaque destination comme une card. Les
éléments normaux devaient rester calmes ; hover et selected devaient produire
la hiérarchie. Le violet devait devenir un accent configurable et retenu, pas
l’identité entière du framework. `SYSTEM` devait être best-effort avec un
fallback `DEFAULT`.

## Heure non consignée — reconstruction — Awra — Premium Gap Audit

La sensation premium ne devait pas venir de davantage de glow, de gradients
décoratifs, d’ombres ou de bordures. Les axes retenus étaient la qualité de la
matière, la lumière, la quantité de wallpaper visible, les proportions, la
densité, la typographie, les formes de contrôle, la profondeur entre canvas,
chrome, contenu et surfaces flottantes, ainsi que des interactions plus
retenues.

Button, Switch, Slider, SegmentedControl, Input, NavigationItem et Floating
devaient acquérir une signature Awra reconnaissable sans perdre leur
comportement GTK accessible.

## Heure non consignée — reconstruction — Showcase — Unifier le header et la fenêtre

Une bande visuelle distincte entre la partie haute et l’application cassait
l’unité observée dans les références. `AwraHeader:blend-with-window` a été
introduit pour résoudre le même matériau que la fenêtre, sans verrouiller Awra
sur une composition unique de header ou de sidebar.

## Heure non consignée — reconstruction — Showcase — Calibration du Canvas

Le laboratoire Platform & Blur a permis d’ajuster les paramètres en temps
réel. La calibration adoptée comme point de départ a été un tint Canvas
`#232323`, une opacité de 60 %, environ 40 % de contribution du wallpaper et
un grain à 20 % du preset, soit un alpha effectif très faible d’environ
`0,0033`. Le blur natif reste simplement activé ou désactivé ; sa force demeure
contrôlée par le compositeur.

Ces valeurs sont des défauts de travail, pas des constantes artistiques
définitives.

## Heure non consignée — reconstruction — Showcase — Édition sémantique des matériaux

Chaque preset de Surfaces & Materials a reçu un éditeur public permettant de
modifier tint, alpha, radius, grain, outline, highlight et intention de blur.
Une modification de Floating doit mettre à jour toutes les surfaces Floating,
pas seulement la carte de démonstration. Les primitives bas niveau sont
restées disponibles pour préserver l’extensibilité.

Le rôle Chrome a reçu des coins sémantiques arrondis afin que les sidebars et
rails inset ne paraissent pas découpés à angle vif.

## Heure non consignée — reconstruction — Overlays — Les widgets étrangers cassent l’immersion

Un `GtkDropDown` natif rendait un popup qui ne ressemblait pas aux surfaces
Floating d’Awra. Le dropdown visible a été remplacé par une composition Awra
reposant sur un bouton et un `AwraPopover`, tout en conservant le modèle et
l’expression GTK comme delegates comportementaux.

Les menus imbriqués ont également révélé des problèmes de fermeture, de double
contour focus, de sous-menu persistant et de clic extérieur. Leur cycle de vie
a été centralisé et testé avec ouverture, réouverture, Escape, activation,
second clic et destruction du parent.

## Heure non consignée — reconstruction — Showcase — Sidebars, panels et contrôles

Les sidebars inset ont gagné un décalage par rapport au bord, de l’espace en
bas et une silhouette Floating plus proche des références. Les variantes
persistent, floating et edge-revealed ont été distinguées. Une sidebar qui
disparaissait complètement en responsive a montré qu’un layout adaptatif doit
toujours laisser un moyen explicite de navigation.

Les différentes implémentations visuelles de Switch ont été réunifiées. Les
panels Layers, dialogs, dropdowns et boutons ont été retravaillés pour éviter
les éléments superposés, les contrôles inexplicablement plus noirs et les
compositions sans intention claire.

## Heure non consignée — reconstruction — Awra — Le spacing doit être un système

L’objectif est qu’un nouveau composant soit raisonnablement beau avant tout
ajustement applicatif. Les espacements, insets, radii, typographies et recettes
de layout ont donc été centralisés dans des tokens et helpers publics. Une
application peut encore composer librement, mais elle ne devrait pas recopier
une collection de valeurs magiques.

## Heure non consignée — reconstruction — Application de référence — Le premier test externe n’était pas suffisant

Le premier essai de reproduction de l’application de référence ressemblait à une maquette
incomplète : densité trop faible, vues manquantes et structures très éloignées
du produit de référence. Le test a été repris comme un vrai port complet, avec
les données, huit pages, les détails fiche/attaques/talents/objets, la
comparaison, les types, les espaces de travail, les onglets et les dialogs.

Cette exigence a été utile : une bibliothèque n’est pas validée parce qu’elle
sait dessiner quelques cards, mais parce qu’elle peut soutenir un produit
dense sans obliger l’application à la contourner.

# 2026-08-18

## Heure non consignée — reconstruction — Application de référence — Un port public complet

L’application de référence a conservé la logique métier, SQLite, les datasets, les modèles,
les recherches et les factories GTK. La couche d’interface a été recomposée
avec les seules API publiques d’Awra et GTK, sans libadwaita, provider CSS
applicatif, API Wayland/KWin ou header privé.

Le port a déclenché des primitives réutilisables plutôt que des exceptions
locales : FilterBar, badges à couleurs de domaine, tabs dynamiques, fusion du
chrome et du canvas, rôles de données isolés et meilleurs overlays.

## Heure non consignée — reconstruction — Application de référence — Focus et thème utilisateur

Une règle réelle du thème utilisateur visant `.sidebar-pane:first-child`
créait une ombre opaque de 9 000 pixels uniquement lorsque la fenêtre était
active. Les rôles publics sont désormais traduits en classes privées
`awra-role-*`, ce qui évite les collisions avec les thèmes GTK.

La comparaison active/inactive de l’application de référence a ensuite été rendue strictement
identique. Awra a finalement choisi l’invariant le plus fort : fill, tint,
grain, radius, border, highlight, shadow et demande de blur ne changent pas
avec le focus.

## Heure non consignée — reconstruction — Awra — Ce que sont les rôles

Un rôle est une intention de présentation appliquée à un widget GTK existant,
par exemple une cellule de table ou une ligne de sidebar. Il permet de garder
le comportement GTK sans CSS applicatif. Il ne remplace ni un matériau, qui
décrit la matière d’une surface, ni un composant typé, qui possède une
structure et un comportement réutilisables.

Les rôles sont utiles comme pont, mais une composition répétée doit devenir un
widget Awra typé plutôt qu’une accumulation de chaînes de rôles.

## Heure non consignée — reconstruction — Awra — Roadmap issue de l’usage réel

L’expérience du Showcase et de l’application de référence a produit une roadmap couvrant stabilité,
lifecycle natif, isolation, ergonomie de construction, données, navigation,
accessibilité, performances, diagnostics, documentation, bindings, packaging
et gouvernance du design system.

La Phase A a durci les fondations, l’ownership, le backend natif, GIR et les
consumers. La Phase B a ajouté Page, PageHeader, Section, Toolbar, Form,
FormRow, états d’application, FilterBar, MasterDetail et DetailPane, puis a
migré Showcase et une première composition réelle de l’application de référence.

# 2026-08-19

## Heure non consignée — reconstruction — Awra — Données, navigation et contrôles stabilisés

La Phase C a ajouté DataView/DataColumn, SelectionToolbar, inputs avancés,
StatusBanner, ResponsiveBin, breadcrumbs, command palette, composants de
métriques et de métadonnées, Skeleton, audits RTL/accessibilité et budgets de
rendu. Showcase est devenu la documentation vivante de ces familles.

Les tests visuels couvrent Dark, Light, accent bleu et niveaux de gris. Le test
en niveaux de gris est important : l’identité doit survivre sans dépendre du
violet ou d’une autre couleur d’accent.

## Heure non consignée — reconstruction — Awra — Bindings Rust et projection sûre

Un consumer Rust expérimental permanent a été ajouté tôt pour détecter les API
qui se projetaient mal. La Phase D a ensuite généré `awra-sys` et la couche
gtk-rs sûre depuis le GIR officiel.

Ce gate a détecté l’absence de `Gio.ActionMap` sur `AwraWindow` et plusieurs
types GTK delegates qui auraient produit des méthodes commentées. La
configuration projette désormais modèles, factories, sorters, expressions,
enums et callbacks. Un test interdit le retour de stubs générés non supportés.

## Heure non consignée — reconstruction — Awra — Gate 1.0

Awra 1.0 a figé `Awra-1.0` et le SONAME `libawra-1.so.1`. Les suites finales
ont validé le build Wayland, le backend générique, ASan/UBSan avec 200 cycles
natifs, les consumers C/Vala/Python/Rust, l’isolation hostile, l’accessibilité
AT-SPI, l’installation, le mode subproject, la documentation et deux archives
source byte-identiques.

Le test KWin a activé le blur intégré seul, désactivé tous les effets pour
prouver le fallback, puis restauré exactement Better Blur DX et le blur intégré
dans leur état initial.

## Heure non consignée — reconstruction — Repository — Publication d’Awra 1.0

L’état validé a été publié sur le dépôt GitHub officiel, branche `main`, dans
le commit `2a93c7a` intitulé `Release Awra 1.0.0`. Les builds Meson, les
dépendances GIR téléchargées et les artefacts Cargo sont exclus du dépôt.

## 02:02 — Application de référence — Expérience de développement face à libadwaita

Pour une application GNOME classique, libadwaita reste aujourd’hui plus simple
grâce à sa maturité, sa documentation, son écosystème et son catalogue. Pour
une application transparente et fortement identitaire comme l’application de référence, Awra est
désormais plus naturel : on ne lutte plus contre des fonds opaques, des
sous-widgets privés ou des contournements CSS pour obtenir le verre, le blur et
une hiérarchie de surfaces cohérente.

Le passage libadwaita vers Awra est un travail modéré sur la couche UI, pas une
réécriture du produit. Les modèles GTK, factories, actions, sélection, IME,
données SQLite, navigation métier et callbacks peuvent rester. Il faut surtout
recomposer les fenêtres, headers, sidebars, rows, cards, overlays et pages
libadwaita avec les primitives sémantiques Awra.

Le port historique n’a pas été parfaitement fluide parce que l’application de référence a servi à
construire une partie du framework. Les difficultés rencontrées ont néanmoins
été transformées en API réutilisables plutôt qu’en hacks applicatifs.

## 02:02 — Application de référence — Dette technique encore visible

L’application de référence conserve actuellement un wrapper FFI Rust manuel d’environ 448
lignes, écrit avant les bindings générés. Il fonctionne et respecte l’API
publique, mais il ne représente plus le chemin recommandé. Le migrer vers le
crate Rust sûr supprimerait beaucoup de pointeurs, conversions GLib et blocs
`unsafe` locaux.

Le port contient aussi de nombreux rôles visuels publics. Ils sont isolés et
supportés, mais leur nombre montre que certaines compositions pourraient
maintenant migrer vers Page, Section, DataView, MasterDetail, DetailPane,
MetadataGroup, MetricRow et les autres composants typés ajoutés depuis.

## 02:02 — Awra — Priorité d’expérience développeur après 1.0

Le prochain gain important ne se situe plus principalement dans le blur ou le
rendu. Il consiste à rendre le chemin le plus simple également le meilleur :
bindings sûrs adoptés par les vraies applications, composants typés faciles à
découvrir, moins de rôles nécessaires, davantage d’exemples complets et une
documentation au niveau de maturité attendu d’un toolkit public.

Si l’application de référence était recommencé aujourd’hui avec Awra 1.0, les modèles et la logique
métier resteraient les mêmes, mais la couche UI demanderait moins de FFI, moins
de compositions manuelles et presque aucun combat contre le thème ou la
transparence. C’est le signe le plus concret qu’Awra va dans la bonne direction.

## 03:19 — Application de référence — La finition d’un vrai produit révèle les petits contrats

La comparaison côte à côte avec l’application de référence standard a montré que la matière et le
blur étaient déjà cohérents, mais que plusieurs petits contrats par défaut
restaient trop faibles : un widget central de header s’étirait verticalement,
un dropdown publiait sa sélection avant le clic, une page épinglée imposait une
icône Home, et une action réversible de tableau se transformait en gros carré
violet.

Ces défauts ont été corrigés dans Awra. Les widgets de header sont centrés par
défaut, les dropdowns ne valident qu’à l’activation, les tabs ont une identité
optionnelle et distribuent l’espace, les actions inline disposent d’un état
sélectionné discret, et les scrollbars de données se révèlent seulement quand
elles sont utiles. L’application de référence ne contient toujours aucun CSS local.

## 03:19 — Application de référence — Densité et couleur de domaine

Les types fiche ont servi de test sévère aux badges : une couleur de texte
qui alternait entre noir et blanc cassait la cohérence du vocabulaire, tandis
qu’un blanc forcé sur les anciennes teintes jaunes ou cyan ne respectait pas le
contraste. La solution retenue combine un contenu blanc stable fourni par
AwraBadge et une palette l’application de référence volontairement assombrie ; les dix-huit teintes
restent reconnaissables et dépassent toutes un ratio de contraste de 4,5:1.

## 03:58 — Application de référence — Les métriques peintes ne suffisent pas

La dernière comparaison géométrique a révélé un contrat GTK facile à manquer :
le padding CSS d’un widget final utilisant `GtkBinLayout` améliorait la peinture
du badge sans toujours augmenter sa taille mesurée. Les types semblaient donc
encore comprimés dans une vraie table. `AwraBadge` porte maintenant ses insets
sur son contenu mesuré, avec un test de taille minimale ; le blanc, le contraste
et la densité deviennent un contrat du composant plutôt qu’une heureuse
conséquence du parent.

## 03:58 — Application de référence — Une barre d’onglets est une géométrie complète

Partager la largeur entre onglets ne suffisait pas : le bouton interne gardait
sa taille naturelle et tassait titre et fermeture à gauche. `AwraTabView`
possède désormais une capsule continue, des séparateurs discrets et une cellule
interactive qui occupe réellement toute sa part. L’application de référence ne règle que son
contenu et son shell ; les formes, le focus, le hover et la distribution restent
la responsabilité d’Awra.

Le même passage a clarifié la frontière des responsabilités : Awra fournit les
métriques stables des badges, onglets, filtres et actions inline ; l’application de référence décide
que sa colonne Type comporte deux emplacements et qu’un type unique les couvre
pour être centré. C’est une composition métier, pas une règle universelle du
framework.

## 04:14 — Application de référence — Les tests ne remplacent pas la comparaison Dark

Une première correction des onglets passait les tests et paraissait correcte
dans la capture Xvfb claire, mais restait insuffisante face au rendu Dark réel :
le puits était trop sombre, le bouton épinglé trop serré et les séparations trop
longues. La navigation souffrait aussi d’une règle de neutralisation plus
spécifique qui annulait son hover.

La correction suivante a été validée avec des états de pointeur déterministes :
capsule translucide, bouton épinglé entièrement contenu, séparateurs verticaux
courts et hover de navigation visible. Ce cas rappelle que les gates techniques
prouvent la stabilité, mais qu’une décision esthétique doit aussi être comparée
dans l’apparence artistique de référence.

## 21:01 — Application de référence — Le débordement des onglets est un contrat de layout

Les captures complètes de l’application de référence Standard ont montré que la barre ne se
résumait pas à une capsule et quelques séparateurs. Avec peu de pages, chaque
onglet grandit jusqu’à une largeur maximale puis laisse respirer la fin du rail.
Avec davantage de pages, les cellules se contractent uniformément jusqu’à un
minimum. Au-delà, seul le rail dynamique défile : l’onglet épinglé reste fixe et
la fenêtre ne change jamais de largeur minimale.

`AwraTabView` possède maintenant un layout privé dédié à cette règle, un
viewport horizontal sans scrollbar décorative, la conversion de la molette
verticale en déplacement horizontal et la révélation automatique de la page
sélectionnée. Les fermetures non pertinentes restent invisibles et non
cliquables, mais conservent leur espace afin que les titres ne sautent pas. Les
séparateurs adjacents au hover et à la sélection disparaissent par opacité, sans
modifier la géométrie.

Le test de sortie ouvre quinze onglets dans une fenêtre de 820 px, prouve que la
fenêtre reste bornée, que le rail déborde et que la sélection du dernier onglet
fait défiler le viewport. Cette fois, le comportement observé est devenu un
invariant mesuré plutôt qu’une simple approximation visuelle.

## 21:21 — Application de référence — Un grand rayon ne constitue pas un cercle

La dernière comparaison a isolé une erreur récurrente : appliquer un rayon
« pill » à un bouton rectangulaire ne le rend pas circulaire. Le Home épinglé
peignait en plus son fond sur une cellule qui contenait aussi le séparateur ; sa
surface s’étirait donc même si l’icône seule semblait correctement dimensionnée.

`AwraTabView` distingue maintenant explicitement les formes. Un onglet épinglé
icon-only reçoit une allocation carrée de 36 px et peint uniquement ce carré ;
le séparateur reste un sibling stable et visible. Les fermetures contextuelles
reçoivent une allocation carrée indépendante de 22 px. Les tests mappés
vérifient largeur égale à hauteur et les tailles exactes, afin qu’une règle GTK
ou un futur changement de padding ne puisse plus les transformer silencieusement
en ovales. Les boutons texte restent, eux, des rectangles arrondis.

## 22:02 — Application de référence — La capsule ne doit pas posséder le séparateur

La vérification suivante a isolé une seconde erreur structurelle dans la barre
d’onglets : même avec des tailles carrées correctes, peindre le hover et la
sélection sur la cellule complète permettait encore à la capsule de consommer
l’espace du séparateur. Deux onglets accentués côte à côte pouvaient alors
sembler collés, et les deux extrémités du rail n’obéissaient pas au même inset.

Chaque cellule d’`AwraTabView` sépare désormais trois responsabilités : une
capsule interactive interne, sa fermeture circulaire éventuelle et un
séparateur sibling qui ne participe jamais au fond hover/selected. Home reste
un repère carré de 36 px dans une zone épinglée fixe ; les onglets dynamiques
gardent une respiration explicite entre capsules ; le dernier onglet supprime
seulement l’espace de son séparateur terminal. Les tests mappés prouvent le
centrage de l’icône à un demi-pixel, un espace minimal de 6 px entre deux
capsules accentuées et des marges gauche/droite symétriques à un pixel près.

Cette passe a aussi révélé un piège de validation : lancer l’application de référence avec le même
identifiant d’application activait l’instance déjà ouverte, laquelle conservait
l’ancienne bibliothèque Awra en mémoire. Les captures sont maintenant faites
sur un bus D-Bus privé et les smoke tests utilisent un répertoire de données
temporaire. Ils ne peuvent donc plus valider une ancienne instance ni modifier
le workspace réel du développeur.

## 22:25 — Application de référence — Le titre et le rail suivent l’intention utilisateur

La barre restait imparfaite sur deux comportements subtils. D’abord, les
transitions hover déclenchées lorsque les onglets défilaient sous le pointeur
replanifiaient la révélation de la page sélectionnée. Le rail revenait donc vers
la sélection et empêchait d’explorer les pages situées après elle. La révélation
automatique ne se produit maintenant qu’après un vrai changement de sélection ;
un geste de scroll annule immédiatement toute correction encore en attente.

Ensuite, le titre et la fermeture partageaient un layout horizontal. Même
invisible, la fermeture réservait de la largeur et décalait le texte. La capsule
utilise désormais un overlay : le titre possède le centre géométrique complet,
tandis que la fermeture carrée se superpose à son extrémité. Un renderer privé
applique un véritable masque alpha de 18 px aux titres trop longs au lieu d’une
ellipse dure. Le comportement s’inverse également en RTL.

Les tests couvrent maintenant le centre avant/après sélection, le débordement
du titre, le maintien du rail à son extrémité pendant un hover et l’isolation
face à un thème hostile. Une capture Dark avec vingt-deux onglets a confirmé
les mêmes proportions dans l’application de référence.

## 23:12 — Application de référence — La respiration prime sur le nombre d’onglets visibles

Une comparaison à géométrie de fenêtre comparable avec l’application de référence Standard a
montré pourquoi la nouvelle mécanique, pourtant plus robuste, paraissait encore
moins premium :
son pas minimal de 118 px faisait entrer un onglet supplémentaire dans le même
espace. Les séparateurs formaient alors une palissade, les noms ordinaires
atteignaient trop tôt leur fondu et la capsule sélectionnée perdait de la
présence. Le minimum est désormais de 132 px. Le rail défile donc légèrement
plus tôt, mais la fenêtre reste strictement bornée et l’exploration au-delà de
la sélection reste libre.

La même comparaison a révélé une réservation fantôme dans `AwraHeader` : le
groupe `GtkWindowControls` de début occupait de la largeur même lorsqu’il était
vide, et le conteneur de titres vide ajoutait encore un espacement. Awra retire
désormais ces groupes de la mesure, centre réellement le widget médian et lui
donne une largeur confortable qui se contracte sans imposer de nouvelle largeur
minimale à la fenêtre. Le bouton de workspace a aussi retrouvé la largeur
compacte de la référence.

Des tests mappés figent à présent le pas des onglets, le centrage du titre malgré
la fermeture superposée, le scroll libre après sélection et le centrage du
contrôle de header à un pixel près. Cette passe confirme qu’une sensation
premium dépend souvent de la quantité d’espace volontairement laissée vide,
pas d’un effet supplémentaire.

# 2026-08-24

## 02:19 — Application de référence — Le bord du viewport fait partie du composant

La comparaison finale avec la barre de l’application de référence Standard a mis en évidence trois
écarts encore liés à la primitive Awra : Home conservait une faible capsule au
repos, le rail réservait trop d’air vertical autour des onglets et une capsule
partiellement visible se terminait sur la coupe dure du `GtkScrolledWindow`.
Ces détails indépendants donnaient ensemble l’impression que les onglets
étaient posés dans la barre plutôt qu’intégrés à celle-ci.

Home est désormais totalement transparent au repos ; seuls hover et selected
font apparaître son cercle de 36 px. L’inset interne du rail est ramené à 1 px,
soit 2 px visibles par bord autour du repère Home une fois la bordure comprise.
Enfin, un wrapper de rendu privé applique un masque alpha de 20 px uniquement
aux bords où du contenu reste à parcourir. Une capsule hover ou selected peut
donc entrer et sortir du viewport sans créer de tranche verticale, y compris en
RTL, tandis que le scroller GTK conserve son comportement natif.

Le test géométrique échoue maintenant si l’inset vertical dépasse 3 px et
vérifie la présence du masque autour du scroller. La suite Awra (27 tests), les
tests et le lint Rust de l’application de référence ainsi que la frontière d’API publique restent
valides. Aucun style local n’a été ajouté à l’application de référence pour obtenir ce résultat.

## 02:41 — Application de référence — La couleur d’une catégorie se suffit à elle-même

Les badges de types révélaient un liseré clair que `AwraBadge` ajoutait
systématiquement aux couleurs personnalisées. Sur une suite dense de capsules,
ce rim fragmentait les aplats et donnait à chaque type l’aspect d’un petit
bouton. La référence l’application de référence montre au contraire que la silhouette, la couleur
pleine et le texte blanc suffisent à identifier la catégorie.

Le rendu `custom-color` est désormais entièrement borderless. La couleur et
les métriques restent contrôlées par Awra, le texte reste blanc et stable en
Light/Dark, mais aucun outline ou highlight décoratif n’est ajouté. Ce choix
vaut pour tous les vocabulaires de catégories utilisant la primitive publique,
pas seulement pour les types fiche.

## 02:56 — Application de référence — Une miniature de domaine reste une primitive d’onglet

Les onglets fiche gagnent leur sprite avant le titre pour accélérer le
repérage visuel. L’ancien contrat d’`AwraTabView` ne savait afficher que des
icônes nommées du thème. Awra accepte maintenant une `GtkImage` applicative
optionnelle et garde la maîtrise de sa taille, de son espacement, des états et
de l’overflow. L’application de référence alimente cette image avec son chargeur asynchrone sans
reconstruire la barre d’onglets.

# 2026-08-25

## 02:04 — Application de référence — Un dialogue court doit assumer sa hauteur naturelle

Les dialogues de workspace révélaient deux défauts génériques. Le bloc de
titres d’`AwraHeader` remplissait verticalement le header : avec un seul titre,
`GtkBox` le plaçait en haut tandis que le contrôle de fermeture restait centré.
Le texte et la croix appartenaient donc visiblement à deux lignes différentes.
Le bloc de titres possède maintenant un alignement vertical central explicite,
couvert par un test de géométrie mappée.

`AwraDialog` imposait aussi une hauteur initiale de 250 px, puis l’application de référence en
imposait une de 230 px à ses formulaires. Cette valeur laissait une bande vide
sous les actions dès que le contenu était court. Awra ne fixe désormais que la
largeur par défaut ; la hauteur provient du contenu. L’application de référence applique le preset
public `OVERLAY` à ses contenus et ne définit plus que la largeur souhaitée.
Des captures isolées des dialogues New Workspace et Manage Workspaces ont
confirmé des marges finales symétriques sans toucher aux données utilisateur.

## 02:23 — Application de référence — Une action discrète doit quand même répondre

Le formulaire Rename Workspace a révélé un conflit de spécificité dans la
grammaire des boutons. L'apparence Ghost annulait la couleur de fond après la
règle générique `:hover` : Cancel restait donc un simple texte, même sous le
pointeur. Le primaire simulait quant à lui son survol avec une ombre inset de
999 px, dont le clipping GTK produisait une seconde masse violette peu nette.

Awra définit maintenant explicitement les états hover et pressed de Ghost et
remplace le voile du primaire par un rim intérieur contenu dans sa silhouette ;
la couleur d'accent et son contraste restent stables. Dans les dialogues
L’application de référence, Cancel utilise Secondary : l'action est identifiable au repos, puis
renforce son plan au survol. Ghost reste disponible pour les actions inline
réellement calmes. La décision appartient donc au vocabulaire public du
framework et non à un style local du consumer.

## 03:44 — Application de référence — Un binding et des composants, pas une couche de traduction

La revue finale du port a isolé deux dettes qui n'étaient plus justifiées : une
façade Rust locale de 37 appels `unsafe` et des rôles visuels qui remplaçaient
encore des composants désormais disponibles. L’application de référence dépend maintenant du
crate sûr officiel `awra`; l'apparence, l'accent, les layouts, les widgets et
les rôles passent par cette projection GIR unique. Une gate interdit tout
retour de `extern "C"`, `awra-sys` ou d'un module `src/awra.rs` dans le projet.

La même passe remplace les rôles de composition par `AwraCard`, `AwraSection`,
`AwraFormRow`, `AwraFilterBar`, `AwraStatusBanner`, `AwraEmptyState`,
`AwraBadge`, `AwraStatBar` et `AwraMetricRow`. Le besoin tabulaire a conduit à
faire posséder à `AwraListView` son viewport et son header optionnel : l’application de référence
ne reconstruit plus une table avec un scroller, trois rôles et un container.
Les rôles restants décrivent uniquement le domaine (types, matchups, matrice,
relations, sprites) ou une ligne GTK dont le comportement doit rester GTK.

# 2026-10-23

## 01:45 — Application de référence — Un clavier, un lecteur et une zone de dépôt génériques

L’application de référence est une table de sons : chaque touche du clavier déclenche des
sons, avec des pistes, des formes d'onde, un point de départ et une
découverte de morceaux. Préparer son portage natif a montré que Awra n'avait
ni touche, ni clavier physique, ni capture de raccourci, ni forme d'onde, ni
ligne de lecture. Les reconstruire dans l'application aurait exigé du CSS
local, du dessin GSK et de la sémantique clavier que le contrat des
consommateurs interdit, et ces besoins ne sont pas propres à l’application de référence :
lanceur de macros, éditeur de raccourcis, lecteur de podcasts ou importateur
de fichiers les partagent.

Décisions confirmées :

- `AwraKeyCap` peint la matière depuis les tokens (puits pour une touche
  libre, cap pour une touche liée) ; teinte, bordure de catégorie, badge,
  sélection, impulsion de lecture et filtrage ne sont que des états de
  peinture, statiques en mouvement réduit.
- `AwraKeyboardView` identifie les touches par `KeyboardEvent.code`, suit la
  disposition logique active via GDK (AZERTY affiche bien `A` sur `KeyQ`),
  garde une géométrie physique non miroir en RTL et réutilise
  `GtkSelectionMode` plutôt qu'un nouvel enum.
- Les tableaux et listes passent par des `strv`, des tableaux annotés et
  `GdkFileList` : Python, Vala et Rust reçoivent des types natifs sans
  surcouche.
- `AwraDropZone` émet `browse-requested` au lieu d'embarquer un sélecteur de
  fichiers : la règle des deux scénarios démontrés reste valable pour les
  pickers.
- Les toasts gagnent une action (« Annuler ») liée à une `GAction`, sans
  changer `show_message()`.

À tester dans l’application de référence : le glisser-déposer réel sur les touches (GTK
n'offre pas de synthèse publique d'un drop) et la capture de raccourcis
lorsque des accélérateurs d'application sont actifs.

## 02:10 — Application de référence — Premier assemblage de l'application réelle

L'application complète (clavier, inspecteur, pistes, ajout de sons, Découvrir,
Mood AI, Bibliothèque, Réglages) a été assemblée sur les nouveaux composants
puis pilotée pour de vrai sous X11 (Xvfb + XTEST) avec le profil d'un
utilisateur. Quatre défauts génériques sont apparus et ont été corrigés dans
Awra plutôt que contournés :

- **Boutons dans le suffixe d'`AwraActionRow` : confirmé, corrigé.** La rangée
  est un `GtkButton`, dont le geste de clic travaille en phase de capture : il
  réclamait chaque clic avant que les boutons du suffixe ne le voient. Un
  « + » ou une corbeille posés en suffixe ne réagissaient jamais. Le geste
  passe en phase de bouillonnement ; un test vérifie la phase.
- **Légendes de clavier sur un backend sans keymap fiable : corrigé.** Sous
  Broadway, `gdk_display_translate_key()` renvoyait des keyvals sans caractère
  et la vue affichait `0xa`, `0xb`… Une touche de disposition dont la
  traduction n'est ni imprimable ni morte garde désormais sa légende physique.
- **Traductions jamais chargées : corrigé.** Awra n'appelait pas
  `bindtextdomain()`, si bien que le domaine `awra-1` ne pouvait venir que du
  répertoire par défaut du système. `awra_init()` le lie au préfixe
  d'installation, ou à `$AWRA_LOCALEDIR` (exporté par `meson devenv`) pour un
  arbre de build. Le catalogue français est ajouté.
- **Focus volé par `GtkFlowBox` : leçon côté application.** Un
  `AwraShortcutCapture` placé dans un `GtkFlowBox` démarrait l'écoute puis
  l'annulait aussitôt : l'enfant de la flowbox prenait le focus au même clic.
  Rien à changer dans Awra, mais à retenir : pas de contrôle interactif dans un
  enfant de flowbox focusable.

Ce qui a bien marché sans retouche : `AwraEdgePanel` comme panneau d'ajout
flottant, `AwraMediaRow` pour le dock des pistes, `AwraWaveformView` en flux
progressif, les toasts avec action « Annuler » liés à `win.undo`, et la palette
de commandes construite depuis un `GMenuModel`. Aucune ligne de CSS locale
dans l'application.

Encore à vérifier sur un vrai bureau Wayland : le flou natif, le
glisser-déposer de fichiers sur les touches et la capture de raccourcis
pendant que la détection globale du clavier est active.

## 03:03 — Application de référence — Seconde passe de finition après un vrai retour

Le premier retour d'utilisation réelle a été sévère : icônes trompeuses,
panneau d'ajout illisible sans flou, espacements irréguliers, artefact au clic
sur une touche. Une partie venait de l'application, une partie d'Awra.

- **Surfaces superposées sans flou : corrigé.** Une surface de rôle flottant,
  popover, menu, dialogue, infobulle ou HUD posée à l'intérieur d'une fenêtre
  n'a pas de fenêtre compositeur propre : le flou du compositeur ne la
  traverse pas, et le contenu de la fenêtre se lisait au travers. Ces surfaces
  basculent désormais sur le rendu opaque de repli ; seule la surface racine
  de la fenêtre garde le verre.
- **Formulaires mal alignés : corrigé.** Les libellés d'`AwraForm` partagent
  un `GtkSizeGroup` (colonne commune, largeur minimale), le champ s'aligne sur
  la ligne de base du libellé, et seuls les champs texte, listes déroulantes
  et zones de texte s'étirent ; interrupteurs, compteurs et captures de
  raccourci gardent leur largeur naturelle au lieu de remplir la ligne.
- **Compteurs trop larges : corrigé.** Un `AwraSpinControl` dimensionne son
  entrée d'après le nombre de chiffres de sa plage.
- **En-tête qui imposait une largeur de fenêtre : corrigé.** `AwraHeader`
  réclamait trois fois la plus large de ses zones pour rester symétrique ; la
  fenêtre ne descendait pas sous 1 560 px. Le minimum vaut maintenant la somme
  des zones, et le centre se décale s'il ne peut plus être centré.
- **Légendes de touches tronquées : corrigé.** Une légende trop longue pour sa
  touche (« Échap » à petite taille) est réduite jusqu'à 65 % plutôt que
  coupée par des points de suspension.
- **Écart des touches de fonction :** ramené à un quart de touche, plus proche
  d'un vrai clavier.

Côté application, sans rien à changer dans Awra : un bouton à bascule dont
seule l'icône dit l'état (le micro de l'écoute) ne se comprend pas ; un
libellé d'état et une bannière avec action règlent le problème. Un panneau
qui se révèle à côté d'une grille fait recalculer la grille à chaque
sélection ; une colonne de contexte permanente évite l'artefact.

## 17:54 — Application de référence — Troisième passe : respiration et détails

Retour de l'utilisateur, captures à l'appui : la barre latérale de l’application de référence
collait ses éléments aux bords, là où celle de l’application de référence respire. La cause
était dans Awra, et d'autres défauts sont sortis en refaisant le tour.

- **Éléments de navigation collés aux bords : corrigé.** Dans un
  `AwraSidebar`, un `AwraNavigationItem` prend désormais 8 px de marge de
  chaque côté ; son libellé s'aligne sur les intitulés de section posés avec
  la même marge que dans l’application de référence.
- **Formulaire empilé sans regroupement : corrigé.** En mode compact, le
  libellé d'un `AwraFormRow` était aussi loin de son champ que de la ligne
  suivante. L'écart libellé–champ tombe à SM ; l'écart entre lignes reste LG.
- **Palette de commandes : retravaillée.** Le titre apparaissait deux fois
  (en-tête de fenêtre et corps) ; il ne reste que l'en-tête. Les entrées
  étaient des libellés centrés : elles sont alignées à gauche, avec l'icône
  du `GMenuItem` (`icon`), un raccourci lu dans l'attribut `accel` et un
  intitulé par section nommée. Lignes plus compactes. Test ajouté.
- **Bouton principal désactivé qui restait violet : corrigé.** Les règles
  d'apparence avaient la même spécificité que `:disabled` et venaient après.
- **Icône de bannière vide : corrigé.** `dialog-information-symbolic`
  manque dans Breeze. Awra embarque ses quatre icônes d'état et ajoute son
  chemin de ressources au thème d'icônes de l'écran.
- **Bannière trop large dans un panneau étroit : corrigé.** Le titre d'une
  `AwraStatusBanner` passe à la ligne et son action est centrée
  verticalement au lieu d'être étirée.

Côté application : la colonne de contexte adoptait la largeur naturelle de
son contenu, si bien qu'une bannière qui apparaissait faisait reculer le
clavier. Un `GtkScrolledWindow` sans défilement ni largeur naturelle propagée
fige la colonne à sa largeur minimale. À retenir pour toute colonne latérale
qui ne doit jamais bouger.

## 02:30 — Application de référence — Awra sous Windows

L'application devait exister aussi sous Windows, avec le même verre. Awra ne
connaissait que Wayland (`ext-background-effect-v1`) et le repli opaque.

- **Backend `win32-dwm` : ajouté.** Sous Windows 11 22H2 et plus, Awra demande
  à DWM le fond système Acrylic (`DWMWA_SYSTEMBACKDROP_TYPE`) et étend le cadre
  à toute la zone cliente ; GTK rend déjà ses fenêtres transparentes au pixel,
  le fond apparaît donc sous chaque surface translucide. Sous Windows 10, c'est
  le flou d'arrière-plan de `SetWindowCompositionAttribute`. Le commutateur
  « Effets de transparence » de Windows coupe le flou, et un refus du système
  fait basculer une fois pour toutes sur le rendu opaque au lieu de réessayer à
  chaque image. Le mode sombre teinte l'Acrylic (`set_dark`, nouveau crochet
  facultatif des backends).
- **Préférences Windows : lues.** GTK n'y suit ni le mode sombre ni la couleur
  d'accent ; Awra lit le registre et les paramètres système (mode, accent,
  contraste élevé, animations) et les reporte dans `GtkSettings`.
- **Fenêtres : ombre et coins natifs.** L'ombre CSD de GTK agrandissait la
  fenêtre et le fond système l'aurait remplie ; sous Windows, DWM dessine
  l'ombre et arrondit les coins (8 px sous Windows 11), Awra suit ce rayon.
- **Codes matériels : corrigés pour Windows.** Les aides clavier supposaient la
  numérotation evdev + 8. Sous Windows le code matériel GDK est la touche
  virtuelle de la disposition : Awra passe par le code de balayage physique,
  si bien que les légendes AZERTY restent justes.
- **Traductions relocalisables.** Le chemin absolu compilé sous Linux n'existe
  pas chez l'utilisateur : la DLL cherche `share/locale` à côté de son `bin/`.

GTK 4.22 est recompilé pour MinGW (Fedora fournit 4.21, et Awra utilise
`GtkPopoverBin` et `GtkReducedMotion`). Testé sous Wine : l'interface complète
s'affiche, en repli opaque puisque Wine n'a pas DWM. Le flou réel reste à voir
sur un vrai Windows.

## 2026-10-29 — Application de référence

A real libVLC consumer exposed a missing interval ruler. Added `AwraTimeline`
for seek requests, point markers, zoomed viewports, hovered-time previews and
live/committed offset editing. Subtitle parsing and decoder ownership stay in
the consumer. The accessible range interface supports both playback and offset
modes; cancellation restores the initial edit. The API is projected through
GIR and gtk-rs, with a Showcase example and external Python consumer.

Media libraries also need GTK's virtualized collections. Added the semantic
`collection-view` style role instead of wrapping GtkGridView or giving l’application de référence its
own stylesheet. L’application de référence composes existing navigation, panels, materials,
controls, volume and waveform widgets without framework bypasses.


## 2026-10-30 — Application de référence workflow audit

Real pointer/keyboard and 5,000-item queue workflows exposed gaps missed by
constructor/render smoke tests. `AwraOverlayBar` now owns the floating surface,
canvas geometry, idle timer, focus/hover protection and reduced-motion policy.
The timeline owns preview positioning in native logical coordinates, including
RTL, drag and unmap cleanup. Overlay chrome is an opt-in `AwraWindow` composition;
its mapped transitions keep the native CSD surface intact. Toasts can reserve
measured bottom clearance. These APIs are projected through GIR and safe Rust.

The audit also exposed a native lifetime error: creating the title binding in
instance initialization could consume a GtkWindow reference too early. Binding
after parent construction restores top-level ownership. Dialog/palette factories
now honor their existing full-transfer contract. Native weak-reference checks
and retained Python wrappers with forced collection cover destruction. Dialogs
join the parent's application, and composite inputs forward focus rather than
requiring consumers to target delegates for ordinary keyboard workflows.

Native AV1 3840×2160/60 playback exposed two further generic rendering costs:
language-bridge pixel copies and static-image mipmap generation on each live
frame. `AwraFramePool` leases aligned RGB, I420 or NV12 storage to native producers;
immutable textures retain the storage until the final consumer releases it.
Its bounded, nonblocking acquisition never overwrites a retained image. RGB
and correctly tagged planar YUV are supported. The pool is thread safe; each
frame has one producer and publishes an immutable texture to consumers after sealing.

`AwraTextureStream` keeps one dynamic paintable across frames, invalidates
size only when geometry changes, and uses linear GPU sampling. It can serve
the main and mini pictures without a second copy or a separate timer.
Actual frame-clock paints on the reference rose from about 12 to 60 per second
on the target GL renderer, keeping native 4K pixels. Vulkan on this driver
remains less regular; the consumer selects GL by default while respecting an
explicit `GSK_RENDERER`. No decoder or driver policy entered Awra.

A smaller video surface also exposed unnecessary full-resolution GPU transfers.
`AwraFrame.copy_scaled()` makes an encoded-plane bilinear preview in another
bounded pool, without changing or downloading the original texture. RGB and
both YUV layouts have color/lifetime tests. Rust keeps each lease with its
producer; only the pool and published textures cross threads. The consumer
keeps a native-resolution image for capture and feeds its compact view a
surface-sized preview. Both views sustain approximately 60 measured paints/s.

## 2026-10-30 — Window frame policy

L’application de référence exposed rounded gaps at the edges of a maximized window. AwraWindow
now resolves the outer frame and native blur region from the actual GTK window
state: square, borderless and shadowless in maximized/fullscreen states by
default. The public `keep-rounded-corners` boolean lets applications retain
rounding, including live changes. Stored radii and corner/edge masks survive
restoration; floating controls and nested surfaces do not inherit this policy.
When GTK hides its native titlebar in fullscreen, the root supplies the top
corners for the rounded opt-in. Consumers no longer need fullscreen corner-mask
workarounds. Native state and rendered-alpha checks cover default/opt-in,
docked/overlay composition and restoration on a private Wayland compositor.

## 2026-10-30 — Integration with Awra 1.1

The media components now share the current 1.1 development line with the newer
Windows lifecycle, dialog responses, accessible toasts and layout fixes. New
media APIs carry `Since: 1.1` metadata and generated Rust feature gates; the
frozen 1.0 contract remains intact. Both the previous main API and the media
API remain available after integration.

Cross-application tests caught an Escape conflict: a focused search entry
consumed the key before the new dialog class binding. Dialog capture now uses
the same close-response path and leaves focused popovers their own dismissal
sequence. The native l’application de référence settings workflow covers that interaction. A DWM
frame-style initialization also needed to run before the explicit radius
policy, retaining both square expanded frames and small popup rounding.


# 2026-10-30

## l’application de référence — collections et panneaux de fichiers

Le développement de l’explorateur a exposé quatre besoins génériques : une
grille virtualisée partageant la sélection du tableau, un fil d’Ariane pouvant
déborder sans élargir la fenêtre, des états de contenu utilisables dans un
panneau étroit, et des en-têtes de colonnes cohérents lors du défilement
horizontal. Ils sont traités dans Awra avec `AwraGridView`, un viewport de
breadcrumb révélant le segment courant (LTR/RTL), des descriptions à largeur
préférée et une correction du padding d’en-tête. L’application de référence n’ajoute aucun
fournisseur CSS ni accès aux enfants privés.

La validation ajoute un scénario C à 20 000 éléments (virtualisation,
sélection, détachement du modèle/factory, destruction) et un chemin de trente
segments dans une fenêtre de 320 pixels avec vérification RTL. La suite
complète passe : 34 succès, un test d’état de fenêtre ignoré sous Xvfb.
Les gates d’installation C/Python/Vala et de sous-projet Meson passent.

Les miniatures, la recherche et les opérations restent dans l’application :
ce sont des services du domaine fichiers. Le benchmark de 30 000 fichiers a
également révélé qu’un tri Python/GI relancé à chaque lot renouvelle trop de
cellules. Le classement initial des métadonnées est donc effectué hors du fil
GTK, avant leur transfert borné au modèle. Cela ne nécessite pas une nouvelle
API de framework.


La revue des captures a aussi révélé que `AwraTextArea` ne posait la couleur
sémantique que sur le nœud CSS `text`, alors que GtkTextView utilise aussi sa
propre couleur pour le rendu. Le delegate reçoit maintenant explicitement la
couleur de premier plan ; un test vérifie le contraste d’un aperçu non éditable
en apparences claire et sombre. Aucun correctif local à l’application de référence.

## Reprise visuelle — Application de référence

Le premier explorateur empilait des barres, réservait trop d’espace aux cellules
et déplaçait le contenu à chaque sélection. La reprise utilise des onglets dans
le chrome, une navigation indépendante par panneau, un tableau compact avec
colonnes adaptatives, des filtres persistants et une sélection flottante.
Un explorateur de fichiers existant sert ici de référence de composition et de densité, pas seulement
de liste de fonctionnalités.

Les besoins réutilisables sont portés dans Awra : propriété `compact` du header,
libellé de désélection personnalisable, recettes publiques de workspace,
ellipsisation des valeurs de métadonnées. Les captures ont révélé un cas où la
valeur de défilement du breadcrumb était correcte mais son viewport conservait
l’ancienne transformation ; la révélation est désormais différée après
l’allocation. Les tests couvrent aussi le chrome compact et les métadonnées
longues dans un panneau étroit.

## Canvas continu et zoom

Le retour d’utilisation a précisé que le workspace devait rester un seul Canvas,
avec les éléments posés dessus. L’application de référence fusionne maintenant navigation et
chemin dans l’en-tête, réutilise le sidebar arrondi/inset d’Awra, et supprime les
plaques séparant les intitulés de colonnes et le contenu.

Le besoin Ctrl+molette devient un contrôleur réutilisable dans Awra. Il capture
le geste modifié, conserve le défilement GTK ordinaire et expose une échelle
bornée/observable. Le texte est agrandi localement à partir de la police système ;
l’application adapte les icônes et miniatures en conservant modèles et sélection.
Les tests C vérifient isolation et durée de vie ; le consommateur vérifie aussi
l’interaction réelle avec XTest, sans événement synthétique limité au callback.

La fusion du chemin dans le chrome a montré que la réservation symétrique des
contrôles latéraux gaspillait la place d’une barre d’adresse. La propriété
optionnelle `AwraHeader:expand-center` utilise tout l’intervalle disponible sans
modifier l’allocation des en-têtes existants. Le test de géométrie vérifie les
deux modes et l’absence de recouvrement avec les commandes latérales.

Le dépôt de fichiers sur un dossier utilise GtkDropTarget. Le rôle générique
`drop-zone` exprime la destination acceptée ; la décision d’accepter un dossier,
le presse-papiers et les opérations restent dans l’application.

## 2026-11-04 — Yapfast — Une chronologie de conversation

Yapfast, client de discussion Discord et Matrix, affiche des salons de dizaines
de milliers de messages. Sous Slint, il recalculait lui-même la position de
défilement à chaque message reçu et à chaque page d’historique chargée, avec des
sauts visibles. Le besoin est générique : toute application de messagerie, fil
de commentaires ou visionneuse de journaux lit une chronologie qui grandit vers
le bas.

`AwraConversationView` enveloppe un `GtkListView` virtualisé et part de la fin.
Il suit les nouveaux éléments tant que la fin est visible (glissement doux,
apparition des seuls éléments ajoutés), cesse de suivre quand l’utilisateur
remonte, et propose une pastille « revenir au présent » qui peut compter les
non-lus. L’historique inséré au début ne déplace pas ce qui est à l’écran ;
`edge-reached` demande la page suivante. Pour savoir quelle rangée montre quelle
position sans lire les rangées privées de GTK, la vue s’intercale entre la liste
et la fabrique de l’application (signal ou modèle GtkBuilder). Un `GtkScrollable`
privé encadre l’allocation de la liste : les positions que la liste fixe
elle-même en réancrant ne sont jamais prises pour un défilement de
l’utilisateur. Les tests couvrent 50 000 éléments, les ajouts en rafale, le
chargement au début et le mouvement réduit.

