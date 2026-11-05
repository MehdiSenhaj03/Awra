#!/usr/bin/env python3
"""All the words of TEMPÊTE, French and English. Writes assets/i18n/fr.json and en.json."""
import json, os
OUT = os.path.join(os.path.dirname(__file__), '..', 'assets', 'i18n')
os.makedirs(OUT, exist_ok=True)

def both(fr, en):
    return (fr, en)

DAYS = {
 'd1': ('Premier jour', 'First day', '12 avril', 'April 12', "Une cellule isolée, du cisaillement juste ce qu'il faut. Eli dit « marginal » ; June dit « parfait pour se dérouiller ». Une photo nette de la structure, peut-être une tornade timide.", 'One isolated cell, just enough shear. Eli says “marginal”; June says “perfect to shake off the rust”. A clean shot of the structure, maybe a shy tornado.'),
 'd2': ('Deux cellules', 'Two cells', '19 avril', 'April 19', 'Deux orages à deux heures d’écart. Il faudra choisir le bon, puis changer de cible sans s’embourber.', 'Two storms two hours apart. Pick the right one, then switch targets without getting stuck in the mud.'),
 'd3': ('Le cow-boy', 'The cowboy', '2 mai', 'May 2', "Dutch Callahan et son équipe sont sur le même orage. Il a déjà vendu trois couvertures cette année. Pas question de lui laisser la quatrième.", 'Dutch Callahan and his crew are on the same storm. He has sold three magazine covers this year already. He is not getting a fourth.'),
 'd4': ('Le train de supercellules', 'Supercell train', '9 mai', 'May 9', 'Un premier orage tornadique, puis un second dans son sillage. La rivière coupe la carte en deux : les ponts sont comptés.', 'A first tornadic storm, then a second in its wake. The river cuts the map in two: the bridges are few.'),
 'd5': ('Crépuscule', 'Dusk', '17 mai', 'May 17', "La lumière baisse vite et l'orage s'allume d'éclairs. Callahan est encore là. Les photos de foudre valent de l'or ce soir.", 'The light is fading fast and the storm lights up with lightning. Callahan is here again. Lightning shots are worth gold tonight.'),
 'd6': ('Risque modéré', 'Moderate risk', '24 mai', 'May 24', 'Le Centre des Prévisions a passé la région en risque modéré. Deux tornades probables, une forte. Teo a révisé les sondes.', 'The Prediction Center has put the region under a moderate risk. Two likely tornadoes, one strong. Teo has serviced the probes.'),
 'd7': ('La boue', 'Mud', '3 juin', 'June 3', "Il a plu toute la nuit : les chemins de terre sont des patinoires. Deux cellules, et Callahan qui ne lâche rien.", 'It rained all night: the dirt roads are skating rinks. Two cells, and Callahan who never lets go.'),
 'd8': ('Haut risque', 'High risk', '11 juin', 'June 11', "Le genre de journée qu'on ne voit qu'une fois par décennie. Une seule supercellule, monstrueuse, trois tornades annoncées. Restez en vie.", 'The kind of day you see once a decade. A single monstrous supercell, three tornadoes forecast. Stay alive.'),
}

T = {
 'common': {'back': both('Retour', 'Back'), 'yes': both('Oui', 'Yes'), 'no': both('Non', 'No'), 'skip': both('Passer', 'Skip'), 'continue': both('Continuer', 'Continue')},
 'names': {'june': both('JUNE', 'JUNE'), 'eli': both('ELI', 'ELI'), 'margot': both('MARGOT', 'MARGOT'), 'teo': both('TEO', 'TEO'), 'rival': both('DUTCH CALLAHAN', 'DUTCH CALLAHAN')},
 'title': {'tagline': both("Le ciel ne prévient qu'une fois.", 'The sky only warns you once.'), 'press': both('Appuie sur une touche', 'Press any key'),
   'season': both('La Saison', 'The Season'), 'season_sub': both("Huit journées de chasse, d'avril à juin", 'Eight chase days, April to June'),
   'free': both('Chasse libre', 'Free chase'), 'free_sub': both('Seul, à deux sur un écran, ou contre une équipe rivale', 'Alone, two on one screen, or against a rival crew'),
   'online': both('En ligne', 'Online'), 'online_sub': both('Un équipage à deux, ou la course aux clichés', 'A crew of two, or the race for the best shots'),
   'garage': both('Le garage', 'The garage'), 'garage_sub': both('Améliorer la Bailleuse', 'Upgrade the truck'),
   'options': both('Options', 'Options'), 'quit': both('Quitter', 'Quit')},
 'season': {'title': both('LA SAISON', 'THE SEASON'), 'sub': both('Caisse de l’équipe : ${money}', 'Crew funds: ${money}'), 'garage': both('Le garage', 'The garage'),
   'done': both('✓ Réussie — meilleur dossier {n} (objectif {goal})', '✓ Done — best dossier {n} (goal {goal})'), 'locked': both('Réussis la journée précédente.', 'Succeed on the previous day.'),
   'goal': both('Objectif du dossier : {goal}', 'Dossier goal: {goal}'), 'rival': both('équipe rivale', 'rival crew')},
 'risk': {'1': both('Risque marginal', 'Marginal risk'), '2': both('Risque faible', 'Slight risk'), '3': both('Risque accru', 'Enhanced risk'), '4': both('Risque modéré', 'Moderate risk'), '5': both('Haut risque', 'High risk')},
 'day': {k: {'name': both(v[0], v[1]), 'date': both(v[2], v[3]), 'desc': both(v[4], v[5])} for k, v in DAYS.items()},
 'chase': {'free': both('Chasse libre', 'Free chase')},
 'dir': {'E': both('E', 'E'), 'NE': both('NE', 'NE'), 'N': both('N', 'N'), 'NO': both('NO', 'NW'), 'O': both('O', 'W'), 'SO': both('SO', 'SW'), 'S': both('S', 'S'), 'SE': both('SE', 'SE')},
 'surface': {'field': both('champ', 'field'), 'dirt': both('chemin de terre', 'dirt road'), 'paved': both('goudron', 'pavement'), 'water': both('rivière', 'river'), 'town': both('en ville', 'in town'), 'mud': both('BOUE', 'MUD')},
 'cat': {'tornado': both('Tornade', 'Tornado'), 'structure': both('Structure', 'Structure'), 'lightning': both('Foudre', 'Lightning'), 'hail': both('Grêle', 'Hail'), 'none': both('Rien', 'Nothing')},
 'hud': {'score': both('DOSSIER {n}   ·   PELLICULE {film}   ·   SONDES {probes}', 'DOSSIER {n}   ·   FILM {film}   ·   PROBES {probes}'),
   'keys_drive': both('<kbd>ZQSD / WASD</kbd> conduire  ·  <kbd>{camera}</kbd> appareil photo  ·  <kbd>{probe}</kbd> poser une sonde (à l’arrêt)  ·  <kbd>{radar}</kbd> radar vitesses  ·  <kbd>{look}</kbd> caméra vers l’orage  ·  clic sur le radar : itinéraire',
                      '<kbd>WASD</kbd> drive  ·  <kbd>{camera}</kbd> camera  ·  <kbd>{probe}</kbd> drop a probe (stopped)  ·  <kbd>{radar}</kbd> velocity radar  ·  <kbd>{look}</kbd> camera on the storm  ·  click the radar: route'),
   'keys_photo': both('souris : viser  ·  molette : zoom  ·  <kbd>{shoot}</kbd> déclencher  ·  <kbd>{camera}</kbd> ranger l’appareil', 'mouse: aim  ·  wheel: zoom  ·  <kbd>{shoot}</kbd> shoot  ·  <kbd>{camera}</kbd> put the camera away'),
   'keys_crew': both('<kbd>{camera}</kbd> appareil photo  ·  <kbd>{probe}</kbd> sonde  ·  clic sur le radar : itinéraire pour la pilote', '<kbd>{camera}</kbd> camera  ·  <kbd>{probe}</kbd> probe  ·  click the radar: route for the driver'),
   'tornado_warning': both('ALERTE TORNADE', 'TORNADO WARNING'), 'hail': both('GRÊLE', 'HAIL'), 'wrecked': both('ÉPAVE', 'WRECKED'),
   'no_film': both('Plus de pellicule !', 'Out of film!'), 'probe_stop': both("Arrête-toi d'abord pour poser une sonde.", 'Stop first to drop a probe.'),
   'photo_nothing': both("Rien de notable dans le cadre… Vise la structure, la tornade ou un éclair.", 'Nothing notable in the frame… Aim at the structure, the tornado or a bolt.')},
 'eli': {
   'new_cell': both('Nouvelle cellule (C{n}) au {dir}, à {km} km.', 'New cell (C{n}) to the {dir}, {km} km out.'),
   'rotation': both('La C{n} s’organise : rotation bien marquée sur le radar vitesses. Place-toi sur son flanc sud-est.', 'C{n} is getting organised: strong rotation on the velocity radar. Get on its south-east flank.'),
   'soon': both('Le crochet se resserre sur la C{n}. Ça va tourner, d’une minute à l’autre.', 'The hook is tightening on C{n}. It will drop any minute now.'),
   'tornado': both('TORNADE AU SOL ! {km} km au {dir}, elle file vers le {to}.', 'TORNADO ON THE GROUND! {km} km to the {dir}, heading {to}.'),
   'tornado_end': both('La tornade de la C{n} se dissipe… elle s’étire en corde.', 'C{n}’s tornado is roping out…'),
   'too_close': both('TROP PRÈS ! Dégage vers le sud-est, maintenant !', 'TOO CLOSE! Get out to the south-east, now!'),
   'hail': both('Grêle ! On ne reste pas sous le cœur, avance ou recule !', 'Hail! Don’t sit under the core — go forward or back!'),
   'mud': both('Ça patine : les chemins sont en boue. Reprends le goudron dès que tu peux.', 'We’re sliding: the dirt roads are mud. Get back on pavement when you can.'),
   'probe_here': both('Fenêtre parfaite pour une sonde, ici, sur sa trajectoire ! Touche {key} !', 'Perfect probe window, right here on its track! Press {key}!'),
   'probe_hit': both('INTERCEPTION ! La sonde est dans la tornade (EF{ef}) : +{pts} de données !', 'INTERCEPT! The probe is inside the tornado (EF{ef}): +{pts} data!'),
   'route': both('Je t’ai tracé une route d’interception en jaune sur le radar.', 'I’ve drawn an intercept route in yellow on the radar.'),
   'wreck': both('… June ? June, tu m’entends ? On a perdu la Bailleuse.', '… June? June, can you hear me? We lost the truck.')},
 'dossier': {'title': both('LE DOSSIER', 'THE DOSSIER'), 'none': both('aucune photo', 'no photo'), 'photos': both('Photographies', 'Photographs'), 'probes': both('Données des sondes', 'Probe data'),
   'total': both('Total', 'Total'), 'rival': both('Dossier de Callahan', "Callahan's dossier"), 'earned': both('Vendu aux journaux et aux labos', 'Sold to papers and labs'), 'repair': both('Réparations', 'Repairs'),
   'ok': both('JOURNÉE RÉUSSIE', 'DAY SUCCESSFUL'), 'short': both('Pas assez pour la rédaction…', 'Not enough for the editors…'), 'wrecked': both('LA BAILLEUSE EST UNE ÉPAVE', 'THE TRUCK IS A WRECK'),
   'beat': both('VOUS BATTEZ L’ÉQUIPE RIVALE', 'YOU BEAT THE RIVAL CREW'), 'beaten': both('LES RIVAUX VOUS DEVANCENT', 'THE RIVALS BEAT YOU'),
   'next': both('Journée suivante', 'Next day'), 'retry': both('Rechasser', 'Chase again'), 'season': both('Retour à la saison', 'Back to the season'), 'menu': both('Menu principal', 'Main menu')},
 'garage': {'title': both('LE GARAGE', 'THE GARAGE'), 'money': both('Caisse : ${n}', 'Funds: ${n}'),
   'intro': both('Chaque dossier vendu paie l’essence, les réparations… et les améliorations. Choisis bien.', 'Every dossier sold pays for fuel, repairs… and upgrades. Choose well.'),
   'armor': both('Blindage', 'Armour'), 'armor_desc': both('Moins de dégâts sous la grêle et les débris', 'Less damage from hail and debris'),
   'engine': both('Moteur', 'Engine'), 'engine_desc': both('Vitesse de pointe', 'Top speed'), 'probes': both('Sondes', 'Probes'), 'probes_desc': both('Une tortue de plus par chasse', 'One more turtle per chase'),
   'zoom': both('Téléobjectif', 'Long lens'), 'zoom_desc': both('Zoom maximal de l’appareil', "The camera's maximum zoom"),
   'val_armor': both('dégâts {v} %', 'damage {v}%'), 'val_engine': both('vitesse {v} %', 'speed {v}%'), 'val_probes': both('{v} sondes', '{v} probes'), 'val_zoom': both('×{v}', '×{v}'), 'max': both('maximum', 'maxed')},
 'setup': {'title': both('CHASSE LIBRE', 'FREE CHASE'), 'rules': both("Une journée générée. À deux sur un écran : la manette conduit, la souris photographie, pose les sondes et trace l'itinéraire.", 'A generated day. Two on one screen: the gamepad drives, the mouse shoots, drops probes and plans the route.'),
   'risk': both('Risque', 'Risk'), 'crew': both('Équipage', 'Crew'), 'crew0': both('Seul', 'Alone'), 'crew1': both('À deux (manette + souris)', 'Two (gamepad + mouse)'),
   'rival': both('Équipe rivale', 'Rival crew'), 'rival0': both('Non', 'No'), 'rival1': both('Oui — course aux clichés', 'Yes — race for the shots'), 'go': both('Partir', 'Go')},
 'online': {'title': both('EN LIGNE', 'ONLINE'), 'rules': both("À deux, à distance. Équipage : l'hôte conduit, l'invité photographie, pose les sondes et trace l'itinéraire. Course : chacun son pick-up, le meilleur dossier l'emporte.", 'For two, at a distance. Crew: the host drives, the guest shoots, drops probes and plans. Race: one truck each, the best dossier wins.'),
   'host': both('Héberger', 'Host'), 'join': both('Rejoindre', 'Join'), 'address': both("Adresse de l'hôte", 'Host address'),
   'waiting': both("En attente… Donne ton adresse : {addr}", 'Waiting… Give your address: {addr}'), 'connected': both('Connecté !', 'Connected!'),
   'failed': both('Connexion impossible.', 'Connection failed.'), 'lost': both('Connexion perdue.', 'Connection lost.'), 'upnp': both('Ouverture du port automatique : {s}', 'Automatic port opening: {s}'),
   'pick': both('Choisis :', 'Choose:'), 'net_coop': both('Équipage — {risk}', 'Crew — {risk}'), 'net_versus': both('Course aux clichés — {risk}', 'Photo race — {risk}'),
   'wait_host': both("En attente de l'hôte…", 'Waiting for the host…'), 'again': both('Encore', 'Again')},
 'pause': {'title': both('PAUSE', 'PAUSE'), 'resume': both('Reprendre', 'Resume'), 'restart': both('Recommencer la chasse', 'Restart the chase'), 'options': both('Options', 'Options'), 'quit': both('Abandonner', 'Give up')},
 'options': {'title': both('OPTIONS', 'OPTIONS'), 'music': both('Musique', 'Music'), 'sfx': both('Effets', 'Effects'),
   'quality': both('Qualité graphique', 'Graphics quality'), 'q0': both('Basse', 'Low'), 'q1': both('Moyenne', 'Medium'), 'q2': both('Haute', 'High'),
   'lang': both('Langue', 'Language'), 'controls': both('Commandes', 'Controls'), 'fullscreen': both('Plein écran', 'Fullscreen'), 'on': both('Oui', 'On'), 'off': both('Non', 'Off'),
   'press': both('Appuie sur une touche…', 'Press a key…'), 'software': both("Accélération graphique indisponible : le jeu tourne en rendu logiciel.", 'Graphics acceleration unavailable: the game runs in software rendering.')},
 'controls': {'throttle': both('Accélérer', 'Throttle'), 'brake': both('Freiner / reculer', 'Brake / reverse'), 'steer_left': both('Tourner à gauche', 'Steer left'), 'steer_right': both('Tourner à droite', 'Steer right'),
   'probe': both('Poser une sonde', 'Drop a probe'), 'camera': both('Appareil photo', 'Camera'), 'shoot': both('Déclencher', 'Shoot'), 'radar': both('Radar vitesses', 'Velocity radar'),
   'look': both('Caméra vers l’orage', 'Camera on the storm'), 'horn': both('Klaxon', 'Horn'), 'pause': both('Pause', 'Pause'), 'reset': both('Rétablir par défaut', 'Reset to default')},
 'radar': {'refl': both('RÉFLECTIVITÉ', 'REFLECTIVITY'), 'vel': both('VITESSES', 'VELOCITY')},
 'ending': {'credits': both("TEMPÊTE — conception, code, musique : un studio d'une seule personne et une poignée de ciels. Merci d'avoir chassé avec nous.", 'TEMPÊTE — design, code, music: a one-person studio and a handful of skies. Thanks for chasing with us.')},
}

BRIEF = {
 'd1': [('eli', "Bon. Une cellule isolée au sud-ouest, du cisaillement correct, une base pas trop haute. Rien d'historique, mais c'est un vrai orage.", "Right. One isolated cell to the south-west, decent shear, a low enough base. Nothing historic, but it's a real storm."),
        ('june', "On roule. Margot, tu prépares l'appareil ? Teo, les sondes sont arrimées ?", "Let's roll. Margot, camera ready? Teo, probes strapped down?"),
        ('margot', "Toujours prête. Je veux sa structure entière dans le cadre, de loin, avant que la pluie la cache.", "Always. I want its whole structure in the frame, from a distance, before the rain hides it."),
        ('teo', "Trois tortues sur le rack. Si une tornade se pose, je veux être devant elle, pas derrière.", "Three turtles on the rack. If a tornado touches down, I want to be in front of it, not behind.")],
 'd2': [('eli', "Deux cellules aujourd'hui. La première est déjà là, la seconde naîtra plus au nord dans l'après-midi.", 'Two cells today. The first one is already there, the second will form further north this afternoon.'),
        ('june', "Et les chemins de terre ?", 'And the dirt roads?'),
        ('eli', "Secs pour l'instant. Sous la pluie, ce sera de la boue : garde un œil sur le goudron.", 'Dry for now. Under the rain they will be mud: keep an eye on the pavement.')],
 'd3': [('rival', "Tiens, la Bailleuse. Vous allez encore photographier mon pare-chocs arrière ?", 'Well, the truck. Going to photograph my rear bumper again?'),
        ('margot', "Dutch Callahan. Il pose devant les tornades pour les couvertures de magazine.", 'Dutch Callahan. He poses in front of tornadoes for magazine covers.'),
        ('june', "Alors on lui prend la tornade. Avant lui, et mieux que lui.", "Then we take the tornado from him. Before him, and better.")],
 'd4': [('eli', "Un train de supercellules. La première est prête à produire, la seconde suit dans son sillage.", 'A supercell train. The first is ready to produce, the second follows in its wake.'),
        ('teo', "La rivière coupe la carte. Combien de ponts ?", 'The river cuts the map. How many bridges?'),
        ('eli', "Pas assez. Regarde-les sur le radar avant de t'engager.", 'Not enough. Look at them on the radar before you commit.')],
 'd5': [('margot', "Crépuscule et orage électrique. C'est le genre de soirée qui fait une carrière.", "Dusk and an electric storm. It's the kind of evening that makes a career."),
        ('rival', "Je vous ai gardé une place, au fond. Derrière moi.", 'I saved you a seat. At the back. Behind me.'),
        ('june', "Garde-la au chaud.", 'Keep it warm.')],
 'd6': [('eli', "Risque modéré. Deux tornades probables, et la seconde pourrait être forte. On joue serré aujourd'hui.", 'Moderate risk. Two likely tornadoes, and the second could be strong. We play it tight today.'),
        ('teo', "J'ai renforcé les sondes. Pose-les sur sa route, je m'occupe du reste.", 'I reinforced the probes. Put them on its path, I’ll handle the rest.')],
 'd7': [('june', "Il a plu toute la nuit. Les chemins sont des patinoires.", 'It rained all night. The dirt roads are skating rinks.'),
        ('eli', "Alors on reste sur le goudron et on anticipe. Celui qui s'embourbe aujourd'hui rentre bredouille.", "Then we stay on pavement and plan ahead. Whoever gets stuck today goes home empty-handed."),
        ('rival', "Bonne chance avec votre tracteur.", 'Good luck with your tractor.')],
 'd8': [('eli', "Haut risque. Je n'ai vu ça que deux fois. Une seule supercellule, mais quelle supercellule.", "High risk. I've only seen this twice. A single supercell — but what a supercell."),
        ('margot', "La photo de la décennie est quelque part là-dedans.", 'The photo of the decade is somewhere in there.'),
        ('june', "Et on rentre tous les quatre. C'est la seule règle.", 'And all four of us come home. That’s the only rule.')],
}

CINE = {
 'intro': [
   ('bg_title', "Les Grandes Plaines, au printemps. Chaque après-midi, le ciel s'empile en tours de vingt kilomètres.", 'The Great Plains, in spring. Every afternoon, the sky stacks itself into twenty-kilometre towers.'),
   ('vista_mothership', "Certaines tournent. On les appelle des supercellules. Quelques-unes accouchent d'une tornade.", 'Some of them rotate. They are called supercells. A few give birth to a tornado.'),
   ('key_art', "June, Eli, Margot et Teo vont au-devant d'elles avec un pick-up blindé, trois sondes et un appareil photo. Pour la science, pour la photo — et pour ce sentiment-là.", 'June, Eli, Margot and Teo drive toward them in an armoured pick-up with three probes and a camera. For science, for the picture — and for that feeling.'),
 ],
 'finale': [
   ('vista_tornado', "Le onze juin, sous le haut risque, la Bailleuse a vu passer la plus grande tornade de la saison à moins de deux kilomètres.", 'On June eleventh, under the high risk, the truck watched the season’s biggest tornado pass less than two kilometres away.'),
   ('vista_after', "Ce soir-là, sur le hayon, personne n'a parlé pendant longtemps. Puis Margot a sorti les tirages.", 'That evening, on the tailgate, nobody spoke for a long time. Then Margot brought out the prints.'),
   ('vista_town', "La saison est finie. Le ciel, lui, recommencera l'année prochaine. Eux aussi.", 'The season is over. The sky will start again next year. So will they.'),
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
    out['brief'] = {n: [[l[0], l[1 + k]] for l in lines] for n, lines in BRIEF.items()}
    out['cine'] = {cid: [{'image': im, 'text': (fr if k == 0 else en)} for im, fr, en in shots] for cid, shots in CINE.items()}
    return out

for lang in ('fr', 'en'):
    with open(os.path.join(OUT, f'{lang}.json'), 'w') as f:
        json.dump(build(lang), f, ensure_ascii=False, indent=0)
print('ok')
