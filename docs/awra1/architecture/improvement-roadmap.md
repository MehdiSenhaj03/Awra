<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Feuille de route d’amélioration d’Awra

> État d’exécution : les Phases A et B sont validées depuis le 18 août 2026 ;
> les Phases C et D ainsi que le Gate 1.0 sont validés depuis le 19 août 2026.
> Les preuves détaillées sont maintenues dans
> [roadmap-status.md](roadmap-status.md).

## Objet du document

Ce document transforme les enseignements du Showcase et du port complet de l’application de référence en feuille de route pour Awra. Il ne décrit pas seulement les widgets
à ajouter : il couvre la stabilité, les invariants visuels, l’ergonomie de
l’API, l’accessibilité, les performances, les tests, les bindings, la
documentation et la préparation d’une future version stable.

Awra 0.1 a validé sa direction fondamentale : une application riche peut
conserver les comportements GTK, utiliser un matériau Frosted natif et une
identité indépendante du thème, sans CSS applicatif, sans libadwaita, sans API
privée Awra et sans appel direct au compositeur. L’application de référence a également validé un
invariant difficile : le rendu Canvas/Frosted est identique entre focus et
unfocus, tout en conservant la demande de blur native.

Le principal enjeu n’est donc plus de changer d’architecture. Il est de rendre
cette architecture plus robuste, plus simple à découvrir et plus rapide à
utiliser dans une nouvelle application.

## Échelle de priorité

- **P0 — fondation** : nécessaire avant de figer l’API ou de recommander Awra
  à des applications externes.
- **P1 — expérience développeur** : fort impact sur la vitesse et la qualité
  de construction d’une application.
- **P2 — couverture fonctionnelle** : élargit les familles d’applications que
  le framework sait exprimer confortablement.
- **P3 — recherche** : utile à terme, mais ne doit pas fragiliser le socle.

## Enseignements déjà acquis

### Ce qui fonctionne bien

- Le matériau racine, le chrome et les surfaces flottantes sont centralisés.
- Le backend d’effet reste privé et se dégrade proprement sans blur.
- Les préférences utiles du système — font, scaling, accessibilité, mode de
  couleur et contraste — restent disponibles.
- Les composants interactifs conservent les comportements GTK : actions,
  modèles, sélection, IME, clavier et accessibilité.
- Les corrections faites dans Awra se propagent à Showcase et l’application de référence au lieu
  d’être dupliquées dans leurs feuilles CSS.
- Les primitives bas niveau Solid, Translucent et Frosted coexistent avec les
  presets Canvas, Chrome, Content, Layer, Floating et Opaque.

### Ce que l’application de référence a révélé

- Une vraie application de données demande davantage de primitives de page,
  de formulaires, de tableaux et d’états applicatifs qu’une galerie de
  composants.
- Les rôles visuels sont un bon pont pour conserver un widget GTK, mais des
  chaînes de caractères ne doivent pas devenir l’API principale de tous les
  patterns courants.
- Toute classe GTK publique non préfixée constitue un risque. La collision
  réelle avec `.sidebar-pane` a montré qu’une propriété externe non
  explicitement réinitialisée peut modifier toute une fenêtre.
- Les cycles de vie de popovers, dropdowns, menus, dialogs et surfaces natives
  doivent être considérés comme une fonctionnalité, pas comme un détail de
  rendu.
- En Rust, l’absence de bindings sûrs est actuellement le principal coût
  ergonomique. Ce chantier peut arriver plus tard, mais l’API C/GIR doit dès
  maintenant être conçue pour le faciliter.
- Une application finie ne doit pas dépendre d’un mode spécial du Showcase ou
  de recettes visuelles privées impossibles à réutiliser.

## Règle de conception : widget, rôle ou matériau ?

Avant d’ajouter une API, Awra doit appliquer la règle suivante :

| Besoin | Primitive appropriée |
| --- | --- |
| Matière, profondeur, blur, rayon ou silhouette | `AwraMaterial`, `AwraSurfaceRole` ou `AwraSurface` |
| Comportement réutilisable, cycle de vie, focus ou accessibilité | véritable widget ou objet Awra typé |
| Présentation d’un widget GTK comportemental existant | rôle visuel public Awra |
| Valeur d’apparence globale | token sémantique ou preset de matériau |
| Concept propre à un domaine métier | composition applicative avec primitives génériques |

Un rôle ne doit pas remplacer un widget lorsque le framework doit gérer du
comportement. À l’inverse, Awra ne doit pas réimplémenter `GtkListView`,
`GtkSelectionModel` ou l’édition de texte uniquement pour changer leur
apparence.

Les rôles publics restent une API valide d’extension, mais ils doivent toujours
être traduits vers des classes privées `awra-role-*`. Une application ne doit
jamais avoir à connaître ces classes internes.

## Axe 1 — Stabilité du runtime et des cycles de vie (P0)

### Objectifs

- Rendre l’initialisation par `GdkDisplay` strictement idempotente.
- Garantir la destruction dans tous les ordres : fenêtre, display, surface,
  popup, contexte et backend.
- Éliminer les références fortes cycliques et documenter chaque transfert de
  propriété GObject.
- Supporter les remappings rapides, les redimensionnements continus et les
  ouvertures/fermetures répétées sans warning, fuite ou objet Wayland zombie.
- Garder le backend générique fonctionnel dans tous les cas d’erreur.

### Travail proposé

- Ajouter des stress tests avec plusieurs fenêtres, popovers imbriqués,
  dialogs successifs et destruction pendant une animation.
- Tester des centaines de cycles map/unmap et focus/unfocus.
- Exécuter régulièrement ASan, UBSan et un détecteur de fuites.
- Vérifier les weak references de l’Inspector, des overlays et des modèles.
- Formaliser le modèle de thread : UI et backend sur le thread GTK, données
  applicatives éventuellement asynchrones, aucune lecture Wayland concurrente.
- Transformer chaque crash ou warning de cycle de vie en test de régression.

### Critère de sortie

Une suite de stress complète doit terminer avec `G_DEBUG=fatal-warnings`, sans
erreur Wayland, sans fuite attribuable à Awra et sans accès après destruction.

## Axe 2 — Invariants de matériaux et de focus (P0)

### Invariants à figer

- Focus/unfocus ne change jamais le RGB ou l’alpha du fill racine.
- Focus/unfocus ne change jamais la demande de blur, son activation ni sa
  région.
- Le rayon, le clipping et la géométrie ne changent pas avec le focus.
- Une variation inactive éventuelle est réservée aux bordures, highlights,
  ombres ou accents secondaires, et doit rester subtile et documentée.
- Le collapse d’une sidebar ou un changement de page ne modifie jamais le
  matériau Canvas.
- Reduced transparency produit un fallback opaque lisible, jamais une surface
  transparente non floutée par accident.

### Travail proposé

- Conserver une matrice déterministe pour chaque preset, rôle, apparence,
  contraste, capacité backend et état actif.
- Ajouter des captures automatisées d’une vraie `AwraWindow`, pas uniquement
  des tests du résolveur.
- Comparer les régions racine, header, sidebar, contenu et popup séparément.
- Tester les changements dynamiques de capacité et le remapping du backend.
- Tester redémarrage du compositeur, hotplug écran et fermeture du
  `GdkDisplay` sans conserver d’objet natif invalide.
- Maintenir une matrice KWin intégré, Better Blur DX, compositeur sans
  `ext-background-effect-v1` et backend générique forcé. Le code applicatif ne
  doit jamais sélectionner une branche par nom de compositeur.
- Documenter clairement ce que le protocole contrôle et ce que le compositeur
  reste libre de choisir.

### Critère de sortie

Les scènes prévues comme stables doivent produire zéro pixel différent entre
focus et unfocus. Les scènes autorisant une atténuation doivent prouver que le
fill et la demande de blur restent identiques.

## Axe 3 — Isolation complète vis-à-vis de GTK (P0)

### Objectifs

- Aucun thème, `GTK_THEME`, fichier `gtk.css` utilisateur ou provider USER ne
  doit changer l’identité d’un composant Awra.
- Les préférences système utiles ne doivent pas être supprimées : fonts,
  scaling, curseurs, IME, sélection de texte, accessibilité et contraste.
- Toutes les classes d’implémentation doivent être préfixées et privées.

### Travail proposé

- Auditer toutes les propriétés sensibles : background, gradient, border,
  radius, shadow, outline, opacity, filter, icon filter, padding, margin,
  minimum size et transitions.
- Étendre le test hostile aux rôles de page, sidebars, tables, menus et
  overlays, pas seulement aux contrôles élémentaires.
- Vérifier les descendants GTK privés uniquement par rendu et comportement ;
  ne jamais les parcourir ou les modifier depuis le code applicatif.
- Maintenir les tests sous au moins deux thèmes volontairement opposés, avec
  un fichier XDG hostile et un provider runtime USER.
- Ajouter un contrôle qui refuse toute classe publique non préfixée introduite
  par l’API de rôles.

### Critère de sortie

Allocations et pixels Awra identiques dans la matrice d’isolation, hors
préférences explicitement supportées par le framework.

## Axe 4 — Contrat API, ABI et introspection (P0)

### Objectifs

- Obtenir une API C cohérente avant le gel ABI.
- Garantir que GIR, Vala, Python/GI et les futurs bindings Rust voient le même
  contrat.
- Réduire les ambiguïtés de propriété, d’ownership et de nullabilité.

### Travail proposé

- Auditer toutes les annotations `(transfer)`, `(nullable)`, tableaux,
  callbacks et erreurs.
- Uniformiser les noms `new`, `get`, `set`, `append`, `remove`, `popup` et
  `popdown`.
- Préférer propriétés GObject et enums pour les concepts finis ; conserver les
  chaînes pour les vocabulaires réellement extensibles.
- Définir une politique de dépréciation et un journal des changements API.
- Ajouter un test de symboles publics et de compatibilité ABI.
- Compiler des consommateurs externes C, Vala et Python uniquement avec le
  `.pc`, le GIR et la typelib installés.
- Vérifier que les erreurs récupérables utilisent `GError` ou un état
  diagnostique plutôt qu’un warning fatal.

### Critère de sortie

Aucune API privée nécessaire à une application de référence, GIR sans warning
important, documentation générée complète et consommateurs externes compilés
contre une installation propre.

## Axe 5 — Construction de pages et ergonomie développeur (P1)

Le manque principal observé dans l’application de référence est la quantité de composition
manuelle nécessaire pour obtenir une page pourtant conventionnelle. Les
nouveaux composants doivent fournir de bons espacements et un comportement
responsive par défaut, tout en restant composables.

### Primitives prioritaires

- `AwraPage` : scroll, largeur de lecture, marges et rythme vertical.
- `AwraPageHeader` : titre, sous-titre, actions et contenu contextuel.
- `AwraSection` : titre, description, actions et contenu sans imposer une
  carte bordée.
- `AwraToolbar` : groupes start/center/end et débordement.
- `AwraFilterBar` : recherche, dropdowns, toggles, résumé et reset.
- `AwraMasterDetail` : liste, détail, breakpoints et navigation compacte.
- `AwraDetailPane` : en-tête d’entité, métadonnées, actions et sections.
- `AwraForm` et `AwraFormRow` : label, contrôle, aide, validation et erreurs.
- `AwraEmptyState`, `AwraLoadingState` et `AwraErrorState`.
- `AwraStatusBanner` : information, succès, warning et erreur non modale.
- `AwraSelectionToolbar` : actions contextuelles liées à une sélection.

### Exigences

- Aucun nombre magique nécessaire dans l’application pour le cas standard.
- Tous les insets et gaps proviennent des tokens/layout presets.
- Tous les composants restent utilisables dans GtkBuilder et Blueprint.
- Les patterns ne doivent pas imposer une composition unique de fenêtre ou de
  sidebar.
- Les valeurs bas niveau restent accessibles pour les compositions atypiques.

### Extensibilité et widgets applicatifs

Awra doit également rendre agréable la création d’un composant qui n’existe
pas dans son catalogue. Un guide et un exemple doivent montrer comment :

- dériver ou composer un widget GTK sans dépendre d’un enfant privé ;
- lire les tokens et résolutions de matériaux publics ;
- appliquer typographie, spacing et rôles sans CSS applicatif ;
- dessiner une visualisation métier avec GSK ou `GtkDrawingArea` tout en
  respectant Light/Dark, contraste, scaling et reduced motion ;
- exposer correctement propriétés, signaux et accessibilité.

L’objectif n’est pas d’interdire tout rendu applicatif, mais d’éviter que
chaque nouveau composant doive recréer le design system ou accéder aux détails
d’implémentation d’Awra.

## Axe 6 — Widgets de données et formulaires (P1/P2)

### Données

- Wrapper léger autour de `GtkColumnView`/`GtkListView`, sans remplacer leurs
  modèles ni leur virtualisation.
- Colonnes typées avec titre, alignement, largeur, tri et cellule accessible.
- Lignes denses, sélection simple/multiple, état hover/focus et actions de fin.
- États chargement, vide, erreur et pagination/infinite loading.
- `AwraMetricRow`, `AwraStatBar`, `AwraBadgeGroup` et groupes de métadonnées.
- Support des tableaux larges, du scroll horizontal et du responsive.

### Formulaires et inputs

- Text area multiligne, password entry, validation et message d’erreur.
- Multi-select, tags/chips éditables et choix avec recherche.
- Date, heure, couleur et fichiers lorsque des use cases réels les justifient.
- Préférences et settings rows sans reproduire les compositions rigides de
  libadwaita.
- Groupes checkbox/radio et relations label-description accessibles.

### Principe

Chaque composant ajouté doit supprimer une composition récurrente observée
dans au moins deux scénarios génériques. Awra ne doit pas incorporer de widget
« fiche », « musique » ou autre concept métier.

## Axe 7 — Navigation, layout et responsive (P1)

- Stabiliser `AwraNavigationView`, `AwraTabView`, `AwraSidebar` et
  `AwraSplitView` comme une famille cohérente.
- Ajouter un objet de breakpoints ou size classes observable par
  l’application.
- Gérer rail compact, sidebar persistante, sidebar overlay et panneau de
  détails sans modifier le matériau racine.
- Ajouter breadcrumbs, back/forward et restauration de focus lorsque les
  patterns le nécessitent.
- Définir des tailles minimales et un comportement correct avec textes longs,
  fonts agrandies et traductions.
- Permettre plusieurs architectures de chrome : sidebar sous le header,
  sidebar pleine hauteur, header local au contenu ou header minimal.
- Tester resize continu, maximisation, fullscreen et fractional scaling.

## Axe 8 — Overlays et interactions (P0/P1)

- Garantir autohide, clic extérieur, Escape et restauration du focus pour
  chaque overlay.
- Formaliser la relation parent/enfant des menus imbriqués et leur fermeture.
- Unifier menus, context menus, dropdowns, popovers, dialogs, tooltips et
  toasts autour des mêmes règles Floating.
- Ajouter sheets, command palette et action popovers seulement après la
  stabilisation des primitives existantes.
- Tester les ouvertures rapides, l’activation clavier, le pointer grab et la
  destruction du parent pendant l’affichage.
- Ne jamais dépendre d’enfants privés de `GtkDropDown` ou `GtkMenuButton` pour
  produire le rendu.

## Axe 9 — États, motion et feedback (P1)

- Définir une matrice commune normal, hover, pressed, checked, selected,
  disabled, focus-visible, inactive, loading et destructive.
- Éviter de coder le même état avec des valeurs différentes dans chaque
  composant.
- Stabiliser les presets de motion et leurs courbes, interruption et
  retargeting.
- Garantir que reduced motion supprime la durée sans casser la transition
  logique ou le focus.
- Ajouter skeletons, progress feedback et busy states sans animation
  obligatoire.
- Réserver l’accent à l’action et à la sélection ; ne pas l’utiliser comme
  matière décorative générale.

## Axe 10 — Accessibilité, clavier, internationalisation et RTL (P0/P1)

- Auditer les rôles accessibles, labels, descriptions, relations et états.
- Garantir un focus clavier visible sur chaque contrôle et chaque ligne
  activable.
- Tester Tab/Shift-Tab, flèches, Home/End, Page Up/Down, Enter, Space et Escape
  selon les conventions GTK.
- Conserver IME, sélection, copier-coller et lecteurs d’écran sur les inputs.
- Tester contraste normal, high contrast, disabled et focus en Light/Dark.
- Tester font scaling, textes longs, pluralisation et localisation.
- Utiliser start/end plutôt que left/right et valider chaque composition en
  RTL.
- Ajouter des tests automatisés d’arbre accessible lorsque GTK le permet, et
  compléter par des tests manuels Orca.

## Axe 11 — Performance et qualité du rendu (P1)

- Mesurer le coût réel du grain, des ombres, du clipping et de l’overdraw en
  1080p, 4K et fractional scaling.
- Conserver une seule texture de grain mise en cache et éviter les allocations
  par frame.
- Ne recalculer les régions natives que lorsque géométrie ou capacité change.
- Éviter l’empilement de surfaces translucides identiques qui assombrit le
  wallpaper sans ajouter de hiérarchie.
- Profiler les listes virtualisées, tableaux larges, animations et overlays.
- Définir des budgets de frame et de mémoire mesurables avant 1.0.
- Ajouter un mode développeur affichant overdraw, régions d’effet et nombre de
  surfaces suivies.

## Axe 12 — Light, Dark et direction artistique (P1)

- Dark reste la référence artistique principale à court terme.
- Light doit recevoir une passe artistique dédiée en plus de ses garanties de
  contraste actuelles.
- Chaque composant doit rester identifiable avec accent bleu puis en niveaux
  de gris ; l’identité d’Awra ne doit pas dépendre du violet.
- SYSTEM accent reste best-effort avec fallback DEFAULT.
- Les presets doivent être calibrés ensemble : Canvas, Chrome, Content, Layer,
  Floating et Opaque, puis composants interactifs.
- Les ajustements numériques d’alpha, rayon, spacing et timing restent des
  calibrations révisables, pas des vérités ABI.
- Le premium doit venir de la matière, de la lumière, de l’espace et des
  proportions, pas de glow, gradients décoratifs, bordures ou ombres ajoutés
  sans fonction.

## Axe 13 — Diagnostics et Inspector (P1)

- Afficher rôle public, classe privée résolue, matériau, tokens, état,
  allocation, scale factor et surface native.
- Comparer côte à côte les résolutions active/inactive.
- Afficher interface annoncée, capacité blur, demande résolue, effet appliqué,
  fallback et raison.
- Exposer le nombre de surfaces suivies et leur cycle map/unmap.
- Visualiser la région exacte envoyée au backend.
- Permettre l’export d’un rapport diagnostique sans exposer les objets Wayland.
- Conserver un mode de fallback forcé pour reproduire les problèmes.
- Ne pas prétendre énumérer les providers GTK lorsque GTK ne fournit pas cette
  information publiquement ; documenter les vérifications par construction.

## Axe 14 — Tests et intégration continue (P0)

### Niveaux de tests

- **Unitaires** : tokens, contrastes, matériaux, fallback et états.
- **Widgets** : propriétés, signaux, focus, clavier, accessibilité et
  destruction.
- **Intégration** : fenêtres, overlays, backend générique/Wayland et
  multi-display lorsque possible.
- **Isolation** : thèmes opposés, user CSS et provider USER hostiles.
- **Visuels** : scènes déterministes Light/Dark, accent bleu et niveaux de
  gris.
- **Applications externes** : Showcase, l’application de référence et petites fixtures C/Vala/
  Python.
- **Manuels compositor** : KWin intégré, Better Blur DX, blur désactivé et
  fractional scaling.

### Infrastructure

- CI avec warnings fatals, ASan/UBSan, GIR `--warn-all`, installation et
  `meson dist`.
- Tests sans libadwaita dans les dépendances et dans `ldd`.
- Vérification qu’aucun exemple n’utilise de CSS applicatif ou d’API privée.
- Captures reproductibles avec dimensions, font et données fixées.
- Conservation de rapports de comparaison plutôt que validation uniquement
  visuelle à l’œil.

## Axe 15 — Documentation et onboarding (P1)

- Guide « première fenêtre Awra » jusqu’à une application multi-page.
- Catalogue visuel des composants, états et propriétés.
- Guide de décision widget/rôle/surface/matériau.
- Guide de migration GTK/libadwaita vers Awra, avec mappings et différences de
  philosophie.
- Recipes complètes : master/detail, settings, navigateur de données, lecteur
  média, launcher, panels et overlays.
- Exemples C, Vala, Python/GI et Blueprint réellement compilés en CI.
- Documentation claire des limites du blur Wayland et des fallbacks.
- Liste des rôles publics supportés, avec widget attendu et exemple.
- Section anti-patterns : CSS local, accès aux enfants GTK privés, appel KWin,
  matériau dupliqué, blur imbriqué et tailles magiques.

Le Showcase doit devenir la documentation vivante de ces patterns, sans
logique métier et uniquement avec les API publiques.

## Axe 16 — Bindings et expérience multi-langage (P1/P2)

Le travail de bindings peut être planifié après la consolidation de l’API, mais
les contraintes doivent être prises en compte dès maintenant.

- Faire de GIR la source de vérité du contrat multi-langage.
- Garder enums, propriétés, signaux et ownership faciles à projeter.
- Valider Vala et Python/GI avant de figer chaque nouvelle famille.
- Générer ensuite des bindings Rust gtk-rs plutôt que maintenir une longue
  façade FFI manuelle par application.
- Ajouter une couche Rust sûre pour l’ownership et les callbacks, sans cacher
  les types GTK utiles.
- Fournir des exemples et tests équivalents dans chaque langage supporté.
- Versionner bindings et ABI C de manière coordonnée.

## Axe 17 — Packaging, versionnement et maintenance (P1)

- Garantir l’installation de la bibliothèque, headers, `.pc`, GIR, typelib,
  VAPI, ressources et documentation.
- Tester préfixes non standards, DESTDIR, builds subproject et Flatpak.
- Produire des archives `meson dist` reproductibles.
- Garder le protocole Wayland épinglé avec sa licence et généré dans le build.
- Définir politique SemVer, SONAME, compatibilité source/ABI et dépréciation.
- Documenter la matrice GTK/compositeurs réellement supportée.
- Ajouter changelog, notes de migration et procédure de release.
- Éviter toute dépendance runtime KWin ou libadwaita.

## Axe 18 — Gouvernance du design system (P1)

- Documenter chaque décision structurante dans une courte note d’architecture.
- Distinguer tokens sémantiques publics et constantes d’implémentation.
- Exiger une démonstration Light/Dark, focus, disabled, fallback et destruction
  pour chaque nouveau composant.
- N’ajouter un preset que s’il exprime une intention réutilisable.
- Déprécier proprement les expériences qui n’ont pas trouvé de use case.
- Faire valider les changements globaux dans Showcase et au moins une vraie
  application externe.

Chaque nouveau composant doit arriver avec : API publique introspectable,
construction GtkBuilder, états complets, clavier, accessibilité, Light/Dark,
isolation GTK, reduced motion/transparency lorsque pertinent, test de
destruction, démonstration Showcase et exemple minimal externe. Un composant
qui ne satisfait pas encore cette définition reste expérimental et ne bloque
pas le gel du reste de l’ABI.

## Ordre recommandé

### Phase A — Durcissement des fondations, 0.1.x

1. Terminer l’audit ownership, destruction et lifecycle natif.
2. Étendre les tests focus/unfocus et isolation aux compositions de pages.
3. Auditer GIR, annotations et cohérence des propriétés.
4. Exécuter ASan/UBSan et corriger tous les défauts.
5. Figer provisoirement les contrats Material, Surface, Window et Overlay.

### Phase B — Expérience de construction, 0.2

1. `AwraPage`, `AwraPageHeader`, `AwraSection` et `AwraToolbar`.
2. `AwraForm`, `AwraFormRow` et états vide/loading/error.
3. `AwraFilterBar`, `AwraMasterDetail` et `AwraDetailPane`.
4. Catalogue de rôles, exemples Blueprint et guide de migration.
5. Migration du Showcase et d’une partie de l’application de référence vers ces primitives pour
   prouver qu’elles réduisent réellement le code.

### Phase C — Données, navigation et contrôles, 0.3

1. Famille table/column view et selection toolbar.
2. Inputs avancés et settings rows.
3. Responsive/breakpoints et variantes de sidebar.
4. Navigation, breadcrumbs, command palette et overlays restants.
5. Passe artistique Light dédiée.

### Phase D — Écosystème, 0.4+

1. Bindings Rust générés et couche sûre.
2. Packaging distributions/Flatpak et documentation publiée.
3. Matrice compositor élargie et CI externe.
4. Applications d’exemple supplémentaires dans plusieurs langages.

### Gate 1.0

Awra ne devrait déclarer son API stable que lorsque :

- aucune application de référence n’utilise d’API privée ou de CSS local ;
- Material, Surface, Window, Header, Sidebar et overlays ont un contrat stable ;
- focus/unfocus, fallback, reduced transparency et isolation sont prouvés ;
- cycles de vie et sanitizers sont propres ;
- accessibilité clavier et lecteur d’écran a été auditée ;
- Light et Dark sont cohérents et contrastés ;
- GIR et au moins trois consommateurs externes sont validés ;
- installation, packaging, documentation et migration sont reproductibles ;
- les limites compositor sont documentées honnêtement.

## Changements à fort impact immédiat

Si l’équipe doit se limiter aux améliorations ayant le meilleur rendement,
l’ordre suivant est recommandé :

1. Stress tests lifecycle, focus et isolation sur de vraies fenêtres.
2. Audit API/GIR/ownership avant que davantage de composants s’y appuient.
3. `AwraPage`, `AwraSection`, `AwraPageHeader` et `AwraToolbar`.
4. `AwraFormRow`, `AwraFilterBar` et états vide/loading/error.
5. `AwraMasterDetail` et wrapper de table GTK.
6. Catalogue typé/documenté remplaçant progressivement les rôles les plus
   courants.
7. Passe artistique Light et validation accent bleu/niveaux de gris.
8. Documentation de migration et exemples externes compilés.

## Anti-objectifs

- Ne pas reproduire libadwaita composant par composant.
- Ne pas créer un wrapper Awra autour de chaque type GTK sans valeur ajoutée.
- Ne pas exposer Wayland, KWin ou un rayon de blur fictif dans l’API publique.
- Ne pas obtenir une identité premium en empilant glow, gradients, bordures et
  ombres.
- Ne pas figer trop tôt les valeurs artistiques numériques.
- Ne pas introduire Liquid ou la réfraction avant stabilisation complète du
  Frosted.
- Ne pas adapter les primitives à un seul Showcase ou à l’application de référence.
- Ne pas accepter une correction applicative lorsqu’un défaut est
  généralisable au framework.

## Définition d’une bonne expérience Awra

Une nouvelle application devrait pouvoir construire une fenêtre, une sidebar,
plusieurs pages, des formulaires, une vue de données et des overlays uniquement
avec GTK et les API publiques Awra. Les valeurs par défaut doivent être belles,
accessibles et correctement espacées. Le développeur ne doit avoir à écrire ni
CSS, ni code compositor, ni logique de focus visuel, tout en conservant la
possibilité de composer des interfaces atypiques à partir des primitives bas
niveau.

L’application de référence montre que cet objectif est atteignable. La feuille de route consiste
maintenant à rendre ce résultat systématique et rapide, plutôt qu’obtenu après
avoir découvert les primitives manquantes pendant le développement.
