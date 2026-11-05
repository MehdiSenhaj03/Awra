class_name Duel
extends RefCounted
## A duel: two mages at both ends of three lanes, the spells they cast, how spells meet. Integers only, 60 Hz:
## identical on every machine (online lockstep sends only the casts).
##
## Positions along a lane: 0 (left mage) .. LEN (right mage). HP and ink are in hundredths.
## A cast: {g: glyph, lane: 0..2, pow: percent} or {step: -1/+1}.

const LEN := 10000
const HP := 7000
const INK := 10000
const INK_REGEN := 20          # per frame (12 per second); doubled in the frenzy
const FRENZY := 60 * 45        # after this, ink flows twice as fast
const COMBO_FRAMES := 72
const ROUND_FRAMES := 60 * 75
const WALL_X := 1400           # walls stand this far in front of their caster
const COST := [1500, 1500, 1800, 2500, 2500, 2000, 2500, 2200, 3000, 2000, 3500, 3500]
const SPEED := {Glyphs.FIRE: 95, Glyphs.WATER: 70, Glyphs.WIND: 80, Glyphs.EARTH: 45, Glyphs.STAR: 110, Glyphs.ICE: 85}
const DMG := {Glyphs.FIRE: 1500, Glyphs.WATER: 1000, Glyphs.WIND: 600, Glyphs.BOLT: 1700, Glyphs.EARTH: 2200, Glyphs.STAR: 650, Glyphs.ICE: 800}
# Combined spells (kinds above the glyphs).
enum { FIRESTORM = 20, STORM, MAGMA, LANCE, MIRROR, CONSTELLATION, WALLE, ICEWALL }
const COMBOS := {
	"0+2": FIRESTORM, "1+3": STORM, "0+4": MAGMA, "1+9": LANCE, "2+5": MIRROR, "7+8": CONSTELLATION,
}

class Mage:
	var hp := HP
	var ink := INK
	var lane := 1
	var shield := 0
	var shield_t := 0
	var heal := 0              # remaining heal, given over time
	var ink_slow := 0          # frames of halved ink regen
	var time_t := 0            # frames during which the other side's spells are slowed
	var seal_t := 0
	var mirror := false
	var step_cd := 0
	var hit_t := 0             # frames since last hit (for the view)
	var last_g := -1
	var last_lane := -1
	var last_f := -100000
	var last_id := -1
	var casts := 0
	var rounds := 0
	func clone() -> Mage:
		var m := Mage.new()
		for p in ["hp", "ink", "lane", "shield", "shield_t", "heal", "ink_slow", "time_t", "seal_t", "mirror", "step_cd", "hit_t", "last_g", "last_lane", "last_f", "last_id", "casts", "rounds"]:
			m.set(p, get(p))
		return m

class Spell:
	var id := 0
	var kind := 0
	var side := 0              # owner: 0 moves right (+x), 1 moves left
	var lane := 1
	var x := 0
	var speed := 0
	var dmg := 0
	var hp := 0                # walls
	var t := 0                 # age, or a timer (bolt charge, wall life)
	var frozen := 0
	var dead := false
	func clone() -> Spell:
		var s := Spell.new()
		s.id = id; s.kind = kind; s.side = side; s.lane = lane; s.x = x; s.speed = speed; s.dmg = dmg; s.hp = hp
		s.t = t; s.frozen = frozen; s.dead = dead
		return s

var mages: Array[Mage] = []
var spells: Array[Spell] = []
var frame := 0
var round_frame := 0
var round_no := 1
var over := -1                 # winner of the match, -1 while it goes on
var round_over := -1           # winner of the current round (for the pause between rounds)
var events: Array[Dictionary] = []
var _next_id := 1
var best_of := 3

func _init() -> void:
	mages = [Mage.new(), Mage.new()]

func _ev(t: String, d: Dictionary = {}) -> void:
	d.t = t
	events.append(d)

func start_round() -> void:
	for m in mages:
		var r := m.rounds
		var c := m.casts
		var nm := Mage.new()
		nm.rounds = r
		nm.casts = c
		mages[mages.find(m)] = nm
	spells.clear()
	round_frame = 0
	round_over = -1

# ---- Casting ---------------------------------------------------------------------------------------------------------

## Applies one side's cast for this frame.
func cast(side: int, c: Dictionary) -> void:
	if round_over >= 0 or over >= 0:
		return
	var m := mages[side]
	var foe := mages[1 - side]
	if c.has("step"):
		if m.step_cd <= 0:
			var nl := clampi(m.lane + int(c.step), 0, 2)
			if nl != m.lane:
				m.lane = nl
				m.step_cd = 10
				_ev("step", {"side": side, "lane": nl})
		return
	var g: int = c.g
	var lane: int = clampi(int(c.lane), 0, 2)
	var pw: int = clampi(int(c.pow), 50, 140)
	if g < 0:
		_ev("fizzle", {"side": side, "lane": lane})
		return
	var cost: int = COST[g]
	if m.ink < cost:
		_ev("noink", {"side": side, "g": g})
		return
	m.ink -= cost
	m.casts += 1
	# The other side's seal dispels it.
	if foe.seal_t > 0:
		foe.seal_t = 0
		_ev("sealed", {"side": side, "g": g, "lane": lane})
		return
	# A combination with this side's previous glyph in the same lane?
	var combo := -1
	if m.last_lane == lane and frame - m.last_f <= COMBO_FRAMES and m.last_g != g:
		var key := "%d+%d" % [mini(m.last_g, g), maxi(m.last_g, g)]
		if COMBOS.has(key):
			combo = COMBOS[key]
	m.last_g = g
	m.last_lane = lane
	m.last_f = frame
	if combo >= 0:
		m.last_g = -1
		# The first spell is absorbed into the combination.
		for s in spells:
			if s.id == m.last_id and not s.dead:
				s.dead = true
		_ev("combo", {"side": side, "kind": combo, "lane": lane})
		m.last_id = _spawn_combo(side, combo, lane, pw)
		return
	_ev("cast", {"side": side, "g": g, "lane": lane, "pow": pw})
	m.last_id = _spawn(side, g, lane, pw)

func _new(side: int, kind: int, lane: int, x: int, speed: int, dmg: int) -> Spell:
	var s := Spell.new()
	s.id = _next_id
	_next_id += 1
	s.side = side
	s.kind = kind
	s.lane = lane
	s.x = x
	s.speed = speed
	s.dmg = dmg
	spells.append(s)
	return s

func _origin(side: int) -> int:
	return 300 if side == 0 else LEN - 300

func _spawn(side: int, g: int, lane: int, pw: int) -> int:
	var m := mages[side]
	match g:
		Glyphs.FIRE, Glyphs.WATER, Glyphs.WIND, Glyphs.EARTH, Glyphs.ICE:
			return _new(side, g, lane, _origin(side), SPEED[g], DMG[g] * pw / 100).id
		Glyphs.BOLT:
			var s := _new(side, g, lane, _origin(side), 0, DMG[g] * pw / 100)
			s.t = 18
			return s.id
		Glyphs.STAR:
			var id := -1
			for l in 3:
				id = _new(side, g, l, _origin(side), SPEED[g], DMG[g] * pw / 100).id
			return id
		Glyphs.SHIELD:
			m.shield = 2000 * pw / 100
			m.shield_t = 360
		Glyphs.WALL:
			_place_wall(side, lane, 3000 * pw / 100)
		Glyphs.HEAL:
			m.heal += 1500 * pw / 100
		Glyphs.TIME:
			m.time_t = 240
		Glyphs.SEAL:
			m.seal_t = 180
	return -1

func _spawn_combo(side: int, kind: int, lane: int, pw: int) -> int:
	var m := mages[side]
	match kind:
		FIRESTORM:
			return _new(side, kind, lane, _origin(side), 75, 2500 * pw / 100).id
		STORM:
			var id := -1
			for l in 3:
				var s := _new(side, kind, l, _origin(side), 0, 1000 * pw / 100)
				s.t = 24
				id = s.id
			return id
		MAGMA:
			return _new(side, kind, lane, _origin(side), 40, 3000 * pw / 100).id
		LANCE:
			return _new(side, kind, lane, _origin(side), 160, 2000 * pw / 100).id
		MIRROR:
			m.mirror = true
			m.shield = maxi(m.shield, 1000)
			m.shield_t = maxi(m.shield_t, 300)
		CONSTELLATION:
			m.heal += 1000
			for l in 3:
				_new(side, Glyphs.STAR, l, _origin(side), SPEED[Glyphs.STAR], 600)
	return -1

func _place_wall(side: int, lane: int, hp: int) -> void:
	for s in spells:
		if s.kind == WALLE and s.side == side and s.lane == lane and not s.dead:
			s.hp = hp
			s.t = 600
			return
	var w := _new(side, WALLE, lane, WALL_X if side == 0 else LEN - WALL_X, 0, 0)
	w.hp = hp
	w.t = 600

# ---- Frame -------------------------------------------------------------------------------------------------------------

## One frame; `casts` = [cast of side 0 or {}, cast of side 1 or {}].
func step(casts: Array) -> void:
	events.clear()
	if over >= 0:
		return
	frame += 1
	if round_over >= 0:
		return
	round_frame += 1
	for side in 2:
		var c: Dictionary = casts[side] if side < casts.size() else {}
		if not c.is_empty():
			cast(side, c)
	for side in 2:
		var m := mages[side]
		var regen := INK_REGEN * (2 if round_frame >= FRENZY else 1)
		if m.ink_slow > 0:
			m.ink_slow -= 1
			regen /= 2
		if m.time_t > 0:
			m.time_t -= 1
			regen = regen * 3 / 2
		m.ink = mini(INK, m.ink + regen)
		if m.shield_t > 0:
			m.shield_t -= 1
			if m.shield_t == 0:
				m.shield = 0
				m.mirror = false
		if m.heal > 0:
			var h := mini(m.heal, 13)
			m.heal -= h
			m.hp = mini(HP, m.hp + h)
		if m.seal_t > 0:
			m.seal_t -= 1
		if m.step_cd > 0:
			m.step_cd -= 1
		m.hit_t += 1
	_move()
	_meet()
	_hits()
	spells = spells.filter(func(s: Spell) -> bool: return not s.dead)
	_check_round()

func _move() -> void:
	for s in spells:
		s.t -= 1 if s.kind == WALLE or s.kind == ICEWALL or s.kind == Glyphs.BOLT or s.kind == STORM else 0
		if s.speed == 0:
			continue
		if s.frozen > 0:
			s.frozen -= 1
			continue
		var sp := s.speed
		if mages[1 - s.side].time_t > 0:
			sp /= 2
		s.x += sp if s.side == 0 else -sp
		if s.x < -500 or s.x > LEN + 500:
			s.dead = true
	for s in spells:
		if (s.kind == WALLE or s.kind == ICEWALL) and s.t <= 0:
			s.dead = true

func _projectile(s: Spell) -> bool:
	return s.speed > 0 and not s.dead

## Spells of both sides that cross each other in a lane.
func _meet() -> void:
	var n := spells.size()
	for i in n:
		var a := spells[i]
		if a.dead:
			continue
		for j in range(i + 1, n):
			var b := spells[j]
			if b.dead or a.dead or a.lane != b.lane:
				continue
			# Walls block the other side's projectiles (an ice wall blocks both).
			var wall: Spell = null
			var p: Spell = null
			if (a.kind == WALLE or a.kind == ICEWALL) and _projectile(b):
				wall = a
				p = b
			elif (b.kind == WALLE or b.kind == ICEWALL) and _projectile(a):
				wall = b
				p = a
			if wall:
				if wall.kind == WALLE and wall.side == p.side:
					continue
				if absi(wall.x - p.x) > 120:
					continue
				_wall_hit(wall, p)
				continue
			if a.side == b.side or not _projectile(a) or not _projectile(b):
				continue
			# Opposite projectiles meet when their gap closes.
			if absi(a.x - b.x) > maxi(a.speed + b.speed, 100):
				continue
			_clash(a, b)

func _wall_hit(w: Spell, p: Spell) -> void:
	if p.kind == LANCE:
		return
	var d := p.dmg * (2 if p.kind == Glyphs.FIRE or p.kind == FIRESTORM else 1)
	if p.kind == Glyphs.EARTH or p.kind == MAGMA:
		w.hp = 0
		p.dmg = p.dmg / 2
	else:
		w.hp -= d
		p.dead = true
	_ev("wall_hit", {"lane": w.lane, "x": w.x, "kind": p.kind})
	if w.hp <= 0:
		w.dead = true
		_ev("wall_break", {"lane": w.lane, "x": w.x})

func _clash(a: Spell, b: Spell) -> void:
	var at := a.x
	var ka := a.kind
	var kb := b.kind
	_ev("clash", {"lane": a.lane, "x": (a.x + b.x) / 2, "a": ka, "b": kb})
	# Wind sends back anything but earth.
	if ka == Glyphs.WIND and kb != Glyphs.EARTH and kb != MAGMA and kb != FIRESTORM:
		_reflect(b)
		return
	if kb == Glyphs.WIND and ka != Glyphs.EARTH and ka != MAGMA and ka != FIRESTORM:
		_reflect(a)
		return
	var pair := [mini(ka, kb), maxi(ka, kb)]
	if pair == [Glyphs.FIRE, Glyphs.WATER]:
		var fire := a if ka == Glyphs.FIRE else b
		var water := b if ka == Glyphs.FIRE else a
		fire.dead = true
		water.dmg /= 2
		return
	if pair == [Glyphs.WATER, Glyphs.ICE]:
		a.dead = true
		b.dead = true
		var w := _new(a.side, ICEWALL, a.lane, at, 0, 0)
		w.hp = 2000
		w.t = 360
		_ev("freeze", {"lane": a.lane, "x": at})
		return
	if ka == Glyphs.ICE or kb == Glyphs.ICE:
		# Ice freezes what it meets for a while.
		var ice := a if ka == Glyphs.ICE else b
		var other := b if ka == Glyphs.ICE else a
		ice.dead = true
		other.frozen = 120
		return
	if ka == Glyphs.EARTH or ka == MAGMA or kb == Glyphs.EARTH or kb == MAGMA:
		var e := a if (ka == Glyphs.EARTH or ka == MAGMA) else b
		var o := b if e == a else a
		if o.kind == Glyphs.EARTH or o.kind == MAGMA:
			_trade(a, b)
			return
		o.dead = true
		e.dmg = e.dmg * 7 / 10
		return
	_trade(a, b)

## Power against power: the stronger goes on, weakened.
func _trade(a: Spell, b: Spell) -> void:
	if a.dmg > b.dmg:
		a.dmg -= b.dmg
		b.dead = true
	elif b.dmg > a.dmg:
		b.dmg -= a.dmg
		a.dead = true
	else:
		a.dead = true
		b.dead = true

func _reflect(s: Spell) -> void:
	s.side = 1 - s.side
	_ev("reflect", {"lane": s.lane, "x": s.x})

func _hits() -> void:
	for s in spells:
		if s.dead:
			continue
		# Bolts and storms strike their whole lane when charged.
		if (s.kind == Glyphs.BOLT or s.kind == STORM) and s.t <= 0:
			s.dead = true
			var target := 1 - s.side
			# Walls in the way take it.
			var blocked := false
			for w in spells:
				if (w.kind == WALLE and w.side == target or w.kind == ICEWALL) and w.lane == s.lane and not w.dead:
					w.hp -= s.dmg
					blocked = true
					if w.hp <= 0:
						w.dead = true
						_ev("wall_break", {"lane": w.lane, "x": w.x})
					break
			var wet := false
			for o in spells:
				if o.kind == Glyphs.WATER and o.lane == s.lane and not o.dead:
					wet = true
			_ev("bolt", {"side": s.side, "lane": s.lane, "blocked": blocked})
			if not blocked and mages[target].lane == s.lane:
				_damage(target, s.dmg * (3 if wet else 2) / 2, s)
			continue
		if s.speed == 0:
			continue
		var target := 1 - s.side
		var tx := LEN - 300 if s.side == 0 else 300
		var reached := s.x >= tx if s.side == 0 else s.x <= tx
		if not reached:
			continue
		if mages[target].lane != s.lane:
			continue
		s.dead = true
		var foe := mages[target]
		if foe.mirror and s.kind != Glyphs.EARTH and s.kind != MAGMA:
			foe.mirror = false
			var r := _new(target, s.kind, s.lane, tx, s.speed, s.dmg)
			r.frozen = 0
			_ev("mirror", {"side": target, "lane": s.lane})
			continue
		_damage(target, s.dmg, s)
		if s.kind == Glyphs.ICE:
			foe.ink_slow = 180
		if s.kind == Glyphs.WATER:
			foe.ink = maxi(0, foe.ink - 600)

func _damage(side: int, dmg: int, s: Spell) -> void:
	var m := mages[side]
	var breaks := s.kind == Glyphs.EARTH or s.kind == MAGMA
	var absorbed := 0
	if m.shield > 0:
		if breaks:
			m.shield = 0
			m.shield_t = 0
			_ev("shield_break", {"side": side})
		elif s.kind == Glyphs.BOLT or s.kind == STORM:
			absorbed = dmg
			m.shield = maxi(0, m.shield - dmg / 2)
		else:
			absorbed = mini(m.shield, dmg)
			m.shield -= absorbed
	var d := dmg - absorbed
	m.hp -= d
	m.hit_t = 0
	_ev("hit", {"side": side, "dmg": d, "absorbed": absorbed, "kind": s.kind, "lane": s.lane})

func _check_round() -> void:
	var dead0 := mages[0].hp <= 0
	var dead1 := mages[1].hp <= 0
	var w := -1
	if dead0 or dead1:
		if dead0 and dead1:
			w = 0 if mages[0].hp >= mages[1].hp else 1
		else:
			w = 1 if dead0 else 0
	elif round_frame >= ROUND_FRAMES:
		w = 0 if mages[0].hp >= mages[1].hp else 1
	if w < 0:
		return
	round_over = w
	mages[w].rounds += 1
	_ev("round", {"winner": w, "no": round_no})
	if mages[w].rounds >= (best_of + 1) / 2:
		over = w
		_ev("over", {"winner": w})

## After the pause between rounds.
func next_round() -> void:
	round_no += 1
	start_round()
