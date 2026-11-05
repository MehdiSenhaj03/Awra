class_name Chase
extends RefCounted
## A storm chase: the plains, the storms that cross them (development, maturity with tornado windows,
## dissipation), their rain and hail cores, gusts and lightning, the tornadoes (EF0-EF5, wandering tracks), the
## chase vehicles, the probes, and the photographs, all scored by the simulation itself. Integers only, 60 Hz.
##
## Lengths in mm (x east, y north), angles in 1/4096 turn, speeds in 1/16 mm per frame (16 = 0.96 mm/s... i.e.
## 1 m/s = 267 units), damage and health in 1/100.

const FPS := 60
const MS := 267                   # 1 m/s in speed units (1/16 mm per frame)
const HP := 10000
const CLOUD_BASE := 1_100_000     # mm
const FOV := 683                  # horizontal field of view at zoom 1 (60 degrees)
const FILM := 24
const EF_WIDTH := [70_000, 140_000, 260_000, 420_000, 620_000, 900_000]
const EF_WIND := [33, 45, 60, 75, 95, 115]       # m/s
enum C { TORNADO, STRUCTURE, LIGHTNING, HAIL }
const CATS := ["tornado", "structure", "lightning", "hail"]

class Storm:
	var id := 0
	var x := 0
	var y := 0
	var vx := 0                   # 1/16 mm per frame
	var vy := 0
	var t := 0
	var sev := 2                  # 1..5
	var dev := 0                  # frames of development, maturity, decay
	var mature := 0
	var decay := 0
	var windows: Array = []       # tornado windows: [start t, duration, ef]
	var rng: Rng
	var dead := false
	func motion() -> int:
		return Trig.atan2i(vy, vx)
	## 0..1000: how strongly the storm rotates now.
	func rot() -> int:
		if t < dev:
			return t * 700 / maxi(1, dev)
		if t < dev + mature:
			return 700 + 300 * sev / 5
		return maxi(0, 1000 - (t - dev - mature) * 1000 / maxi(1, decay))
	func alive_k() -> int:
		# 0..1000 size of the storm (rain extent), growing then fading.
		if t < dev:
			return 300 + t * 700 / maxi(1, dev)
		if t < dev + mature:
			return 1000
		return maxi(0, 1000 - (t - dev - mature) * 900 / maxi(1, decay))

class Tornado:
	var storm := 0
	var x := 0
	var y := 0
	var t := 0
	var life := 0
	var ef := 0
	var width := 0
	var wob := 0
	var dead := false

class Vehicle:
	var team := 0
	var x := 0
	var y := 0
	var a := 0                    # heading
	var td := 0                   # direction of travel (slides on mud)
	var v := 0                    # forward speed
	var hp := HP
	var probes := 3
	var film := FILM
	var alive := true
	var armor := 100              # percent damage taken
	var engine := 100             # percent top speed
	var horn := false
	var surface := 0
	var dmg_t := 0
	var waypoint := Vector2i(-1, -1)
	var best: Array = [0, 0, 0, 0]   # best photo score per category
	var best_f: Array = [0, 0, 0, 0]
	var probe_score := 0
	var photos := 0

class Probe:
	var team := 0
	var x := 0
	var y := 0
	var hit := false
	var score := 0

var plains: Plains
var storms: Array[Storm] = []
var tornadoes: Array[Tornado] = []
var vehicles: Array[Vehicle] = []
var probes: Array[Probe] = []
var strikes: Array = []           # recent lightning [{x, y, f, storm}]
var frame := 0
var length := FPS * 60 * 9
var events: Array[Dictionary] = []
var over := false
var schedule: Array = []          # storms to come: [{f, x, y, sev, dir, speed, windows}]
var rng: Rng
var _sid := 1

## day: {seed, storms: [{at, x, y, sev, dir, speed, tornadoes}], minutes}
func _init(day: Dictionary, teams: int = 1) -> void:
	var s: int = day.get("seed", 1)
	plains = Plains.new(s)
	rng = Rng.new(s * 31 + 7)
	length = FPS * 60 * int(day.get("minutes", 9))
	schedule = (day.get("storms", []) as Array).duplicate(true)
	for i in teams:
		var v := Vehicle.new()
		v.team = i
		# Start on an existing road in the east half (a north-south section road), facing south-west.
		var k := Plains.N / 2 + 2 + i
		var j := Plains.N / 2 - 1
		for tries in 40:
			if int(plains.vroad[k][j]) > 0 and not plains.in_river(k * Plains.CELL, j * Plains.CELL + Plains.CELL / 2):
				break
			k = Plains.N / 2 + 1 + (tries + i) % 4
			j = Plains.N / 2 - 2 + (tries / 4) % 5
		v.x = k * Plains.CELL
		v.y = j * Plains.CELL + Plains.CELL / 2
		v.a = 3072
		v.td = v.a
		vehicles.append(v)
	_spawn_due()

func _ev(t: String, d: Dictionary = {}) -> void:
	d.t = t
	events.append(d)

func _spawn_due() -> void:
	for sc: Dictionary in schedule:
		if int(sc.get("done", 0)) == 0 and int(sc.at) <= frame:
			sc.done = 1
			var st := Storm.new()
			st.id = _sid
			_sid += 1
			st.x = int(sc.x)
			st.y = int(sc.y)
			var dir: int = sc.get("dir", 512)
			var sp: int = int(sc.get("speed", 14)) * MS
			st.vx = Trig.cosa(dir) * sp >> 16
			st.vy = Trig.sina(dir) * sp >> 16
			st.sev = int(sc.sev)
			st.rng = Rng.new(plains.seed_v * 101 + st.id * 7)
			st.dev = FPS * st.rng.rint(70, 120)
			st.mature = FPS * (150 + st.sev * 60)
			st.decay = FPS * 150
			var nt: int = sc.get("tornadoes", 0)
			var at := st.dev + FPS * st.rng.rint(10, 40)
			for k in nt:
				var dur := FPS * st.rng.rint(70, 120 + st.sev * 30)
				var ef := clampi(st.sev - 1 + st.rng.rint(-1, 1) + (1 if k == nt - 1 and st.sev >= 4 else 0), 0, 5)
				st.windows.append([at, dur, ef])
				at += dur + FPS * st.rng.rint(40, 90)
			storms.append(st)
			_ev("storm", {"id": st.id, "sev": st.sev})

# ---- Fields ------------------------------------------------------------------------------------------------------------

func core_center(st: Storm) -> Vector2i:
	var o := Trig.rot(2_400_000, 1_400_000, st.motion())
	return Vector2i(st.x + o.x, st.y + o.y)

func hail_center(st: Storm) -> Vector2i:
	var o := Trig.rot(800_000, 700_000, st.motion())
	return Vector2i(st.x + o.x, st.y + o.y)

func hook(st: Storm) -> Vector2i:
	var o := Trig.rot(-1_200_000, -700_000, st.motion())
	return Vector2i(st.x + o.x, st.y + o.y)

## Rain 0..100 at a point (the heaviest of all storms).
func rain_at(x: int, y: int) -> int:
	var best := 0
	for st in storms:
		if st.dead:
			continue
		var k := st.alive_k()
		var c := core_center(st)
		# Ellipse aligned with the storm's motion: 4.5 km along, 3 km across, scaled with the storm's size.
		var d := Trig.rot(x - c.x, y - c.y, -st.motion())
		var rx := 4_500 * k / 1000 + 600
		var ry := 3_000 * k / 1000 + 400
		var ex := d.x / 1000
		var ey := d.y / 1000
		var q := ex * ex * 1000 / (rx * rx) + ey * ey * 1000 / (ry * ry)
		var r := maxi(0, 100 - q / 10)
		# The anvil's light rain, wider and downstream.
		var dd := Trig.rot(x - st.x, y - st.y, -st.motion())
		var ax := dd.x / 1000 - 6_000
		var ay := dd.y / 1000
		var qa := ax * ax * 1000 / (9_000 * 9_000) + ay * ay * 1000 / (5_000 * 5_000)
		r = maxi(r, maxi(0, 25 - qa / 40) * k / 1000)
		best = maxi(best, r)
	return best

## Hail 0..100 at a point.
func hail_at(x: int, y: int) -> int:
	var best := 0
	for st in storms:
		if st.dead or st.sev < 2 or st.t < st.dev * 2 / 3:
			continue
		var c := hail_center(st)
		var r := (700 + st.sev * 260) * st.alive_k() / 1000
		var dx := (x - c.x) / 1000
		var dy := (y - c.y) / 1000
		var q := (dx * dx + dy * dy) * 100 / maxi(1, r * r)
		best = maxi(best, maxi(0, 100 - q) * (40 + st.sev * 12) / 100)
	return best

## Wind in m/s at a point (x, y components).
func wind_at(x: int, y: int) -> Vector2i:
	var w := Vector2i(3, 7)      # southerly ambient flow
	for st in storms:
		if st.dead:
			continue
		var dx := (st.x - x) / 1000
		var dy := (st.y - y) / 1000
		var d := Trig.isqrt(dx * dx + dy * dy)
		if d < 9_000 and d > 0:
			# Inflow toward the updraft.
			var inflow := 6 * st.rot() / 1000 * (9_000 - d) / 9_000
			w += Vector2i(dx * inflow / d, dy * inflow / d)
		var h := hook(st)
		var hx := (x - h.x) / 1000
		var hy := (y - h.y) / 1000
		var hd := Trig.isqrt(hx * hx + hy * hy)
		if hd < 2_500 and hd > 0:
			# The rear-flank downdraft blows out.
			var g := 22 * st.rot() / 1000 * (2_500 - hd) / 2_500
			w += Vector2i(hx * g / hd, hy * g / hd)
	for tn in tornadoes:
		if tn.dead:
			continue
		var dx := (x - tn.x) / 1000
		var dy := (y - tn.y) / 1000
		var d := Trig.isqrt(dx * dx + dy * dy)
		var rc := maxi(20, tn.width / 2000)
		var vmax: int = EF_WIND[tn.ef] * _strength(tn) / 1000
		if d > 0 and d < rc * 12:
			var vt := vmax * d / rc if d < rc else vmax * rc / d
			# Counter-clockwise tangential wind, plus some inflow.
			w += Vector2i(-dy * vt / d, dx * vt / d)
			w += Vector2i(-dx * vt / (3 * d), -dy * vt / (3 * d))
	return w

## 0..1000: how far along its life a tornado is in strength (spin-up, mature, rope-out).
func _strength(tn: Tornado) -> int:
	var k := tn.t * 1000 / maxi(1, tn.life)
	if k < 250:
		return 300 + k * 700 / 250
	if k > 800:
		return maxi(150, 1000 - (k - 800) * 4)
	return 1000

# ---- Frame ----------------------------------------------------------------------------------------------------------------

## inputs: per player {team, steer -8..8, throttle, brake, horn, probe, photo: {yaw, pitch, zoom}, waypoint: Vector2i}
func step(inputs: Array) -> void:
	events.clear()
	if over:
		return
	frame += 1
	_spawn_due()
	_storms()
	_tornadoes()
	if frame % 30 == 0:
		_wetness()
	for inp: Dictionary in inputs:
		var team: int = inp.get("team", 0)
		if team < vehicles.size():
			_control(vehicles[team], inp)
	for v in vehicles:
		_drive(v)
	_probes()
	strikes = strikes.filter(func(s: Dictionary) -> bool: return frame - int(s.f) < 40)
	var alive := 0
	for v in vehicles:
		if v.alive:
			alive += 1
	if frame >= length or alive == 0:
		over = true
		_ev("over", {})

func _storms() -> void:
	for st in storms:
		if st.dead:
			continue
		st.t += 1
		st.x += st.vx >> 4
		st.y += st.vy >> 4
		if st.t > st.dev + st.mature + st.decay:
			st.dead = true
			_ev("storm_end", {"id": st.id})
			continue
		# Tornado windows.
		for w: Array in st.windows:
			if int(w[0]) == st.t:
				var tn := Tornado.new()
				tn.storm = st.id
				var h := hook(st)
				tn.x = h.x
				tn.y = h.y
				tn.life = int(w[1])
				tn.ef = int(w[2])
				tn.wob = st.rng.rint(0, 4095)
				tornadoes.append(tn)
				_ev("tornado", {"storm": st.id, "ef": tn.ef, "x": tn.x, "y": tn.y})
		# Lightning.
		var rate := (st.alive_k() * (st.sev + 1)) / 120
		if st.rng.rint(0, 1999) < rate:
			var c := core_center(st)
			var far := st.rng.chance(150)
			var r := 9_000_000 if far else 3_500_000
			var sx := c.x + st.rng.rint(-r, r)
			var sy := c.y + st.rng.rint(-r, r)
			strikes.append({"x": sx, "y": sy, "f": frame, "storm": st.id})
			_ev("strike", {"x": sx, "y": sy, "storm": st.id})
			for v in vehicles:
				if v.alive and absi(v.x - sx) < 40_000 and absi(v.y - sy) < 40_000:
					_hurt(v, 1200, "lightning")

func _tornadoes() -> void:
	for tn in tornadoes:
		if tn.dead:
			continue
		tn.t += 1
		var st := _storm(tn.storm)
		if tn.t >= tn.life or st == null:
			tn.dead = true
			_ev("tornado_end", {"storm": tn.storm})
			continue
		# Moves with its storm, wandering across the track; drifts a bit left of the motion as it matures.
		var m := st.motion()
		var wob := Trig.sina(tn.wob + tn.t * 6) * 6 * MS >> 16
		var side := Trig.rot(0, wob, m)
		tn.x += (st.vx + side.x) >> 4
		tn.y += (st.vy + side.y) >> 4
		var s := _strength(tn)
		tn.width = EF_WIDTH[tn.ef] * s / 1000
		# Damage to vehicles: the core destroys, the debris ring batters.
		for v in vehicles:
			if not v.alive:
				continue
			var dx := (v.x - tn.x) / 1000
			var dy := (v.y - tn.y) / 1000
			var d := Trig.isqrt(dx * dx + dy * dy)
			var rc := tn.width / 2000
			if d < rc * 7 / 10:
				_hurt(v, 400 + tn.ef * 200, "tornado")
			elif d < rc * 3:
				_hurt(v, (8 + tn.ef * 6) * s / 1000, "debris")

func _storm(id: int) -> Storm:
	for st in storms:
		if st.id == id:
			return st
	return null

func _wetness() -> void:
	for fy in Plains.FN:
		for fx in Plains.FN:
			var i := fy * Plains.FN + fx
			var r := rain_at(fx * Plains.FIELD + Plains.FIELD / 2, fy * Plains.FIELD + Plains.FIELD / 2)
			plains.wet[i] = clampi(plains.wet[i] + r / 4 - 2, 0, 1000)

func _control(v: Vehicle, inp: Dictionary) -> void:
	if not v.alive:
		return
	v.set_meta("in", inp)
	if inp.get("probe", false) and v.probes > 0 and absi(v.v) < 3 * MS:
		v.probes -= 1
		var p := Probe.new()
		p.team = v.team
		p.x = v.x
		p.y = v.y
		probes.append(p)
		_ev("probe_drop", {"team": v.team, "x": v.x, "y": v.y})
	if inp.has("waypoint"):
		v.waypoint = inp.waypoint
	if inp.has("photo") and v.film > 0:
		v.film -= 1
		v.photos += 1
		_photo(v, inp.photo)

func _drive(v: Vehicle) -> void:
	if not v.alive:
		return
	var inp: Dictionary = v.get_meta("in", {})
	v.set_meta("in", {})
	var surf := plains.surface(v.x, v.y)
	v.surface = surf
	var wetv := plains.wet_at(v.x, v.y)
	var muddy := wetv > 300
	var top := 0
	var grip := 0
	var acc := 0
	match surf:
		Plains.S.PAVED:
			top = 42; grip = 210; acc = 26
		Plains.S.DIRT:
			top = 18 if muddy else 30; grip = 90 if muddy else 160; acc = 14 if muddy else 22
		Plains.S.TOWN:
			top = 20; grip = 190; acc = 20
		_:
			top = 6 if muddy else 13; grip = 60 if muddy else 120; acc = 7 if muddy else 12
	top = top * v.engine / 100
	var steer: int = clampi(int(inp.get("steer", 0)), -8, 8)
	var thr: bool = inp.get("throttle", false)
	var brk: bool = inp.get("brake", false)
	v.horn = inp.get("horn", false)
	# Speed.
	if thr:
		v.v += acc
	elif brk:
		if v.v > 0:
			v.v = maxi(0, v.v - 60)
		else:
			v.v = maxi(-5 * MS, v.v - 10)
	else:
		v.v -= signi(v.v) * mini(absi(v.v), 6)
	var cap := top * MS
	if v.v > cap:
		v.v = maxi(cap, v.v - 40)
	# Steering: sharper at low speed; reversing inverts.
	var sp := absi(v.v)
	var rate := 0
	if sp > 20:
		rate = clampi(14 - sp / (4 * MS), 4, 14)
	v.a = (v.a + steer * rate * signi(v.v) / 4) & (Trig.TURN - 1)
	# Travel direction follows the heading by the grip of the ground.
	v.td = (v.td + Trig.adiff(v.td, v.a) * grip / 256) & (Trig.TURN - 1)
	var nx := v.x + (Trig.cosa(v.td) * v.v >> 16) / 16
	var ny := v.y + (Trig.sina(v.td) * v.v >> 16) / 16
	# Wind pushes.
	var w := wind_at(v.x, v.y)
	var ws := absi(w.x) + absi(w.y)
	if ws > 30:
		nx += w.x * (ws - 30) / 8
		ny += w.y * (ws - 30) / 8
	nx = clampi(nx, 20_000, Plains.SIZE - 20_000)
	ny = clampi(ny, 20_000, Plains.SIZE - 20_000)
	if plains.surface(nx, ny) == Plains.S.WATER:
		# The river bank: stop dead.
		v.v = -v.v / 4
		_ev("bank", {"team": v.team})
	else:
		v.x = nx
		v.y = ny
	# Hail.
	var h := hail_at(v.x, v.y)
	if h > 10:
		_hurt(v, h * h / 900 * v.armor / 100, "hail")

func _hurt(v: Vehicle, d: int, why: String) -> void:
	if not v.alive or d <= 0:
		return
	v.hp -= d * v.armor / 100 if why != "hail" else d
	v.dmg_t += 1
	if v.dmg_t % 30 == 1 or why == "lightning":
		_ev("hurt", {"team": v.team, "why": why})
	if v.hp <= 0:
		v.hp = 0
		v.alive = false
		_ev("wreck", {"team": v.team, "why": why})

func _probes() -> void:
	for p in probes:
		if p.hit:
			continue
		for tn in tornadoes:
			if tn.dead:
				continue
			var dx := (p.x - tn.x) / 1000
			var dy := (p.y - tn.y) / 1000
			var d := Trig.isqrt(dx * dx + dy * dy)
			var rc := tn.width / 2000 + 150
			if d < rc:
				p.hit = true
				var close := 1000 - d * 1000 / rc
				p.score = 500 + tn.ef * 250 + close * (300 + tn.ef * 150) / 1000
				for v in vehicles:
					if v.team == p.team:
						v.probe_score += p.score
				_ev("probe_hit", {"team": p.team, "x": p.x, "y": p.y, "score": p.score, "ef": tn.ef})
				break

# ---- Photographs --------------------------------------------------------------------------------------------------------

## photo: {yaw: 0..4095 world bearing, pitch: -1024..1024, zoom: percent 100..600}
func _photo(v: Vehicle, ph: Dictionary) -> void:
	var yaw: int = int(ph.yaw) & (Trig.TURN - 1)
	var pitch: int = int(ph.pitch)
	var zoom: int = clampi(int(ph.zoom), 100, 800)
	var fovh := FOV * 100 / zoom
	var fovv := fovh * 9 / 16
	var here_rain := rain_at(v.x, v.y)
	var res := [0, 0, 0, 0]
	# Tornadoes.
	for tn in tornadoes:
		if tn.dead:
			continue
		var q := _framing(v, tn.x, tn.y, CLOUD_BASE / 2, yaw, pitch, fovh, fovv)
		if q.is_empty():
			continue
		var dist: int = q.dist
		var ang_h := Trig.atan2i(CLOUD_BASE, maxi(1, dist))
		var fill := clampi(ang_h * 1000 / maxi(1, fovv), 0, 2000)
		# Best when the funnel fills 50-90% of the frame's height.
		var fq := 1000 - absi(fill - 700) * 1000 / 700
		fq = clampi(fq, 150, 1000)
		var vis := _visibility(v, tn.x, tn.y, here_rain)
		var close := clampi(1000 - (dist - 1_500_000) * 1000 / 9_000_000, 300, 1000)
		var s: int = (400 + tn.ef * 220) * fq / 1000 * (400 + int(q.center) * 6 / 10) / 1000 * vis / 1000 * close / 1000 * _strength(tn) / 1000
		res[C.TORNADO] = maxi(res[C.TORNADO], s)
	# Storm structure: the mesocyclone and its wall cloud, best from 4-12 km in clear air.
	for st in storms:
		if st.dead:
			continue
		var q := _framing(v, st.x, st.y, CLOUD_BASE * 3 / 2, yaw, pitch, fovh, fovv)
		if q.is_empty():
			continue
		var dist: int = q.dist
		var dq := 1000
		if dist < 3_000_000:
			dq = dist * 1000 / 3_000_000
		elif dist > 14_000_000:
			dq = maxi(0, 1000 - (dist - 14_000_000) / 6_000)
		var vis := _visibility(v, st.x, st.y, here_rain)
		var s: int = (220 + st.sev * 70) * st.rot() / 1000 * dq / 1000 * (400 + int(q.center) * 6 / 10) / 1000 * vis / 1000 * (1500 if zoom <= 160 else 1000) / 1000
		res[C.STRUCTURE] = maxi(res[C.STRUCTURE], s)
	# Lightning in the frame within the last few frames.
	for sk: Dictionary in strikes:
		if frame - int(sk.f) > 20:
			continue
		var q := _framing(v, int(sk.x), int(sk.y), CLOUD_BASE / 2, yaw, pitch, fovh, fovv)
		if q.is_empty():
			continue
		var dist: int = q.dist
		var close := clampi(1000 - dist * 1000 / 12_000_000, 0, 1000)
		var s: int = 320 * close / 1000 * (500 + int(q.center) / 2) / 1000 * _visibility(v, int(sk.x), int(sk.y), here_rain) / 1000
		res[C.LIGHTNING] = maxi(res[C.LIGHTNING], s)
	# Hail: in the core, lens down at the stones bouncing on the road.
	var h := hail_at(v.x, v.y)
	if h > 25 and pitch < -150:
		res[C.HAIL] = 120 + h * 3
	var total := 0
	var top := -1
	for c in 4:
		total += int(res[c])
		if int(res[c]) > 0 and (top < 0 or int(res[c]) > int(res[top])):
			top = c
		if int(res[c]) > int(v.best[c]):
			v.best[c] = res[c]
			v.best_f[c] = frame
	_ev("photo", {"team": v.team, "scores": res, "top": top, "total": total, "yaw": yaw, "pitch": pitch, "zoom": zoom})

## Is a point (with a height) in the frame? Returns {} or {dist, center: 0..1000}.
func _framing(v: Vehicle, x: int, y: int, h: int, yaw: int, pitch: int, fovh: int, fovv: int) -> Dictionary:
	var dx := (x - v.x) / 1000
	var dy := (y - v.y) / 1000
	var dist := Trig.isqrt(dx * dx + dy * dy) * 1000
	var bearing := Trig.atan2i(dy, dx)
	var ey := Trig.atan2i(h, maxi(1, dist))
	var oy := absi(Trig.adiff(yaw, bearing))
	var op := absi(ey - pitch)
	if oy > fovh / 2 or op > fovv / 2:
		return {}
	var c := 1000 - maxi(oy * 1000 / maxi(1, fovh / 2), op * 1000 / maxi(1, fovv / 2))
	return {"dist": dist, "center": c}

## 0..1000: how much of a subject the rain lets through.
func _visibility(v: Vehicle, x: int, y: int, here: int) -> int:
	var mid := rain_at((v.x + x) / 2, (v.y + y) / 2)
	return clampi(1000 - here * 5 - mid * 6, 80, 1000)

## The dossier: the best photo in each category plus the probes.
func dossier(team: int) -> Dictionary:
	var v := vehicles[team]
	var photos := 0
	for c in 4:
		photos += int(v.best[c])
	return {"best": v.best.duplicate(), "probes": v.probe_score, "total": photos + v.probe_score, "hp": v.hp, "alive": v.alive, "photos": v.photos}
