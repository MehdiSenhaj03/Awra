class_name Sim
extends RefCounted
## The ball: a hundred guests with their routines, one or more of them driven by players. Integers only, so a
## match is identical on every machine (replays, lockstep network play).
##
## Modes: "contract" (a spy among the guests, a sniper above) and "party" (2-4 players, each hunting another).
## Player input byte: bits 0-3 direction (0 none, 1..8 E SE S SW W NW N NE), bit 4 hurry, bit 5 act held,
## bit 6 stab (party).

const IN_HURRY := 16
const IN_ACT := 32
const IN_STAB := 64      # party mode: a stab (sent on the press only)
const R := 60            # guest radius (1/256 tile)
const WALK := 9          # units per frame (≈2.1 tiles/s)
const HURRY := 15

enum St { WALK, ACT, DANCE_WAIT, DANCE, DEAD, OUT }
## Gestures (for the eye — and the sniper's eye).
enum G { NONE, SIP, ADMIRE, TALK, WRITE, POUR, TOUCH, REACH, STAB, BOW }

const ACT_FRAMES := {"buffet": [300, 720], "painting": [240, 540], "statue": [240, 480], "talk": [360, 900], "seat": [420, 1000], "window": [240, 600], "letter": [200, 330], "poison": [300, 600], "bug": [240, 420], "entrance": [60, 120]}
const GESTURE := {"buffet": G.SIP, "poison": G.SIP, "painting": G.ADMIRE, "statue": G.ADMIRE, "bug": G.ADMIRE, "window": G.ADMIRE, "talk": G.TALK, "letter": G.WRITE, "seat": G.NONE, "entrance": G.NONE}
## What each mission needs: frames of act held, and the tell shown meanwhile.
const MISSION := {
	"letter": {"frames": 150, "tell": G.WRITE},
	"poison": {"frames": 100, "tell": G.POUR},
	"bug": {"frames": 70, "tell": G.TOUCH},
	"contact": {"frames": 180, "tell": G.TALK},
	"pocket": {"frames": 90, "tell": G.REACH},
	"countess": {"frames": 200, "tell": G.BOW},
}

class Guest:
	var x := 0
	var y := 0
	var face := 64
	var st := 0
	var t := 0
	var poi := -1
	var sx := 0
	var sy := 0
	var speed := 9
	var mask := 0
	var coat := 0
	var hat := 0
	var ctrl := -1
	var partner := -1
	var pivot_x := 0
	var pivot_y := 0
	var ang := 0
	var gesture := 0
	var g_t := 0
	var off_x := 0
	var off_y := 0
	var role := ""
	var moving := false
	func clone() -> Guest:
		var g := Guest.new()
		g.x = x; g.y = y; g.face = face; g.st = st; g.t = t; g.poi = poi; g.sx = sx; g.sy = sy; g.speed = speed
		g.mask = mask; g.coat = coat; g.hat = hat; g.ctrl = ctrl; g.partner = partner; g.pivot_x = pivot_x
		g.pivot_y = pivot_y; g.ang = ang; g.gesture = gesture; g.g_t = g_t; g.off_x = off_x; g.off_y = off_y
		g.role = role; g.moving = moving
		return g

var venue: Venue
var rng: Rng
var mode := "contract"
var frame := 0
var limit := 60 * 210
var guests: Array[Guest] = []
var occ := PackedInt32Array()
var events: Array[Dictionary] = []
var over := ""
## Contract.
var spy := -1
var missions: Array = []
var need := 3
var done: Dictionary = {}
var progress: Dictionary = {}
var cursor := Vector2i.ZERO
var shot := false
var shot_at := -1
var contact := -1
var ambassador := -1
var countess := -1
## Party: one entry per player {guest, target, score, exposed, respawn}.
var players: Array[Dictionary] = []
var party_goal := 3
var _prev_act := PackedByteArray()

func _init(v: Venue, m: String, seed_v: int, n_guests: int, n_players: int = 1, mission_list: Array = []) -> void:
	venue = v
	mode = m
	rng = Rng.new(seed_v)
	occ.resize(venue.pois.size())
	cursor = Vector2i(v.w * 128, v.h * 128)
	for i in n_guests:
		guests.append(_spawn(i))
	# Settle the crowd: everyone already somewhere at the start of the evening.
	for i in guests.size():
		_choose(guests[i], true)
		var gg := guests[i]
		if gg.poi < 0:
			# Every spot is taken: just stand somewhere in the room.
			var tx := rng.rint(1, venue.w - 2)
			var ty := rng.rint(1, venue.h - 2)
			while venue.tile(tx, ty) != 0:
				tx = rng.rint(1, venue.w - 2)
				ty = rng.rint(1, venue.h - 2)
			gg.sx = tx * 256 + 128
			gg.sy = ty * 256 + 128
		gg.x = gg.sx
		gg.y = gg.sy
		if gg.st == St.WALK:
			_arrive(gg)
	_prev_act.resize(4)
	if mode == "contract":
		missions = mission_list if mission_list.size() > 0 else ["letter", "poison", "bug", "contact"]
		need = mini(3, missions.size())
		spy = rng.rint(0, guests.size() - 1)
		guests[spy].ctrl = 0
		_special_roles()
	else:
		var used: Array[int] = []
		for p in n_players:
			var gi := rng.rint(0, guests.size() - 1)
			while used.has(gi):
				gi = rng.rint(0, guests.size() - 1)
			used.append(gi)
			guests[gi].ctrl = p
			players.append({"guest": gi, "target": (p + 1) % n_players, "score": 0, "exposed": 0, "respawn": 0})
		limit = 60 * 240

func _special_roles() -> void:
	var pick := func() -> int:
		var k := rng.rint(0, guests.size() - 1)
		while k == spy or guests[k].role != "":
			k = rng.rint(0, guests.size() - 1)
		return k
	contact = pick.call()
	guests[contact].role = "contact"
	guests[contact].mask = 6
	guests[contact].coat = 7
	ambassador = pick.call()
	guests[ambassador].role = "ambassador"
	guests[ambassador].mask = 3
	guests[ambassador].coat = 8
	guests[ambassador].hat = 2
	countess = pick.call()
	guests[countess].role = "countess"
	guests[countess].mask = 1
	guests[countess].coat = 9

func _spawn(_i: int) -> Guest:
	var g := Guest.new()
	g.speed = rng.rint(WALK - 1, WALK + 1)
	g.mask = rng.rint(0, 7)
	g.coat = rng.rint(0, 6)
	g.hat = rng.rint(0, 3)
	g.off_x = rng.rint(-50, 50)
	g.off_y = rng.rint(-50, 50)
	var e: Dictionary = venue.pois[venue.entrances[rng.rint(0, venue.entrances.size() - 1)]] if venue.entrances.size() > 0 else venue.pois[0]
	g.x = int(e.x) * 256 + 128
	g.y = int(e.y) * 256 + 128
	return g

# ---- Routines -----------------------------------------------------------------------------------------------------

const WEIGHTS := {"buffet": 22, "painting": 14, "statue": 10, "talk": 30, "dance": 18, "seat": 6, "window": 8, "letter": 4, "poison": 6, "bug": 3}

func _choose(g: Guest, anywhere: bool = false) -> void:
	if g.poi >= 0:
		occ[g.poi] = maxi(0, occ[g.poi] - 1)
	g.poi = -1
	var total := 0
	for i in venue.pois.size():
		var p: Dictionary = venue.pois[i]
		if occ[i] < int(p.cap):
			total += int(WEIGHTS.get(p.kind, 0))
	if total == 0:
		g.st = St.ACT
		g.t = 120
		return
	var r := rng.rint(0, total - 1)
	for i in venue.pois.size():
		var p: Dictionary = venue.pois[i]
		if occ[i] >= int(p.cap):
			continue
		r -= int(WEIGHTS.get(p.kind, 0))
		if r < 0:
			g.poi = i
			break
	if g.poi < 0:
		return
	var p: Dictionary = venue.pois[g.poi]
	var k: int = occ[g.poi]
	occ[g.poi] += 1
	var cx := int(p.x) * 256 + 128
	var cy := int(p.y) * 256 + 128
	match p.kind:
		"talk":
			var a := (k * 256 / int(p.cap) + rng.rint(0, 20)) & 255
			g.sx = cx + Fx.cosa(a) * 240 / 1024
			g.sy = cy + Fx.sina(a) * 240 / 1024
		"dance":
			var a := rng.rint(0, 255)
			var rr := rng.rint(100, mini(900, int(p.get("rad", 420))))
			g.sx = cx + Fx.cosa(a) * rr / 1024
			g.sy = cy + Fx.sina(a) * rr / 1024
		_:
			g.sx = cx + g.off_x / 3
			g.sy = cy + g.off_y / 3
	g.st = St.WALK
	g.t = 0

func _arrive(g: Guest) -> void:
	var p: Dictionary = venue.pois[g.poi]
	if p.kind == "dance":
		g.st = St.DANCE_WAIT
		g.t = 600
		return
	g.st = St.ACT
	var range_f: Array = ACT_FRAMES.get(p.kind, [240, 480])
	g.t = rng.rint(range_f[0], range_f[1])
	if p.kind == "talk":
		g.face = Fx.angle(int(p.x) * 256 + 128 - g.x, int(p.y) * 256 + 128 - g.y)
	else:
		g.face = int(p.face)

func _walk_to(g: Guest, sp: int) -> bool:
	var tx := g.x >> 8
	var ty := g.y >> 8
	var p: Dictionary = venue.pois[g.poi]
	var f: PackedInt32Array = p.field
	var gx := g.sx
	var gy := g.sy
	if tx != int(p.x) or ty != int(p.y):
		var best := f[ty * venue.w + tx] if venue.walkable(tx, ty) else 1 << 30
		var bx := tx
		var by := ty
		for d in [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [1, -1], [-1, 1], [-1, -1]]:
			var nx: int = tx + d[0]
			var ny: int = ty + d[1]
			if venue.walkable(nx, ny) and f[ny * venue.w + nx] < best:
				best = f[ny * venue.w + nx]
				bx = nx
				by = ny
		gx = bx * 256 + 128 + g.off_x
		gy = by * 256 + 128 + g.off_y
	var dx := gx - g.x
	var dy := gy - g.y
	var dist := Fx.isqrt(dx * dx + dy * dy)
	if dist <= sp:
		g.x = gx
		g.y = gy
		return gx == g.sx and gy == g.sy
	g.x += dx * sp / dist
	g.y += dy * sp / dist
	var want := Fx.angle(dx, dy)
	g.face = (g.face + clampi(Fx.adiff(g.face, want), -12, 12)) & 255
	g.moving = true
	return false

func _npc(gi: int, g: Guest) -> void:
	match g.st:
		St.WALK:
			if g.poi < 0:
				_choose(g)
				return
			# Stuck in the crowd for long: change one's mind.
			g.t += 1
			if g.t > 900:
				_choose(g)
				return
			if _walk_to(g, g.speed):
				_arrive(g)
		St.ACT:
			g.t -= 1
			if g.gesture == G.NONE and g.poi >= 0 and rng.chance(6):
				g.gesture = GESTURE.get(venue.pois[g.poi].kind, G.NONE)
				g.g_t = 70
			if g.t <= 0:
				_choose(g)
		St.DANCE_WAIT:
			g.t -= 1
			if g.t <= 0:
				_choose(g)
				return
			# Find a partner nearby who also waits.
			for oi in guests.size():
				var o := guests[oi]
				if oi != gi and o.st == St.DANCE_WAIT and o.ctrl < 0 and absi(o.x - g.x) < 900 and absi(o.y - g.y) < 900:
					var len := rng.rint(600, 960)
					for pr in [[g, oi], [o, gi]]:
						var a: Guest = pr[0]
						a.st = St.DANCE
						a.partner = pr[1]
						a.t = len
						a.pivot_x = (g.x + o.x) / 2
						a.pivot_y = (g.y + o.y) / 2
					g.ang = Fx.angle(g.x - g.pivot_x, g.y - g.pivot_y)
					o.ang = (g.ang + 128) & 255
					break
		St.DANCE:
			g.t -= 1
			# Waltz: turn around the pair's pivot, the pivot drifts around the floor's centre.
			g.ang = (g.ang + 3) & 255
			var p: Dictionary = venue.pois[g.poi] if g.poi >= 0 else {"x": g.pivot_x >> 8, "y": g.pivot_y >> 8}
			var cx := int(p.x) * 256 + 128
			var cy := int(p.y) * 256 + 128
			var pa := Fx.angle(g.pivot_x - cx, g.pivot_y - cy)
			if gi < g.partner:
				var pr := Fx.isqrt((g.pivot_x - cx) * (g.pivot_x - cx) + (g.pivot_y - cy) * (g.pivot_y - cy))
				pa = (pa + 1) & 255
				g.pivot_x = cx + Fx.cosa(pa) * maxi(pr, 200) / 1024
				g.pivot_y = cy + Fx.sina(pa) * maxi(pr, 200) / 1024
				var o := guests[g.partner]
				o.pivot_x = g.pivot_x
				o.pivot_y = g.pivot_y
			g.x = g.pivot_x + Fx.cosa(g.ang) * 70 / 1024
			g.y = g.pivot_y + Fx.sina(g.ang) * 70 / 1024
			g.face = (g.ang + 64) & 255
			g.moving = true
			if g.t <= 0:
				g.partner = -1
				_choose(g)

# ---- Frame ----------------------------------------------------------------------------------------------------------

## One frame. `inp`: one byte per player; `aim`: sniper cursor (units) and whether it fires this frame.
func step(inp: PackedByteArray, aim: Vector2i = Vector2i(-1, -1), fire: bool = false) -> void:
	events.clear()
	if over != "":
		return
	frame += 1
	for gi in guests.size():
		var g := guests[gi]
		g.moving = false
		if g.g_t > 0:
			g.g_t -= 1
			if g.g_t == 0:
				g.gesture = G.NONE
		if g.st == St.DEAD or g.st == St.OUT:
			continue
		if g.ctrl >= 0:
			_player(gi, g, inp[g.ctrl] if g.ctrl < inp.size() else 0)
		else:
			_npc(gi, g)
	_separate()
	if mode == "contract":
		if aim.x >= 0:
			cursor = aim
		if fire and not shot:
			_fire()
		if over == "" and frame >= limit:
			_end("hunter", "time")
	else:
		_party()

func _separate() -> void:
	# Light push between guests closer than two radii (grid buckets of a tile).
	var buckets: Dictionary = {}
	for i in guests.size():
		var g := guests[i]
		if g.st == St.DEAD or g.st == St.OUT:
			continue
		var k := (g.y >> 8) * 256 + (g.x >> 8)
		if not buckets.has(k):
			buckets[k] = []
		buckets[k].append(i)
	for i in guests.size():
		var g := guests[i]
		if g.st != St.WALK and g.ctrl < 0:
			continue
		if g.st == St.DEAD or g.st == St.OUT:
			continue
		var tx := g.x >> 8
		var ty := g.y >> 8
		for dy in range(-1, 2):
			for dx in range(-1, 2):
				var list: Variant = buckets.get((ty + dy) * 256 + tx + dx)
				if list == null:
					continue
				for j: int in list:
					if j == i:
						continue
					var o := guests[j]
					var ex := g.x - o.x
					var ey := g.y - o.y
					var d2 := ex * ex + ey * ey
					if d2 < (R * 2) * (R * 2) and d2 > 0:
						var d := Fx.isqrt(d2)
						var push := (R * 2 - d) / 4
						var nx := g.x + ex * push / d
						var ny := g.y + ey * push / d
						if venue.walkable(nx >> 8, ny >> 8):
							g.x = nx
							g.y = ny

func _ev(t: String, extra: Dictionary = {}) -> void:
	var e := {"t": t}
	e.merge(extra)
	events.append(e)

func _end(winner: String, why: String) -> void:
	if over != "":
		return
	over = winner
	_ev("over", {"winner": winner, "why": why})

# ---- Players ----------------------------------------------------------------------------------------------------------

func _player(gi: int, g: Guest, b: int) -> void:
	var d := b & 15
	var act := (b & IN_ACT) != 0
	var pressed := act and _prev_act[g.ctrl] == 0
	_prev_act[g.ctrl] = 1 if act else 0
	if g.st == St.DANCE:
		# Leaving a dance: any move.
		if d != 0:
			var o := guests[g.partner] if g.partner >= 0 else null
			if o and o.partner == gi:
				o.partner = -1
				_choose(o)
			g.partner = -1
			g.st = St.WALK
		else:
			_npc(gi, g)
			_missions_hold(gi, g, act)
			return
	if mode == "party" and (b & IN_STAB) != 0:
		_stab(gi, g)
	if d != 0:
		if g.poi >= 0:
			occ[g.poi] = maxi(0, occ[g.poi] - 1)
			g.poi = -1
		var sp := HURRY if (b & IN_HURRY) != 0 else WALK
		var mx := Fx.DIR_X[d]
		var my := Fx.DIR_Y[d]
		if mx != 0 and my != 0:
			sp = sp * 181 / 256
		var nx := g.x + mx * sp
		var ny := g.y + my * sp
		if venue.walkable((nx + mx * R) >> 8, g.y >> 8):
			g.x = nx
		if venue.walkable(g.x >> 8, (ny + my * R) >> 8):
			g.y = ny
		var want := Fx.DIR_A[d]
		g.face = (g.face + clampi(Fx.adiff(g.face, want), -16, 16)) & 255
		g.moving = true
		g.st = St.WALK
		return
	g.st = St.ACT
	if act:
		_blend(gi, g, pressed)
	if mode == "contract" and gi == spy:
		_missions_hold(gi, g, act)

## Doing what guests do at the nearest point of interest (the spy hides in plain sight).
func _blend(gi: int, g: Guest, pressed: bool) -> void:
	var best := -1
	var bd := 320 * 320
	for i in venue.pois.size():
		var p: Dictionary = venue.pois[i]
		var dx := int(p.x) * 256 + 128 - g.x
		var dy := int(p.y) * 256 + 128 - g.y
		var d2 := dx * dx + dy * dy
		if d2 < bd:
			bd = d2
			best = i
	if best < 0:
		return
	var p: Dictionary = venue.pois[best]
	if p.kind == "dance" and pressed:
		# Ask the nearest waiting guest for a dance.
		for oi in guests.size():
			var o := guests[oi]
			if o.st == St.DANCE_WAIT and absi(o.x - g.x) < 600 and absi(o.y - g.y) < 600:
				var len := 600
				o.st = St.DANCE
				g.st = St.DANCE
				o.partner = gi
				g.partner = oi
				o.t = len
				g.t = len
				o.pivot_x = (g.x + o.x) / 2
				o.pivot_y = (g.y + o.y) / 2
				g.pivot_x = o.pivot_x
				g.pivot_y = o.pivot_y
				g.poi = best
				g.ang = Fx.angle(g.x - g.pivot_x, g.y - g.pivot_y)
				o.ang = (g.ang + 128) & 255
				return
		return
	if p.kind == "talk":
		g.face = Fx.angle(int(p.x) * 256 + 128 - g.x, int(p.y) * 256 + 128 - g.y)
	elif p.kind != "dance":
		g.face = int(p.face)
	if g.gesture == G.NONE:
		g.gesture = GESTURE.get(p.kind, G.NONE)
		g.g_t = 70

# ---- Contract ----------------------------------------------------------------------------------------------------------

## Which mission can be done here, now (or "").
func mission_here(g: Guest) -> String:
	for m: String in missions:
		if done.has(m):
			continue
		match m:
			"letter", "poison", "bug":
				if venue.missions.has(m):
					var p: Dictionary = venue.pois[venue.missions[m]]
					var dx := int(p.x) * 256 + 128 - g.x
					var dy := int(p.y) * 256 + 128 - g.y
					if dx * dx + dy * dy < 200 * 200:
						return m
			"contact":
				var c := guests[contact]
				if c.st == St.ACT and absi(c.x - g.x) < 330 and absi(c.y - g.y) < 330:
					return m
			"pocket":
				var a := guests[ambassador]
				var dx := g.x - a.x
				var dy := g.y - a.y
				if dx * dx + dy * dy < 200 * 200 and dx * Fx.cosa(a.face) + dy * Fx.sina(a.face) < 0:
					return m
			"countess":
				var c := guests[countess]
				if absi(c.x - g.x) < 300 and absi(c.y - g.y) < 300:
					return m
	return ""

func _missions_hold(gi: int, g: Guest, act: bool) -> void:
	if not act:
		return
	var m := mission_here(g)
	if m == "":
		return
	var spec: Dictionary = MISSION[m]
	g.gesture = spec.tell
	g.g_t = 12
	progress[m] = int(progress.get(m, 0)) + 1
	if int(progress[m]) >= int(spec.frames):
		done[m] = frame
		_ev("mission", {"m": m, "x": g.x, "y": g.y, "guest": gi})
		if done.size() >= need:
			_end("spy", "missions")

func _fire() -> void:
	shot = true
	var best := -1
	var bd := 170 * 170
	for i in guests.size():
		var g := guests[i]
		if g.st == St.DEAD:
			continue
		var dx := g.x - cursor.x
		var dy := g.y - cursor.y
		var d2 := dx * dx + dy * dy
		if d2 < bd:
			bd = d2
			best = i
	shot_at = best
	_ev("shot", {"x": cursor.x, "y": cursor.y, "guest": best})
	if best < 0:
		_end("spy", "miss")
		return
	guests[best].st = St.DEAD
	if best == spy:
		_end("hunter", "spy")
	else:
		_end("spy", "innocent")

# ---- Party ----------------------------------------------------------------------------------------------------------------

func _stab(gi: int, g: Guest) -> void:
	var p: Dictionary = players[g.ctrl]
	if int(p.respawn) > 0:
		return
	var best := -1
	var bd := 230 * 230
	for i in guests.size():
		var o := guests[i]
		if i == gi or o.st == St.DEAD or o.st == St.OUT:
			continue
		var dx := o.x - g.x
		var dy := o.y - g.y
		var d2 := dx * dx + dy * dy
		if d2 < bd:
			bd = d2
			best = i
	if best < 0:
		return
	g.gesture = G.STAB
	g.g_t = 40
	var o := guests[best]
	if o.ctrl >= 0 and o.ctrl != g.ctrl:
		# Another player: a kill. They come back elsewhere with a new disguise.
		p.score = int(p.score) + 1
		o.st = St.OUT
		players[o.ctrl].respawn = 180
		_ev("kill", {"by": g.ctrl, "victim": o.ctrl, "x": o.x, "y": o.y})
		if int(p.score) >= party_goal:
			_end("p%d" % g.ctrl, "score")
	else:
		# An innocent: the killer is exposed for a while (the others see who did it) and loses a point.
		o.st = St.DEAD
		p.exposed = 300
		p.score = maxi(0, int(p.score) - 1)
		_ev("innocent", {"by": g.ctrl, "x": o.x, "y": o.y})

func _party() -> void:
	for pi in players.size():
		var p: Dictionary = players[pi]
		if int(p.exposed) > 0:
			p.exposed = int(p.exposed) - 1
		if int(p.respawn) > 0:
			p.respawn = int(p.respawn) - 1
			if int(p.respawn) == 0:
				# A new guest among the crowd: new disguise, at an entrance.
				var g := guests[int(p.guest)]
				var e: Dictionary = venue.pois[venue.entrances[rng.rint(0, venue.entrances.size() - 1)]]
				g.x = int(e.x) * 256 + 128
				g.y = int(e.y) * 256 + 128
				g.mask = rng.rint(0, 7)
				g.coat = rng.rint(0, 6)
				g.hat = rng.rint(0, 3)
				g.st = St.WALK
				g.poi = -1
				_ev("respawn", {"player": pi})
	if over == "" and frame >= limit:
		var best := 0
		for pi in players.size():
			if int(players[pi].score) > int(players[best].score):
				best = pi
		_end("p%d" % best, "time")
