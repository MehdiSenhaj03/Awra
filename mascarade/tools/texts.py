#!/usr/bin/env python3
"""All the words of MASCARADE, French and English. Writes assets/i18n/fr.json and en.json."""
import json, os
OUT = os.path.join(os.path.dirname(__file__), '..', 'assets', 'i18n')
os.makedirs(OUT, exist_ok=True)

def both(fr, en):
    return (fr, en)

T = {
 'common': {'back': both('Retour', 'Back'), 'yes': both('Oui', 'Yes'), 'no': both('Non', 'No'), 'skip': both('Passer', 'Skip'),
   'continue': both('Continuer', 'Continue')},
 'names': {'corbeau': both('LE CORBEAU', 'THE RAVEN'), 'lynx': both('LA LYNX', 'THE LYNX'), 'dogesse': both('LA DOGESSE', 'THE DOGARESSA')},
 'title': {'tagline': both('Cent masques. Un seul ment. Une seule balle.', 'A hundred masks. One of them lies. One bullet.'),
   'press': both('Appuie sur une touche', 'Press any key'),
   'season': both('La Saison', 'The Season'), 'season_sub': both('Neuf bals, deux points de vue — solo', 'Nine balls, two sides — solo'),
   'online': both('Le Contrat', 'The Contract'), 'online_sub': both("Espion contre tireuse, en ligne à deux", 'Spy versus sniper, online for two'),
   'party': both('Le Bal des Assassins', "The Assassins' Ball"), 'party_sub': both('2 à 4 joueurs sur un écran', '2 to 4 players on one screen'),
   'options': both('Options', 'Options'), 'quit': both('Quitter', 'Quit')},
 'venue': {'salon': both('Le Salon Vermeil', 'The Vermeil Salon'), 'serre': both('La Serre aux Orchidées', 'The Orchid Glasshouse'), 'opera': both("Le Foyer de l'Opéra", 'The Opera Foyer')},
 'season': {'title': both('LA SAISON', 'THE SEASON'), 'sub': both('Neuf bals au Palais Vermeil. Tour à tour le Corbeau et la Lynx.', 'Nine balls at the Palazzo Vermeil. In turn the Raven and the Lynx.'),
   'night': both('Nuit {n}', 'Night {n}'), 'as_spy': both("l'espion", 'the spy'), 'as_hunter': both('la tireuse', 'the sniper'),
   'done': both('✦ Réussie — meilleur temps {t}', '✦ Done — best time {t}'), 'locked': both('Réussis la nuit précédente pour ouvrir celle-ci.', 'Win the previous night to open this one.'),
   'open': both('Adversaire : {skill}', 'Opponent: {skill}'),
   'skill0': both('novice', 'novice'), 'skill1': both('attentive', 'attentive'), 'skill2': both('redoutable', 'formidable'), 'skill3': both('implacable', 'relentless')},
 'night': {
   '1': {'name': both('Le Premier Bal', 'The First Ball'), 'desc': both("Ton premier contrat pour la Dogesse. Fonds-toi dans la foule, accomplis deux missions, ne te fais pas abattre.", "Your first contract for the Dogaressa. Blend into the crowd, do two missions, don't get shot.")},
   '2': {'name': both('La Galerie', 'The Gallery'), 'desc': both("Changement de point de vue : tu es la Lynx, dans la galerie. Quelqu'un en bas n'est pas ce qu'il prétend. Une balle.", "Change of sides: you are the Lynx, up in the gallery. Someone below is not who they claim. One bullet.")},
   '3': {'name': both('Le Menuet des Ombres', 'The Minuet of Shadows'), 'desc': both("Trois missions sur quatre, et la Lynx a pris ses marques. Ne cours pas. Ne reviens pas sur tes pas.", "Three missions out of four, and the Lynx knows the room now. Don't run. Don't double back.")},
   '4': {'name': both('Les Orchidées', 'The Orchids'), 'desc': both("La serre de verre, ses palmiers, sa fontaine. L'espion d'en face est plus fin : regarde qui s'arrête là où personne ne s'arrête.", "The glasshouse, its palms, its fountain. The spy below is subtler: watch who stops where nobody stops.")},
   '5': {'name': both('La Comtesse', 'The Countess'), 'desc': both("Une révérence devant la comtesse Morosini : c'est le signal convenu. Encore faut-il la trouver parmi cent masques.", "A bow before Countess Morosini: that is the agreed signal. You still have to find her among a hundred masks.")},
   '6': {'name': ("Les Poches de l'Ambassadeur", "The Ambassador's Pockets"), 'desc': both("Quelqu'un va faire les poches de l'ambassadeur de Vienne. Il faut l'approcher par derrière : surveille son dos.", "Someone will pick the Viennese ambassador's pockets. It takes approaching him from behind: watch his back.")},
   '7': {'name': both('Le Grand Escalier', 'The Grand Staircase'), 'desc': both("Le foyer de l'Opéra, ses miroirs, ses loges. Cinq missions possibles, trois à faire, et une tireuse qui ne pardonne plus.", "The Opera foyer, its mirrors, its boxes. Five possible missions, three to do, and a sniper who no longer forgives.")},
   '8': {'name': both('Le Rideau Rouge', 'The Red Curtain'), 'desc': both("Le Corbeau est le meilleur de la Saison. Ce soir, c'est toi ou lui.", "The Raven is the best of the Season. Tonight it's you or him.")},
   '9': {'name': both('La Dernière Valse', 'The Last Waltz'), 'desc': both("Quatre missions sur cinq. La Dogesse elle-même est au bal. Et la Lynx sait exactement qui tu es… ou presque.", "Four missions out of five. The Dogaressa herself is at the ball. And the Lynx knows exactly who you are… almost.")},
 },
 'mission': {
   'letter': {'name': both('Recopier la lettre', 'Copy the letter'), 'where': both("au secrétaire d'écriture", 'at the writing desk')},
   'poison': {'name': both('Verser la poudre', 'Pour the powder'), 'where': both('dans une coupe du buffet marqué', 'into a glass at the marked buffet')},
   'bug': {'name': both('Déposer le chiffre', 'Drop the cipher'), 'where': both("dans le socle d'une statue", "inside a statue's plinth")},
   'contact': {'name': both('Parler au contact', 'Talk to the contact'), 'where': both("rester près de lui quand il s'arrête", 'stay by him when he stops')},
   'pocket': {'name': both("Faire les poches de l'ambassadeur", "Pick the ambassador's pocket"), 'where': both('en arrivant derrière lui', 'coming from behind him')},
   'countess': {'name': both('Saluer la comtesse', 'Bow to the countess'), 'where': both('une révérence tout près d’elle', 'a bow right next to her')},
 },
 'ball': {
   'you_spy': both('LE CORBEAU', 'THE RAVEN'), 'you_hunter': both('LA LYNX', 'THE LYNX'),
   'goal_spy': both("Accomplis {n} missions. Ne te fais pas remarquer.", "Do {n} missions. Don't get noticed."),
   'goal_hunter': both("Trouve l'espion parmi les invités. Une seule balle.", 'Find the spy among the guests. One bullet.'),
   'missions': both('TES MISSIONS', 'YOUR MISSIONS'), 'missions_hunter': both("CE QUE L'ESPION PEUT FAIRE", 'WHAT THE SPY MAY DO'),
   'progress': both('MISSIONS ACCOMPLIES  {n} / {need}', 'MISSIONS DONE  {n} / {need}'),
   'keys_spy': both("<kbd>{move}</kbd> marcher  ·  <kbd>{act}</kbd> maintenir : faire comme les autres (boire, admirer, danser) — et accomplir une mission  ·  <kbd>{hurry}</kbd> presser le pas (on le remarque)",
                    "<kbd>{move}</kbd> walk  ·  <kbd>{act}</kbd> hold: do as the others do (drink, admire, dance) — and do a mission  ·  <kbd>{hurry}</kbd> hurry (it shows)"),
   'keys_hunter': both("Souris : viser  ·  <kbd>{scope}</kbd> épauler (lunette)  ·  <kbd>{fire}</kbd> tirer — une seule balle  ·  <kbd>{mark}</kbd> marquer un suspect",
                       "Mouse: aim  ·  <kbd>{scope}</kbd> shoulder (scope)  ·  <kbd>{fire}</kbd> fire — one bullet  ·  <kbd>{mark}</kbd> mark a suspect"),
   'keys_party': both("Fonds-toi parmi les invités et poignarde les autres joueurs. Poignarder un innocent te démasque.", 'Blend in among the guests and stab the other players. Stabbing an innocent exposes you.'),
   'hold': both('Maintiens {key} — {m}', 'Hold {key} — {m}'), 'scope_first': both("Épaule d'abord ({key}) pour tirer.", 'Shoulder first ({key}) to fire.'),
   'no_bullet': both('plus de balle', 'no bullet left'),
   'mission_ok': both('Mission accomplie : {m}', 'Mission done: {m}'),
   'mission_seen': both('Une bougie vacille… une mission vient d’être accomplie : {m}', 'A candle flickers… a mission was just done: {m}'),
   'won': both('VICTOIRE', 'VICTORY'), 'lost': both('DÉFAITE', 'DEFEAT'),
   'why_spy_missions': both("Le Corbeau a accompli ses missions et s'est fondu dans la nuit.", 'The Raven did his missions and melted into the night.'),
   'why_spy_innocent': both("La Lynx a abattu un innocent. Dans le scandale, le Corbeau s'éclipse.", 'The Lynx shot an innocent. In the uproar, the Raven slips away.'),
   'why_spy_miss': both('La balle s’est perdue dans le stuc.', 'The bullet was lost in the stucco.'),
   'why_hunter_spy': both('La Lynx a abattu le Corbeau.', 'The Lynx shot the Raven.'),
   'why_hunter_time': both("Minuit sonne : le bal s'achève avant que le Corbeau ait fini.", 'Midnight strikes: the ball ends before the Raven is done.'),
   'party_title': both('LE BAL DES ASSASSINS', "THE ASSASSINS' BALL"), 'party_goal': both('Premier à {n} victimes', 'First to {n} kills'),
   'party_sub': both('PREMIER À {n}', 'FIRST TO {n}'), 'player': both('Joueur {n}', 'Player {n}'),
   'dev_kb': both('clavier', 'keyboard'), 'dev_kb2': both('clavier (droite)', 'keyboard (right)'), 'dev_pad': both('manette', 'gamepad'),
   'kill': both('Le joueur {a} a poignardé le joueur {b} !', 'Player {a} stabbed player {b}!'),
   'innocent': both('Le joueur {a} a poignardé un innocent — démasqué !', 'Player {a} stabbed an innocent — exposed!'),
   'respawn': both('Le joueur {a} revient sous un nouveau masque.', 'Player {a} is back under a new mask.'),
   'respawning': both('Nouveau masque dans {s} s…', 'New mask in {s} s…'), 'exposed': both('DÉMASQUÉ', 'EXPOSED'), 'hidden': both('caché dans la foule', 'hidden in the crowd'),
   'party_win': both('LE JOUEUR {n} L’EMPORTE', 'PLAYER {n} WINS'), 'party_why_score': both('Le compte est bon.', 'The count is complete.'), 'party_why_time': both('Minuit : le meilleur score l’emporte.', 'Midnight: the best score wins.'),
 },
 'party': {'title': both('LE BAL DES ASSASSINS', "THE ASSASSINS' BALL"),
   'rules': both("Chaque joueur est un invité parmi cent. Personne ne sait qui est qui : repère les autres joueurs à leur façon de bouger, et poignarde-les. Poignarder un innocent te démasque (tout le monde te voit) et te coûte un point. Premier à 3.",
                 "Each player is a guest among a hundred. Nobody knows who is who: spot the other players by the way they move, and stab them. Stabbing an innocent exposes you (everyone sees you) and costs a point. First to 3."),
   'players': both('Joueurs', 'Players'), 'venue': both('Lieu', 'Venue'), 'start': both('Ouvrir le bal', 'Open the ball'),
   'keys_kb': both('{up}{left}{down}{right} marcher · {act} se fondre · Maj presser · {stab} poignarder', '{up}{left}{down}{right} walk · {act} blend in · Shift hurry · {stab} stab'),
   'keys_kb2': both('flèches marcher · Entrée se fondre · Ctrl presser · / ou . poignarder', 'arrows walk · Enter blend in · Ctrl hurry · / or . stab'),
   'keys_pad': both('stick marcher · A se fondre · RB presser · X poignarder', 'stick walk · A blend in · RB hurry · X stab')},
 'results': {'won': both('VICTOIRE', 'VICTORY'), 'lost': both('DÉFAITE', 'DEFEAT'), 'party': both('JOUEUR {n}', 'PLAYER {n}'),
   'time': both('Heure du dénouement', 'Time of the outcome'), 'missions': both('Missions accomplies', 'Missions done'), 'shot': both('Coup de feu', 'Shot fired'),
   'shot_yes': both('oui', 'yes'), 'shot_no': both('non', 'no'), 'next': both('Nuit suivante', 'Next night'), 'retry': both('Rejouer', 'Play again'),
   'season': both('Retour à la Saison', 'Back to the Season'), 'menu': both('Menu principal', 'Main menu'), 'again': both('Encore un bal', 'Another ball')},
 'pause': {'title': both('PAUSE', 'PAUSE'), 'resume': both('Reprendre', 'Resume'), 'restart': both('Recommencer', 'Restart'), 'options': both('Options', 'Options'), 'quit': both('Quitter le bal', 'Leave the ball')},
 'options': {'title': both('OPTIONS', 'OPTIONS'), 'music': both('Musique', 'Music'), 'sfx': both('Effets', 'Effects'), 'shake': both("Secousses de l'écran", 'Screen shake'),
   'quality': both('Qualité graphique', 'Graphics quality'), 'q0': both('Basse', 'Low'), 'q1': both('Moyenne', 'Medium'), 'q2': both('Haute', 'High'),
   'lang': both('Langue', 'Language'), 'controls': both('Commandes', 'Controls'), 'fullscreen': both('Plein écran', 'Fullscreen'), 'on': both('Oui', 'On'), 'off': both('Non', 'Off'),
   'sens': both('Sensibilité de la lunette', 'Scope sensitivity'),
   'press': both('Appuie sur une touche…', 'Press a key…'), 'software': both("Accélération graphique indisponible : le jeu tourne en rendu logiciel. Mets à jour tes pilotes graphiques.", 'Graphics acceleration unavailable: the game runs in software rendering. Update your graphics drivers.')},
 'controls': {'up': both('Haut', 'Up'), 'down': both('Bas', 'Down'), 'left': both('Gauche', 'Left'), 'right': both('Droite', 'Right'), 'act': both('Agir / se fondre', 'Act / blend in'),
   'hurry': both('Presser le pas', 'Hurry'), 'fire': both('Tirer', 'Fire'), 'scope': both('Épauler', 'Shoulder'), 'mark': both('Marquer', 'Mark'),
   'pause': both('Pause', 'Pause'), 'reset': both('Rétablir par défaut', 'Reset to default')},
 'online': {'title': both('LE CONTRAT', 'THE CONTRACT'),
   'rules': both("À deux, en ligne. L'un est le Corbeau parmi les invités, l'autre la Lynx dans la galerie ; on échange les rôles à chaque manche. L'hôte choisit le lieu.",
                 'For two, online. One is the Raven among the guests, the other the Lynx in the gallery; roles swap every round. The host picks the venue.'),
   'host': both('Héberger', 'Host'), 'join': both('Rejoindre', 'Join'), 'address': both("Adresse de l'hôte", 'Host address'),
   'waiting': both("En attente d'un adversaire… Donne-lui ton adresse : {addr}", 'Waiting for an opponent… Give them your address: {addr}'), 'connected': both('Connecté !', 'Connected!'),
   'failed': both('Connexion impossible.', 'Connection failed.'), 'lost': both('Connexion perdue.', 'Connection lost.'), 'upnp': both('Ouverture du port automatique : {s}', 'Automatic port opening: {s}'),
   'pick_venue': both('Choisis le lieu :', 'Pick the venue:'), 'wait_host': both("En attente de l'hôte…", 'Waiting for the host…'), 'next_round': both('Manche suivante (rôles inversés)', 'Next round (roles swapped)')},
 'ending': {'credits': both("MASCARADE — conception, code, musique : un studio d'une seule personne, cent masques. Merci d'avoir joué.", 'MASCARADE — design, code, music: a one-person studio, a hundred masks. Thanks for playing.')},
}

# Briefings before each night: [speaker, fr, en].
BRIEF = {
 1: [('dogesse', "Approche, Corbeau. Ce soir s'ouvre ma Saison : neuf bals, cent masques chaque nuit, et Venise entière qui fait semblant de s'amuser.", "Come closer, Raven. Tonight my Season opens: nine balls, a hundred masks each night, and all of Venice pretending to have fun."),
     ('dogesse', "Une lettre à recopier sur mon secrétaire, une poudre à verser au buffet, un ami à saluer. Deux de ces choses suffiront.", "A letter to copy at my writing desk, a powder to pour at the buffet, a friend to greet. Two of these will do."),
     ('corbeau', "Et la femme au fusil, dans la galerie ?", "And the woman with the rifle, up in the gallery?"),
     ('dogesse', "La Lynx. Elle n'a qu'une balle, et elle déteste se tromper. Fais ce que font les autres : bois, admire, danse. Ne cours jamais.", "The Lynx. She has one bullet, and she hates being wrong. Do what the others do: drink, admire, dance. Never run.")],
 2: [('dogesse', "Lynx, ma chère. On m'a volé une lettre hier soir, sous mon propre toit. Le voleur reviendra : ils reviennent toujours.", "Lynx, my dear. A letter was stolen from me last night, under my own roof. The thief will come back: they always do."),
     ('lynx', "Cent invités, une balle. Que dois-je chercher ?", "A hundred guests, one bullet. What am I looking for?"),
     ('dogesse', "Celui qui marche comme quelqu'un qui a un but. Les invités flânent ; un espion, lui, va quelque part. Et quand une bougie vacille, c'est qu'il vient d'agir.", "The one who walks like someone with a purpose. Guests stroll; a spy goes somewhere. And when a candle flickers, he has just acted."),
     ('lynx', "Épauler, viser, attendre. Je connais.", "Shoulder, aim, wait. I know the drill.")],
 3: [('corbeau', "La Lynx m'a cherché toute la soirée d'hier. Elle a abattu un marchand de soie. Il n'avait rien fait.", "The Lynx looked for me all last night. She shot a silk merchant. He had done nothing."),
     ('dogesse', "Elle apprend. Toi aussi, j'espère. Trois missions ce soir, et elle connaît la salle maintenant.", "She learns. So do you, I hope. Three missions tonight, and she knows the room now."),
     ('dogesse', "Un conseil : agis quand son laser regarde ailleurs. Le point rouge te dit où sont ses yeux.", "A piece of advice: act when her laser looks elsewhere. The red dot tells you where her eyes are.")],
 4: [('lynx', "Une serre. Des palmiers, une fontaine, du verre partout. Mauvais endroit pour tirer.", "A glasshouse. Palms, a fountain, glass everywhere. A bad place to shoot."),
     ('dogesse', "Le Corbeau aime les plantes, paraît-il. Il a déjà trois de mes secrets. Je ne tiens pas à ce qu'il en ait un quatrième.", "The Raven likes plants, they say. He already has three of my secrets. I would rather he did not get a fourth."),
     ('lynx', "Marquez-le pour moi, alors.", "Mark him for me, then."),
     ('dogesse', "Si je savais qui il est, ma chère, je n'aurais pas besoin de vous.", "If I knew who he was, my dear, I would not need you.")],
 5: [('dogesse', "La comtesse Morosini porte ce soir un masque comme les autres. Fais-lui une révérence : elle saura ce que cela veut dire.", "Countess Morosini wears a mask like everyone tonight. Bow to her: she will know what it means."),
     ('corbeau', "Vous me faites voler vos propres invités, et vous payez une tireuse pour m'abattre. Je commence à ne plus comprendre votre jeu.", "You have me rob your own guests, and you pay a sniper to shoot me. I'm starting not to understand your game."),
     ('dogesse', "Mon jeu, c'est la Saison. Personne ne la comprend avant la dernière valse.", "My game is the Season. Nobody understands it before the last waltz.")],
 6: [('lynx', "Le Corbeau travaille pour vous. Je l'ai vu sortir de votre cabinet.", "The Raven works for you. I saw him leave your study."),
     ('dogesse', "Tout Venise sort de mon cabinet, ma chère. Ce soir, quelqu'un va faire les poches de l'ambassadeur de Vienne. Si c'est lui, abattez-le.", "All of Venice leaves my study, my dear. Tonight someone will pick the Viennese ambassador's pockets. If it is him, shoot him."),
     ('lynx', "Et si c'est vous qui l'envoyez ?", "And if you are the one sending him?"),
     ('dogesse', "Alors vous aurez la preuve que je sais choisir mes gens.", "Then you will have proof that I know how to pick my people.")],
 7: [('corbeau', "Le foyer de l'Opéra. Des miroirs partout : elle me verra deux fois.", "The Opera foyer. Mirrors everywhere: she will see me twice."),
     ('dogesse', "Elle a manqué six fois. Elle ne manquera pas la septième. Fais tes trois missions, et ne danse pas avec la même dame deux fois.", "She has missed six times. She will not miss the seventh. Do your three missions, and do not dance with the same lady twice.")],
 8: [('lynx', "Je sais comment il marche maintenant. Il s'arrête une seconde de trop devant les tableaux. Il ne boit jamais vraiment.", "I know how he walks now. He stops a second too long in front of paintings. He never really drinks."),
     ('dogesse', "Alors ce soir, vous l'aurez.", "Then tonight, you will have him."),
     ('lynx', "Et vous, Dogesse, de quel côté êtes-vous ?", "And you, Dogaressa, which side are you on?"),
     ('dogesse', "Du côté de celui qui gagne. C'est toujours le même.", "On the side of whoever wins. It is always the same side.")],
 9: [('dogesse', "Dernière valse, Corbeau. Ce soir, je suis au bal. Quatre missions : la lettre, le chiffre, la poudre… et une révérence pour moi.", "Last waltz, Raven. Tonight I am at the ball. Four missions: the letter, the cipher, the powder… and a bow for me."),
     ('corbeau', "Et la Lynx ?", "And the Lynx?"),
     ('dogesse', "Je lui ai dit qui tu es. Ou plutôt, je lui ai décrit quelqu'un qui te ressemble beaucoup. À toi de la faire mentir.", "I told her who you are. Or rather, I described someone who looks a lot like you. Make her wrong."),
     ('dogesse', "Celui de vous deux qui sera encore debout à minuit héritera de la Maison Vermeil. C'était cela, la Saison.", "Whichever of you is still standing at midnight inherits the House of Vermeil. That is what the Season was.")],
}

# Cinematics: (image, fr, en).
CINE = {
 'intro': [
   ('bg_title', "Venise, carnaval de 1890. Sur le Grand Canal, le Palais Vermeil s'allume pour la Saison : neuf bals, cent masques chaque nuit.", 'Venice, carnival of 1890. On the Grand Canal, the Palazzo Vermeil lights up for the Season: nine balls, a hundred masks each night.'),
   ('cine_contrat', "La Dogesse Vermeil tient Venise par ses secrets. Ce soir, elle confie une lettre cachetée à un homme qui n'a pas de nom. On l'appelle le Corbeau.", 'The Dogaressa Vermeil holds Venice by its secrets. Tonight she hands a sealed letter to a man with no name. They call him the Raven.'),
   ('key_art', "Dans la galerie, une femme épaule un fusil à lunette. La Lynx n'a qu'une balle. Elle cherche celui qui, parmi cent invités, n'est pas venu pour danser.", 'In the gallery, a woman shoulders a scoped rifle. The Lynx has one bullet. She is looking for the one, among a hundred guests, who did not come to dance.'),
 ],
 'finale': [
   ('cine_tir', "Minuit. La dernière valse s'achève. Dans la lunette de la Lynx, le Corbeau la regarde — et ne baisse pas les yeux.", 'Midnight. The last waltz ends. In the Lynx’s scope, the Raven looks back at her — and does not look away.'),
   ('cine_contrat', "La Dogesse avait engagé les deux. Le meilleur espion de Venise, la meilleure tireuse : elle voulait savoir lequel méritait sa Maison.", 'The Dogaressa had hired them both. The best spy in Venice, the best sniper: she wanted to know which one deserved her House.'),
   ('bg_title', "Le Corbeau a gagné la Saison. La Lynx range son fusil ; on dit qu'à la prochaine, ils travailleront ensemble. Les masques, eux, ne tombent jamais.", 'The Raven won the Season. The Lynx puts her rifle away; they say that next time, they will work together. The masks never come off.'),
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
