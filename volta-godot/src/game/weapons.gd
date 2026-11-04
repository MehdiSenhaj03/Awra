class_name Weapons
extends RefCounted
## Weapons fire on the music grid: holding the trigger plays a steady pattern; a press landed on the beat triggers
## that weapon's PERFECT volley. Tapping in time beats holding.

const ORDER := ["prism", "chord", "lance", "arc", "swarm"]

static func lvl(p: Player) -> float:
	return [1.0, 1.25, 1.5][clampi(p.weapon_level - 1, 0, 2)]

## A press on the beat, at most once per beat: "perfect", "great" or "".
static func tap(w: World, p: Player, c: Controls) -> String:
	if not c.fire_pressed:
		return ""
	var j := w.beat.judge()
	var bi := int(U.jround(w.beat.beat))
	if j == "off" or bi == p.last_perfect_beat:
		return ""
	p.last_perfect_beat = bi
	w.judged(j, "shot", p.x, p.y)
	return j

static func update(id: String, w: World, p: Player, c: Controls) -> void:
	match id:
		"prism": prism(w, p, c)
		"chord": chord(w, p, c)
		"lance": lance(w, p, c)
		"arc": arc(w, p, c)
		"swarm": swarm(w, p, c)

static func prism(w: World, p: Player, c: Controls) -> void:
	p.firing = c.fire
	var m := lvl(p) * w.mods.damage
	var od := p.is_overdrive()
	var j := tap(w, p, c)
	if j != "":
		var mx := p.x + cos(p.aim) * 0.7
		var my := p.y + sin(p.aim) * 0.7
		var n := 5 if j == "perfect" else 3
		for i in n:
			var a := p.aim + (float(i) - float(n - 1) / 2.0) * 0.07
			w.shots.add(mx, my, a, 62.0, (3.0 if j == "perfect" else 2.0) * m, Shots.PERFECT, 0.8, 3 if od else 1)
		w.emit({"t": "shot", "x": mx, "y": my, "a": p.aim, "accent": true, "weapon": "prismPerfect"})
		w.recoil(0.25)
		return
	if c.fire and w.beat.crossed(4):
		var accent := w.beat.crossed(1)
		var mx := p.x + cos(p.aim) * 0.7
		var my := p.y + sin(p.aim) * 0.7
		var spread := (w.rng.next() - 0.5) * 0.035
		var down := 2.0 if accent and w.mods.has.has("tempsfort") and w.beat.index(1) % 4 == 0 else 1.0
		w.shots.add(mx, my, p.aim + spread, 56.0 if accent else 50.0, (2.5 * down if accent else 1.0) * m, Shots.ACCENT if accent else Shots.BOLT, 0.75, 2 if od else (1 if accent else 0))
		if od:
			for s in [-0.12, 0.12]:
				w.shots.add(mx, my, p.aim + s, 50.0, m, Shots.BOLT, 0.6, 1)
		w.emit({"t": "shot", "x": mx, "y": my, "a": p.aim, "accent": accent, "weapon": "prism"})
		if accent:
			w.recoil(0.08)

static func _volley(w: World, p: Player, n: int, spread: float, d: float, k: int, speed: float) -> void:
	var mx := p.x + cos(p.aim) * 0.7
	var my := p.y + sin(p.aim) * 0.7
	for i in n:
		var a := p.aim + (float(i) / float(n - 1) - 0.5) * spread + (w.rng.next() - 0.5) * 0.05
		w.shots.add(mx, my, a, speed * (0.9 + w.rng.next() * 0.2), d, k, 0.32, 1 if p.is_overdrive() else 0)

static func chord(w: World, p: Player, c: Controls) -> void:
	p.firing = c.fire
	var m := lvl(p) * w.mods.damage
	var j := tap(w, p, c)
	if j != "":
		_volley(w, p, 11 if j == "perfect" else 8, 0.75, 1.3 * m, Shots.PERFECT, 46.0)
		w.emit({"t": "shot", "x": p.x, "y": p.y, "a": p.aim, "accent": true, "weapon": "chordPerfect"})
		w.recoil(0.5)
		p.vx -= cos(p.aim) * 9.0
		p.vy -= sin(p.aim) * 9.0
		return
	if c.fire and w.beat.crossed(2):
		var accent := w.beat.crossed(1)
		_volley(w, p, 7 if accent else 5, 0.6 if accent else 0.5, (1.0 if accent else 0.75) * m, Shots.ACCENT if accent else Shots.PELLET, 42.0)
		w.emit({"t": "shot", "x": p.x, "y": p.y, "a": p.aim, "accent": accent, "weapon": "chord"})
		w.recoil(0.2 if accent else 0.1)

static func lance(w: World, p: Player, c: Controls) -> void:
	var m := lvl(p) * w.mods.damage
	if c.fire:
		if p.charge_start_beat < 0.0:
			p.charge_start_beat = w.beat.beat
		var held := w.beat.beat - p.charge_start_beat
		var lv := mini(3, int(floorf(held + 0.15)))
		if lv > p.charge:
			p.charge = lv
			w.sfx("charge", 0.8 + lv * 0.15)
		p.firing = true
		return
	if p.charge_start_beat >= 0.0:
		var lv := maxi(1, p.charge)
		var j := w.beat.judge() if p.charge > 0 else "off"
		if j != "off":
			w.judged(j, "shot", p.x, p.y)
		var kk := 1.8 if j == "perfect" else (1.35 if j == "great" else 1.0)
		var mx := p.x + cos(p.aim) * 0.9
		var my := p.y + sin(p.aim) * 0.9
		var d: float = [3.0, 9.0, 17.0, 28.0][lv] * kk * m
		w.shots.add(mx, my, p.aim, 85.0, d, Shots.LANCE, 0.55, 99)
		w.emit({"t": "shot", "x": mx, "y": my, "a": p.aim, "accent": lv >= 2, "weapon": "lancePerfect" if j == "perfect" else "lance"})
		w.recoil(0.15 + lv * 0.12)
		p.vx -= cos(p.aim) * lv * 4.0
		p.vy -= sin(p.aim) * lv * 4.0
	p.charge_start_beat = -1.0
	p.charge = 0
	p.firing = false

static func arc(w: World, p: Player, c: Controls) -> void:
	p.firing = c.fire
	var m := lvl(p) * w.mods.damage
	var j := tap(w, p, c)
	if j != "":
		w.chain(p.x, p.y, p.aim, 7 if j == "perfect" else 5, 2.6 * m, 11.0, true)
		w.emit({"t": "shot", "x": p.x, "y": p.y, "a": p.aim, "accent": true, "weapon": "arcPerfect"})
		return
	if c.fire and w.beat.crossed(2):
		var accent := w.beat.crossed(1)
		w.chain(p.x, p.y, p.aim, 4 if accent else 3, (2.2 if accent else 1.5) * m, 9.0, false)
		w.emit({"t": "shot", "x": p.x, "y": p.y, "a": p.aim, "accent": accent, "weapon": "arc"})

static func _launch(w: World, p: Player, n: int, d: float) -> void:
	for i in n:
		var a := p.aim + PI + (float(i) / float(maxi(1, n - 1)) - 0.5) * 2.4
		var s := w.shots.add(p.x, p.y, a, 16.0, d, Shots.MISSILE, 2.2, 0)
		if s >= 0:
			var tgt := w.nearest_enemy(p.x + cos(p.aim) * 6.0, p.y + sin(p.aim) * 6.0, 30.0)
			w.shots.target[s] = tgt.id if tgt else 0

static func swarm(w: World, p: Player, c: Controls) -> void:
	p.firing = c.fire
	var m := lvl(p) * w.mods.damage
	var j := tap(w, p, c)
	if j != "":
		_launch(w, p, 7 if j == "perfect" else 5, 2.6 * m)
		w.emit({"t": "shot", "x": p.x, "y": p.y, "a": p.aim, "accent": true, "weapon": "swarmPerfect"})
		return
	if c.fire and w.beat.crossed(1):
		_launch(w, p, 4 if p.is_overdrive() else 2, 2.8 * m)
		w.emit({"t": "shot", "x": p.x, "y": p.y, "a": p.aim, "accent": true, "weapon": "swarm"})
