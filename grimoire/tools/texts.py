#!/usr/bin/env python3
"""All the words of GRIMOIRE, French and English. Writes assets/i18n/fr.json and en.json."""
import json, os
OUT = os.path.join(os.path.dirname(__file__), '..', 'assets', 'i18n')
os.makedirs(OUT, exist_ok=True)

def both(fr, en):
    return (fr, en)

GLYPHS = {
 'fire': ('Feu', 'Fire', "Un trait de feu le long du couloir. Éteint par l'Eau ; brûle les murs deux fois plus vite.", 'A bolt of fire down the lane. Put out by Water; burns walls twice as fast.'),
 'water': ('Eau', 'Water', "Une vague qui éteint le Feu et noie l'encre de l'adversaire. La Glace la change en mur.", "A wave that puts out Fire and drowns the foe's ink. Ice turns it into a wall."),
 'wind': ('Vent', 'Wind', "Un tourbillon qui renvoie à l'envoyeur les sorts qu'il croise (sauf la Terre).", 'A whirlwind that sends back the spells it meets (except Earth).'),
 'bolt': ('Foudre', 'Lightning', "Frappe tout le couloir après un instant. Un Bouclier l'arrête net ; l'Eau la rend plus forte.", 'Strikes the whole lane after a moment. A Shield stops it dead; Water makes it stronger.'),
 'earth': ('Terre', 'Earth', "Des pics lents et lourds. Brisent Boucliers et Murs ; le Vent n'y peut rien.", 'Slow, heavy spikes. Break Shields and Walls; Wind cannot turn them.'),
 'shield': ('Bouclier', 'Shield', 'Un cercle d’or qui encaisse les coups quelques secondes.', 'A golden circle that takes the blows for a few seconds.'),
 'wall': ('Mur', 'Wall', 'Un mur de pierre devant toi, dans le couloir où tu le traces. Il bloque tout.', 'A stone wall before you, in the lane where you draw it. It blocks everything.'),
 'star': ('Étoile', 'Star', 'Trois étoiles, une par couloir : on ne peut pas toutes les esquiver.', 'Three stars, one per lane: there is no dodging them all.'),
 'heal': ('Soin', 'Healing', 'Rend des forces peu à peu.', 'Restores strength little by little.'),
 'ice': ('Glace', 'Ice', "Gèle ce qu'elle touche ; touché, l'adversaire voit son encre ralentir.", 'Freezes what it touches; hit, the foe’s ink flows slower.'),
 'time': ('Sablier', 'Hourglass', "Les sorts adverses ralentissent de moitié, et ton encre coule plus vite.", 'The foe’s spells slow to half speed, and your ink flows faster.'),
 'seal': ('Sceau', 'Seal', 'Le prochain sort adverse lancé dans les trois secondes se dissipe.', 'The next spell the foe casts within three seconds fizzles out.'),
}
COMBOS = {20: ('Tornade de feu !', 'Firestorm!'), 21: ('Orage !', 'Storm!'), 22: ('Magma !', 'Magma!'), 23: ('Lance de glace !', 'Ice lance!'), 24: ('Miroir !', 'Mirror!'), 25: ('Constellation !', 'Constellation!')}

T = {
 'common': {'back': both('Retour', 'Back'), 'yes': both('Oui', 'Yes'), 'no': both('Non', 'No'), 'skip': both('Passer', 'Skip'), 'continue': both('Continuer', 'Continue')},
 'names': {'ysolde': both('YSOLDE', 'YSOLDE'), 'aldric': both('MAÎTRE ALDRIC', 'MASTER ALDRIC'), 'reine': both("LA REINE D'ENCRE", 'THE INK QUEEN'),
   'cendres': both('LA SORCIÈRE DES CENDRES', 'THE ASH WITCH'), 'marees': both('LE MOINE DES MARÉES', 'THE TIDE MONK'), 'horloger': both("L'HORLOGER", 'THE CLOCKMAKER')},
 'glyph': {k: both(v[0], v[1]) for k, v in GLYPHS.items()},
 'glyph_desc': {k: both(v[2], v[3]) for k, v in GLYPHS.items()},
 'combo': {str(k): both(*v) for k, v in COMBOS.items()},
 'arena': {'ar_scriptorium': both('Le Scriptorium', 'The Scriptorium'), 'ar_cendres': both('La Forge des Cendres', 'The Ash Forge'), 'ar_marees': both('Le Cloître des Marées', 'The Tide Cloister'),
   'ar_horloge': both("Le Cœur de l'Horloge", 'The Heart of the Clock'), 'ar_reine': both('La Bibliothèque Engloutie', 'The Drowned Library')},
 'title': {'tagline': both("Ce que tu traces, le monde l'écrit.", 'What you draw, the world writes.'), 'press': both('Appuie sur une touche', 'Press any key'),
   'campaign': both("L'Apprentie", 'The Apprentice'), 'campaign_sub': both('La campagne — dix chapitres', 'The campaign — ten chapters'),
   'duel': both('Duel', 'Duel'), 'duel_sub': both('Contre un maître, ou à deux sur un écran', 'Against a master, or two on one screen'),
   'rives': both('Deux Rives', 'Two Banks'), 'rives_sub': both('Expédition seul ou à deux', 'An expedition, alone or with a friend'),
   'online': both('En ligne', 'Online'), 'online_sub': both('Duel ou Deux Rives à distance', 'Duel or Two Banks over the network'),
   'book': both('Le Grimoire', 'The Grimoire'), 'book_sub': both("Les glyphes, et une page pour s'exercer", 'The glyphs, and a page to practise on'),
   'options': both('Options', 'Options'), 'quit': both('Quitter', 'Quit')},
 'campaign': {'title': both("L'APPRENTIE", 'THE APPRENTICE'), 'sub': both("Ysolde, apprentie de Maître Aldric, et l'encre qui s'est échappée.", 'Ysolde, apprentice to Master Aldric, and the ink that got away.'),
   'duel': both('duel', 'duel'), 'rives': both('expédition sur les Deux Rives', 'expedition on the Two Banks'),
   'done': both('✦ Réussi', '✦ Done'), 'locked': both('Réussis le chapitre précédent.', 'Win the previous chapter.'),
   'teach': both('Glyphes appris dans ce chapitre :', 'Glyphs learned in this chapter:'), 'all': both('Tous tes glyphes.', 'All your glyphs.')},
 'chapter': {
   '1': {'name': both('La Leçon', 'The Lesson'), 'desc': both("Premier jour au scriptorium. Aldric t'apprend trois glyphes, puis te met à l'épreuve — gentiment.", 'First day in the scriptorium. Aldric teaches you three glyphs, then puts you to the test — gently.')},
   '2': {'name': both('Les Deux Rives', 'The Two Banks'), 'desc': both("L'encre a fui par le fleuve. Tu tiens les deux rives à la fois : ce que tu traces sur une rive y prend corps.", 'The ink has fled down the river. You hold both banks at once: what you draw on a bank comes alive there.')},
   '3': {'name': both('La Sorcière des Cendres', 'The Ash Witch'), 'desc': both("Elle ne parle qu'en flammes. Apprends la Terre : le feu ne l'arrête pas.", 'She speaks only in flames. Learn Earth: fire cannot stop it.')},
   '4': {'name': both('Le Gué des Brumes', 'The Misty Ford'), 'desc': both("Brumes, ronces et gouffres. Les créatures d'encre se multiplient.", 'Mists, thorns and chasms. The ink creatures are multiplying.')},
   '5': {'name': both('Le Moine des Marées', 'The Tide Monk'), 'desc': both("Vagues et glace, à perte de vue. La Foudre traverse l'eau : à toi d'en profiter.", 'Waves and ice as far as the eye can see. Lightning runs through water: make use of it.')},
   '6': {'name': both("La Forêt d'Encre", 'The Ink Forest'), 'desc': both('Plus de lumière sous ces arbres. Étoiles pour y voir, Soin pour tenir.', 'No light under these trees. Stars to see, Healing to endure.')},
   '7': {'name': both("L'Horloger", 'The Clockmaker'), 'desc': both('Il ralentit le temps et scelle tes sorts. Apprends le Sablier, et ne lance pas à l’aveugle.', 'He slows time and seals your spells. Learn the Hourglass, and do not cast blindly.')},
   '8': {'name': both('La Bête du Fleuve', 'The Beast of the River'), 'desc': both("Au bout du fleuve, la Bête garde la Bibliothèque. Son œil passe d'une rive à l'autre.", 'At the end of the river, the Beast guards the Library. Its eye moves from bank to bank.')},
   '9': {'name': both("L'Épreuve du Maître", "The Master's Trial"), 'desc': both("Aldric ne retient plus ses coups. Il veut savoir si tu es prête.", 'Aldric no longer holds back. He wants to know if you are ready.')},
   '10': {'name': both("La Reine d'Encre", 'The Ink Queen'), 'desc': both("Elle s'est échappée d'un livre interdit. Elle connaît tous les glyphes. Toi aussi, maintenant.", 'She escaped from a forbidden book. She knows every glyph. So do you, now.')},
 },
 'book': {'title': both('LE GRIMOIRE', 'THE GRIMOIRE'), 'lesson': both('LEÇON', 'LESSON'), 'practice': both('Trace un glyphe sur la page', 'Draw a glyph on the page'),
   'draw_here': both('Trace ici', 'Draw here'), 'fizzle': both("Raté — l'encre ne reconnaît rien", 'Missed — the ink recognises nothing'),
   'unknown': both("Tu n'as pas encore appris ce glyphe.", 'You have not learned this glyph yet.'), 'cost': both('Encre : {n}', 'Ink: {n}'),
   'start': both('Au combat', 'To battle')},
 'duel': {'round': both('Manche {n}', 'Round {n}'), 'round_small': both('MANCHE {n}', 'ROUND {n}'), 'draw_to_cast': both('Trace un glyphe pour lancer un sort', 'Draw a glyph to cast a spell'),
   'hint': both("Trace dans le couloir visé  ·  trait vertical ou <kbd>{up}</kbd>/<kbd>{down}</kbd> : changer de couloir  ·  clic droit : effacer  ·  <kbd>{book}</kbd> : grimoire",
                "Draw in the lane you aim at  ·  vertical stroke or <kbd>{up}</kbd>/<kbd>{down}</kbd>: change lane  ·  right click: erase  ·  <kbd>{book}</kbd>: grimoire"),
   'fizzle': both('raté', 'missed'), 'unknown': both('glyphe inconnu', 'unknown glyph'), 'noink': both("plus d'encre", 'out of ink'), 'sealed': both('dissipé !', 'dispelled!'),
   'frenzy': both("L'ENCRE S'EMBALLE", 'THE INK RUNS WILD'), 'round_to': both('Manche pour {name}', 'Round to {name}'),
   'victory': both('VICTOIRE', 'VICTORY'), 'defeat': both('DÉFAITE', 'DEFEAT'), 'winner': both('{name} remporte le duel', '{name} wins the duel')},
 'rives': {'title': both('LES DEUX RIVES', 'THE TWO BANKS'), 'solo_hint': both('Trace sur une rive : le sort y prend corps, là où tu le dessines.', 'Draw on a bank: the spell takes shape there, where you draw it.'),
   'coop_hint': both("Chacun ne peut tracer que sur la rive de l'autre : protégez-vous mutuellement.", "Each of you can only draw on the other's bank: guard each other."),
   'other_bank': both("ta rive est gardée par l'autre", 'your bank is guarded by the other'), 'cleared': both('le chemin est libre', 'the way is clear'),
   'combo': both('combinaison !', 'combination!'), 'duo': both('À DEUX MAINS !', 'TWO HANDS!'), 'blocked': both('chemin barré', 'way barred'),
   'beast': both('LA BÊTE DU FLEUVE', 'THE BEAST OF THE RIVER'), 'beast_sub': both("Frappe son œil d'or — Foudre, Feu, Terre. Bouclier contre ses coups, Sceau pour les dissiper.", 'Strike its golden eye — Lightning, Fire, Earth. Shield against its blows, Seal to dispel them.'),
   'won': both('EXPÉDITION RÉUSSIE', 'EXPEDITION COMPLETE'), 'won_sub': both('Les deux rives sont sauves.', 'Both banks are safe.'), 'lost': both("L'ENCRE L'EMPORTE", 'THE INK PREVAILS'), 'lost_sub': both('Un mage est tombé.', 'A mage has fallen.')},
 'setup': {'duel': both('DUEL', 'DUEL'), 'rives': both('DEUX RIVES', 'TWO BANKS'),
   'duel_rules': both('Trois couloirs, deux mages. Trace tes glyphes, lis ceux de l’adversaire, réponds plus vite. Tous les glyphes sont ouverts. À deux : souris contre manette.',
                      'Three lanes, two mages. Draw your glyphs, read your opponent’s, answer faster. Every glyph is open. Two players: mouse against gamepad.'),
   'rives_rules': both("Deux mages avancent le long du fleuve. Seul, tu tiens les deux rives ; à deux (souris + manette), chacun protège la rive de l'autre.",
                       'Two mages walk along the river. Alone, you hold both banks; with two (mouse + gamepad), each guards the other’s bank.'),
   'opponent': both('Adversaire', 'Opponent'), 'p2': both('Joueur 2 (manette)', 'Player 2 (gamepad)'), 'level': both('Niveau', 'Level'),
   'level0': both('Apprenti', 'Apprentice'), 'level1': both('Mage', 'Mage'), 'level2': both('Maître', 'Master'), 'level3': both('Archimage', 'Archmage'),
   'arena': both('Arène', 'Arena'), 'rounds': both('Manches', 'Rounds'), 'players': both('Joueurs', 'Players'), 'solo': both('Seul', 'Alone'), 'duo': both('À deux (souris + manette)', 'Two (mouse + gamepad)'),
   'expedition': both('Expédition', 'Expedition'), 'exp0': both('I — La rive douce', 'I — The gentle bank'), 'exp1': both('II — Le gué', 'II — The ford'),
   'exp2': both('III — La forêt', 'III — The forest'), 'exp3': both('IV — La Bête', 'IV — The Beast'), 'go': both('Commencer', 'Begin')},
 'results': {'won': both('VICTOIRE', 'VICTORY'), 'lost': both('DÉFAITE', 'DEFEAT'), 'time': both('Durée', 'Time'), 'rounds': both('Manches', 'Rounds'), 'casts': both('Sorts lancés', 'Spells cast'),
   'duel_won': both("Ton encre a été la plus juste.", 'Your ink was the truest.'), 'duel_lost': both("L'adversaire a lu tes glyphes avant toi.", 'Your opponent read your glyphs before you did.'),
   'rives_won': both('Les deux mages ont atteint le bout du chemin.', 'Both mages reached the end of the road.'), 'rives_lost': both("L'encre a submergé une rive.", 'The ink overwhelmed a bank.'),
   'next': both('Chapitre suivant', 'Next chapter'), 'retry': both('Recommencer', 'Try again'), 'campaign': both('Retour aux chapitres', 'Back to the chapters'), 'menu': both('Menu principal', 'Main menu')},
 'pause': {'title': both('PAUSE', 'PAUSE'), 'resume': both('Reprendre', 'Resume'), 'restart': both('Recommencer', 'Restart'), 'options': both('Options', 'Options'), 'quit': both('Quitter', 'Quit')},
 'options': {'title': both('OPTIONS', 'OPTIONS'), 'music': both('Musique', 'Music'), 'sfx': both('Effets', 'Effects'), 'sens': both('Sensibilité', 'Sensitivity'),
   'quality': both('Qualité graphique', 'Graphics quality'), 'q0': both('Basse', 'Low'), 'q1': both('Moyenne', 'Medium'), 'q2': both('Haute', 'High'),
   'lang': both('Langue', 'Language'), 'controls': both('Commandes', 'Controls'), 'fullscreen': both('Plein écran', 'Fullscreen'), 'on': both('Oui', 'On'), 'off': both('Non', 'Off'),
   'press': both('Appuie sur une touche…', 'Press a key…'), 'software': both('Accélération graphique indisponible.', 'Graphics acceleration unavailable.')},
 'controls': {'step_up': both('Couloir du dessus', 'Lane above'), 'step_down': both('Couloir du dessous', 'Lane below'), 'book': both('Grimoire', 'Grimoire'), 'pause': both('Pause', 'Pause'), 'reset': both('Rétablir par défaut', 'Reset to default')},
 'online': {'title': both('EN LIGNE', 'ONLINE'), 'rules': both("À deux, à distance : un duel, ou une expédition sur les Deux Rives. L'hôte choisit.", 'For two, at a distance: a duel, or an expedition on the Two Banks. The host chooses.'),
   'host': both('Héberger', 'Host'), 'join': both('Rejoindre', 'Join'), 'address': both("Adresse de l'hôte", 'Host address'),
   'waiting': both("En attente… Donne ton adresse : {addr}", 'Waiting… Give your address: {addr}'), 'connected': both('Connecté !', 'Connected!'),
   'failed': both('Connexion impossible.', 'Connection failed.'), 'lost': both('Connexion perdue.', 'Connection lost.'), 'upnp': both('Ouverture du port automatique : {s}', 'Automatic port opening: {s}'),
   'pick': both('Choisis :', 'Choose:'), 'duel_in': both('Duel — {arena}', 'Duel — {arena}'), 'rives_in': both('Deux Rives — {exp}', 'Two Banks — {exp}'),
   'wait_host': both("En attente de l'hôte…", 'Waiting for the host…'), 'again': both('Encore', 'Again')},
 'ending': {'credits': both("GRIMOIRE — conception, code, musique : un studio d'une seule personne et sa plume. Merci d'avoir joué.", 'GRIMOIRE — design, code, music: a one-person studio and its quill. Thanks for playing.')},
}

BRIEF = {
 1: [('aldric', "Ah, te voilà. Pose ce livre, Ysolde : aujourd'hui, on n'écrit pas de mots. On écrit le monde.", "Ah, there you are. Put that book down, Ysolde: today we do not write words. We write the world."),
     ('aldric', "Un glyphe, c'est une forme que l'encre reconnaît. Trace-la bien, et elle obéit. Trace-la grand, et elle obéit fort.", 'A glyph is a shape the ink recognises. Draw it well, and it obeys. Draw it large, and it obeys hard.'),
     ('ysolde', "Et si je la trace mal ?", 'And if I draw it badly?'),
     ('aldric', "Elle se moque de toi, et ton adversaire aussi. Allons : le Feu, l'Eau, le Bouclier. Puis un petit duel avec ton vieux maître.", 'It laughs at you, and so does your opponent. Come: Fire, Water, Shield. Then a little duel with your old master.')],
 2: [('aldric', "Ysolde ! Le livre interdit, sur le pupitre du fond… il est ouvert. Et vide.", 'Ysolde! The forbidden book, on the back lectern… it is open. And empty.'),
     ('ysolde', "L'encre a coulé jusqu'au fleuve. Il y en a sur les deux rives !", 'The ink ran down to the river. It is on both banks!'),
     ('aldric', "Je prends la rive de nuit, toi celle du jour. Mais c'est toi qui traceras : ce que tu dessines sur une rive y prend corps. Le Mur fait un pont, le Vent chasse la brume.", 'I take the night bank, you the day bank. But you will draw: what you draw on a bank comes alive there. Wall makes a bridge, Wind clears the mist.')],
 3: [('cendres', "Une apprentie ? Aldric m'envoie une apprentie ? Je vais la rendre à son maître en petit tas de cendres.", 'An apprentice? Aldric sends me an apprentice? I will send her back to her master as a little pile of ash.'),
     ('aldric', "Elle a l'encre noire de la Reine sur les mains, Ysolde. Elle sert l'ennemi sans le savoir. Apprends la Terre : ses flammes n'y peuvent rien.", 'She has the Queen’s black ink on her hands, Ysolde. She serves the enemy without knowing it. Learn Earth: her flames can do nothing against it.')],
 4: [('ysolde', "La Sorcière m'a dit où l'encre se cache : plus bas, au gué. Il y a des créatures partout.", 'The Witch told me where the ink hides: further down, at the ford. There are creatures everywhere.'),
     ('aldric', "Alors gèle-les. La Glace arrête ce qui court — et ce qui nage.", 'Then freeze them. Ice stops what runs — and what swims.')],
 5: [('marees', "La mer efface tout, petite. Les traces sur le sable, les noms sur les tombes… et les glyphes mal tracés.", 'The sea erases everything, little one. Footprints in the sand, names on graves… and badly drawn glyphs.'),
     ('ysolde', "Alors je tracerai bien.", 'Then I will draw well.'),
     ('aldric', "La Foudre, Ysolde. Elle frappe tout le couloir d'un coup, et l'eau la porte plus loin encore.", 'Lightning, Ysolde. It strikes the whole lane at once, and water carries it further still.')],
 6: [('aldric', "La forêt d'encre. Plus un rayon de lumière. Tu auras besoin d'Étoiles pour voir, et de Soin pour tenir jusqu'au bout.", 'The ink forest. Not a single ray of light. You will need Stars to see, and Healing to last to the end.'),
     ('ysolde', "Maître… vous boitez.", 'Master… you are limping.'),
     ('aldric', "Je suis vieux, c'est tout. Avance.", 'I am old, that is all. Walk on.')],
 7: [('horloger', "Tic. Tu as trois secondes de retard sur tout. Tac. Je vais te les prendre.", 'Tick. You are three seconds late for everything. Tock. I am going to take them from you.'),
     ('aldric', "Il scelle les sorts qu'on lance sans réfléchir. Regarde ses glyphes se former avant de répondre. Et prends-lui son Sablier.", 'He seals spells cast without thinking. Watch his glyphs form before you answer. And take his Hourglass.')],
 8: [('aldric', "La Bibliothèque engloutie est au bout du fleuve. La Bête la garde. Son œil passe d'une rive à l'autre : frappe là où il regarde.", 'The drowned Library lies at the end of the river. The Beast guards it. Its eye moves from bank to bank: strike where it looks.'),
     ('aldric', "Et le Sceau, enfin. Le dernier glyphe que je peux t'apprendre. Il dissipe ce qui va frapper.", 'And the Seal, at last. The last glyph I can teach you. It dispels what is about to strike.')],
 9: [('aldric', "Avant que tu entres dans la Bibliothèque, je dois savoir. Pas de leçon, cette fois. Tous les glyphes. Je ne retiendrai rien.", 'Before you enter the Library, I must know. No lesson this time. Every glyph. I will hold nothing back.'),
     ('ysolde', "Et si je vous blesse ?", 'And if I hurt you?'),
     ('aldric', "Alors j'aurai été un bon maître.", 'Then I will have been a good teacher.')],
 10: [('reine', "Petite scribe. Tu crois que les glyphes t'appartiennent ? Ils sont faits d'encre. Et l'encre, c'est moi.", 'Little scribe. You think the glyphs belong to you? They are made of ink. And the ink is me.'),
      ('ysolde', "Les glyphes appartiennent à celle qui les trace.", 'The glyphs belong to whoever draws them.'),
      ('reine', "Alors trace. Trace jusqu'à ce que ta main tremble.", 'Then draw. Draw until your hand shakes.')],
}

CINE = {
 'intro': [
   ('bg_title', "Au sommet de la falaise, la tour-scriptorium d'Aldric. Ici, on n'écrit pas des livres : on écrit le monde.", 'At the top of the cliff, Aldric’s scriptorium tower. Here, one does not write books: one writes the world.'),
   ('key_art', "Car l'encre d'or obéit à qui sait la tracer. Une flamme, une vague, une spirale de vent : chaque glyphe est un sort.", 'For the golden ink obeys whoever can draw it. A flame, a wave, a spiral of wind: each glyph is a spell.'),
   ('ar_reine', "Mais au fond de la bibliothèque dort un livre que nul ne doit ouvrir. Son encre, elle, ne dort jamais.", 'But at the back of the library sleeps a book no one must open. Its ink never sleeps.'),
 ],
 'finale': [
   ('ar_reine', "La Reine d'Encre se défait en mille lettres, qui retombent sur les pages comme une pluie noire.", 'The Ink Queen unravels into a thousand letters, falling onto the pages like black rain.'),
   ('key_art', "Ysolde les rassemble une à une, et referme le livre. Sur la couverture, un glyphe nouveau : le sien.", 'Ysolde gathers them one by one, and closes the book. On the cover, a new glyph: her own.'),
   ('bg_title', "Le soir même, Maître Aldric lui confie la clef du scriptorium. « Ce que tu traces, le monde l'écrit. N'oublie jamais de bien tracer. »", 'That evening, Master Aldric hands her the key to the scriptorium. “What you draw, the world writes. Never forget to draw well.”'),
 ],
}

def build(lang):
    k = 0 if lang == 'fr' else 1
    def conv(v):
        if isinstance(v, tuple):
            return v[k]
        if isinstance(v, dict):
            return {a: conv(b) for a, b in v.items()}
        return v
    out = conv(T)
    out['brief'] = {str(n): [[l[0], l[1 + k]] for l in lines] for n, lines in BRIEF.items()}
    out['cine'] = {cid: [{'image': im, 'text': (fr if k == 0 else en)} for im, fr, en in shots] for cid, shots in CINE.items()}
    return out

for lang in ('fr', 'en'):
    with open(os.path.join(OUT, f'{lang}.json'), 'w') as f:
        json.dump(build(lang), f, ensure_ascii=False, indent=0)
print('ok')
