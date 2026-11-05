class_name DuelAI
extends RefCounted
## A computer mage. It "draws" like a person: each glyph takes time to trace (shown as a ghost glyph forming on
## its side, so you can read it and answer), it may botch one, and it reads the lanes: dodge, counter, or attack.
## Its style (preferred glyphs) comes from the rival it plays.

const DRAW := [40, 40, 50, 35, 35, 30, 40, 55, 45, 55, 50, 40]

var duel: Duel
var side := 1
var skill := 1.0
var rng: Rng
var style: Dictionary = {}          # glyph -> weight bonus
var known: Array = []               # glyphs it may use (campaign: the ones taught so far)
var drawing := -1                   # glyph being traced
var draw_lane := 1
var draw_t := 0
var draw_len := 1
var _pow := 100
var _think := 40
var _combo_next := -1

func _init(d: Duel, s: int, sk: float, seed_v: int, rival: String = "", glyphs: Array = []) -> void:
	duel = d
	side = s
	skill = sk
	rng = Rng.new(seed_v)
	known = glyphs if glyphs.size() > 0 else range(Glyphs.COUNT)
	match rival:
		"cendres": style = {Glyphs.FIRE: 6, Glyphs.EARTH: 3, Glyphs.WIND: 2}
		"marees": style = {Glyphs.WATER: 5, Glyphs.ICE: 3, Glyphs.BOLT: 3}
		"horloger": style = {Glyphs.TIME: 3, Glyphs.SEAL: 3, Glyphs.WALL: 3, Glyphs.BOLT: 2}
		"reine": style = {Glyphs.EARTH: 2, Glyphs.BOLT: 2, Glyphs.STAR: 2, Glyphs.FIRE: 2}
		_: style = {}

func progress() -> float:
	return clampf(float(draw_t) / maxf(1.0, draw_len), 0.0, 1.0) if drawing >= 0 else 0.0

func _me() -> Duel.Mage:
	return duel.mages[side]

func _foe() -> Duel.Mage:
	return duel.mages[1 - side]

func _can(g: int, reserve: int = 0) -> bool:
	return known.has(g) and _me().ink >= Duel.COST[g] + reserve

## Called every frame: returns a cast for this frame, or {}.
func update() -> Dictionary:
	if duel.round_over >= 0 or duel.over >= 0:
		drawing = -1
		return {}
	if drawing >= 0:
		draw_t += 1
		# An urgent threat can make it abandon a slow glyph to dodge.
		var th := _threat(_me().lane)
		if th.size() > 0 and int(th.eta) < 14 and skill > 0.8 and drawing != Glyphs.SHIELD and rng.chance(int(40 * skill)):
			drawing = -1
			return _dodge()
		if draw_t >= draw_len:
			var g := drawing
			drawing = -1
			_think = rng.rint(int(18 / skill), int(55 / skill))
			if _combo_next >= 0:
				_think = rng.rint(2, 8)
			# A botched glyph fizzles.
			if rng.chance(int(maxf(0.0, 0.16 - 0.1 * skill) * 1000)):
				return {"g": -1, "lane": draw_lane, "pow": 0}
			return {"g": g, "lane": draw_lane, "pow": _pow}
		return {}
	_think -= 1
	if _think > 0:
		return {}
	return _decide()

func _start(g: int, lane: int) -> Dictionary:
	drawing = g
	draw_lane = lane
	draw_t = 0
	var base: float = DRAW[g]
	draw_len = maxi(12, int(base * lerpf(1.7, 0.75, clampf(skill / 1.8, 0.0, 1.0)) * (0.85 + rng.rint(0, 30) / 100.0)))
	_pow = clampi(int(70 + skill * 28 + rng.rint(-12, 12)), 55, 135)
	return {}

## The most pressing enemy spell coming down a lane: {eta: frames, spell}.
func _threat(lane: int) -> Dictionary:
	var best := {}
	var eta_best := 1 << 30
	for s in duel.spells:
		if s.dead or s.side == side or s.lane != lane:
			continue
		var eta := 0
		if s.kind == Glyphs.BOLT or s.kind == Duel.STORM:
			eta = s.t
		elif s.speed > 0:
			var dist := (s.x - 300) if side == 0 else (Duel.LEN - 300 - s.x)
			if dist < 0:
				continue
			eta = dist / maxi(1, s.speed)
		else:
			continue
		if eta < eta_best:
			eta_best = eta
			best = {"eta": eta, "spell": s}
	return best

func _lane_safe(l: int) -> bool:
	var t := _threat(l)
	return t.is_empty() or int(t.eta) > 50

func _dodge() -> Dictionary:
	var me := _me()
	var opts: Array = []
	for d in [-1, 1]:
		var l: int = me.lane + d
		if l >= 0 and l <= 2 and _lane_safe(l):
			opts.append(d)
	if opts.is_empty():
		return {}
	_think = rng.rint(6, 20)
	return {"step": opts[rng.rint(0, opts.size() - 1)]}

func _decide() -> Dictionary:
	var me := _me()
	var foe := _foe()
	# Finish a planned combination.
	if _combo_next >= 0:
		var g := _combo_next
		_combo_next = -1
		if _can(g):
			return _start(g, draw_lane)
	var th := _threat(me.lane)
	if th.size() > 0:
		var s: Duel.Spell = th.spell
		var eta: int = th.eta
		var react := int(DRAW[Glyphs.WATER] * lerpf(1.7, 0.75, clampf(skill / 1.8, 0.0, 1.0)))
		var big := s.dmg >= 1500 or s.kind == Duel.FIRESTORM or s.kind == Duel.MAGMA
		# Not enough time to draw an answer: step aside.
		if eta < react + 6 or (big and rng.chance(500)):
			var d := _dodge()
			if not d.is_empty():
				return d
		if eta >= react:
			var answer := _answer(s)
			if answer >= 0 and _can(answer) and rng.chance(int(450 + 300 * minf(skill, 1.5))):
				return _start(answer, me.lane)
	# Heal when hurt and calm.
	if me.hp < Duel.HP * 4 / 10 and _can(Glyphs.HEAL, 800) and rng.chance(300):
		return _start(Glyphs.HEAL, me.lane)
	# Attack the lane the foe stands in.
	var g := _attack_glyph(foe)
	if g >= 0 and _can(g, 300):
		# Sometimes a combination: start with its first half.
		if skill >= 0.9 and rng.chance(int(160 * skill)):
			for key: String in Duel.COMBOS:
				var parts := key.split("+")
				var a := int(parts[0])
				var b := int(parts[1])
				if (a == g or b == g) and known.has(a) and known.has(b) and me.ink >= Duel.COST[a] + Duel.COST[b]:
					_combo_next = b if a == g else a
					break
		return _start(g, foe.lane)
	_think = rng.rint(10, 30)
	return {}

func _answer(s: Duel.Spell) -> int:
	match s.kind:
		Glyphs.FIRE, Duel.FIRESTORM:
			return Glyphs.WATER if rng.chance(600) else Glyphs.WALL
		Glyphs.WATER:
			return Glyphs.ICE if rng.chance(500) else Glyphs.SHIELD
		Glyphs.EARTH, Duel.MAGMA:
			return Glyphs.EARTH
		Glyphs.ICE:
			return Glyphs.FIRE
		Glyphs.BOLT, Duel.STORM:
			return Glyphs.SHIELD
		Glyphs.STAR:
			return Glyphs.SHIELD
		Glyphs.WIND:
			return -1
		Duel.LANCE:
			return Glyphs.SHIELD
	return Glyphs.WIND if rng.chance(400) else Glyphs.SHIELD

func _attack_glyph(foe: Duel.Mage) -> int:
	var w := {}
	for g in [Glyphs.FIRE, Glyphs.WATER, Glyphs.BOLT, Glyphs.EARTH, Glyphs.STAR, Glyphs.ICE, Glyphs.WIND]:
		if known.has(g):
			w[g] = 4 + int(style.get(g, 0))
	if foe.shield > 0 or _foe_wall(foe.lane):
		w[Glyphs.EARTH] = int(w.get(Glyphs.EARTH, 0)) + 8
	if known.has(Glyphs.TIME) and skill > 1.0:
		w[Glyphs.TIME] = 1 + int(style.get(Glyphs.TIME, 0))
	if known.has(Glyphs.SEAL) and skill > 1.0:
		w[Glyphs.SEAL] = 1 + int(style.get(Glyphs.SEAL, 0))
	if known.has(Glyphs.WALL) and rng.chance(200):
		w[Glyphs.WALL] = 2 + int(style.get(Glyphs.WALL, 0))
	var total := 0
	for g: int in w:
		total += int(w[g])
	if total == 0:
		return -1
	var r := rng.rint(0, total - 1)
	for g: int in w:
		r -= int(w[g])
		if r < 0:
			return g
	return -1

func _foe_wall(lane: int) -> bool:
	for s in duel.spells:
		if s.kind == Duel.WALLE and s.side != side and s.lane == lane and not s.dead:
			return true
	return false
