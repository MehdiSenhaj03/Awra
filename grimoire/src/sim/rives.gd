class_name Rives
extends RefCounted
## The Two Banks: two mages walk the two banks of a river. A spell lands where its glyph is drawn — and in co-op,
## each player may only draw on the OTHER bank: I guard your road, you guard mine. Ink creatures come along the
## banks, obstacles bar the way, and at the end the Beast of the River rises. Integers, 60 Hz, deterministic.
##
## Along a bank: 0 (the mage) .. LEN (the right edge of the view). A cast: {g, bank, x, pow}.

const LEN := 10000
const MAGE_X := 900
const HP := 10000
const INK := 10000
const INK_REGEN := 22
const COMBO_FRAMES := 72
const WALK := 6                 # world units per frame while the road is clear
enum E { RAT, WOLF, CROW, GOLEM, BLOT, SNAKE, BEAST }
# hp, speed, damage per second at the mage, flying
const ESPEC := {
	E.RAT: [600, 30, 400, false], E.WOLF: [1200, 22, 600, false], E.CROW: [700, 28, 450, true],
	E.GOLEM: [3200, 9, 1000, false], E.BLOT: [1400, 13, 550, true], E.SNAKE: [1000, 36, 550, false],
}
enum O { THORNS, FOG, CHASM, DARK }
const CLEARS := {O.THORNS: Glyphs.FIRE, O.FOG: Glyphs.WIND, O.CHASM: Glyphs.WALL, O.DARK: Glyphs.STAR}

class Bank:
	var hp := HP
	var ink := INK
	var shield := 0
	var shield_t := 0
	var heal := 0
	var slow_t := 0             # enemies on this bank slowed (time glyph)
	var walked := 0             # distance travelled
	var hit_t := 99
	var blocked := false
	var wall_x := -1
	var wall_hp := 0
	var last_g := -1
	var last_x := 0
	var last_f := -1000
	var last_by := -1

class Foe:
	var id := 0
	var kind := 0
	var bank := 0
	var x := LEN
	var hp := 0
	var max_hp := 0
	var frozen := 0
	var push := 0
	var dead := false
	var hit_t := 99

class Obstacle:
	var kind := 0
	var bank := 0
	var at := 0                 # world distance on the bank's road
	var cleared := false

var banks: Array[Bank] = []
var foes: Array[Foe] = []
var obstacles: Array[Obstacle] = []
var frame := 0
var length := 60000            # road length (world units) to the end
var events: Array[Dictionary] = []
var over := ""                 # "won" / "lost"
var rng: Rng
var waves: Array = []          # [{at_frame, bank, kind}]
var boss := false
var beast_hp := 0
var beast_max := 0
var beast_eye := 0             # which bank its eye is on
var beast_t := 0
var strikes: Array = []        # telegraphed tentacle strikes {bank, x, t}
var players := 1
var _next := 1

func _init(seed_v: int, level: int, is_boss: bool, n_players: int) -> void:
	rng = Rng.new(seed_v)
	players = n_players
	banks = [Bank.new(), Bank.new()]
	boss = is_boss
	length = 42000 + level * 9000
	_plan(level)
	if boss:
		beast_max = 20000 + level * 1000
		beast_hp = beast_max

func _ev(t: String, d: Dictionary = {}) -> void:
	d.t = t
	events.append(d)

## Waves and obstacles along the road, from the seed and the level (0..3).
func _plan(level: int) -> void:
	var kinds: Array = [E.RAT, E.WOLF]
	if level >= 1:
		kinds.append(E.CROW)
	if level >= 2:
		kinds.append_array([E.GOLEM, E.SNAKE])
	if level >= 3:
		kinds.append(E.BLOT)
	var t := 240
	var end_f := length / WALK
	while t < end_f:
		var bank := rng.rint(0, 1)
		var n := rng.rint(1, 2 + level / 2)
		for k in n:
			waves.append({"f": t + k * rng.rint(30, 70), "bank": bank if rng.chance(700) else 1 - bank, "kind": kinds[rng.rint(0, kinds.size() - 1)]})
		t += rng.rint(320, 580) - level * 30
	var okinds: Array = [O.THORNS, O.FOG]
	if level >= 1:
		okinds.append(O.CHASM)
	if level >= 2:
		okinds.append(O.DARK)
	var at := 9000
	while at < length - 6000:
		var o := Obstacle.new()
		o.kind = okinds[rng.rint(0, okinds.size() - 1)]
		o.bank = rng.rint(0, 1)
		o.at = at
		obstacles.append(o)
		at += rng.rint(9000, 15000)

## Screen x (bank units) of an obstacle on its bank, or -1 when out of view.
func obstacle_x(o: Obstacle) -> int:
	var d := o.at - banks[o.bank].walked
	return MAGE_X + 700 + d if d > -1200 and d < LEN else -1

func cast(by: int, c: Dictionary) -> void:
	if over != "":
		return
	var g: int = c.g
	var bank: int = clampi(int(c.bank), 0, 1)
	var x: int = clampi(int(c.x), 0, LEN)
	var pw: int = clampi(int(c.pow), 50, 140)
	var b := banks[bank]
	# Ink comes from the caster's own mage (in co-op, the one on the other bank).
	var src := banks[1 - bank] if players == 2 else b
	if g < 0:
		_ev("fizzle", {"bank": bank, "x": x})
		return
	var cost: int = Duel.COST[g]
	if src.ink < cost:
		_ev("noink", {"bank": bank, "by": by})
		return
	src.ink -= cost
	# Two glyphs on the same spot within a moment: a combination (the strongest in co-op, by two hands).
	var combo := false
	var duo := false
	if b.last_g >= 0 and b.last_g != g and frame - b.last_f <= COMBO_FRAMES and absi(b.last_x - x) < 1600:
		combo = true
		duo = b.last_by != by
	b.last_g = g
	b.last_x = x
	b.last_f = frame
	b.last_by = by
	var mult := 100
	if combo:
		mult = 180 if duo else 140
		b.last_g = -1
		_ev("combo", {"bank": bank, "x": x, "duo": duo})
	_ev("cast", {"g": g, "bank": bank, "x": x, "by": by})
	var p := pw * mult / 100
	match g:
		Glyphs.FIRE: _area(bank, x, 1300, 1300 * p / 100, g)
		Glyphs.EARTH: _area(bank, x, 900, 2000 * p / 100, g)
		Glyphs.WATER:
			for f in _near(bank, x, 1600):
				if f.kind != E.GOLEM:
					f.push = 50
				_hurt(f, 500 * p / 100, g)
		Glyphs.WIND:
			for f in _near(bank, x, 1500):
				_hurt(f, (900 if ESPEC[f.kind][3] else 300) * p / 100, g)
				if ESPEC[f.kind][3]:
					f.push = 70
		Glyphs.BOLT:
			var best: Foe = null
			for f in _near(bank, x, 2500):
				if best == null or f.hp > best.hp:
					best = f
			if best:
				_hurt(best, 2200 * p / 100, g)
				_ev("bolt", {"bank": bank, "x": best.x})
			_beast_hit(bank, x, 2000 * p / 100, g)
		Glyphs.ICE:
			for f in _near(bank, x, 1400):
				f.frozen = 200
				_hurt(f, 300 * p / 100, g)
		Glyphs.STAR:
			var list := _near(bank, LEN / 2, LEN)
			for k in 3:
				if list.is_empty():
					break
				var f: Foe = list[rng.rint(0, list.size() - 1)]
				_hurt(f, (900 if ESPEC[f.kind][3] else 600) * p / 100, g)
		Glyphs.SHIELD:
			b.shield = 2500 * p / 100
			b.shield_t = 420
		Glyphs.HEAL:
			b.heal += 1800 * p / 100
		Glyphs.WALL:
			b.wall_x = maxi(MAGE_X + 500, x)
			b.wall_hp = 3000 * p / 100
		Glyphs.TIME:
			b.slow_t = 300
		Glyphs.SEAL:
			# Dispels a tentacle strike about to fall there.
			for s: Dictionary in strikes:
				if int(s.bank) == bank and absi(int(s.x) - x) < 1800:
					s.t = -1
					_ev("dispel", {"bank": bank, "x": s.x})
	if g == Glyphs.FIRE or g == Glyphs.EARTH:
		_beast_hit(bank, x, (1500 if g == Glyphs.FIRE else 2000) * p / 100, g)
	# Obstacles in reach.
	for o in obstacles:
		if o.cleared or o.bank != bank:
			continue
		var ox := obstacle_x(o)
		if ox >= 0 and absi(ox - x) < 1800 and CLEARS[o.kind] == g:
			o.cleared = true
			_ev("cleared", {"bank": bank, "x": ox, "kind": o.kind})

func _near(bank: int, x: int, r: int) -> Array[Foe]:
	var out: Array[Foe] = []
	for f in foes:
		if not f.dead and f.bank == bank and absi(f.x - x) <= r:
			out.append(f)
	return out

func _area(bank: int, x: int, r: int, dmg: int, g: int) -> void:
	for f in _near(bank, x, r):
		var d := dmg
		if g == Glyphs.FIRE and f.kind == E.SNAKE:
			d /= 3
		if g == Glyphs.EARTH and ESPEC[f.kind][3]:
			continue
		if g == Glyphs.FIRE and f.kind == E.GOLEM:
			d = d * 3 / 2
		_hurt(f, d, g)

func _hurt(f: Foe, d: int, g: int) -> void:
	if f.dead:
		return
	f.hp -= d
	f.hit_t = 0
	_ev("foe_hit", {"id": f.id, "bank": f.bank, "x": f.x, "g": g, "dmg": d})
	# Blots split when struck by anything but lightning.
	if f.kind == E.BLOT and g != Glyphs.BOLT and f.hp > 0 and f.max_hp > 800:
		f.max_hp /= 2
		f.hp = mini(f.hp, f.max_hp)
		var n := _spawn(f.bank, E.BLOT, f.x + 500)
		n.max_hp = f.max_hp
		n.hp = f.hp
	if f.hp <= 0:
		f.dead = true
		_ev("foe_dead", {"id": f.id, "bank": f.bank, "x": f.x, "kind": f.kind})

func _spawn(bank: int, kind: int, x: int = LEN) -> Foe:
	var f := Foe.new()
	f.id = _next
	_next += 1
	f.kind = kind
	f.bank = bank
	f.x = x
	f.max_hp = ESPEC[kind][0]
	f.hp = f.max_hp
	foes.append(f)
	return f

func _beast_hit(bank: int, x: int, d: int, g: int) -> void:
	if not boss or beast_hp <= 0 or beast_t < 300:
		return
	# The eye shows on one bank, near the river's far side.
	if bank == beast_eye and x > 6000:
		var dd := d * (2 if g == Glyphs.BOLT else 1)
		beast_hp -= dd
		_ev("beast_hit", {"bank": bank, "dmg": dd})
		if beast_hp <= 0:
			_ev("beast_dead", {})

## One frame; `casts` from each player (index = player).
func step(casts: Array) -> void:
	events.clear()
	if over != "":
		return
	frame += 1
	for i in casts.size():
		var c: Dictionary = casts[i]
		if not c.is_empty():
			cast(i, c)
	for w: Dictionary in waves:
		if int(w.f) == frame:
			_spawn(int(w.bank), int(w.kind))
	for bi in 2:
		var b := banks[bi]
		b.ink = mini(INK, b.ink + INK_REGEN)
		if b.shield_t > 0:
			b.shield_t -= 1
			if b.shield_t == 0:
				b.shield = 0
		if b.heal > 0:
			var h := mini(b.heal, 15)
			b.heal -= h
			b.hp = mini(HP, b.hp + h)
		if b.slow_t > 0:
			b.slow_t -= 1
		b.hit_t += 1
	_foes()
	_walk()
	if boss:
		_beast()
	foes = foes.filter(func(f: Foe) -> bool: return not f.dead)
	if banks[0].hp <= 0 or banks[1].hp <= 0:
		over = "lost"
		_ev("over", {"won": false})
	elif boss and beast_hp <= 0 and beast_max > 0:
		over = "won"
		_ev("over", {"won": true})
	elif not boss and banks[0].walked >= length and banks[1].walked >= length:
		over = "won"
		_ev("over", {"won": true})

func _foes() -> void:
	for f in foes:
		if f.dead:
			continue
		f.hit_t += 1
		var b := banks[f.bank]
		if f.frozen > 0:
			f.frozen -= 1
			continue
		if f.push > 0:
			f.push -= 1
			f.x = mini(LEN, f.x + 60)
			continue
		var sp: int = ESPEC[f.kind][1]
		if b.slow_t > 0:
			sp /= 2
		# A wall stops the walkers (fliers pass over).
		if b.wall_hp > 0 and not ESPEC[f.kind][3] and f.x > b.wall_x and f.x - sp <= b.wall_x + 200:
			b.wall_hp -= int(ESPEC[f.kind][2]) / 60
			if b.wall_hp <= 0:
				_ev("wall_break", {"bank": f.bank, "x": b.wall_x})
			continue
		if f.x > MAGE_X + 300:
			f.x -= sp
			continue
		# At the mage: bites.
		var d: int = int(ESPEC[f.kind][2]) / 60
		if b.shield > 0:
			var a := mini(b.shield, d)
			b.shield -= a
			d -= a
		if d > 0:
			b.hp -= d
			if b.hit_t > 20:
				_ev("mage_hit", {"bank": f.bank})
			b.hit_t = 0

func _walk() -> void:
	for bi in 2:
		var b := banks[bi]
		var block := false
		for f in foes:
			if not f.dead and f.bank == bi and f.x < MAGE_X + 2600:
				block = true
		for o in obstacles:
			if not o.cleared and o.bank == bi and o.at - b.walked < 900 and o.at - b.walked > -400:
				block = true
		if boss and b.walked >= length - 4000:
			block = true
		b.blocked = block
		if not block and b.walked < length:
			b.walked += WALK
			# The world moves under the mage: everything ahead comes closer.
			for f in foes:
				if f.bank == bi:
					f.x -= WALK
			if b.wall_x > 0:
				b.wall_x -= WALK
				if b.wall_x < MAGE_X + 300:
					b.wall_hp = 0

func _beast() -> void:
	if banks[0].walked < length - 4000 or banks[1].walked < length - 4000:
		return
	beast_t += 1
	if beast_t == 1:
		_ev("beast_rise", {})
	if beast_t < 300:
		return
	if beast_t % 420 == 0:
		beast_eye = 1 - beast_eye
		_ev("beast_eye", {"bank": beast_eye})
	# Tentacle strikes, telegraphed 1.5 s ahead, on the bank without the eye.
	if beast_t % 150 == 0:
		strikes.append({"bank": 1 - beast_eye, "x": MAGE_X + rng.rint(0, 2) * 400, "t": 90})
		_ev("strike_warn", {"bank": 1 - beast_eye})
	if beast_t % 360 == 0:
		_spawn(rng.rint(0, 1), [E.RAT, E.WOLF, E.CROW][rng.rint(0, 2)])
	for s: Dictionary in strikes:
		if int(s.t) < 0:
			continue
		s.t = int(s.t) - 1
		if int(s.t) == 0:
			var b := banks[int(s.bank)]
			var d := 1200
			if b.wall_hp > 0:
				b.wall_hp -= d
				d = 0
				if b.wall_hp <= 0:
					_ev("wall_break", {"bank": int(s.bank), "x": b.wall_x})
			if d > 0 and b.shield > 0:
				var a := mini(b.shield, d)
				b.shield -= a
				d -= a
			b.hp -= d
			_ev("strike", {"bank": int(s.bank), "x": int(s.x), "dmg": d})
	strikes = strikes.filter(func(s: Dictionary) -> bool: return int(s.t) > 0)
