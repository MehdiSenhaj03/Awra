class_name Sim
extends RefCounted
## The vault simulation: one loop of the world, 60 frames per second, integers only.
## Feed one input byte per echo per frame (see Input bits below); read the state to draw it.

# Input byte: bits 0-3 direction (0 none, 1..8 = E SE S SW W NW N NE), bit 4 sprint, bit 5 interact held,
# bit 6 throw.
const IN_SPRINT := 16
const IN_USE := 32
const IN_THROW := 64

const R_ECHO := 70
const WALK := 16
const RUN := 24
const GUARD_SPEED := 13
const GUARD_RUN := 18
const REACH := 370
const TAKEDOWN := 240
const DOWN_FRAMES := 1200
const HACK_FRAMES := 45
const CRACK_FRAMES := 90
const SUS_ALARM := 30
const CAM_SUS_ALARM := 40
const BODY_ALARM := 45
const KEY_OPEN := 240
const DARK_RANGE := 384

# ---- State --------------------------------------------------------------------------------------------------------

class Echo:
	var x := 0
	var y := 0
	var face := 0
	var item := ""
	var coin := true
	var use_prev := false
	var hold := 0
	var hold_target := -1
	var caught := false
	var desync := false
	var moving := false
	var sprint := false
	var noise_t := 0
	func clone() -> Echo:
		var e := Echo.new()
		e.x = x; e.y = y; e.face = face; e.item = item; e.coin = coin; e.use_prev = use_prev
		e.hold = hold; e.hold_target = hold_target; e.caught = caught; e.desync = desync
		e.moving = moving; e.sprint = sprint; e.noise_t = noise_t
		return e

class Guard:
	var x := 0
	var y := 0
	var face := 0
	var path: Array = []
	var wp := 0
	var wait := 60
	var wait_t := 0
	var range_t := 6
	var mode := 0 # 0 patrol, 1 investigate, 2 look around, 3 down
	var route: Array = []
	var sus := 0
	var body := 0
	var down_t := 0
	var look_t := 0
	var look_n := 0
	var spot_x := 0
	var spot_y := 0
	var seen := -1
	func clone() -> Guard:
		var g := Guard.new()
		g.x = x; g.y = y; g.face = face; g.path = path; g.wp = wp; g.wait = wait; g.wait_t = wait_t
		g.range_t = range_t; g.mode = mode; g.route = route.duplicate(); g.sus = sus; g.body = body
		g.down_t = down_t; g.look_t = look_t; g.look_n = look_n; g.spot_x = spot_x; g.spot_y = spot_y
		g.seen = seen
		return g

class Dev:
	var t := ""
	var x := 0
	var y := 0
	var k := ""
	var ch := 0
	var color := 0
	var inv := false
	var dur := 0
	var t1 := 0
	var t2 := 0
	var dir := 0
	var len := 1
	var w := 1
	var h := 1
	var a0 := 0
	var a1 := 0
	var spd := 6
	var rng := 6
	# Runtime.
	var on := false
	var timer := 0
	var ang := 0
	var adir := 1
	var sus := 0
	var gone := false
	var pulled := 0
	func clone() -> Dev:
		var d := Dev.new()
		d.t = t; d.x = x; d.y = y; d.k = k; d.ch = ch; d.color = color; d.inv = inv; d.dur = dur
		d.t1 = t1; d.t2 = t2; d.dir = dir; d.len = len; d.w = w; d.h = h; d.a0 = a0; d.a1 = a1
		d.spd = spd; d.rng = rng; d.on = on; d.timer = timer; d.ang = ang; d.adir = adir; d.sus = sus
		d.gone = gone; d.pulled = pulled
		return d

class Coin:
	var x := 0
	var y := 0
	var tx := 0
	var ty := 0
	var t := 0
	func clone() -> Coin:
		var c := Coin.new()
		c.x = x; c.y = y; c.tx = tx; c.ty = ty; c.t = t
		return c

var level: Level
var w := 0
var h := 0
var tiles := PackedByteArray() # 0 floor, 1 wall, 2 glass, 3 exit, 4 void
var frame := 0
var loop_frames := 2400
var echoes: Array[Echo] = []
var guards: Array[Guard] = []
var devs: Array[Dev] = []
var coins: Array[Coin] = []
var chan := PackedByteArray()
var door_at: Dictionary = {}
## Tiles taken by furniture (safe, terminal, pedestals, camera poles).
var furn := PackedByteArray()
var events: Array[Dictionary] = []
## "", "alarm" or "win".
var over := ""
var alarm_by := -1
var alarm_x := 0
var alarm_y := 0
var gems := 0
var gems_total := 0
var win_frame := -1

const T_FLOOR := 0
const T_WALL := 1
const T_GLASS := 2
const T_EXIT := 3
const T_VOID := 4

func _init(lv: Level = null) -> void:
	if lv:
		load_level(lv)

func load_level(lv: Level) -> void:
	level = lv
	var d := lv.data
	w = int(d.w)
	h = int(d.h)
	loop_frames = int(d.get("loop", 2400))
	tiles.resize(w * h)
	for y in h:
		var row: String = d.rows[y]
		for x in w:
			var c := row[x] if x < row.length() else " "
			tiles[y * w + x] = {".": T_FLOOR, "#": T_WALL, "g": T_GLASS, "E": T_EXIT}.get(c, T_VOID)
	chan.resize(10)
	devs.clear()
	door_at.clear()
	gems_total = 0
	furn.resize(w * h)
	furn.fill(0)
	for o: Dictionary in d.objs:
		var v := Dev.new()
		v.t = o.t
		v.x = int(o.x)
		v.y = int(o.y)
		v.k = o.get("k", "")
		v.ch = int(o.get("ch", 0))
		v.color = int(o.get("color", 0))
		v.inv = bool(o.get("inv", false))
		v.dur = int(o.get("dur", 300))
		v.t1 = int(o.get("t1", 0))
		v.t2 = int(o.get("t2", 99999))
		v.dir = int(o.get("dir", 0))
		v.len = int(o.get("len", 1))
		v.w = int(o.get("w", 1))
		v.h = int(o.get("h", 1))
		v.a0 = int(o.get("a0", 0))
		v.a1 = int(o.get("a1", 64))
		v.spd = int(o.get("speed", 6))
		v.rng = int(o.get("range", 6))
		v.ang = v.a0 * 16
		if v.t == "door":
			door_at[v.y * w + v.x] = devs.size()
		if v.t in ["safe", "term", "button", "lever", "vlever", "cam"]:
			furn[v.y * w + v.x] = 1
		if v.t == "gem":
			gems_total += 1
		devs.append(v)
	guards.clear()
	for gd: Dictionary in d.guards:
		var g := Guard.new()
		g.path = gd.path
		g.wait = int(gd.get("wait", 60))
		g.range_t = int(gd.get("range", 6))
		var p0: Array = g.path[0]
		g.x = int(p0[0]) * 256 + 128
		g.y = int(p0[1]) * 256 + 128
		g.face = int(gd.get("dir", 0))
		g.wp = 1 % g.path.size()
		g.wait_t = 0
		guards.append(g)
	reset_echoes(0)

## Back to frame 0 with n echoes at the spawn point.
func reset_echoes(n: int) -> void:
	echoes.clear()
	var sp: Array = level.data.spawn
	for i in n:
		var e := Echo.new()
		e.x = int(sp[0]) * 256 + 128
		e.y = int(sp[1]) * 256 + 128
		e.face = 192
		echoes.append(e)

func clone() -> Sim:
	var s := Sim.new()
	s.level = level
	s.w = w; s.h = h; s.tiles = tiles; s.frame = frame; s.loop_frames = loop_frames
	for e in echoes: s.echoes.append(e.clone())
	for g in guards: s.guards.append(g.clone())
	for v in devs: s.devs.append(v.clone())
	for c in coins: s.coins.append(c.clone())
	s.chan = chan.duplicate(); s.door_at = door_at; s.furn = furn; s.over = over; s.alarm_by = alarm_by
	s.alarm_x = alarm_x; s.alarm_y = alarm_y; s.gems = gems; s.gems_total = gems_total; s.win_frame = win_frame
	return s

# ---- Map queries --------------------------------------------------------------------------------------------------

func tile_at(tx: int, ty: int) -> int:
	if tx < 0 or ty < 0 or tx >= w or ty >= h:
		return T_VOID
	return tiles[ty * w + tx]

func door_closed(tx: int, ty: int) -> bool:
	var i: Variant = door_at.get(ty * w + tx)
	return i != null and not devs[i].on

## Solid for walkers: walls, void, closed doors.
func solid(tx: int, ty: int) -> bool:
	var t := tile_at(tx, ty)
	return t == T_WALL or t == T_VOID or door_closed(tx, ty) or furn[ty * w + tx] == 1

func blocks_sight(tx: int, ty: int) -> bool:
	var t := tile_at(tx, ty)
	return t == T_WALL or door_closed(tx, ty)

func dark_at(px: int, py: int) -> bool:
	var tx := px >> 8
	var ty := py >> 8
	for v in devs:
		if v.t == "dark" and tx >= v.x and ty >= v.y and tx < v.x + v.w and ty < v.y + v.h:
			var lit := v.ch != 0 and chan[v.ch] == 1
			if v.inv:
				lit = not lit
			if not lit:
				return true
	return false

func los(x0: int, y0: int, x1: int, y1: int) -> bool:
	var dx := x1 - x0
	var dy := y1 - y0
	var n: int = maxi(absi(dx), absi(dy)) / 64 + 1
	for i in range(1, n):
		var px := x0 + dx * i / n
		var py := y0 + dy * i / n
		if blocks_sight(px >> 8, py >> 8):
			return false
	return true

func _move(px: int, py: int, dx: int, dy: int, r: int) -> Vector2i:
	var nx := px + dx
	if dx != 0:
		var edge := nx + (r if dx > 0 else -r)
		var tx := edge >> 8
		for ty in range((py - r) >> 8, ((py + r) >> 8) + 1):
			if solid(tx, ty):
				nx = (tx * 256 - r - 1) if dx > 0 else ((tx + 1) * 256 + r)
				break
	var ny := py + dy
	if dy != 0:
		var edge := ny + (r if dy > 0 else -r)
		var ty := edge >> 8
		for tx in range((nx - r) >> 8, ((nx + r) >> 8) + 1):
			if solid(tx, ty):
				ny = (ty * 256 - r - 1) if dy > 0 else ((ty + 1) * 256 + r)
				break
	return Vector2i(nx, ny)

# ---- Frame --------------------------------------------------------------------------------------------------------

## One frame. inputs[i] drives echo i.
func step(inputs: PackedByteArray) -> void:
	events.clear()
	if over != "":
		return
	for i in echoes.size():
		_echo(i, inputs[i] if i < inputs.size() else 0)
	_devices()
	for gi in guards.size():
		_guard(gi)
	_cams()
	_coins()
	_detect()
	_win()
	frame += 1

func _ev(t: String, x: int, y: int, extra: Dictionary = {}) -> void:
	var e := {"t": t, "x": x, "y": y}
	e.merge(extra)
	events.append(e)

func _echo(i: int, inp: int) -> void:
	var e := echoes[i]
	if e.caught:
		return
	var d := inp & 15
	var use := (inp & IN_USE) != 0
	var pressed := use and not e.use_prev
	e.use_prev = use
	e.sprint = (inp & IN_SPRINT) != 0
	e.moving = d != 0
	if d != 0 and d <= 8:
		var sp := RUN if e.sprint else WALK
		var mx := Fx.DIR_X[d]
		var my := Fx.DIR_Y[d]
		if mx != 0 and my != 0:
			sp = sp * 181 / 256
		var p := _move(e.x, e.y, mx * sp, my * sp, R_ECHO)
		e.x = p.x
		e.y = p.y
		e.face = Fx.DIR_A[d]
		# Noise: running anywhere, walking on glass.
		var on_glass := tile_at(e.x >> 8, e.y >> 8) == T_GLASS
		if e.sprint or on_glass:
			e.noise_t -= 1
			if e.noise_t <= 0:
				e.noise_t = 10 if e.sprint and on_glass else 20
				_noise(e.x, e.y, (5 if e.sprint and on_glass else 3) * 256, i)
	# Gems are picked up by walking over them.
	for v in devs:
		if v.t == "gem" and not v.gone and absi(v.x * 256 + 128 - e.x) < 160 and absi(v.y * 256 + 128 - e.y) < 160:
			v.gone = true
			gems += 1
			_ev("gem", e.x, e.y, {"echo": i})
	if (inp & IN_THROW) != 0 and e.coin:
		e.coin = false
		var c := Coin.new()
		c.x = e.x
		c.y = e.y
		var fx := Fx.cosa(e.face)
		var fy := Fx.sina(e.face)
		var tx := e.x
		var ty := e.y
		for s in range(1, 21):
			var nx := e.x + fx * s * 64 / 1024
			var ny := e.y + fy * s * 64 / 1024
			if solid(nx >> 8, ny >> 8):
				break
			tx = nx
			ty = ny
		c.tx = tx
		c.ty = ty
		c.t = 14
		coins.append(c)
		_ev("throw", e.x, e.y, {"echo": i})
	# Hold actions: hack a terminal, crack the safe.
	if use:
		var ti := _nearest(e, ["term", "safe"])
		if ti >= 0 and (devs[ti].t == "term" or e.item == ""):
			if ti != e.hold_target:
				e.hold_target = ti
				e.hold = 0
			e.hold += 1
			var v := devs[ti]
			if v.t == "term" and e.hold == HACK_FRAMES:
				v.timer = v.dur
				_ev("hack", v.x * 256 + 128, v.y * 256 + 128, {"echo": i})
			elif v.t == "safe" and e.hold == CRACK_FRAMES and not v.gone:
				v.gone = true
				e.item = "loot"
				_ev("crack", v.x * 256 + 128, v.y * 256 + 128, {"echo": i})
		else:
			e.hold = 0
			e.hold_target = -1
	else:
		e.hold = 0
		e.hold_target = -1
	if pressed:
		_use(i, e)

func _nearest(e: Echo, kinds: Array) -> int:
	var best := -1
	var bd := REACH * REACH
	for vi in devs.size():
		var v := devs[vi]
		if v.gone or not kinds.has(v.t):
			continue
		var dx := v.x * 256 + 128 - e.x
		var dy := v.y * 256 + 128 - e.y
		var d2 := dx * dx + dy * dy
		if d2 < bd:
			bd = d2
			best = vi
	return best

func _use(i: int, e: Echo) -> void:
	# 1. Takedown: a guard within reach that does not face us.
	for gi in guards.size():
		var g := guards[gi]
		if g.mode == 3:
			continue
		var dx := e.x - g.x
		var dy := e.y - g.y
		if dx * dx + dy * dy < TAKEDOWN * TAKEDOWN:
			var dot := dx * Fx.cosa(g.face) + dy * Fx.sina(g.face)
			if dot <= 0:
				g.mode = 3
				g.down_t = DOWN_FRAMES
				g.sus = 0
				g.route.clear()
				_ev("takedown", g.x, g.y, {"echo": i, "guard": gi})
				return
	# 2. Pick up a card lying on the floor (hands empty).
	if e.item == "":
		var ci := _nearest(e, ["card", "loot"])
		if ci >= 0:
			var v := devs[ci]
			v.gone = true
			e.item = ("card:%d" % v.color) if v.t == "card" else "loot"
			_ev("pick", e.x, e.y, {"echo": i, "item": e.item})
			return
	# 3. Devices.
	var di := _nearest(e, ["lever", "button", "vlever", "door"])
	if di >= 0:
		var v := devs[di]
		match v.t:
			"lever":
				v.on = not v.on
				_ev("lever", v.x * 256 + 128, v.y * 256 + 128, {"on": v.on})
				return
			"button":
				v.timer = v.dur
				_ev("button", v.x * 256 + 128, v.y * 256 + 128)
				return
			"vlever":
				v.pulled = 20
				_ev("vlever", v.x * 256 + 128, v.y * 256 + 128)
				return
			"door":
				if v.k == "door":
					if v.on and _occupied(v.x, v.y):
						return
					v.on = not v.on
					_ev("door", v.x * 256 + 128, v.y * 256 + 128, {"open": v.on})
					return
				if v.k == "key" and e.item == "card:%d" % v.color:
					v.timer = KEY_OPEN
					if not v.on:
						v.on = true
						_ev("door", v.x * 256 + 128, v.y * 256 + 128, {"open": true})
					return
	# 4. Drop what we carry.
	if e.item != "":
		var v := Dev.new()
		v.t = "card" if e.item.begins_with("card:") else "loot"
		v.color = int(e.item.substr(5)) if v.t == "card" else 0
		v.x = e.x >> 8
		v.y = e.y >> 8
		devs.append(v)
		_ev("drop", e.x, e.y, {"echo": i, "item": e.item})
		e.item = ""

func _occupied(tx: int, ty: int) -> bool:
	for e in echoes:
		if not e.caught and absi((e.x >> 8) - tx) + absi((e.y >> 8) - ty) == 0:
			return true
		if not e.caught and (e.x - R_ECHO) >> 8 <= tx and (e.x + R_ECHO) >> 8 >= tx and (e.y - R_ECHO) >> 8 <= ty and (e.y + R_ECHO) >> 8 >= ty:
			return true
	for g in guards:
		if (g.x >> 8) == tx and (g.y >> 8) == ty:
			return true
	return false

func _devices() -> void:
	for c in 10:
		chan[c] = 0
	var vault_pulled: Dictionary = {}
	var vault_total: Dictionary = {}
	for v in devs:
		match v.t:
			"plate":
				var pressed := false
				for e in echoes:
					if not e.caught and (e.x >> 8) == v.x and (e.y >> 8) == v.y:
						pressed = true
				for g in guards:
					if (g.x >> 8) == v.x and (g.y >> 8) == v.y:
						pressed = true
				if pressed != v.on:
					_ev("plate", v.x * 256 + 128, v.y * 256 + 128, {"on": pressed})
				v.on = pressed
				if pressed and v.ch > 0:
					chan[v.ch] = 1
			"lever":
				if v.on and v.ch > 0:
					chan[v.ch] = 1
			"button", "term":
				if v.timer > 0:
					v.timer -= 1
					if v.ch > 0:
						chan[v.ch] = 1
			"vlever":
				vault_total[v.ch] = int(vault_total.get(v.ch, 0)) + 1
				if v.pulled > 0:
					v.pulled -= 1
					vault_pulled[v.ch] = int(vault_pulled.get(v.ch, 0)) + 1
	for v in devs:
		if v.t != "door":
			continue
		var want := v.on
		match v.k:
			"chan":
				want = (v.ch > 0 and chan[v.ch] == 1) != v.inv
			"time":
				want = frame >= v.t1 and frame < v.t2
			"key":
				if v.timer > 0:
					v.timer -= 1
				want = v.timer > 0
			"vault":
				if not v.on and int(vault_total.get(v.ch, 0)) > 0 and int(vault_pulled.get(v.ch, 0)) >= int(vault_total.get(v.ch, 0)):
					want = true
				elif v.on:
					want = true
		if want != v.on:
			if not want and _occupied(v.x, v.y):
				continue
			v.on = want
			_ev("door", v.x * 256 + 128, v.y * 256 + 128, {"open": want, "k": v.k})

# ---- Guards -------------------------------------------------------------------------------------------------------

func _guard_pass(tx: int, ty: int) -> bool:
	var t := tile_at(tx, ty)
	if t == T_WALL or t == T_VOID or furn[ty * w + tx] == 1:
		return false
	var i: Variant = door_at.get(ty * w + tx)
	if i != null:
		var v := devs[i]
		return v.on or v.k == "door"
	return true

## Breadth-first route between tiles (4-neighbour), as a list of tile coordinates (start excluded).
func route(sx: int, sy: int, gx: int, gy: int) -> Array:
	if sx == gx and sy == gy:
		return []
	var prev := PackedInt32Array()
	prev.resize(w * h)
	prev.fill(-1)
	var q := PackedInt32Array([sy * w + sx])
	prev[sy * w + sx] = sy * w + sx
	var head := 0
	var goal := gy * w + gx
	var found := false
	while head < q.size():
		var c := q[head]
		head += 1
		if c == goal:
			found = true
			break
		var cx := c % w
		var cy := c / w
		for k in 4:
			var nx: int = cx + [1, 0, -1, 0][k]
			var ny: int = cy + [0, 1, 0, -1][k]
			if nx < 0 or ny < 0 or nx >= w or ny >= h:
				continue
			var n: int = ny * w + nx
			if prev[n] != -1 or not _guard_pass(nx, ny):
				continue
			prev[n] = c
			q.append(n)
	if not found:
		return []
	var out: Array = []
	var c := goal
	while c != sy * w + sx:
		out.push_front([c % w, c / w])
		c = prev[c]
	return out

func _turn(g: Guard, target: int, rate: int) -> void:
	var d := Fx.adiff(g.face, target)
	g.face = (g.face + clampi(d, -rate, rate)) & 255

## Walks towards a tile centre; returns true when reached.
func _walk(g: Guard, tx: int, ty: int, sp: int) -> bool:
	var cx := tx * 256 + 128
	var cy := ty * 256 + 128
	var dx := cx - g.x
	var dy := cy - g.y
	var dist := Fx.isqrt(dx * dx + dy * dy)
	if dist <= sp:
		g.x = cx
		g.y = cy
		return true
	# Manual doors open for guards.
	var nx := (g.x + dx * sp / dist) >> 8
	var ny := (g.y + dy * sp / dist) >> 8
	var di: Variant = door_at.get(ny * w + nx)
	if di != null and not devs[di].on and devs[di].k == "door":
		devs[di].on = true
		_ev("door", devs[di].x * 256 + 128, devs[di].y * 256 + 128, {"open": true, "k": "door"})
	g.x += dx * sp / dist
	g.y += dy * sp / dist
	_turn(g, Fx.angle(dx, dy), 14)
	return false

func _follow(g: Guard, sp: int) -> bool:
	if g.route.is_empty():
		return true
	var nxt: Array = g.route[0]
	if not _guard_pass(nxt[0], nxt[1]):
		return true
	if _walk(g, nxt[0], nxt[1], sp):
		g.route.pop_front()
	return g.route.is_empty()

func _guard(gi: int) -> void:
	var g := guards[gi]
	if g.mode == 3:
		g.down_t -= 1
		if g.down_t <= 0:
			g.mode = 0
			g.route.clear()
			_ev("wake", g.x, g.y, {"guard": gi})
		return
	# Something suspicious in sight: stop and stare.
	if g.sus > 0 and g.seen >= 0:
		_turn(g, Fx.angle(g.spot_x - g.x, g.spot_y - g.y), 10)
		return
	match g.mode:
		0:
			if g.path.size() < 2:
				var home: Array = g.path[0]
				if (g.x >> 8) != int(home[0]) or (g.y >> 8) != int(home[1]):
					if g.route.is_empty():
						g.route = route(g.x >> 8, g.y >> 8, home[0], home[1])
					_follow(g, GUARD_SPEED)
				elif absi(g.x - int(home[0]) * 256 - 128) > 4 or absi(g.y - int(home[1]) * 256 - 128) > 4:
					_walk(g, home[0], home[1], GUARD_SPEED)
				else:
					_turn(g, int(level.data.guards[gi].get("dir", 0)), 6)
				return
			if g.wait_t > 0:
				g.wait_t -= 1
				var nxt: Array = g.path[g.wp]
				_turn(g, Fx.angle(int(nxt[0]) * 256 + 128 - g.x, int(nxt[1]) * 256 + 128 - g.y), 6)
				return
			var target: Array = g.path[g.wp]
			if g.route.is_empty():
				g.route = route(g.x >> 8, g.y >> 8, target[0], target[1])
				if g.route.is_empty():
					g.wp = (g.wp + 1) % g.path.size()
					g.wait_t = g.wait
					return
			if _follow(g, GUARD_SPEED):
				g.wp = (g.wp + 1) % g.path.size()
				g.wait_t = g.wait
		1:
			if _follow(g, GUARD_RUN):
				g.mode = 2
				g.look_t = 40
				g.look_n = 0
		2:
			g.look_t -= 1
			_turn(g, (g.face + 64) & 255 if g.look_n % 2 == 0 else (g.face - 64) & 255, 4)
			if g.look_t <= 0:
				g.look_n += 1
				g.look_t = 40
				if g.look_n >= 3:
					g.mode = 0
					g.route.clear()

func _noise(x: int, y: int, r: int, _who: int) -> void:
	_ev("noise", x, y, {"r": r})
	for g in guards:
		if g.mode == 3:
			continue
		var dx := x - g.x
		var dy := y - g.y
		if dx * dx + dy * dy <= r * r:
			var rt := route(g.x >> 8, g.y >> 8, x >> 8, y >> 8)
			if rt.is_empty() and ((g.x >> 8) != (x >> 8) or (g.y >> 8) != (y >> 8)):
				continue
			g.mode = 1
			g.route = rt
			_ev("alert", g.x, g.y)

func _cams() -> void:
	for v in devs:
		if v.t != "cam":
			continue
		if v.a0 == v.a1:
			continue
		v.ang += v.spd * v.adir
		var lo := v.a0 * 16
		var hi := v.a1 * 16
		if hi < lo:
			hi += 256 * 16
		var rel := v.ang
		if rel >= hi:
			v.ang = hi
			v.adir = -1
		elif rel <= lo:
			v.ang = lo
			v.adir = 1

func _coins() -> void:
	var i := coins.size() - 1
	while i >= 0:
		var c := coins[i]
		c.t -= 1
		if c.t <= 0:
			_noise(c.tx, c.ty, 10 * 256, -1)
			_ev("coin", c.tx, c.ty)
			coins.remove_at(i)
		i -= 1

# ---- Detection ----------------------------------------------------------------------------------------------------

## Can a viewer at (x, y) facing `face` with a cone of half-angle cosine `cosk` (×1024) see the point?
func sees(x: int, y: int, face: int, cosk: int, rng: int, px: int, py: int) -> bool:
	var dx := px - x
	var dy := py - y
	var d2 := dx * dx + dy * dy
	if dark_at(px, py):
		rng = mini(rng, DARK_RANGE)
	if d2 > rng * rng:
		return false
	if d2 > 150 * 150:
		var dot := dx * Fx.cosa(face) + dy * Fx.sina(face)
		if dot <= 0 or dot * dot < d2 * cosk * cosk:
			return false
	return los(x, y, px, py)

func _alarm(by: int, x: int, y: int) -> void:
	if over != "":
		return
	over = "alarm"
	alarm_by = by
	alarm_x = x
	alarm_y = y
	_ev("alarm", x, y, {"by": by})

func _detect() -> void:
	for gi in guards.size():
		var g := guards[gi]
		if g.mode == 3:
			continue
		var best := -1
		var bd := 1 << 40
		for ei in echoes.size():
			var e := echoes[ei]
			if e.caught:
				continue
			var dx := e.x - g.x
			var dy := e.y - g.y
			var d2 := dx * dx + dy * dy
			if d2 < 110 * 110:
				_alarm(gi, e.x, e.y)
				return
			if sees(g.x, g.y, g.face, 724, g.range_t * 256, e.x, e.y) and d2 < bd:
				bd = d2
				best = ei
		if best >= 0:
			var e := echoes[best]
			g.seen = best
			g.spot_x = e.x
			g.spot_y = e.y
			g.sus += 3 if bd < 768 * 768 else (2 if bd < 1280 * 1280 else 1)
			if g.sus == 1 or g.sus == 2 or g.sus == 3:
				_ev("spot", g.x, g.y, {"guard": gi})
			if g.sus >= SUS_ALARM:
				_alarm(gi, e.x, e.y)
				return
		else:
			g.seen = -1
			if g.sus > 0:
				g.sus -= 1
				if g.sus == 0 and g.mode == 0:
					g.route.clear()
		# Colleagues lying on the floor.
		var saw_body := false
		for oi in guards.size():
			var o := guards[oi]
			if oi != gi and o.mode == 3 and sees(g.x, g.y, g.face, 724, g.range_t * 256, o.x, o.y):
				saw_body = true
		if saw_body:
			g.body += 1
			if g.body >= BODY_ALARM:
				_alarm(gi, g.x, g.y)
				return
		else:
			g.body = maxi(0, g.body - 1)
	for v in devs:
		if v.t == "cam":
			if v.ch > 0 and chan[v.ch] == 1:
				v.sus = 0
				continue
			var a := (v.ang / 16) & 255
			var seen := false
			for e in echoes:
				if not e.caught and sees(v.x * 256 + 128, v.y * 256 + 128, a, 887, v.rng * 256, e.x, e.y):
					seen = true
					v.sus += 1
					if v.sus >= CAM_SUS_ALARM:
						_alarm(100, e.x, e.y)
						return
					break
			if not seen:
				v.sus = maxi(0, v.sus - 2)
		elif v.t == "laser":
			if laser_on(v):
				for e in echoes:
					if not e.caught and _in_laser(v, e.x, e.y):
						_alarm(200, e.x, e.y)
						return

func laser_on(v: Dev) -> bool:
	var off := v.ch > 0 and chan[v.ch] == 1
	return off == v.inv

func _in_laser(v: Dev, px: int, py: int) -> bool:
	var cx := v.x * 256 + 128
	var cy := v.y * 256 + 128
	if v.dir == 0:
		return absi(py - cy) < 50 + R_ECHO / 2 and px >= v.x * 256 and px < (v.x + v.len) * 256
	return absi(px - cx) < 50 + R_ECHO / 2 and py >= v.y * 256 and py < (v.y + v.len) * 256

func _win() -> void:
	for i in echoes.size():
		var e := echoes[i]
		if e.item == "loot" and tile_at(e.x >> 8, e.y >> 8) == T_EXIT:
			over = "win"
			win_frame = frame
			_ev("win", e.x, e.y, {"echo": i})
			return
