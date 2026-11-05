#!/usr/bin/env python3
"""All the words of CASSE, French and English. Writes assets/i18n/fr.json and en.json."""
import json, os
OUT = os.path.join(os.path.dirname(__file__), '..', 'assets', 'i18n')

def both(fr, en):
    return (fr, en)

T = {
 'common': {'back': both('Retour', 'Back'), 'yes': both('Oui', 'Yes'), 'no': both('Non', 'No'), 'skip': both('Passer', 'Skip'),
   'copy': both('Copier', 'Copy'), 'paste': both('Coller', 'Paste'), 'continue': both('Continuer', 'Continue'), 'locked': both('Verrouillé', 'Locked'),
   'copied': both('Copié dans le presse-papiers', 'Copied to the clipboard'), 'play': both('Jouer', 'Play'), 'cancel': both('Annuler', 'Cancel')},
 'echo': {str(i): both(f, e) for i, (f, e) in enumerate([('Écho cyan', 'Cyan echo'), ('Écho magenta', 'Magenta echo'), ('Écho ambre', 'Amber echo'), ('Écho menthe', 'Mint echo'), ('Écho violet', 'Violet echo'), ('Écho blanc', 'White echo')])},
 'heist': {
   'par': both("Record de l'architecte :", "Architect's record:"), 'loop': both('BOUCLE {n}', 'LOOP {n}'),
   'echo_turn': both('{name} — à toi', '{name} — your turn'), 'ready': both("Bouge pour lancer l'horloge", 'Move to start the clock'),
   'keys': both("<kbd>{use}</kbd> agir (maintenir : crocheter, pirater)  ·  <kbd>{sprint}</kbd> courir (bruyant)  ·  <kbd>{throw}</kbd> lancer une pièce  ·  <kbd>{rewind}</kbd> rembobiner  ·  <kbd>{restart}</kbd> reprendre la boucle  ·  <kbd>{finish}</kbd> terminer l'écho  ·  <kbd>{echoes}</kbd> échos",
                "<kbd>{use}</kbd> act (hold: crack, hack)  ·  <kbd>{sprint}</kbd> run (noisy)  ·  <kbd>{throw}</kbd> throw a coin  ·  <kbd>{rewind}</kbd> rewind  ·  <kbd>{restart}</kbd> restart the loop  ·  <kbd>{finish}</kbd> end this echo  ·  <kbd>{echoes}</kbd> echoes"),
   'loop_line': both('BOUCLE {n}  ·  ÉCHO {echo} / {budget}', 'LOOP {n}  ·  ECHO {echo} / {budget}'), 'loop_end': both('FIN DE BOUCLE', 'END OF LOOP'),
   'out': both("PLUS D'ÉCHO", 'NO ECHO LEFT'), 'out_sub': both('Toutes tes versions sont déjà sur le coup.', 'Every version of you is already on the job.'),
   'rewind_loop': both('Rembobiner cette boucle', 'Rewind this loop'), 'echoes': both('Gérer les échos', 'Manage echoes'), 'restart_all': both('Tout recommencer', 'Start over'),
   'quit': both('Quitter le coffre', 'Leave the vault'), 'rerecord': both('Réenregistrer : {name}', 'Re-record: {name}'), 'erase_last': both('Effacer le dernier écho', 'Erase the last echo'),
   'alarm': both('REPÉRÉE', 'SPOTTED'), 'by_guard': both("un garde t'a vue", 'a guard saw you'), 'by_camera': both("une caméra t'a filmée", 'a camera caught you'), 'by_laser': both('tu as coupé un laser', 'you broke a laser'),
   'alarm_keys': both("<kbd>{rewind}</kbd> maintenir : rembobiner  ·  <kbd>{restart}</kbd> reprendre la boucle  ·  <kbd>{echoes}</kbd> échos  ·  clic sur la frise : revenir à cet instant",
                      '<kbd>{rewind}</kbd> hold: rewind  ·  <kbd>{restart}</kbd> restart the loop  ·  <kbd>{echoes}</kbd> echoes  ·  click the timeline: go back to that moment'),
   'won': both('CASSE RÉUSSI', 'HEIST COMPLETE'), 'won_sub': both('{n} écho(s)', '{n} echo(es)'), 'finishing': both("L'écho attend la fin de la boucle…", 'The echo waits for the loop to end…'),
   'p2': both('Joueur 2', 'Player 2')},
 'act': {'takedown': both('Assommer', 'Knock out'), 'lever': both('Actionner', 'Pull'), 'button': both('Appuyer', 'Press'), 'vlever': both('Tirer le levier', 'Pull the lever'),
   'door_open': both('Ouvrir', 'Open'), 'door_close': both('Fermer', 'Close'), 'term': both('Pirater (maintenir)', 'Hack (hold)'), 'safe': both('Crocheter (maintenir)', 'Crack (hold)'),
   'card': both('Prendre le badge', 'Take the keycard'), 'loot': both('Prendre le butin', 'Take the loot'), 'drop': both('Poser', 'Drop')},
 'title': {'tagline': both("Tu as quarante secondes. Puis tout recommence, et celle que tu étais t'aide.", 'You have forty seconds. Then it all starts over, and who you were helps you.'),
   'press': both('Appuie sur une touche', 'Press any key'), 'campaign': both('Campagne', 'Campaign'), 'coop': both('Coopération', 'Co-op'), 'online': both('En ligne', 'Online'),
   'workshop': both("L'Atelier", 'The Workshop'), 'code': both('Entrer un code', 'Enter a code'), 'options': both('Options', 'Options'), 'quit': both('Quitter', 'Quit'),
   'coop_sub': both('Deux voleuses, même écran', 'Two thieves, one screen'), 'online_sub': both('Coopération ou duel à distance', 'Co-op or duel over the network'),
   'workshop_sub': both('Concevoir des coffres et les partager', 'Design vaults and share them'), 'code_sub': both("Coffre ou replay d'un ami", "A friend's vault or replay")},
 'map': {'title': both('VALMONT', 'VALMONT'), 'sub': both('Choisis ton coffre', 'Pick your vault'), 'medals': both('médailles', 'medals'), 'par': both('record : {n} écho(s)', 'record: {n} echo(es)'),
   'best': both('ton meilleur : {n}', 'your best: {n}'), 'echoes': both('{n} échos max', 'up to {n} echoes')},
 'district': {
   '1': {'name': both('La Galerie Delacroix', 'The Delacroix Gallery'), 'desc': both("Un musée de verre et de laiton. Les gardes s'ennuient ; le temps, lui, ne s'ennuie jamais.", 'A museum of glass and brass. The guards are bored; time never is.')},
   '2': {'name': both("L'Hôtel de Saint-Ange", 'The Saint-Ange Mansion'), 'desc': both("Un bal masqué, des badges qui changent de main, et des lustres qu'on peut éteindre.", 'A masked ball, keycards changing hands, and chandeliers that can go dark.')},
   '3': {'name': both('La Banque Lantenac', 'The Lantenac Bank'), 'desc': both('Caméras, lasers, verrous horaires : la maison qui ne dort jamais.', 'Cameras, lasers, time locks: the house that never sleeps.')},
   '4': {'name': both('Le Casino Flottant', 'The Floating Casino'), 'desc': both('Un paquebot de jeu amarré sur le fleuve. Tout le monde triche ; toi aussi.', 'A gambling liner moored on the river. Everyone cheats; so do you.')},
   '5': {'name': both('La Tour Horlogère', 'The Clock Tower'), 'desc': both("Là où la Montre a été forgée. Là où elle veut retourner.", 'Where the Watch was forged. Where it wants to go back.')}},
 'vault': {
   '1-1': both('Vernissage', 'Private View'), '1-2': both('La plaque', 'The Plate'), '1-3': both('Le bouton', 'The Button'), '1-4': both('Le gardien', 'The Keeper'),
   '1-5': both('Pile ou face', 'Heads or Tails'), '1-6': both('Le grand salon', 'The Great Hall'), '2-1': both('Le vestiaire', 'The Cloakroom'), '2-2': both('Lumières éteintes', 'Lights Out'),
   '2-3': both('Le majordome', 'The Butler'), '2-4': both('Portes battantes', 'Swinging Doors'), '2-5': both('Le bal masqué', 'The Masked Ball'), '3-1': both('Les yeux du hall', 'Eyes of the Hall'),
   '3-2': both('Les fils rouges', 'Red Threads'), '3-3': both('Deux clés', 'Two Keys'), '3-4': both('Le verrou horaire', 'The Time Lock'), '3-5': both('La salle des coffres', 'The Strongroom'),
   '4-1': both('La salle des jeux', 'The Gaming Room'), '4-2': both('Le pont supérieur', 'The Upper Deck'), '4-3': both('La cale', 'The Hold'), '4-4': both('Le coffre du capitaine', "The Captain's Safe"),
   '5-1': both('Les engrenages', 'The Gears'), '5-2': both('Le balancier', 'The Pendulum')},
 'names': {'odile': both('ODILE', 'ODILE'), 'basile': both('BASILE', 'BASILE'), 'milo': both('MILO', 'MILO'), 'lantenac': both('LANTENAC', 'LANTENAC')},
 'results': {'title': both('CASSE RÉUSSI', 'HEIST COMPLETE'), 'echoes': both('Échos utilisés', 'Echoes used'), 'par': both("Record de l'architecte", "Architect's record"),
   'time': both('Butin sorti à', 'Loot out at'), 'gems': both('Bijoux', 'Jewels'), 'loops': both('Boucles jouées', 'Loops played'), 'medal': both('MÉDAILLE', 'MEDAL'),
   'm1': both('Bronze', 'Bronze'), 'm2': both('Argent', 'Silver'), 'm3': both('Or', 'Gold'), 'next': both('Coffre suivant', 'Next vault'), 'retry': both('Rejouer', 'Play again'),
   'map': both('Retour à la carte', 'Back to the map'), 'replay': both('Copier le code du replay', 'Copy the replay code'), 'record': both('NOUVEAU RECORD', 'NEW RECORD'),
   'gold_hint': both("L'or demande au plus {n} écho(s).", 'Gold takes {n} echo(es) at most.')},
 'pause': {'title': both('PAUSE', 'PAUSE'), 'resume': both('Reprendre', 'Resume'), 'restart': both('Recommencer le coffre', 'Restart the vault'), 'options': both('Options', 'Options'), 'quit': both('Quitter le coffre', 'Leave the vault')},
 'options': {'title': both('OPTIONS', 'OPTIONS'), 'music': both('Musique', 'Music'), 'sfx': both('Effets', 'Effects'), 'shake': both("Secousses de l'écran", 'Screen shake'),
   'quality': both('Qualité graphique', 'Graphics quality'), 'q0': both('Basse', 'Low'), 'q1': both('Moyenne', 'Medium'), 'q2': both('Haute', 'High'),
   'lang': both('Langue', 'Language'), 'controls': both('Commandes', 'Controls'), 'fullscreen': both('Plein écran', 'Fullscreen'), 'on': both('Oui', 'On'), 'off': both('Non', 'Off'),
   'press': both('Appuie sur une touche…', 'Press a key…'), 'software': both("Accélération graphique indisponible : le jeu tourne en rendu logiciel. Mets à jour tes pilotes graphiques.", 'Graphics acceleration unavailable: the game runs in software rendering. Update your graphics drivers.')},
 'controls': {'up': both('Haut', 'Up'), 'down': both('Bas', 'Down'), 'left': both('Gauche', 'Left'), 'right': both('Droite', 'Right'), 'use': both('Agir', 'Act'), 'sprint': both('Courir', 'Run'),
   'throw': both('Lancer une pièce', 'Throw a coin'), 'rewind': both('Rembobiner', 'Rewind'), 'restart': both('Reprendre la boucle', 'Restart the loop'), 'finish': both("Terminer l'écho", 'End the echo'),
   'pause': both('Pause', 'Pause'), 'reset': both('Rétablir par défaut', 'Reset to default')},
}

# Story: dialogues before vaults ([speaker, fr, en]); cinematics as lists of (image, fr, en).
STORY = {
 '1-1': [('basile', "Alors c'est toi, la fille qui a volé la Montre de Saint-Ange. Tu sais au moins ce que tu portes au poignet ?", "So you're the girl who stole the Saint-Ange Watch. Do you even know what's on your wrist?"),
         ('odile', "Une montre qui revient quarante secondes en arrière quand on la serre. Et un mal de crâne.", "A watch that goes back forty seconds when you squeeze it. And a headache."),
         ('basile', "Le Consortium Horloger la cherche dans toute la ville. Il te faut de l'argent pour disparaître. Commence petit : la Galerie Delacroix, un coffre, personne dedans.", "The Clockmakers' Consortium is combing the city for it. You need money to disappear. Start small: the Delacroix Gallery, one safe, nobody around.")],
 '1-2': [('basile', "Une porte reliée à une plaque de pression. Il faut quelqu'un dessus, et quelqu'un d'autre pour passer.", "A door wired to a pressure plate. Someone has to stand on it, and someone else has to go through."),
         ('odile', "Je travaille seule, Basile.", "I work alone, Basile."),
         ('basile', "Plus maintenant. Fais une boucle sur la plaque ; à la suivante, ton écho t'attendra dessus.", "Not anymore. Spend one loop on the plate; on the next one, your echo will be waiting there.")],
 '1-3': [('basile', "Ce bouton n'ouvre la porte que trois secondes. Ton écho devra appuyer au bon moment… deux fois.", "This button opens the door for three seconds only. Your echo will have to press it at the right moment… twice."),
         ('odile', "Elle saura. Elle me connaît.", "She'll know. She knows me.")],
 '1-4': [('basile', "Un garde. Il voit loin devant lui, pas derrière. Regarde sa lumière au sol : là où elle n'est pas, tu es invisible.", "A guard. He sees far ahead of him, not behind. Watch his light on the floor: where it isn't, you're invisible.")],
 '1-5': [('basile', "Ce garde-là ne bouge pas de son poste. Sauf si une pièce tombe quelque part, loin de lui…", "This one never leaves his post. Unless a coin drops somewhere, far from him…"),
         ('odile', "Tout le monde se retourne pour une pièce.", "Everyone turns around for a coin.")],
 '1-6': [('basile', "Le grand salon : deux portes, deux plaques, un garde au milieu. Ce soir, tu seras trois.", "The great hall: two doors, two plates, a guard in the middle. Tonight there will be three of you."),
         ('odile', "Trois Odile. La ville n'est pas prête.", "Three Odiles. The city isn't ready.")],
 '2-1': [('milo', "Salut ! Milo. Basile m'a dit que tu avais besoin d'un œil sur les serrures électroniques.", "Hi! Milo. Basile said you needed an eye on electronic locks."),
         ('milo', "Les portes de l'hôtel s'ouvrent avec des badges de couleur. Prends-en un, et pose-le avant de crocheter : tu n'as que deux mains.", "The mansion's doors open with coloured keycards. Grab one, and put it down before you crack the safe: you only have two hands."),
         ('odile', "Deux mains par écho, Milo.", "Two hands per echo, Milo.")],
 '2-2': [('milo', "Le bal ! Deux gardes balaient la salle. Mais si quelqu'un se tient sur la plaque du vestiaire, les lustres s'éteignent.", "The ball! Two guards sweep the room. But if someone stands on the cloakroom plate, the chandeliers go out."),
         ('milo', "Dans le noir, ils ne voient qu'à un pas et demi.", "In the dark they only see a step and a half ahead.")],
 '2-3': [('lantenac', "Mademoiselle Varenne. Vous avez quelque chose qui m'appartient.", "Miss Varenne. You have something that belongs to me."),
         ('lantenac', "Le Consortium ne poursuit pas les voleuses. Il attend qu'elles se fatiguent. Vous verrez : on se fatigue vite, à se répéter.", "The Consortium doesn't chase thieves. It waits for them to tire. You'll see: repeating yourself is exhausting."),
         ('odile', "…Qui c'était ?", "…Who was that?"),
         ('basile', "Augustine Lantenac. Concentre-toi. Ce majordome tourne le dos de temps en temps : approche-toi par derrière et assomme-le.", "Augustine Lantenac. Focus. That butler turns his back now and then: sneak up behind him and knock him out.")],
 '2-4': [('milo', "Deux portes à badge. Quand le badge ouvre une porte, elle se referme quatre secondes plus tard… sauf si quelqu'un reste dans l'encadrement.", "Two keycard doors. When a card opens a door, it closes four seconds later… unless someone stays in the doorway."),
         ('odile', "Alors je vais me tenir dans les embrasures. Plusieurs fois.", "Then I'll stand in the doorways. Several times.")],
 '2-5': [('milo', "Le parquet de la salle de bal est en verre. Il chante sous les pas. Les gardes l'entendent à trois mètres.", "The ballroom floor is glass. It sings underfoot. The guards hear it three metres away."),
         ('basile', "Alors on fait chanter autre chose, ailleurs. Une pièce, Odile.", "So we make something else sing, somewhere else. A coin, Odile.")],
 '3-1': [('basile', "La Banque Lantenac. Là où le Consortium garde ses secrets. Le hall est surveillé par une caméra.", "The Lantenac Bank. Where the Consortium keeps its secrets. A camera watches the hall."),
         ('milo', "Ce terminal la coupe dix secondes. Maintiens la touche pour le pirater.", "This terminal cuts it for ten seconds. Hold the key to hack it.")],
 '3-2': [('milo', "Des lasers. Un bouton les coupe un instant. Un seul écho ne peut pas appuyer et traverser.", "Lasers. A button cuts them for a moment. One echo can't press it and get across."),
         ('odile', "J'ai l'habitude d'être deux.", "I'm used to being two.")],
 '3-3': [('basile', "La porte forte s'ouvre quand ses deux leviers sont tirés en même temps. À la même seconde.", "The strongroom door opens when both its levers are pulled at once. The same second."),
         ('odile', "La pendule de ma tête est très précise.", "The clock in my head is very precise."),
         ('basile', "Regarde la frise en bas de l'écran : elle te dit où sont tes échos, à chaque instant.", "Watch the timeline at the bottom of the screen: it tells you where your echoes are, at every instant.")],
 '3-4': [('milo', "Un verrou horaire ! Il ne s'ouvre qu'entre vingt et trente secondes. Et la caméra veille sur l'attente.", "A time lock! It only opens between twenty and thirty seconds. And the camera watches the wait.")],
 '3-5': [('basile', "La salle des coffres. Trois leviers, trois Odile. C'est le dernier casse dont j'ai besoin.", "The strongroom. Three levers, three Odiles. It's the last job I need."),
         ('odile', "Tu as besoin ? Je croyais que c'était pour moi, l'argent.", "You need? I thought the money was for me."),
         ('basile', "…Concentre-toi.", "…Focus.")],
 '4-1': [('odile', "Basile m'a vendue. Lantenac m'attendait au point de rendez-vous.", "Basile sold me out. Lantenac was waiting at the meeting point."),
         ('milo', "Je suis désolé. Mais j'ai trouvé quelque chose sur le paquebot : le Consortium y planque les plans de la Tour.", "I'm sorry. But I found something on the liner: the Consortium hides the Tower's plans there."),
         ('milo', "Deux caméras, deux terminaux. Je ne peux pas les couper en même temps à distance. Toi, si.", "Two cameras, two terminals. I can't cut them at once from afar. You can.")],
 '4-2': [('milo', "Le pont supérieur, plongé dans le noir si quelqu'un tient la plaque. Trois gardes. L'un d'eux fait les cent pas au milieu.", "The upper deck, plunged into darkness if someone holds the plate. Three guards. One of them paces in the middle.")],
 '4-3': [('milo', "Dans la cale : un badge derrière les lasers, et la porte du coffre ne s'ouvre qu'avec lui.", "In the hold: a keycard behind the lasers, and the safe's door only opens with it.")],
 '4-4': [('milo', "Le coffre du capitaine. Les plans de la Tour sont dedans, j'en suis sûr.", "The captain's safe. The Tower's plans are inside, I'm sure of it."),
         ('odile', "Deux leviers aux deux bouts du navire, un garde au milieu. Il me faut trois moi.", "Two levers at both ends of the ship, a guard in the middle. I need three of me.")],
 '5-1': [('milo', "Les plans disent tout. Lantenac veut remettre la Montre dans le grand mécanisme de la Tour, à midi pile.", "The plans say it all. Lantenac wants to put the Watch back into the Tower's great mechanism, at noon sharp."),
         ('milo', "Toute la ville rejouerait les mêmes quarante secondes. Pour toujours. Les dettes ne seraient jamais dues ; le Consortium ne perdrait jamais rien.", "The whole city would replay the same forty seconds. Forever. Debts would never come due; the Consortium would never lose a thing."),
         ('odile', "Une boucle pour tout le monde. Je sais ce que ça fait. Non.", "One loop for everyone. I know what that feels like. No."),
         ('milo', "Les portes de la Tour s'ouvrent à tour de rôle, comme des engrenages. Tiens-en une ouverte.", "The Tower's doors open in turn, like gears. Hold one open.")],
 '5-2': [('basile', "Odile. Attends. Je… je ne savais pas ce qu'elle voulait en faire.", "Odile. Wait. I… I didn't know what she meant to do with it."),
         ('odile', "Tu savais combien elle payait.", "You knew how much she was paying."),
         ('basile', "Le balancier garde le cœur du mécanisme. Le terminal coupe la caméra assez longtemps. Laisse-moi t'aider, une dernière fois.", "The pendulum guards the heart of the mechanism. The terminal cuts the camera long enough. Let me help you, one last time.")],
}
CINE = {
 'intro': [('cine_vol', "Valmont, une nuit de pluie. Odile Varenne brise une vitrine du musée et prend la seule chose qui brille encore : une montre de laiton.", 'Valmont, a rainy night. Odile Varenne breaks a museum showcase and takes the only thing still shining: a brass watch.'),
           ('cine_vol', "Le verre ne tombe pas. Il reste suspendu. Puis il remonte.", "The glass doesn't fall. It hangs in the air. Then it rises back."),
           ('cine_boucle', "Quarante secondes plus tard, elle se croise elle-même dans le couloir.", 'Forty seconds later, she passes herself in the corridor.'),
           ('cine_boucle', "La Montre ne remonte pas le temps. Elle le recommence, et ce qu'on y a fait reste là, à côté de soi.", "The Watch doesn't turn time back. It starts it over, and what you did there stays, right beside you.")],
 'betrayal': [('cine_trahison', "Le rendez-vous était sur le quai. Basile y était. Lantenac aussi.", 'The meeting was on the quay. Basile was there. So was Lantenac.'),
              ('cine_trahison', "Une enveloppe a changé de main. Odile, elle, avait déjà rembobiné.", 'An envelope changed hands. Odile had already rewound.')],
 'finale': [('cine_tour', "Midi. Au sommet de la Tour, Lantenac attend devant le cadran.", 'Noon. At the top of the Tower, Lantenac waits before the dial.'),
            ('cine_tour', "Odile n'est pas venue seule. Elle n'est jamais vraiment seule.", 'Odile did not come alone. She is never really alone.'),
            ('cine_fin', "À l'aube, la Montre s'est arrêtée. Odile l'a laissée s'arrêter.", 'At dawn, the Watch stopped. Odile let it stop.'),
            ('cine_fin', "Pour la première fois depuis des semaines, la matinée n'avait qu'une seule version.", 'For the first time in weeks, the morning had only one version.')],
}
EXTRA = {
 'workshop': {'title': both("L'ATELIER", 'THE WORKSHOP'), 'new': both('Nouveau coffre', 'New vault'), 'mine': both('Mes coffres', 'My vaults'),
   'test': both('Percer pour valider', 'Crack it to validate'), 'publish': both('Copier le code', 'Copy the code'), 'name': both('Nom du coffre', 'Vault name'),
   'tools': both('Outils', 'Tools'), 'validated': both('Validé en {n} écho(s)', 'Validated with {n} echo(es)'), 'not_validated': both('Pas encore percé par son architecte', 'Not cracked by its architect yet'),
   'help': both("Clic gauche : poser  ·  Clic droit : effacer  ·  Molette : tourner  ·  Chiffres 1-8 : canal  ·  G : tracer une ronde de garde", 'Left click: place  ·  Right click: erase  ·  Wheel: rotate  ·  Digits 1-8: channel  ·  G: draw a guard patrol'),
   'loop': both('Durée de la boucle', 'Loop length'), 'budget': both("Échos permis", 'Echoes allowed'), 'size': both('Taille', 'Size'), 'delete': both('Supprimer', 'Delete'),
   'need_safe': both('Il faut un coffre, une sortie et un point de départ.', 'A vault needs a safe, an exit and a start point.')},
 'code': {'title': both('ENTRER UN CODE', 'ENTER A CODE'), 'help': both("Colle un code de coffre (CASSE1:…) ou de replay (REPLAY1:…), reçu d'un ami.", 'Paste a vault code (CASSE1:…) or a replay code (REPLAY1:…) a friend sent you.'),
   'paste': both('Coller depuis le presse-papiers', 'Paste from the clipboard'), 'invalid': both('Code illisible.', 'Unreadable code.'), 'vault': both('Coffre « {name} » par {author}', 'Vault “{name}” by {author}'),
   'replay': both('Replay : percé en {n} écho(s)', 'Replay: cracked with {n} echo(es)'), 'watch': both('Regarder', 'Watch'), 'try': both('Tenter ce coffre', 'Try this vault')},
 'coop': {'title': both('COOPÉRATION', 'CO-OP'), 'help': both("Deux voleuses jouent chaque boucle ensemble : deux échos par boucle. Joueur 1 : ZQSD/WASD + E. Joueur 2 : manette, ou flèches + Entrée.", 'Two thieves play every loop together: two echoes per loop. Player 1: WASD + E. Player 2: gamepad, or arrows + Enter.'),
   'pick': both('Choisis un coffre de la campagne déjà ouvert.', 'Pick an unlocked campaign vault.')},
 'online': {'title': both('EN LIGNE', 'ONLINE'), 'host': both('Héberger une partie', 'Host a game'), 'join': both('Rejoindre', 'Join'), 'address': both('Adresse de l’hôte', 'Host address'),
   'waiting': both("En attente d'un ami… Donne-lui ton adresse : {addr}", 'Waiting for a friend… Give them your address: {addr}'), 'connected': both('Connecté !', 'Connected!'),
   'mode_coop': both('Coopération : un coffre, deux voleuses', 'Co-op: one vault, two thieves'), 'mode_duel': both('Duel : chacun perce le coffre de l’autre', "Duel: each cracks the other's vault"),
   'failed': both('Connexion impossible.', 'Connection failed.'), 'lost': both('Connexion perdue.', 'Connection lost.'), 'upnp': both('Ouverture du port automatique : {s}', 'Automatic port opening: {s}'),
   'duel_pick': both('Choisis le coffre que ton adversaire devra percer.', 'Pick the vault your opponent will have to crack.'),
   'duel_wait': both("Ton adversaire choisit…", 'Your opponent is choosing…'), 'duel_won': both('Tu as percé le premier !', 'You cracked it first!'), 'duel_lost': both("L'adversaire a percé en premier.", 'Your opponent cracked it first.'),
   'opp': both('Adversaire : boucle {loop}, {echoes} écho(s)', 'Opponent: loop {loop}, {echoes} echo(es)')},
 'ending': {'credits': both("CASSE — conception, code, musique : un studio d'une seule personne et ses échos. Merci d'avoir joué.", 'CASSE — design, code, music: a one-person studio and its echoes. Thanks for playing.')},
}
EXTRA['tool'] = {k: both(f, e) for k, f, e in [
  ('wall', 'Mur', 'Wall'), ('floor', 'Sol', 'Floor'), ('glass', 'Verre', 'Glass'), ('exit', 'Sortie', 'Exit'), ('spawn', 'Départ', 'Start'), ('safe', 'Coffre', 'Safe'),
  ('door', 'Porte', 'Door'), ('door_chan', 'Porte à canal', 'Channel door'), ('door_key', 'Porte à badge', 'Keycard door'), ('door_vault', 'Porte forte', 'Vault door'),
  ('door_time', 'Verrou horaire', 'Time lock'), ('plate', 'Plaque', 'Plate'), ('button', 'Bouton', 'Button'), ('lever', 'Levier', 'Lever'), ('vlever', 'Levier fort', 'Vault lever'),
  ('term', 'Terminal', 'Terminal'), ('laser', 'Laser', 'Laser'), ('cam', 'Caméra', 'Camera'), ('dark', 'Zone sombre', 'Dark zone'), ('card', 'Badge', 'Keycard'),
  ('gem', 'Bijou', 'Jewel'), ('guard', 'Garde', 'Guard'), ('erase', 'Gomme', 'Eraser')]}
EXTRA['workshop'].update({'size_w': both('Largeur', 'Width'), 'size_h': both('Hauteur', 'Height'), 'channel': both('Canal {n}', 'Channel {n}')})
T.update(EXTRA)

def build(lang):
    k = 0 if lang == 'fr' else 1
    def conv(v):
        if isinstance(v, tuple):
            return v[k]
        if isinstance(v, dict):
            return {a: conv(b) for a, b in v.items()}
        return v
    out = conv(T)
    out['story'] = {vid: [[l[0], l[1 + k]] for l in lines] for vid, lines in STORY.items()}
    out['cine'] = {cid: [{'image': im, 'text': (fr if k == 0 else en)} for im, fr, en in shots] for cid, shots in CINE.items()}
    return out

for lang in ('fr', 'en'):
    with open(os.path.join(OUT, f'{lang}.json'), 'w') as f:
        json.dump(build(lang), f, ensure_ascii=False, indent=0)
print('ok')
