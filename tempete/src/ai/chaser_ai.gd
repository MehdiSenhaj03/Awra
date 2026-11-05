class_name ChaserAI
extends RefCounted
## A computer chase team: reads the storms like a navigator (where the tornado will be in two minutes, on the
## inflow side, safely ahead), drives the section roads there (A* on the road grid), shoots when the subject is
## good, and drops probes on the projected track. Also Eli's routing advice for the player.

var chase: Chase
var team := 0
var skill := 1.0
var rng: Rng
var route: Array[Vector2i] = []      # world points to follow
var goal := Vector2i(-1, -1)
var _replan := 0
var _photo_cd := 0
var _probe_cd := 0
var _stuck := 0
var _last := Vector2i.ZERO
var _from := Vector2i(-1, -1)

func _init(c: Chase, t: int, sk: float, seed_v: int) -> void:
	chase = c
	team = t
	skill = sk
	rng = Rng.new(seed_v)

func _me() -> Chase.Vehicle:
	return chase.vehicles[team]

## The best target now: a tornado, else the most rotating storm. Returns {x, y, vx, vy, kind} or {}.
func target() -> Dictionary:
	var best := {}
	var score := -1
	for tn in chase.tornadoes:
		if tn.dead:
			continue
		var st := chase._storm(tn.storm)
		if st == null:
			continue
		var s := 5000 + tn.ef * 1000
		if s > score:
			score = s
			best = {"x": tn.x, "y": tn.y, "vx": st.vx, "vy": st.vy, "kind": "tornado", "storm": st}
	for st in chase.storms:
		if st.dead:
			continue
		var s := st.rot() * st.sev
		if st.windows.size() > 0 and st.t < int(st.windows[st.windows.size() - 1][0]):
			s += 1500
		if s > score:
			score = s
			var h := chase.hook(st)
			best = {"x": h.x, "y": h.y, "vx": st.vx, "vy": st.vy, "kind": "meso", "storm": st}
	return best

## Where to wait: ahead of the target along its motion, a little to the right (south-east) of its track.
func intercept(t: Dictionary, lead_s: int, side_m: int) -> Vector2i:
	var vx: int = t.vx
	var vy: int = t.vy
	var px: int = int(t.x) + (vx >> 4) * Chase.FPS * lead_s
	var py: int = int(t.y) + (vy >> 4) * Chase.FPS * lead_s
	var m := Trig.atan2i(vy, vx)
	var o := Trig.rot(0, -side_m * 1000, m)
	return Vector2i(clampi(px + o.x, 300_000, Plains.SIZE - 300_000), clampi(py + o.y, 300_000, Plains.SIZE - 300_000))

# ---- Roads ------------------------------------------------------------------------------------------------------------

func _node_of(p: Vector2i) -> Vector2i:
	return Vector2i(clampi((p.x + Plains.CELL / 2) / Plains.CELL, 1, Plains.N - 1), clampi((p.y + Plains.CELL / 2) / Plains.CELL, 1, Plains.N - 1))

func _edges(n: Vector2i) -> Array:
	var pl := chase.plains
	var out: Array = []
	# Along the vertical road x = n.x: segment j = n.y - 1 (south) and j = n.y (north).
	if n.y + 1 < Plains.N and int(pl.vroad[n.x][n.y]) > 0:
		out.append([Vector2i(n.x, n.y + 1), int(pl.vroad[n.x][n.y])])
	if n.y - 1 >= 1 and int(pl.vroad[n.x][n.y - 1]) > 0:
		out.append([Vector2i(n.x, n.y - 1), int(pl.vroad[n.x][n.y - 1])])
	if n.x + 1 < Plains.N and int(pl.hroad[n.y][n.x]) > 0:
		out.append([Vector2i(n.x + 1, n.y), int(pl.hroad[n.y][n.x])])
	if n.x - 1 >= 1 and int(pl.hroad[n.y][n.x - 1]) > 0:
		out.append([Vector2i(n.x - 1, n.y), int(pl.hroad[n.y][n.x - 1])])
	return out

## A* over crossroads; paved roads cost less, muddy dirt more.
func plan(from: Vector2i, to: Vector2i) -> Array[Vector2i]:
	var a := _node_of(from)
	var b := _node_of(to)
	var open := {a: 0}
	var came := {}
	var g := {a: 0}
	var guard := 0
	while not open.is_empty() and guard < 2000:
		guard += 1
		var cur: Vector2i = open.keys()[0]
		var bf := 1 << 40
		for n: Vector2i in open:
			var f: int = int(open[n])
			if f < bf:
				bf = f
				cur = n
		if cur == b:
			break
		open.erase(cur)
		for e: Array in _edges(cur):
			var nb: Vector2i = e[0]
			var mid := Vector2i((cur.x + nb.x) * Plains.CELL / 2, (cur.y + nb.y) * Plains.CELL / 2)
			var wet := chase.plains.wet_at(mid.x, mid.y)
			var cost := 10 if int(e[1]) == 2 else (14 if wet < 300 else 26)
			var ng: int = int(g[cur]) + cost
			if not g.has(nb) or ng < int(g[nb]):
				g[nb] = ng
				came[nb] = cur
				open[nb] = ng + (absi(nb.x - b.x) + absi(nb.y - b.y)) * 10
	var path: Array[Vector2i] = []
	var n := b
	if not came.has(b) and b != a:
		# Unreachable: go as close as the roads allow.
		n = a
	while n != a and came.has(n):
		path.push_front(Vector2i(n.x * Plains.CELL, n.y * Plains.CELL))
		n = came[n]
	path.push_front(Vector2i(a.x * Plains.CELL, a.y * Plains.CELL))
	# Off the road, first rejoin it the short way (along the nearest road line).
	var px := clampi(roundi(float(from.x) / Plains.CELL), 1, Plains.N - 1) * Plains.CELL
	var py := clampi(roundi(float(from.y) / Plains.CELL), 1, Plains.N - 1) * Plains.CELL
	if absi(from.x - px) < absi(from.y - py):
		path.push_front(Vector2i(px, from.y))
	else:
		path.push_front(Vector2i(from.x, py))
	return path

# ---- Driving ------------------------------------------------------------------------------------------------------------

func update() -> Dictionary:
	var v := _me()
	var inp := {"team": team}
	if not v.alive:
		return inp
	_replan -= 1
	_photo_cd -= 1
	_probe_cd -= 1
	var t := target()
	if not t.is_empty() and _replan <= 0:
		_replan = 240
		var lead := 120 if t.kind == "tornado" else 180
		var side := 1800 if t.kind == "tornado" else 3500
		# With probes left and a tornado down: a little closer to its track (a probe may fit on the way).
		if t.kind == "tornado" and v.probes > 0:
			side = 900
		goal = intercept(t, lead, side)
		route = plan(Vector2i(v.x, v.y), goal)
		_from = Vector2i(v.x, v.y)
		# Do not turn back for a crossroads just passed.
		while route.size() > 1:
			var p0: Vector2i = route[0]
			var near0 := absi(p0.x - v.x) + absi(p0.y - v.y)
			var behind := absi(Trig.adiff(v.a, Trig.atan2i((p0.y - v.y) / 1000, (p0.x - v.x) / 1000))) > 1024
			if near0 < 50_000 or (behind and near0 < 700_000 and absi(v.v) > 3 * Chase.MS):
				route.pop_front()
			else:
				break
	# Follow the route: pure pursuit along the current road line, 70 m ahead.
	var aim := Vector2i(v.x, v.y)
	while route.size() > 0:
		var p: Vector2i = route[0]
		if absi(p.x - v.x) + absi(p.y - v.y) < 50_000:
			_from = p
			route.pop_front()
			continue
		aim = p
		break
	if route.size() > 0 and _from.x >= 0:
		var seg := Vector2(aim - _from)
		var l := seg.length()
		if l > 1.0:
			var dir := seg / l
			var rel := Vector2(v.x - _from.x, v.y - _from.y)
			var along := clampf(rel.dot(dir) + 70_000.0, 0.0, l)
			var cross := absf(rel.x * dir.y - rel.y * dir.x)
			if cross < 120_000.0:
				var q := Vector2(_from) + dir * along
				aim = Vector2i(int(q.x), int(q.y))
	var dx := aim.x - v.x
	var dy := aim.y - v.y
	var dist := absi(dx) + absi(dy)
	var want := Trig.atan2i(dy / 1000, dx / 1000) if dist > 0 else v.a
	var da := Trig.adiff(v.a, want)
	inp.steer = clampi(da / 8, -8, 8)
	# Throttle: slow down before a turning crossroads, for sharp corners, and stop at the goal.
	var slow := absi(da) > 500
	if route.size() >= 2 and dist < 160_000:
		var nxt: Vector2i = route[1]
		var turn := absi(Trig.adiff(want, Trig.atan2i((nxt.y - aim.y) / 1000, (nxt.x - aim.x) / 1000)))
		if turn > 300 and v.v > 9 * Chase.MS:
			slow = true
	var near_goal := route.size() <= 1 and absi(goal.x - v.x) + absi(goal.y - v.y) < 900_000
	inp.throttle = not near_goal and (not slow or v.v < 8 * Chase.MS)
	inp.brake = near_goal or (absi(da) > 900 and v.v > 6 * Chase.MS) or (slow and v.v > 11 * Chase.MS)
	# Unstick: reverse a little.
	if absi(v.v) < Chase.MS and inp.throttle:
		_stuck += 1
	else:
		_stuck = 0
	_last = Vector2i(v.x, v.y)
	if _stuck > 120:
		inp.throttle = false
		inp.brake = true
		inp.steer = 8
		if _stuck > 200:
			_stuck = 0
			_replan = 0
	# Danger: hail core or too close to a tornado — leave to the south-east.
	if chase.hail_at(v.x, v.y) > 40 or _tornado_close(v, 1_200_000):
		_replan = 0
	# Photographs.
	if _photo_cd <= 0 and v.film > 0 and not t.is_empty():
		var tx: int = t.x
		var ty: int = t.y
		var d := Trig.isqrt(((tx - v.x) / 1000) * ((tx - v.x) / 1000) + ((ty - v.y) / 1000) * ((ty - v.y) / 1000)) * 1000
		var good: bool = (t.kind == "tornado" and d < 6_000_000) or (t.kind == "meso" and d > 3_500_000 and d < 13_000_000 and (t.storm as Chase.Storm).rot() > 650)
		if good and absi(v.v) < 12 * Chase.MS:
			var yaw := Trig.atan2i((ty - v.y) / 1000, (tx - v.x) / 1000) + rng.rint(-30, 30) * int(2.0 - skill)
			var elev := Trig.atan2i(Chase.CLOUD_BASE / 2 if t.kind == "tornado" else Chase.CLOUD_BASE * 3 / 2, maxi(1, d))
			var zoom := clampi(int(160.0 * 6_000_000 / maxi(1, d)), 100, 500) if t.kind == "tornado" else 100
			inp.photo = {"yaw": yaw, "pitch": elev, "zoom": zoom}
			_photo_cd = int(900 / skill)
	# Probes: on the projected track of a tornado, well ahead of it — stop and drop one.
	if _probe_cd <= 0 and v.probes > 0:
		for tn in chase.tornadoes:
			if tn.dead:
				continue
			var st := chase._storm(tn.storm)
			if st == null:
				continue
			var rel := Trig.rot(v.x - tn.x, v.y - tn.y, -st.motion())
			if rel.x > 700_000 and rel.x < 3_500_000 and absi(rel.y) < 250_000 + tn.width / 2:
				inp.throttle = false
				inp.brake = true
				if absi(v.v) < 2 * Chase.MS:
					inp.probe = true
					_probe_cd = 300
					_replan = 0
				break
	return inp

func _tornado_close(v: Chase.Vehicle, r: int) -> bool:
	for tn in chase.tornadoes:
		if not tn.dead and absi(tn.x - v.x) < r and absi(tn.y - v.y) < r:
			return true
	return false
