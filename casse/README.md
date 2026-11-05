# CASSE

Jeu de cambriolage à boucles temporelles (Godot 4.4). Tu as 40 secondes ; à chaque boucle, tes versions
précédentes rejouent leurs gestes à côté de toi. Campagne de 22 coffres, Atelier pour concevoir et partager ses
coffres par code, replays partageables, coopération sur un écran ou en ligne, duel en ligne.

## Lancer
- Windows : `CASSE.exe`. Linux : `./CASSE.x86_64`.
- En ligne : l'hôte choisit « Héberger » (port UDP 47640, ouvert automatiquement par UPnP si la box le permet) ;
  l'ami entre l'adresse affichée.

## Développement
- `godot --headless --path . -s res://tests/run_tests.gd` : chaque coffre de la campagne est percé par sa
  solution écrite (tools/levels.py) et son replay vérifié déterministe.
- `python3 tools/levels.py` : compile les coffres ; `python3 tools/texts.py` : textes FR/EN.
- `node tools/audio-render.cjs assets/audio` : rend la musique (stems) et les effets.
- Réseau : `tests/net_test.gd` lance deux instances (hôte / client) et vérifie l'identité des simulations.
