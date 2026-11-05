class_name Planner
extends RefCounted
## Turns a written plan into the input bytes a player would press, frame by frame, inside the live
## simulation. Used to author and verify the campaign's solutions, and for the title-screen demo.
##
## Plan steps: ["go", x, y] walk to a tile (opens manual doors on the way) · ["run", x, y] same, sprinting ·
## ["at", frame] wait until that loop frame · ["wait", n] · ["use"] press interact · ["hold", n] hold interact ·
## ["throw", dir] face dir (1..8) and throw the coin · ["face", dir]

var plan: Array
var i := 0
var t := 0
var slot := 0
var _route: Array = []
var _goal := Vector2i(-1, -1)
var _stuck := 0
## Past echoes' recordings (for looking ahead when sneaking).
var recs: Array = []
const LOOK := 40

func _init(p: Array, s: int) -> void:
	plan = p
	slot = s

func done() -> bool:
	return i >= plan.size()

static func _pass(sim: Sim, x: int, y: int) -> bool:
	var tt := sim.tile_at(x, y)
	if tt == Sim.T_WALL or tt == Sim.T_VOID or sim.furn[y * sim.w + x] == 1:
		return false
	var di: Variant = sim.door_at.get(y * sim.w + x)
	if di != null:
		var v: Sim.Dev = sim.devs[di]
		return v.on or v.k == "door"
	return true

static func path(sim: Sim, sx: int, sy: int, gx: int, gy: int) -> Array:
	if sx == gx and sy == gy:
		return []
	var w := sim.w
	var prev := PackedInt32Array()
	prev.resize(w * sim.h)
	prev.fill(-1)
	var q := PackedInt32Array([sy * w + sx])
	prev[sy * w + sx] = sy * w + sx
	var head := 0
	while head < q.size():
		var c := q[head]
		head += 1
		if c == gy * w + gx:
			break
		for k in 4:
			var nx: int = c % w + [1, 0, -1, 0][k]
			var ny: int = c / w + [0, 1, 0, -1][k]
			if nx < 0 or ny < 0 or nx >= w or ny >= sim.h:
				continue
			var n: int = ny * w + nx
			if prev[n] != -1 or not _pass(sim, nx, ny):
				continue
			prev[n] = c
			q.append(n)
	if prev[gy * w + gx] == -1:
		return []
	var out: Array = []
	var c := gy * w + gx
	while c != sy * w + sx:
		out.push_front(Vector2i(c % w, c / w))
		c = prev[c]
	return out

static func dir_to(dx: int, dy: int) -> int:
	var sx := 0 if absi(dx) <= 6 else signi(dx)
	var sy := 0 if absi(dy) <= 6 else signi(dy)
	for d in range(1, 9):
		if Fx.DIR_X[d] == sx and Fx.DIR_Y[d] == sy:
			return d
	return 0

## The input byte for this frame.
func next(sim: Sim) -> int:
	if i >= plan.size():
		return 0
	var st: Array = plan[i]
	var e: Sim.Echo = sim.echoes[slot]
	match st[0]:
		"go", "run", "sneak":
			var g := Vector2i(int(st[1]), int(st[2]))
			var cx := g.x * 256 + 128
			var cy := g.y * 256 + 128
			if absi(e.x - cx) <= 8 and absi(e.y - cy) <= 8:
				i += 1
				t = 0
				_route.clear()
				return next(sim)
			var tx := e.x >> 8
			var ty := e.y >> 8
			if _goal != g or _route.is_empty() or _stuck > 30:
				_route = path(sim, tx, ty, g.x, g.y)
				_goal = g
				_stuck = 0
			while not _route.is_empty() and (_route[0] as Vector2i) == Vector2i(tx, ty) and (absi(e.x - (tx * 256 + 128)) < 40 and absi(e.y - (ty * 256 + 128)) < 40):
				_route.pop_front()
			var nt := g if _route.is_empty() else (_route[0] as Vector2i)
			# A closed manual door ahead: open it.
			var di: Variant = sim.door_at.get(nt.y * sim.w + nt.x)
			if di != null and not (sim.devs[di] as Sim.Dev).on and absi(nt.x - tx) + absi(nt.y - ty) <= 1:
				_stuck += 1
				return Sim.IN_USE if t % 2 == 0 else 0
			var tcx := nt.x * 256 + 128
			var tcy := nt.y * 256 + 128
			# Align on the corridor before turning a corner.
			var dx := tcx - e.x
			var dy := tcy - e.y
			if absi(dx) > 6 and absi(dy) > 6:
				if absi(dx) < absi(dy):
					dy = 0
				else:
					dx = 0
			t += 1
			var d := dir_to(dx, dy)
			if d == 0:
				_stuck += 1
			var mv := d | (Sim.IN_SPRINT if st[0] == "run" else 0)
			if st[0] == "sneak" and not _safe(sim, mv):
				if _safe(sim, 0):
					return 0
				# Back off the way we came.
				var back := dir_to(-dx, -dy)
				if back != 0 and _safe(sim, back):
					return back
			return mv
		"chase":
			# Walk up behind a guard and knock him out.
			var g: Sim.Guard = sim.guards[int(st[1])]
			if g.mode == 3:
				i += 1
				return next(sim)
			var dx := g.x - e.x
			var dy := g.y - e.y
			if dx * dx + dy * dy < (Sim.TAKEDOWN - 30) * (Sim.TAKEDOWN - 30) and -dx * Fx.cosa(g.face) - dy * Fx.sina(g.face) <= 0:
				t += 1
				return Sim.IN_USE if t % 2 == 0 else 0
			var cm := dir_to(dx, dy)
			if not _safe(sim, cm):
				if _safe(sim, 0):
					return 0
				var back := dir_to(-dx, -dy)
				if _safe(sim, back):
					return back
			return cm
		"at":
			if sim.frame + 1 >= int(st[1]):
				i += 1
			return 0
		"wait":
			t += 1
			if t >= int(st[1]):
				t = 0
				i += 1
			return 0
		"use":
			i += 1
			t = 0
			return Sim.IN_USE
		"hold":
			t += 1
			if t > int(st[1]):
				t = 0
				i += 1
				return 0
			return Sim.IN_USE
		"throw":
			i += 1
			return int(st[1]) | Sim.IN_THROW
		"face":
			i += 1
			return int(st[1])
	i += 1
	return 0

## Looks ahead: would pressing `b` for the next frames get this echo noticed?
func _safe(sim: Sim, b: int) -> bool:
	var s := sim.clone()
	var inp := PackedByteArray()
	inp.resize(s.echoes.size())
	for f in LOOK:
		for j in s.echoes.size():
			if j == slot:
				inp[j] = b if f < 12 else 0
			elif j < recs.size() and s.frame < (recs[j] as PackedByteArray).size():
				inp[j] = (recs[j] as PackedByteArray)[s.frame]
			else:
				inp[j] = 0
		s.step(inp)
		if s.over == "alarm":
			return false
		for g in s.guards:
			if g.seen == slot and g.sus > 0:
				return false
		for v in s.devs:
			if v.t == "cam" and v.sus > 0:
				return false
	return true

## Plays the plans one loop after another, like a player would (echo k is recorded while echoes 0..k-1
## replay). Returns {"win": bool, "frame": int, "rec": [...], "over": ..., "sim": Sim}.
static func solve(lv: Level, plans: Array) -> Dictionary:
	var recs: Array = []
	var last: Sim = null
	for k in plans.size():
		var s := Sim.new(lv)
		s.reset_echoes(k + 1)
		var p := Planner.new(plans[k], k)
		p.recs = recs
		var r := PackedByteArray()
		r.resize(s.loop_frames)
		var inp := PackedByteArray()
		inp.resize(k + 1)
		while s.frame < s.loop_frames and s.over == "":
			for j in k:
				inp[j] = (recs[j] as PackedByteArray)[s.frame]
			var b := p.next(s)
			r[s.frame] = b
			inp[k] = b
			s.step(inp)
		recs.append(r)
		last = s
	var fin := Run.simulate(lv, recs)
	return {"win": fin.over == "win", "frame": fin.win_frame, "rec": recs, "over": fin.over, "sim": fin, "alarm_by": fin.alarm_by, "gems": fin.gems}
