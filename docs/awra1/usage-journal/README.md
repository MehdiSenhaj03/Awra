<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Journal d’utilisation d’Awra

Ce dossier conserve les observations faites en utilisant Awra dans le
Showcase, l’application de référence et les futurs projets de référence. Il accueille aussi
bien les réussites que les frustrations, les contournements, les questions et
les idées encore imparfaites.

Ce journal n’est pas une spécification normative. Les contrats stables restent
décrits dans `docs/api/`, les décisions dans `docs/architecture/decisions/` et
la validation dans `docs/architecture/roadmap-status.md`.

## Format

```markdown
# 2026-08-19

## 14:30 — Application de référence — Construction d’une page de détails

Contenu de l’observation, de la décision ou de la question.
```

Lorsqu’une entrée passée est reconstruite sans heure fiable, elle utilise
`Heure non consignée — reconstruction` au lieu d’inventer une heure.

## Règles simples

- Ajouter les nouvelles observations à la fin de la journée concernée.
- Conserver le contexte du projet et décrire le comportement réellement vu.
- Dire explicitement si une conclusion est confirmée, provisoire ou à tester.
- Préférer une nouvelle entrée corrective à la réécriture silencieuse d’une
  ancienne observation.
- Ne jamais y placer de secret, token, donnée personnelle ou identifiant de
  production.

Le journal courant se trouve dans [`journal.md`](journal.md).
