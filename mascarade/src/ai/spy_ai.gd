class_name SpyAI
extends RefCounted
## A computer spy: does its missions in a sensible order and, in between, behaves like a guest (goes to points of
## interest, lingers, makes the same gestures). Its skill sets how often it betrays itself (hesitations, beelines).

var sim: Sim
var skill := 1.0
var rng: Rng
var _goal := -1          # poi index we walk to
var _linger := 0
var _mission := ""
var _hesitate := 0
var _gesture_t := 0
var _visits := 2
var _stuck_t := 0
var _sx := 0
var _sy := 0

func _init(s: Sim, sk: float, seed_v: int) -> void:
	sim = s
	skill = sk
	rng = Rng.new(seed_v)

func _me() -> Sim.Guest:
	return sim.guests[sim.spy]

func _next_mission() -> String:
	var g := _me()
	var best := ""
	var bd := 1 << 40
	for m: String in sim.missions:
		if sim.done.has(m):
			continue
		var p := _mission_pos(m)
		var d := (p.x - g.x) * (p.x - g.x) + (p.y - g.y) * (p.y - g.y)
		if d < bd:
			bd = d
			best = m
	return best

func _mission_pos(m: String) -> Vector2i:
	match m:
		"contact": return Vector2i(sim.guests[sim.contact].x, sim.guests[sim.contact].y)
		"pocket":
			var a := sim.guests[sim.ambassador]
			return Vector2i(a.x - Fx.cosa(a.face) * 150 / 1024, a.y - Fx.sina(a.face) * 150 / 1024)
		"countess": return Vector2i(sim.guests[sim.countess].x, sim.guests[sim.countess].y)
		_:
			var p: Dictionary = sim.venue.pois[sim.venue.missions[m]]
			return Vector2i(int(p.x) * 256 + 128, int(p.y) * 256 + 128)

## Direction byte towards a point, following the venue's walkable tiles (greedy 8-neighbour descent of a
## distance to the target computed on the fly for nearby tiles).
func _dir_to(p: Vector2i) -> int:
	var g := _me()
	var dx := p.x - g.x
	var dy := p.y - g.y
	if absi(dx) < 40 and absi(dy) < 40:
		return 0
	var best := 0
	var bd := 1 << 40
	for d in range(1, 9):
		var nx := g.x + Fx.DIR_X[d] * 128
		var ny := g.y + Fx.DIR_Y[d] * 128
		if not sim.venue.walkable(nx >> 8, ny >> 8):
			continue
		var e := (p.x - nx) * (p.x - nx) + (p.y - ny) * (p.y - ny)
		# Prefer the tiles whose distance field (to the target's tile) is lower: walls in the way are avoided.
		if e < bd:
			bd = e
			best = d
	return best

func _poi_near(p: Vector2i) -> int:
	var best := -1
	var bd := 1 << 40
	for i in sim.venue.pois.size():
		var q: Dictionary = sim.venue.pois[i]
		if q.kind in ["letter", "poison", "bug", "entrance"]:
			continue
		var d := (int(q.x) * 256 - p.x) * (int(q.x) * 256 - p.x) + (int(q.y) * 256 - p.y) * (int(q.y) * 256 - p.y) + rng.rint(0, 9) * 256 * 256 * 4
		if d < bd:
			bd = d
			best = i
	return best

## Follows a poi's distance field like the guests do.
func _dir_field(pi: int) -> int:
	var g := _me()
	var p: Dictionary = sim.venue.pois[pi]
	var f: PackedInt32Array = p.field
	var tx := g.x >> 8
	var ty := g.y >> 8
	if tx == int(p.x) and ty == int(p.y):
		return 0
	var best := f[ty * sim.venue.w + tx] if sim.venue.walkable(tx, ty) else 1 << 30
	var bx := tx
	var by := ty
	for d in [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [1, -1], [-1, 1], [-1, -1]]:
		var nx: int = tx + d[0]
		var ny: int = ty + d[1]
		if sim.venue.walkable(nx, ny) and f[ny * sim.venue.w + nx] < best:
			best = f[ny * sim.venue.w + nx]
			bx = nx
			by = ny
	return _dir_to(Vector2i(bx * 256 + 128, by * 256 + 128))

func input() -> int:
	var g := _me()
	_stuck_t += 1
	if _stuck_t >= 90:
		_stuck_t = 0
		var moved := absi(g.x - _sx) + absi(g.y - _sy)
		_sx = g.x
		_sy = g.y
		if moved < 60 and _linger <= 0 and _hesitate <= 0 and g.st == Sim.St.WALK and _mission == "":
			_goal = -1
	if _hesitate > 0:
		_hesitate -= 1
		return 0
	if _linger > 0:
		_linger -= 1
		_gesture_t -= 1
		if _gesture_t <= 0:
			_gesture_t = rng.rint(90, 200)
			return Sim.IN_ACT
		return 0
	if _mission != "" and sim.done.has(_mission):
		_mission = ""
		_linger = rng.rint(60, 240)
		return 0
	if _mission == "":
		if _goal < 0:
			# Decide: a mission now, or blend in somewhere first.
			if _visits <= 0 and _next_mission() != "":
				_visits = rng.rint(1, 3)
				_mission = _next_mission()
			else:
				_goal = _poi_near(Vector2i(g.x, g.y))
		if _goal >= 0:
			var d := _dir_field(_goal)
			if d == 0:
				_goal = -1
				_visits -= 1
				_linger = rng.rint(300, 720)
				_gesture_t = rng.rint(20, 80)
			elif not rng.chance(int(1000 - 6 * (1.0 - skill) * 10)):
				_hesitate = rng.rint(20, 70)
			return d
	# Mission: get there, then hold.
	var here := sim.mission_here(g)
	if here == _mission:
		return Sim.IN_ACT
	var target := _mission_pos(_mission)
	if _mission in ["contact", "countess"] and absi(target.x - g.x) + absi(target.y - g.y) < 300:
		return 0
	if _mission in ["letter", "poison", "bug"]:
		return _dir_field(sim.venue.missions[_mission])
	return _path_to(target)

var _pf := PackedInt32Array()
var _pf_tile := -1
var _pf_age := 0

## Follows a breadth-first distance field to any point (recomputed when the target changes tile, at most twice
## a second), so moving targets behind furniture are reached too.
func _path_to(p: Vector2i) -> int:
	var g := _me()
	var w := sim.venue.w
	var tt := (p.y >> 8) * w + (p.x >> 8)
	_pf_age -= 1
	if tt != _pf_tile and _pf_age <= 0 and sim.venue.walkable(p.x >> 8, p.y >> 8):
		_pf = sim.venue._field(p.x >> 8, p.y >> 8)
		_pf_tile = tt
		_pf_age = 30
	if _pf.is_empty():
		return _dir_to(p)
	var tx := g.x >> 8
	var ty := g.y >> 8
	if (ty * w + tx) == _pf_tile or absi(p.x - g.x) + absi(p.y - g.y) < 400:
		return _dir_to(p)
	var best := _pf[ty * w + tx] if sim.venue.walkable(tx, ty) else 1 << 30
	var bx := tx
	var by := ty
	for d in [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [1, -1], [-1, 1], [-1, -1]]:
		var nx: int = tx + d[0]
		var ny: int = ty + d[1]
		if sim.venue.walkable(nx, ny) and _pf[ny * w + nx] < best:
			best = _pf[ny * w + nx]
			bx = nx
			by = ny
	return _dir_to(Vector2i(bx * 256 + 128, by * 256 + 128))
