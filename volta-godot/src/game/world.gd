class_name World
extends RefCounted
## The simulation of one stage at 60 Hz. Pure logic: it never touches rendering or sound, it only emits events.

const RANKS := ["D", "C", "B", "A", "S", "SS", "V"]
const RANK_AT := [0.0, 100.0, 230.0, 380.0, 550.0, 740.0, 920.0]
const RANK_MULT := [1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0]
const STYLE_MAX := 1000.0

var rng: Rng
var beat: BeatClock
var arena: Arena
var player := Player.new()
var enemies: Array[Enemy] = []
var bullets := EnemyBullets.new()
var shots := Shots.new()
## Pickups: Array of Dictionary {x, y, vx, vy, kind, t, id}.
var pickups: Array[Dictionary] = []
var hazards: Array[Hazard] = []
var events: Array[Dictionary] = []
var mods := Mods.new()
var frame := 0
## Hit-stop frames (the beat keeps running).
var freeze := 0
var _last_stop := -999
var score := 0.0
var style := 0.0
var rank := 0
var _style_idle := 0
var gauge := 0.0
var stats := {"kills": 0, "perfects": 0, "greats": 0, "offs": 0, "parries": 0, "grazes": 0, "hits": 0, "max_rank": 0, "notes": 0, "frames": 0}
var next_id := 1
## "", "win" or "lose".
var over := ""
## Tests: the hero never takes damage.
var god := false
## Enemy time scale (overdrive slows the world).
var k := 1.0
var can_pulse := true
var can_overdrive := true
## Sourdine: one absorbed hit per wave. Reprise: one second chance per stage.
var guard := false
var second_chance := false
var _perfects_for_heal := 0

func _init(arena_def: Dictionary, bpm: float = 128.0, seed_value: int = 1) -> void:
	rng = Rng.new(seed_value)
	beat = BeatClock.new(bpm)
	arena = Arena.new(arena_def)

## Requests a short hit-stop. Small ones are skipped when another happened recently.
func hit_stop(frames: int) -> void:
	if frames < 6 and frame - _last_stop < 24:
		return
	if frames > freeze:
		freeze = frames
	_last_stop = frame

func apply_mods(m: Mods) -> void:
	mods = m
	if m.has.has("trille"):
		player.max_dash = 3
		player.dash_charges = 3
	if m.has.has("virtuose"):
		player.max_hp = maxi(1, player.max_hp - 1)
		player.hp = player.max_hp
	if m.has.has("metronome"):
		beat.perfect_window = 0.1
	second_chance = m.has.has("reprise")
	guard = m.has.has("sourdine")

func score_mult() -> float:
	return (1.25 if mods.has.has("virtuose") else 1.0) * (0.9 if mods.has.has("metronome") else 1.0)

func mult() -> float:
	return RANK_MULT[rank] * (2.0 if player.is_overdrive() else 1.0)

func emit(e: Dictionary) -> void:
	events.append(e)

func sfx(name: String, pitch: float = 1.0, vol: float = 1.0) -> void:
	events.append({"t": "sfx", "name": name, "pitch": pitch, "vol": vol})

## Visual-only randomness (never affects determinism of tests that use god mode and fixed seeds much).
func fx_rand() -> float:
	return randf()

func add_hazard(h: Hazard) -> Hazard:
	h.id = next_id
	next_id += 1
	hazards.append(h)
	return h

func add(e: Enemy) -> Enemy:
	e.id = next_id
	next_id += 1
	enemies.append(e)
	return e

# ---- Style, score, gauge ------------------------------------------------------------------------

func gain(st: float, g: float, sc: float, x: float = NAN, y: float = NAN) -> void:
	style = minf(STYLE_MAX, style + st)
	_style_idle = 0
	gauge = minf(100.0, gauge + g * mods.gauge)
	if sc > 0.0:
		var v := U.jround(sc * mult() * score_mult())
		score += v
		if not is_nan(x) and not is_nan(y):
			emit({"t": "score", "x": x, "y": y, "value": int(v)})
	_update_rank()

func _update_rank() -> void:
	var r := 0
	for i in RANK_AT.size():
		if style >= RANK_AT[i]:
			r = i
	if r != rank:
		emit({"t": "rank", "rank": r, "up": r > rank})
		if r > rank:
			sfx("rankUp", 1.0 + r * 0.06)
		rank = r
		stats.max_rank = maxi(stats.max_rank, r)

func judged(j: String, what: String, x: float, y: float) -> void:
	if j == "perfect":
		stats.perfects += 1
		gain(26.0, 5.0, 60.0, x, y)
		if mods.has.has("harmonie"):
			_perfects_for_heal += 1
			if _perfects_for_heal >= 30:
				_perfects_for_heal = 0
				if player.hp < player.max_hp:
					player.hp += 1
					emit({"t": "heal", "x": x, "y": y})
					sfx("heal")
	elif j == "great":
		stats.greats += 1
		gain(10.0, 2.5, 25.0)
	else:
		stats.offs += 1
		return
	emit({"t": "judge", "x": x, "y": y, "judge": j, "what": what})

# ---- Player actions --------------------------------------------------------------------------

func recoil(a: float) -> void:
	emit({"t": "shake", "a": a * 0.35})

## A perfect dash leaves a burst of light behind.
func dash_burst(x: float, y: float) -> void:
	emit({"t": "boom", "x": x, "y": y, "r": 2.6, "color": 0x2ef2ff})
	for e in enemies.duplicate():
		if e.is_active() and U.hypot(e.x - x, e.y - y) < 2.6 + e.r:
			e.hit(self, 6.0 * mods.damage, x, y)
	clear_bullets(x, y, 2.6, true)

## Reflects enemy bullets inside the pulse. Returns how many.
func reflect(x: float, y: float, r: float, j: String) -> int:
	var b := bullets
	var bx := b.x
	var by := b.y
	var bvx := b.vx
	var bvy := b.vy
	var br := b.r
	var bf := b.flags
	var n := 0
	var i := 0
	while i < b.n:
		if bf[i] == 2:
			i += 1
			continue
		var dx := bx[i] - x
		var dy := by[i] - y
		if dx * dx + dy * dy > (r + br[i]) * (r + br[i]):
			i += 1
			continue
		var tg := nearest_enemy(bx[i], by[i], 40.0)
		var a := atan2(tg.y - by[i], tg.x - bx[i]) if tg else atan2(dy, dx)
		var sp := maxf(26.0, U.hypot(bvx[i], bvy[i]) * 1.8)
		bvx[i] = cos(a) * sp
		bvy[i] = sin(a) * sp
		bf[i] = 2
		b.wait[i] = 0.0
		b.acc[i] = 0.0
		b.turn[i] = 0.0
		b.life[i] = 2.0
		b.color[i] = 0xffffff if j == "perfect" else 0x2ef2ff
		b.dmg[i] = (7.0 if j == "perfect" else (5.0 if j == "great" else 3.5)) * mods.damage
		n += 1
		if mods.has.has("contrepoint"):
			for da in [-0.35, 0.35]:
				var kk := b.add(bx[i], by[i], a + da, sp, {"color": b.color[i], "r": br[i], "life": 2.0})
				if kk >= 0:
					bf[kk] = 2
					b.dmg[kk] = b.dmg[i] * 0.6
		i += 1
	if n > 0:
		stats.parries += n
		gain(18.0 + n * 6.0, 6.0 + n * 2.0, 80.0 * n, x, y)
		emit({"t": "parry", "x": x, "y": y, "n": n})
		hit_stop(4 if j == "perfect" else 2)
		sfx("parry", 1.25 if j == "perfect" else 1.0)
	if j != "off":
		for e in enemies:
			if not e.is_active() or e.heavy:
				continue
			var d := U.hypot(e.x - x, e.y - y)
			if d < r + e.r:
				var dd := d if d != 0.0 else 1.0
				e.vx += ((e.x - x) / dd) * 14.0
				e.vy += ((e.y - y) / dd) * 14.0
				e.stun = maxf(e.stun, 40.0 if j == "perfect" else 20.0)
	return n

## Removes bullets in a radius (optionally turning some into notes).
func clear_bullets(x: float, y: float, r: float, to_notes: bool) -> void:
	var b := bullets
	var i := b.n - 1
	while i >= 0:
		if b.flags[i] != 2 and (b.x[i] - x) * (b.x[i] - x) + (b.y[i] - y) * (b.y[i] - y) < r * r:
			if to_notes and rng.chance(0.3):
				drop(b.x[i], b.y[i], 1, "note")
			emit({"t": "hit", "x": b.x[i], "y": b.y[i], "color": b.color[i], "big": false})
			b.remove(i)
		i -= 1

## Chain lightning from the hero along the aim.
func chain(x: float, y: float, aim: float, jumps: int, dmg: float, rng_: float, stun_it: bool) -> void:
	var cx := x
	var cy := y
	var hit_ids := {}
	var first := true
	for kk in jumps:
		var best: Enemy = null
		var bd := rng_ if first else rng_ * 0.7
		for e in enemies:
			if not e.is_active() or hit_ids.has(e.id):
				continue
			var d := U.hypot(e.x - cx, e.y - cy) - e.r
			if d > bd:
				continue
			if first and absf(U.angle_diff(aim, atan2(e.y - cy, e.x - cx))) > 0.75:
				continue
			bd = d
			best = e
		if best == null:
			if first:
				emit({"t": "shot", "x": cx + cos(aim) * rng_ * 0.6, "y": cy + sin(aim) * rng_ * 0.6, "a": aim, "accent": false, "weapon": "arcMiss"})
			break
		emit({"t": "shot", "x": best.x, "y": best.y, "a": atan2(cy - best.y, cx - best.x), "accent": true, "weapon": "arcLink", "x1": cx, "y1": cy})
		best.hit(self, dmg * (1.0 if first else pow(0.8, kk)), cx, cy)
		if stun_it:
			best.stun = maxf(best.stun, 30.0)
		hit_ids[best.id] = true
		cx = best.x
		cy = best.y
		first = false

func nearest_enemy(x: float, y: float, max_d: float) -> Enemy:
	var best: Enemy = null
	var bd := max_d
	for e in enemies:
		if not e.is_active() or not e.targetable:
			continue
		var d := U.hypot(e.x - x, e.y - y)
		if d < bd:
			bd = d
			best = e
	return best

func enemy_by_id(i: int) -> Enemy:
	for e in enemies:
		if e.id == i:
			return e
	return null

func drop(x: float, y: float, n: int, kind: String, id: String = "") -> void:
	for i in n:
		var a := rng.range_f(0.0, U.TAU_F)
		var s := rng.range_f(2.0, 8.0)
		pickups.append({"x": x, "y": y, "vx": cos(a) * s, "vy": sin(a) * s, "kind": kind, "t": 0, "id": id})

# ---- Damage -------------------------------------------------------------------------------------

func hurt_player(from_x: float, from_y: float) -> void:
	var p := player
	if p.invulnerable() or god:
		return
	if guard:
		guard = false
		p.inv = 50
		clear_bullets(p.x, p.y, 4.0, false)
		emit({"t": "ring", "x": p.x, "y": p.y, "r": 3.0, "color": 0xffffff, "life": 24})
		emit({"t": "judge", "x": p.x, "y": p.y, "judge": "great", "what": "guard"})
		sfx("parry", 0.7)
		return
	p.hp -= 1
	if p.hp <= 0 and second_chance:
		second_chance = false
		p.hp = 1
		emit({"t": "heal", "x": p.x, "y": p.y})
	p.inv = Player.HURT_INV
	stats.hits += 1
	# Losing two ranks hurts more than the hit itself.
	var r := maxi(0, rank - 2)
	style = minf(style, RANK_AT[r])
	_update_rank()
	hit_stop(6)
	var a := atan2(p.y - from_y, p.x - from_x)
	p.vx = cos(a) * 16.0
	p.vy = sin(a) * 16.0
	clear_bullets(p.x, p.y, 4.5, false)
	emit({"t": "hurt", "x": p.x, "y": p.y, "hp": p.hp})
	emit({"t": "shake", "a": 0.9})
	emit({"t": "chroma", "a": 1.0})
	sfx("hurt")
	if p.hp <= 0:
		p.dead = true
		emit({"t": "die", "x": p.x, "y": p.y})
		sfx("die")
		hit_stop(18)

## Called by foes when they die.
func killed(e: Enemy) -> void:
	stats.kills += 1
	gain(10.0 + e.value / 25.0, 2.5 + e.value / 60.0, e.value, e.x, e.y)
	emit({"t": "kill", "x": e.x, "y": e.y, "size": e.r, "color": e.color, "look": e.look, "heavy": e.heavy})
	drop(e.x, e.y, e.notes, "note")
	if e.heavy:
		hit_stop(3)

## Frees everything (call when the stage ends).
func dispose() -> void:
	for e in enemies:
		e.release()
	enemies.clear()
	hazards.clear()
	pickups.clear()

# ---- Frame --------------------------------------------------------------------------------------

func update(c: Controls) -> void:
	beat.tick()
	frame += 1
	if freeze > 0:
		freeze -= 1
		return
	if over != "":
		return
	stats.frames += 1
	var p := player
	k = 0.55 if p.is_overdrive() else 1.0
	p.update(self, c)
	var i := 0
	while i < enemies.size():
		enemies[i].tick(self)
		i += 1
	i = 0
	while i < hazards.size():
		hazards[i].update(self)
		i += 1
	var n := 0
	for h in hazards:
		if not h.done:
			hazards[n] = h
			n += 1
	hazards.resize(n)
	_separate()
	_update_shots()
	_update_bullets()
	_update_pickups()
	n = 0
	for e in enemies:
		if not e.dead:
			enemies[n] = e
			n += 1
		else:
			e.release()
	enemies.resize(n)
	# Style slowly drains when nothing happens.
	_style_idle += 1
	if _style_idle > 90.0 * mods.style_keep:
		style = maxf(0.0, style - (1.0 + rank * 0.35))
		_update_rank()

## Soft push between foes so they never merge into one blob.
func _separate() -> void:
	var es := enemies
	var cnt := es.size()
	for i in cnt:
		var a := es[i]
		if not a.is_active() or a.boss:
			continue
		for j in range(i + 1, cnt):
			var b := es[j]
			if not b.is_active() or b.boss:
				continue
			var dx := b.x - a.x
			var dy := b.y - a.y
			var m := a.r + b.r
			var d2 := dx * dx + dy * dy
			if d2 >= m * m or d2 < 1e-6:
				continue
			var d := sqrt(d2)
			var push := (m - d) / d
			var wa := 0.5 if a.heavy == b.heavy else (0.05 if a.heavy else 0.95)
			var wb := 1.0 - wa
			a.x -= dx * push * wa * 0.5
			a.y -= dy * push * wa * 0.5
			b.x += dx * push * wb * 0.5
			b.y += dy * push * wb * 0.5

func _update_shots() -> void:
	var s := shots
	var sx := s.x
	var sy := s.y
	var svx := s.vx
	var svy := s.vy
	var sl := s.life
	var dt := 1.0 / 60.0
	var i := s.n - 1
	while i >= 0:
		if s.kind[i] == Shots.MISSILE:
			# Homing: turn towards the target, accelerate.
			var tg: Enemy = enemy_by_id(s.target[i]) if s.target[i] != 0 else null
			if tg == null or not tg.is_active():
				tg = nearest_enemy(sx[i], sy[i], 30.0)
				s.target[i] = tg.id if tg else 0
			var sp := minf(48.0, U.hypot(svx[i], svy[i]) + 1.4)
			var a := atan2(svy[i], svx[i])
			if tg:
				a += maxf(-0.16, minf(0.16, U.angle_diff(a, atan2(tg.y - sy[i], tg.x - sx[i]))))
			svx[i] = cos(a) * sp
			svy[i] = sin(a) * sp
		sx[i] += svx[i] * dt
		sy[i] += svy[i] * dt
		sl[i] -= dt
		if sl[i] <= 0.0 or not arena.inside(sx[i], sy[i], -1.0) or arena.blocked(sx[i], sy[i]):
			if sl[i] > 0.0:
				emit({"t": "hit", "x": sx[i], "y": sy[i], "color": 0x9ff8ff, "big": false})
			s.remove(i)
			i -= 1
			continue
		var lance_k := s.kind[i] == Shots.LANCE
		for e in enemies:
			if not e.is_active() or not e.hurtable:
				continue
			if s.last[i] == e.id:
				continue
			var dx := sx[i] - e.x
			var dy := sy[i] - e.y
			var rr := e.r + (0.6 if lance_k else 0.15)
			if dx * dx + dy * dy > rr * rr:
				continue
			e.hit(self, s.dmg[i], sx[i] - svx[i] * 0.02, sy[i] - svy[i] * 0.02, s.kind[i])
			s.last[i] = e.id
			if s.pierce[i] > 0:
				s.pierce[i] -= 1
				continue
			sl[i] = 0.0
			break
		if sl[i] <= 0.0:
			s.remove(i)
		i -= 1

func _update_bullets() -> void:
	var b := bullets
	var p := player
	var kk := k
	var dt := (1.0 / 60.0) * kk
	var bx := b.x
	var by := b.y
	var bvx := b.vx
	var bvy := b.vy
	var bl := b.life
	var bw := b.wait
	var bf := b.flags
	var br := b.r
	var bt := b.turn
	var ba := b.acc
	var i := b.n - 1
	while i >= 0:
		var reflected := bf[i] == 2
		var dtt := 1.0 / 60.0 if reflected else dt
		if bw[i] > 0.0:
			bw[i] -= dtt
			i -= 1
			continue
		if bt[i] != 0.0:
			var c := cos(bt[i] * dtt)
			var s := sin(bt[i] * dtt)
			var nvx := bvx[i] * c - bvy[i] * s
			bvy[i] = bvx[i] * s + bvy[i] * c
			bvx[i] = nvx
		if ba[i] != 0.0:
			var sp := U.hypot(bvx[i], bvy[i])
			var ns := maxf(0.5, sp + ba[i] * dtt)
			bvx[i] *= ns / sp
			bvy[i] *= ns / sp
		bx[i] += bvx[i] * dtt
		by[i] += bvy[i] * dtt
		bl[i] -= dtt
		if bl[i] <= 0.0 or not arena.inside(bx[i], by[i], -1.5) or arena.blocked(bx[i], by[i]):
			if bl[i] > 0.0:
				emit({"t": "hit", "x": bx[i], "y": by[i], "color": b.color[i], "big": false})
			b.remove(i)
			i -= 1
			continue
		if reflected:
			for e in enemies:
				if not e.is_active() or not e.hurtable:
					continue
				if (bx[i] - e.x) * (bx[i] - e.x) + (by[i] - e.y) * (by[i] - e.y) < (e.r + br[i]) * (e.r + br[i]):
					e.hit(self, b.dmg[i], bx[i], by[i], Shots.ACCENT)
					emit({"t": "boom", "x": bx[i], "y": by[i], "r": 1.2, "color": 0x2ef2ff})
					bl[i] = 0.0
					break
			if bl[i] <= 0.0:
				b.remove(i)
			i -= 1
			continue
		if p.dead:
			i -= 1
			continue
		var dx := bx[i] - p.x
		var dy := by[i] - p.y
		var d2 := dx * dx + dy * dy
		var hr := br[i] + Player.RADIUS
		if d2 < hr * hr and not p.invulnerable():
			hurt_player(bx[i], by[i])
			b.remove(i)
			i -= 1
			continue
		var gr := br[i] + Player.GRAZE
		if bf[i] == 0 and d2 < gr * gr:
			bf[i] = 1
			stats.grazes += 1
			gain(5.0, 4.4 if mods.has.has("frolement") else 2.2, 10.0)
			emit({"t": "graze", "x": bx[i], "y": by[i]})
		i -= 1

func _update_pickups() -> void:
	var p := player
	var reach := 3.2 * mods.magnet
	var i := pickups.size() - 1
	while i >= 0:
		var q: Dictionary = pickups[i]
		q.t += 1
		var dx: float = p.x - q.x
		var dy: float = p.y - q.y
		var d := U.hypot(dx, dy)
		if q.t > 20 and (d < reach or q.t > 420 or p.is_overdrive()) and not p.dead:
			var sp: float = 30.0 + q.t * 0.1
			q.vx += ((dx / d) * sp - q.vx) * 0.2
			q.vy += ((dy / d) * sp - q.vy) * 0.2
		else:
			q.vx *= 0.92
			q.vy *= 0.92
		q.x += q.vx / 60.0
		q.y += q.vy / 60.0
		var c := arena.confine(q.x, q.y, 0.3)
		q.x = c.x
		q.y = c.y
		if d < 0.7 and q.t > 20 and not p.dead:
			if q.kind == "note":
				stats.notes += 1
				gain(0.0, 0.4, 5.0)
				emit({"t": "note", "x": q.x, "y": q.y})
			elif q.kind == "heal":
				p.hp = mini(p.max_hp, p.hp + 1)
				emit({"t": "heal", "x": q.x, "y": q.y})
				sfx("heal")
			elif q.kind == "fragment":
				emit({"t": "fragment", "x": q.x, "y": q.y, "id": q.id})
				sfx("fragment")
			pickups.remove_at(i)
		i -= 1
