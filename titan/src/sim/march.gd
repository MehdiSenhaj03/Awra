class_name March
extends RefCounted
## One leg of the journey on Ama's back (or a titan duel): the titan walking the ash (heading, pace, heat, pressure,
## supplies, health), the deck and its crew at their posts (pilot, two harpoons, the heart forge, the lookout),
## what comes at them (scavengers, raiders, fires), the land ahead (chasms, rockfalls, geysers, oases, ash storms,
## wrecks to salvage), and the fight of colossi. Integers only, 60 Hz.
##
## Deck coordinates in cm on the titan's back (x forward, y left); world positions in cm relative to the titan
## (x forward, y left, z up from the ground; the deck is DECK_Z high).

enum P { PILOT, GUN_L, GUN_R, HEART, LOOKOUT }
const POSTS := [Vector2i(1800, 0), Vector2i(800, 950), Vector2i(800, -950), Vector2i(0, 0), Vector2i(-900, 500)]
const RX := 2200
const RY := 1250
const DECK_Z := 2600
const WALK := 7                    # cm per frame on the deck
const HP := 10000
const GUN_RANGE := 9000
const HARPOON_V := 260             # cm per frame
const RELOAD := 70
enum H { CHASM, ROCKS, GEYSER, OASIS, STORM, WRECK }
enum T { IDLE, WINDUP, STRIKE, RECOVER, PARRY, GRAB_WINDUP, HOLDING, HELD, STAGGER, FALLEN }

class Crew:
	var id := 0
	var x := 0
	var y := 0
	var post := -1
	var goal := -1                 # post the crew member walks to
	var ctrl := -1                 # player index, -1 for the AI
	var hp := 100
	var down := 0                  # frames before getting up again
	var role := 0                  # P.* the member is best at
	var age := 0
	var gift := ""
	var act := 0                   # frames of the current "use" (repair, fight)
	var aim := Vector3i(6000, 0, 0)
	var reload := 0

class Bird:
	var id := 0
	var x := 0
	var y := 0
	var z := 0
	var vx := 0
	var vy := 0
	var vz := 0
	var mode := 0                  # 0 circling, 1 diving, 2 climbing away
	var pecks := 0
	var t := 0
	var tx := 0
	var ty := 0
	var dead := false

class Skiff:
	var id := 0
	var x := 0
	var y := 0
	var hp := 2
	var t := 0
	var docked := false
	var sent := 0
	var dead := false

class Boarder:
	var id := 0
	var x := 0
	var y := 0
	var hp := 60
	var post := -1
	var dead := false

class Hazard:
	var kind := 0
	var at := 0                    # distance along the route (cm)
	var y := 0                     # lateral offset of its centre (cm)
	var r := 0                     # radius / half-length (cm)
	var done := false
	var seen := false

class Shot:
	var x := 0
	var y := 0
	var z := 0
	var tx := 0
	var ty := 0
	var tz := 0
	var t := 0
	var gun := 0

class Titan:
	var hp := HP
	var heat := 600
	var pressure := 300
	var balance := 1000
	var state := 0
	var t := 0
	var anchor := false
	var calm := 0                  # frames since last hit (balance regen)
	var power := 100               # strength (percent)
	var hold := 0
	var parry_cd := 0

var crew: Array[Crew] = []
var birds: Array[Bird] = []
var skiffs: Array[Skiff] = []
var boarders: Array[Boarder] = []
var hazards: Array[Hazard] = []
var shots: Array[Shot] = []
var fires: Array[Vector2i] = []
var fire_t: Array[int] = []
var titan := Titan.new()
var foe: Titan = null              # the wild titan, in a fight
var foe_weak: Array[Vector3i] = [Vector3i(0, 700, 3000), Vector3i(-600, -500, 2200), Vector3i(400, 0, 4200)]
var weak_seen := false
var progress := 0                  # cm along the route
var length := 300_000
var lateral := 0                   # titan's offset from the route centre (cm)
var steer := 0                     # -8..8
var pace := 1                      # 0 halt, 1 walk, 2 stride
var supplies := 600
var vision := 30_000
var in_storm := false
var frame := 0
var events: Array[Dictionary] = []
var over := ""                     # "done", "lost", "won" (fight)
var rng: Rng
var leg: Dictionary = {}
var fight := false
var duel := false                  # titan against titan from the start (versus mode)
var _ids := 1
var _schedule: Array = []          # [{f, kind, side, n}]

## leg: {kind, length_m, seed, level, boss: bool, duel: bool}; crew_info: [{role, gift, age}]
func _init(l: Dictionary, crew_info: Array = []) -> void:
	leg = l
	rng = Rng.new(int(l.get("seed", 1)) * 977 + 5)
	length = int(l.get("length_m", 3000)) * 100
	duel = l.get("duel", false)
	var roles := [P.PILOT, P.GUN_L, P.HEART, P.LOOKOUT]
	for i in 4:
		var c := Crew.new()
		c.id = i
		var info: Dictionary = crew_info[i] if i < crew_info.size() else {}
		c.role = int(info.get("role", roles[i]))
		c.gift = info.get("gift", "")
		c.age = int(info.get("age", 25))
		c.post = c.role
		c.goal = c.role
		var p: Vector2i = POSTS[c.role]
		c.x = p.x
		c.y = p.y
		crew.append(c)
	supplies = int(l.get("supplies", 800))
	titan.hp = int(l.get("hp", HP))
	if duel:
		_start_fight()
	else:
		_plan()

func _ev(t: String, d: Dictionary = {}) -> void:
	d.t = t
	events.append(d)

func _id() -> int:
	_ids += 1
	return _ids

# ---- The land and its threats ----------------------------------------------------------------------------------------------

func _plan() -> void:
	var kind: String = leg.get("kind", "ash")
	var lv: int = int(leg.get("level", 1))
	var at := 25_000
	while at < length - 20_000:
		var h := Hazard.new()
		var r := rng.rint(0, 99)
		match kind:
			"ruins": h.kind = H.WRECK if r < 45 else (H.ROCKS if r < 75 else H.CHASM)
			"forest": h.kind = H.GEYSER if r < 40 else (H.ROCKS if r < 70 else (H.WRECK if r < 85 else H.CHASM))
			"storm": h.kind = H.STORM if r < 25 else (H.CHASM if r < 55 else (H.ROCKS if r < 80 else H.WRECK))
			"oasis": h.kind = H.OASIS if at > length / 2 and not _has(H.OASIS) else (H.WRECK if r < 50 else (H.GEYSER if r < 75 else H.ROCKS))
			_: h.kind = H.WRECK if r < 35 else (H.CHASM if r < 60 else (H.ROCKS if r < 85 else H.GEYSER))
		h.at = at
		match h.kind:
			H.CHASM:
				h.r = rng.rint(1500, 2400)
				h.y = rng.rint(-2500, 2500)
			H.ROCKS:
				h.r = 3000
			H.GEYSER:
				h.r = 1800
				h.y = rng.rint(-3000, 3000)
			H.OASIS:
				h.r = 6000
			H.STORM:
				h.r = rng.rint(20_000, 35_000)
			H.WRECK:
				h.r = 600
				h.y = (1 if rng.chance(500) else -1) * rng.rint(4000, 8000)
		hazards.append(h)
		at += rng.rint(15_000, 28_000) + (3 - mini(lv, 3)) * 4000 - lv * 1500
	# Threats over time.
	var f := 60 * 25
	var end_f := length / 17 + 60 * 20
	while f < end_f:
		var k := rng.rint(0, 99)
		var side := 1 if rng.chance(500) else -1
		if kind == "ruins" or k < 55:
			_schedule.append({"f": f, "kind": "birds", "side": side, "n": rng.rint(4, 6 + lv * 2)})
		else:
			_schedule.append({"f": f, "kind": "skiffs", "side": side, "n": rng.rint(1, 1 + lv)})
		f += 60 * rng.rint(32, 55) - lv * 120

func _has(k: int) -> bool:
	for h in hazards:
		if h.kind == k:
			return true
	return false

func _spawn_birds(n: int, side: int) -> void:
	for i in n:
		var b := Bird.new()
		b.id = _id()
		b.x = rng.rint(-3000, 6000)
		b.y = side * rng.rint(9000, 14000)
		b.z = DECK_Z + rng.rint(1500, 3500)
		b.t = rng.rint(0, 600)
		birds.append(b)
	_ev("birds", {"side": side, "n": n})

func _spawn_skiffs(n: int, side: int) -> void:
	for i in n:
		var s := Skiff.new()
		s.id = _id()
		s.x = rng.rint(-6000, 12000)
		s.y = side * rng.rint(16000, 22000)
		skiffs.append(s)
	_ev("skiffs", {"side": side, "n": n})

# ---- Frame ----------------------------------------------------------------------------------------------------------------

## inputs: per player {crew: index, move: Vector2i(-1..1), use: bool (held), take: post or -1,
##   steer: -8..8, pace: -1/0/+1, verb: "strike"|"parry"|"grab"|"", anchor: bool,
##   aim: Vector3i, fire: bool, stoke: bool, vent: bool}
func step(inputs: Array) -> void:
	events.clear()
	if over != "":
		return
	frame += 1
	for f: Dictionary in _schedule:
		if int(f.f) == frame:
			if f.kind == "birds":
				_spawn_birds(int(f.n), int(f.side))
			else:
				_spawn_skiffs(int(f.n), int(f.side))
	var pilot_in := {}
	for c in crew:
		c.ctrl = -1
	for k in inputs.size():
		var ci0: int = (inputs[k] as Dictionary).get("crew", -1)
		if ci0 >= 0 and ci0 < crew.size() and int((inputs[k] as Dictionary).get("titan", 0)) == 0:
			crew[ci0].ctrl = k
	for inp: Dictionary in inputs:
		# A second player at the helm of the other titan (duel).
		if int(inp.get("titan", 0)) == 1 and foe != null:
			foe.anchor = inp.get("anchor", false)
			var vb: String = inp.get("verb", "")
			if vb != "":
				_verb(foe, titan, vb)
			continue
		var ci: int = inp.get("crew", -1)
		if ci < 0 or ci >= crew.size():
			continue
		var c := crew[ci]
		_player(c, inp)
		if c.post == P.PILOT:
			pilot_in = inp
	for c in crew:
		if c.ctrl < 0:
			var ai := _crew_ai(c)
			_player(c, ai)
			if c.post == P.PILOT and pilot_in.is_empty():
				pilot_in = ai
	_pilot(pilot_in)
	_heart()
	if fight:
		_fight()
	else:
		_walk()
	_threats()
	_shots()
	_fires()
	_crew_tick()
	if titan.hp <= 0:
		titan.hp = 0
		over = "lost"
		_ev("over", {"won": false})
	elif not fight and progress >= length:
		if leg.get("boss", false):
			_start_fight()
		else:
			over = "done"
			_ev("over", {"won": true})

## Control of one crew member (by a player or by its AI).
func _player(c: Crew, inp: Dictionary) -> void:
	if c.down > 0:
		c.down -= 1
		if c.down == 0:
			c.hp = 60
			_ev("up", {"crew": c.id})
		return
	var take: int = inp.get("take", -1)
	if take >= 0 and take < POSTS.size():
		c.goal = take
		if c.ctrl >= 0:
			# A player taking a post somewhere else: whoever held it steps aside.
			for o in crew:
				if o != c and o.post == take:
					o.post = -1
					o.goal = o.role if o.role != take else _free_post(o)
			c.post = -1
	var mv: Vector2i = inp.get("move", Vector2i.ZERO)
	if mv != Vector2i.ZERO:
		c.post = -1
		c.goal = -1
		_move(c, mv.x * WALK, mv.y * WALK)
	elif c.goal >= 0 and c.post != c.goal:
		var p: Vector2i = POSTS[c.goal]
		var dx := p.x - c.x
		var dy := p.y - c.y
		var d := Trig.isqrt(dx * dx + dy * dy)
		if d <= WALK * 2:
			c.x = p.x
			c.y = p.y
			c.post = c.goal
			_ev("post", {"crew": c.id, "post": c.post})
		else:
			var sp := WALK * (2 if c.ctrl >= 0 else 1)
			_move(c, dx * sp / d, dy * sp / d)
	# Use: at a fire or next to a boarder (fight), or at a post with an action.
	c.act = c.act + 1 if inp.get("use", false) else 0
	if inp.get("use", false):
		for i in fires.size():
			if absi(fires[i].x - c.x) < 300 and absi(fires[i].y - c.y) < 300:
				fire_t[i] -= 3
	if c.post == P.GUN_L or c.post == P.GUN_R:
		if inp.has("aim"):
			c.aim = inp.aim
		if c.reload > 0:
			c.reload -= 1
		elif inp.get("fire", false) and not duel:
			_fire(c)
	if c.post == P.HEART:
		if inp.get("stoke", false) and supplies >= 6 and titan.heat < 1000 and frame % 6 == 0:
			titan.heat = mini(1000, titan.heat + 20)
			titan.pressure = mini(1000, titan.pressure + 6)
			supplies -= 1 if c.gift == "frugal" and frame % 12 == 0 else 2
		if inp.get("vent", false) and frame % 4 == 0:
			titan.pressure = maxi(0, titan.pressure - 30)
			titan.heat = maxi(0, titan.heat - 3)

func _free_post(c: Crew) -> int:
	for p in [P.HEART, P.LOOKOUT, P.GUN_L, P.GUN_R, P.PILOT]:
		var taken := false
		for o in crew:
			if o != c and (o.post == p or o.goal == p):
				taken = true
		if not taken:
			return p
	return -1

func _move(c: Crew, dx: int, dy: int) -> void:
	var nx := c.x + dx
	var ny := c.y + dy
	# Stay on the back (an ellipse).
	if nx * nx / RX * 1000 / RX + ny * ny / RY * 1000 / RY <= 1000:
		c.x = nx
		c.y = ny

func _fire(c: Crew) -> void:
	var g: Vector2i = POSTS[c.post]
	var s := Shot.new()
	s.x = g.x
	s.y = g.y
	s.z = DECK_Z + 300
	var a := c.aim
	# Out of range: fly the full range in that direction.
	var dx := a.x - s.x
	var dy := a.y - s.y
	var dz := a.z - s.z
	var d := Trig.isqrt(dx * dx + dy * dy + dz * dz)
	if d > GUN_RANGE:
		a = Vector3i(s.x + dx * GUN_RANGE / d, s.y + dy * GUN_RANGE / d, s.z + dz * GUN_RANGE / d)
		d = GUN_RANGE
	s.tx = a.x
	s.ty = a.y
	s.tz = a.z
	s.t = maxi(1, d / HARPOON_V)
	s.gun = c.post
	shots.append(s)
	c.reload = RELOAD - (15 if c.gift == "quick" else 0)
	_ev("fire", {"gun": c.post})

func _pilot(inp: Dictionary) -> void:
	if inp.is_empty():
		titan.anchor = false
		steer = 0
		return
	steer = clampi(int(inp.get("steer", 0)), -8, 8)
	var dp: int = inp.get("pace", 0)
	if dp != 0 and not fight:
		pace = clampi(pace + dp, 0, 2)
		_ev("pace", {"pace": pace})
	titan.anchor = inp.get("anchor", false)
	var verb: String = inp.get("verb", "")
	if fight and verb != "":
		_verb(titan, foe, verb)

func _heart() -> void:
	var t := titan
	# The heart burns: faster when striding, slower at a halt; pressure follows the heat.
	var burn: int = [40, 18, 9][pace] if not fight else 14
	if frame % burn == 0:
		t.heat = maxi(0, t.heat - 1)
	if frame % 20 == 0:
		t.pressure = mini(1000, t.pressure + t.heat / 220)
	if frame % 400 == 0:
		supplies = maxi(0, supplies - 1)
	if t.pressure >= 1000:
		t.pressure = 450
		t.hp -= 500
		_fire_at(Vector2i(rng.rint(-300, 300), rng.rint(-300, 300)))
		_ev("burst", {})
	if t.heat >= 950 and frame % 90 == 0 and rng.chance(400):
		_fire_at(Vector2i(rng.rint(-1500, 1500), rng.rint(-900, 900)))
		_ev("overheat", {})

func speed() -> int:
	if titan.heat < 80:
		return 0
	var p := pace
	if titan.heat < 200:
		p = mini(p, 1)
	var base: int = [0, 17, 27][p]
	return base * (60 if in_storm else 100) / 100

func _walk() -> void:
	var sp := speed()
	progress += sp
	lateral = clampi(lateral + steer * sp / 10, -6000, 6000)
	# Lookout: see further; storms blind.
	var watching := _manned(P.LOOKOUT)
	in_storm = false
	for h in hazards:
		if h.kind == H.STORM and progress > h.at - h.r and progress < h.at + h.r:
			in_storm = true
	vision = (40_000 if watching else 18_000) if in_storm else (90_000 if watching else 35_000)
	if watching and _gifted(P.LOOKOUT, "keen"):
		vision = vision * 4 / 3
	if in_storm:
		vision = vision / 4
	for h in hazards:
		if h.done:
			continue
		var ahead := h.at - progress
		if not h.seen and ahead < vision:
			h.seen = true
			_ev("seen", {"kind": h.kind, "at": h.at, "y": h.y})
		match h.kind:
			H.CHASM:
				if absi(ahead) < 300:
					h.done = true
					if absi(lateral - h.y) < h.r + 900:
						titan.hp -= 900
						titan.balance = 300
						_fire_at(Vector2i(rng.rint(-1500, 1500), rng.rint(-800, 800)))
						_ev("chasm", {})
						_knock(500)
					else:
						_ev("dodged", {"kind": "chasm"})
			H.ROCKS:
				if absi(ahead) < 300:
					h.done = true
					if titan.anchor or pace == 0:
						_ev("braced", {})
						titan.hp -= 100
					else:
						titan.hp -= 700
						_ev("rocks", {})
						_knock(300)
			H.GEYSER:
				if absi(ahead) < h.r and absi(lateral - h.y) < h.r:
					if frame % 10 == 0:
						titan.heat = mini(1000, titan.heat + 4)
					if not h.done:
						h.done = true
						_ev("geyser", {})
			H.OASIS:
				if absi(ahead) < h.r and pace == 0:
					if frame % 6 == 0:
						supplies = mini(1000, supplies + 2)
						titan.hp = mini(HP, titan.hp + 12)
					if not h.done:
						h.done = true
						_ev("oasis", {})
			H.STORM:
				if ahead < 0 and not h.done:
					h.done = true
					_ev("storm", {})
				if in_storm and frame % 45 == 0:
					titan.heat = maxi(0, titan.heat - 1)
			H.WRECK:
				if ahead < -15_000:
					h.done = true

## A blow that throws the crew off their feet if they are not braced at a post.
func _knock(p: int) -> void:
	for c in crew:
		if c.post < 0 and rng.rint(0, 999) < p:
			c.hp -= 20
			if c.hp <= 0:
				_down(c)

func _down(c: Crew) -> void:
	c.hp = 0
	c.down = 60 * (6 if c.gift == "stout" else 12)
	c.post = -1
	_ev("crew_down", {"crew": c.id})

## The member at a post has this gift.
func _gifted(p: int, g: String) -> bool:
	for c in crew:
		if c.post == p and c.down == 0 and c.gift == g:
			return true
	return false

func _manned(p: int) -> bool:
	for c in crew:
		if c.post == p and c.down == 0:
			return true
	return false

func _fire_at(p: Vector2i) -> void:
	fires.append(p)
	fire_t.append(60 * 10)
	_ev("fire_start", {"x": p.x, "y": p.y})

func _fires() -> void:
	for i in range(fires.size() - 1, -1, -1):
		fire_t[i] -= 1
		if frame % 30 == 0:
			titan.hp -= 15
		if fire_t[i] <= 0:
			fires.remove_at(i)
			fire_t.remove_at(i)
			_ev("fire_out", {})

func _threats() -> void:
	# Birds circle, then dive at a crew member, peck, climb away.
	for b in birds:
		if b.dead:
			continue
		b.t += 1
		match b.mode:
			0:
				var a := b.t * 9 + b.id * 300
				var r := 9000 + (b.id % 4) * 1200
				var wx := Trig.cosa(a) * r >> 16
				var wy := Trig.sina(a) * r >> 16
				b.x += clampi(wx - b.x, -60, 60)
				b.y += clampi(wy - b.y, -60, 60)
				b.z += clampi(DECK_Z + 2500 - b.z, -20, 20)
				if b.t > 360 and rng.chance(3):
					b.mode = 1
					var c := crew[rng.rint(0, crew.size() - 1)]
					b.tx = c.x
					b.ty = c.y
					_ev("dive", {"id": b.id})
			1:
				var dx := b.tx - b.x
				var dy := b.ty - b.y
				var dz := DECK_Z + 150 - b.z
				var d := maxi(1, Trig.isqrt(dx * dx + dy * dy + dz * dz))
				b.x += dx * 120 / d
				b.y += dy * 120 / d
				b.z += dz * 120 / d
				if d < 200:
					b.mode = 2
					b.t = 0
					b.pecks += 1
					titan.hp -= 25
					for c in crew:
						if absi(c.x - b.x) < 350 and absi(c.y - b.y) < 350 and c.down == 0:
							c.hp -= 25
							if c.hp <= 0:
								_down(c)
					_ev("peck", {"x": b.x, "y": b.y})
			2:
				b.z += 40
				b.x -= 30
				if b.z > DECK_Z + 3000:
					b.mode = 0
					b.t = 0
					# Fed twice, a scavenger leaves.
					if b.pecks >= 2:
						b.dead = true
						_ev("bird_gone", {})
	birds = birds.filter(func(b: Bird) -> bool: return not b.dead)
	# Skiffs sail in; docked, they send boarders up the legs.
	for s in skiffs:
		if s.dead:
			continue
		s.t += 1
		if not s.docked:
			var dy := -signi(s.y) * 30
			s.y += dy
			s.x += clampi(-s.x, -20, 20)
			if absi(s.y) < 4500:
				s.docked = true
				s.t = 0
				_ev("docked", {"id": s.id})
		elif s.t % 360 == 120 and boarders.size() < 4 and s.sent < 2:
			s.sent += 1
			var b := Boarder.new()
			b.id = _id()
			b.x = rng.rint(-1200, 1200)
			b.y = signi(s.y) * (RY - 100)
			b.post = rng.rint(0, 4)
			boarders.append(b)
			_ev("boarder", {"id": b.id})
	skiffs = skiffs.filter(func(s: Skiff) -> bool: return not s.dead)
	# Boarders walk to a post and wreck it; crew next to them fight.
	for b in boarders:
		if b.dead:
			continue
		var p: Vector2i = POSTS[b.post]
		var dx := p.x - b.x
		var dy := p.y - b.y
		var d := Trig.isqrt(dx * dx + dy * dy)
		if d > 120:
			b.x += dx * 5 / maxi(1, d)
			b.y += dy * 5 / maxi(1, d)
		elif frame % 30 == 0:
			titan.hp -= 10
		for c in crew:
			if c.down == 0 and absi(c.x - b.x) < 260 and absi(c.y - b.y) < 260:
				b.hp -= (2 if c.act > 0 else 1) * (2 if c.gift == "brave" else 1)
				if frame % 20 == 0:
					c.hp -= 3
					if c.hp <= 0:
						_down(c)
		if b.hp <= 0:
			b.dead = true
			_ev("boarder_down", {"id": b.id})
	boarders = boarders.filter(func(b: Boarder) -> bool: return not b.dead)

func _shots() -> void:
	for s in shots:
		s.t -= 1
		if s.t > 0:
			var k := maxi(1, s.t)
			s.x += (s.tx - s.x) / k
			s.y += (s.ty - s.y) / k
			s.z += (s.tz - s.z) / k
			continue
		s.x = s.tx
		s.y = s.ty
		s.z = s.tz
		var hit := false
		for b in birds:
			if not b.dead and absi(b.x - s.x) < 450 and absi(b.y - s.y) < 450 and absi(b.z - s.z) < 450:
				b.dead = true
				hit = true
				_ev("bird_hit", {"x": b.x, "y": b.y, "z": b.z})
		for k2 in skiffs:
			if not k2.dead and absi(k2.x - s.x) < 700 and absi(k2.y - s.y) < 700 and s.z < 800:
				k2.hp -= 1
				hit = true
				_ev("skiff_hit", {"x": k2.x, "y": k2.y})
				if k2.hp <= 0:
					k2.dead = true
					_ev("skiff_sunk", {"x": k2.x, "y": k2.y})
		for h in hazards:
			if h.kind == H.WRECK and not h.done:
				var wx := h.at - progress
				var wy := h.y - lateral
				if absi(wx - s.x) < 900 and absi(wy - s.y) < 900 and s.z < 1000:
					h.done = true
					hit = true
					supplies = mini(1000, supplies + 90)
					_ev("salvage", {"x": wx, "y": wy})
		if fight and foe != null:
			for i in foe_weak.size():
				var w := weak_world(i)
				if absi(w.x - s.x) < 500 and absi(w.y - s.y) < 500 and absi(w.z - s.z) < 600:
					hit = true
					# Weak points hurt most when the titan is held or down, and when the lookout shows them.
					var dmg := 120 * (5 if foe.state == T.HELD or foe.state == T.FALLEN else 2) / 2 * (3 if weak_seen else 1) / 2
					foe.hp -= dmg
					foe.balance = maxi(0, foe.balance - 35)
					foe.calm = 0
					_ev("weak_hit", {"i": i, "dmg": dmg})
			var body := Vector3i(FOE_X, 0, 3000)
			if not hit and absi(body.x - s.x) < 1800 and absi(s.y) < 1500 and s.z > 600 and s.z < 5200:
				hit = true
				foe.hp -= 40
				_ev("foe_hit", {})
		if not hit:
			_ev("miss", {"x": s.x, "y": s.y, "z": s.z})
	shots = shots.filter(func(s: Shot) -> bool: return s.t > 0)

func _crew_tick() -> void:
	for c in crew:
		if c.down == 0 and c.hp < 100 and frame % 120 == 0:
			c.hp += 1

# ---- The fight of colossi ------------------------------------------------------------------------------------------------------

const FOE_X := 7600                 # the wild titan stands this far ahead

func _start_fight() -> void:
	fight = true
	pace = 0
	foe = Titan.new()
	foe.hp = int(leg.get("foe_hp", 10000 + int(leg.get("level", 1)) * 4000))
	foe.power = int(leg.get("foe_power", 100))
	foe.heat = 700
	titan.balance = 1000
	_ev("fight", {})

func weak_world(i: int) -> Vector3i:
	var w: Vector3i = foe_weak[i]
	# The weak points face us when the wild titan is held or fallen; otherwise they are on its flanks.
	var open := foe.state == T.HELD or foe.state == T.FALLEN
	return Vector3i(FOE_X - w.x, w.y if open else w.y * 2, w.z)

const VERB_T := {"strike": 40, "grab": 30}

func _verb(a: Titan, b: Titan, verb: String) -> void:
	if a.state == T.FALLEN or a.state == T.HELD or a.state == T.STAGGER:
		return
	if a.state != T.IDLE and not (verb == "parry" and a.state == T.RECOVER):
		return
	match verb:
		"strike":
			if a.anchor:
				return
			a.state = T.WINDUP
			a.t = VERB_T.strike
		"grab":
			a.state = T.GRAB_WINDUP
			a.t = VERB_T.grab
		"parry":
			if a.parry_cd <= 0:
				a.state = T.PARRY
				a.t = 26 + (8 if a == titan and _gifted(P.PILOT, "steady") else 0)
				a.parry_cd = 70
	_ev("verb", {"who": 0 if a == titan else 1, "verb": verb})

func _fight() -> void:
	weak_seen = _manned(P.LOOKOUT)
	if duel and frame % 9 == 0:
		foe.heat = mini(700, foe.heat + 1)
	if not duel or leg.get("foe_ai", true):
		_foe_ai()
	_combat(titan, foe, 0)
	_combat(foe, titan, 1)
	if foe.hp <= 0:
		foe.hp = 0
		over = "won"
		_ev("over", {"won": true})

## A titan's state machine: wind-ups land or are parried, grabs hold, balance breaks.
func _combat(a: Titan, b: Titan, who: int) -> void:
	a.calm += 1
	if a.parry_cd > 0:
		a.parry_cd -= 1
	if a.calm > 60 and a.balance < 1000 and a.state != T.FALLEN:
		a.balance = mini(1000, a.balance + 3)
	if a.t > 0:
		a.t -= 1
	match a.state:
		T.WINDUP:
			if a.t == 0:
				a.state = T.RECOVER
				a.t = 28
				if b.state == T.PARRY:
					a.balance = maxi(0, a.balance - 260)
					a.calm = 0
					b.state = T.IDLE
					_ev("parried", {"who": who})
				else:
					var dmg := (350 + a.heat * 2 / 3) * a.power / 100
					if a == titan and _gifted(P.PILOT, "steady"):
						dmg = dmg * 115 / 100
					if b.state == T.FALLEN:
						dmg *= 2
					b.hp -= dmg
					var bal := 210 if not b.anchor else 80
					if b.state == T.HELD:
						bal = 0
					b.balance = maxi(0, b.balance - bal)
					b.calm = 0
					a.heat = maxi(0, a.heat - 70)
					_ev("strike", {"who": who, "dmg": dmg})
		T.GRAB_WINDUP:
			if a.t == 0:
				if b.state == T.WINDUP or b.anchor or b.state == T.PARRY:
					a.state = T.RECOVER
					a.t = 40
					a.balance = maxi(0, a.balance - 120)
					_ev("grab_fail", {"who": who})
				else:
					a.state = T.HOLDING
					a.t = 200
					b.state = T.HELD
					b.t = 200
					_ev("grab", {"who": who})
		T.HOLDING:
			if frame % 6 == 0:
				a.heat = maxi(0, a.heat - 2)
			if a.t == 0 or b.state != T.HELD:
				a.state = T.RECOVER
				a.t = 30
		T.HELD:
			if b.state != T.HOLDING:
				a.state = T.IDLE
			elif a.anchor and a.t < 150:
				# Breaking free costs strength but frees.
				a.state = T.STAGGER
				a.t = 30
				b.state = T.RECOVER
				b.t = 40
				_ev("break_free", {"who": who})
			elif a.t == 0:
				a.state = T.IDLE
		T.PARRY, T.RECOVER, T.STAGGER:
			if a.t == 0:
				a.state = T.IDLE
		T.FALLEN:
			if a.t == 0:
				a.state = T.IDLE
				a.balance = 650
				_ev("rise", {"who": who})
	if a.balance <= 0 and a.state != T.FALLEN:
		a.state = T.FALLEN
		a.t = 220
		a.hp -= 300
		if b.state == T.HOLDING:
			b.state = T.IDLE
		_ev("fall", {"who": who})
		if who == 0:
			_knock(700)

## The wild titan: strikes with a readable wind-up, grabs, parries when it sees a strike coming.
func _foe_ai() -> void:
	var f := foe
	var lv: int = int(leg.get("level", 1))
	if f.state != T.IDLE:
		return
	if titan.state == T.WINDUP and titan.t > 12 and rng.chance(120 + lv * 60):
		_verb(f, titan, "parry")
		return
	if titan.state == T.FALLEN and rng.chance(100):
		_verb(f, titan, "strike")
		return
	if rng.chance(26 + lv * 8):
		_verb(f, titan, "grab" if rng.chance(300) else "strike")

# ---- Crew AI ------------------------------------------------------------------------------------------------------------------

## What a crew member left to the AI does this frame at its post.
func _crew_ai(c: Crew) -> Dictionary:
	var inp := {}
	if c.down > 0:
		return inp
	# Fires near the heart: the mechanic goes; boarders: whoever is closest.
	if c.post < 0 and c.goal < 0:
		var held := false
		for o in crew:
			if o != c and (o.post == c.role or o.goal == c.role):
				held = true
		inp.take = c.role if not held else _free_post(c)
	match c.post:
		P.PILOT:
			inp.steer = _ai_steer()
			var want := 1
			for h in hazards:
				if h.kind == H.OASIS and not h.done and absi(h.at - progress) < h.r and supplies < 900:
					want = 0
			if titan.heat < 150:
				want = 1
			inp.pace = signi(want - pace)
			var rocks := false
			for h in hazards:
				if h.kind == H.ROCKS and not h.done and h.seen and h.at - progress < 2500 and h.at - progress > -200:
					rocks = true
			inp.anchor = rocks
			if fight:
				inp.anchor = foe.state == T.GRAB_WINDUP or titan.state == T.HELD
				if titan.state == T.IDLE:
					if foe.state == T.WINDUP and foe.t < 22 and rng.chance(28):
						inp.verb = "parry"
					elif foe.state == T.FALLEN or foe.state == T.HELD:
						inp.verb = "strike"
					elif rng.chance(18):
						inp.verb = "grab" if rng.chance(350) else "strike"
		P.GUN_L, P.GUN_R:
			var t := _ai_target(c.post)
			if t != Vector3i(-1, -1, -1):
				inp.aim = t + Vector3i(rng.rint(-250, 250), rng.rint(-250, 250), rng.rint(-150, 150))
				inp.fire = c.reload == 0 and rng.chance(30 if fight else 60)
		P.HEART:
			inp.stoke = titan.heat < 520 and supplies > 50
			inp.vent = titan.pressure > 720
	# A fire close by: put it out.
	for i in fires.size():
		if absi(fires[i].x - c.x) < 900 and absi(fires[i].y - c.y) < 900:
			inp.use = true
	for b in boarders:
		if absi(b.x - c.x) < 260 and absi(b.y - c.y) < 260:
			inp.use = true
	# Raiders on the deck: the closest member (not the pilot) goes to fight them.
	if c.post != P.PILOT and not boarders.is_empty() and c.down == 0:
		var nb: Boarder = null
		var bd := 1 << 40
		for b in boarders:
			var d := (b.x - c.x) * (b.x - c.x) + (b.y - c.y) * (b.y - c.y)
			if d < bd:
				bd = d
				nb = b
		var closest := true
		for o in crew:
			if o != c and o.ctrl < 0 and o.down == 0 and o.post != P.PILOT:
				if (nb.x - o.x) * (nb.x - o.x) + (nb.y - o.y) * (nb.y - o.y) < bd:
					closest = false
		if closest and bd > 200 * 200:
			inp.move = Vector2i(signi(nb.x - c.x) if absi(nb.x - c.x) > 100 else 0, signi(nb.y - c.y) if absi(nb.y - c.y) > 100 else 0)
	return inp

func _ai_steer() -> int:
	for h in hazards:
		if h.done or not h.seen or h.kind != H.CHASM:
			continue
		var ahead := h.at - progress
		if ahead > 0 and ahead < 30_000 and absi(lateral - h.y) < h.r + 1400:
			return 8 if lateral >= h.y else -8
	for h in hazards:
		if h.done or not h.seen or h.kind != H.WRECK:
			continue
	return clampi(-lateral / 300, -3, 3)

func _ai_target(post: int) -> Vector3i:
	var g: Vector2i = POSTS[post]
	var side := signi(g.y)
	var best := Vector3i(-1, -1, -1)
	var bd := 1 << 40
	for b in birds:
		if b.dead:
			continue
		var dx := b.x - g.x
		var dy := b.y - g.y
		var d := dx * dx + dy * dy
		if (b.mode == 1 or signi(b.y) == side) and d < bd and d < GUN_RANGE * GUN_RANGE:
			bd = d
			# Lead the target a little.
			best = Vector3i(b.x, b.y, b.z)
	for s in skiffs:
		var d := (s.x - g.x) * (s.x - g.x) + (s.y - g.y) * (s.y - g.y)
		if signi(s.y) == side and d < bd and d < GUN_RANGE * GUN_RANGE:
			bd = d
			best = Vector3i(s.x, s.y, 0)
	if fight and foe != null and best == Vector3i(-1, -1, -1):
		if weak_seen or foe.state == T.HELD or foe.state == T.FALLEN:
			var i := frame / 300 % foe_weak.size()
			best = weak_world(i)
		else:
			best = Vector3i(FOE_X, 0, 3000)
	if best == Vector3i(-1, -1, -1):
		for h in hazards:
			if h.kind == H.WRECK and not h.done and signi(h.y - lateral) == side:
				var wx := h.at - progress
				if wx > -2000 and wx < 8000:
					best = Vector3i(wx, h.y - lateral, 200)
	return best
