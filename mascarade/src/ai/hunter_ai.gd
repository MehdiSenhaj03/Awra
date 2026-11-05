class_name HunterAI
extends RefCounted
## A computer sniper. It does not know who the spy is: it watches the crowd like a person would — one area at a
## time — and builds suspicion from what is unusual: guests who stop where nobody stops, walk back and forth,
## hurry, make telling gestures, or were close to a mission when it happened. Then it takes its one shot.

var sim: Sim
var skill := 1.0
var rng: Rng
var sus := PackedFloat32Array()
var _prev := PackedInt32Array()
var _dir := PackedInt32Array()
var _focus := Vector2.ZERO
var _focus_target := Vector2.ZERO
var _dwell := 0
var aim := Vector2i.ZERO
var _fire := false
var _patience := 0
var _win := 0
var _was_moving := PackedByteArray()

func _init(s: Sim, sk: float, seed_v: int) -> void:
	sim = s
	skill = sk
	rng = Rng.new(seed_v)
	sus.resize(s.guests.size())
	_prev.resize(s.guests.size() * 2)
	_dir.resize(s.guests.size())
	_was_moving.resize(s.guests.size())
	for i in s.guests.size():
		_prev[i * 2] = s.guests[i].x
		_prev[i * 2 + 1] = s.guests[i].y
	_focus = Vector2(s.venue.w * 128, s.venue.h * 128)
	_focus_target = _focus
	aim = Vector2i(int(_focus.x), int(_focus.y))

func _near_poi(g: Sim.Guest) -> bool:
	for p: Dictionary in sim.venue.pois:
		var dx := int(p.x) * 256 + 128 - g.x
		var dy := int(p.y) * 256 + 128 - g.y
		if dx * dx + dy * dy < 450 * 450:
			return true
	return false

## Called every frame; returns [aim, fire].
func update() -> Array:
	_fire = false
	var radius := 5.5 * 256.0
	# Wander the attention: towards the most suspicious guest, or a mission spot, or anywhere.
	_dwell -= 1
	if _dwell <= 0:
		_dwell = int(lerpf(220.0, 120.0, clampf(skill, 0.0, 2.0) / 2.0))
		var top := _top()
		if top >= 0 and sus[top] > 2.0 and rng.chance(600):
			_focus_target = Vector2(sim.guests[top].x, sim.guests[top].y)
		elif rng.chance(500) and sim.venue.missions.size() > 0:
			var keys := sim.venue.missions.keys()
			var p: Dictionary = sim.venue.pois[sim.venue.missions[keys[rng.rint(0, keys.size() - 1)]]]
			_focus_target = Vector2(int(p.x) * 256 + 128, int(p.y) * 256 + 128)
		else:
			_focus_target = Vector2(rng.rint(2, sim.venue.w - 3) * 256, rng.rint(2, sim.venue.h - 3) * 256)
	_focus = _focus.lerp(_focus_target, 0.03 * (0.6 + skill * 0.4))
	aim = Vector2i(int(_focus.x), int(_focus.y))
	# Observe, a tenth of a second at a time (people read movement over moments, not frames).
	_win += 1
	if _win >= 10:
		_win = 0
		for i in sim.guests.size():
			var g := sim.guests[i]
			if g.st == Sim.St.DEAD:
				continue
			var vx := g.x - _prev[i * 2]
			var vy := g.y - _prev[i * 2 + 1]
			_prev[i * 2] = g.x
			_prev[i * 2 + 1] = g.y
			var moving := absi(vx) + absi(vy) > 30
			var was := _was_moving[i] == 1
			_was_moving[i] = 1 if moving else 0
			var seen := Vector2(g.x, g.y).distance_to(_focus) < radius
			sus[i] *= 0.996
			if not seen:
				if moving:
					_dir[i] = Fx.angle(vx, vy)
				continue
			var gain := 0.0
			if moving:
				var d := Fx.angle(vx, vy)
				if was and absi(Fx.adiff(_dir[i], d)) > 100:
					gain += 0.5
				_dir[i] = d
				if vx * vx + vy * vy > (Sim.WALK + 3) * (Sim.WALK + 3) * 100:
					gain += 0.7
			elif g.st != Sim.St.DANCE and g.st != Sim.St.DANCE_WAIT and not _near_poi(g):
				gain += 0.18
			if g.gesture in [Sim.G.POUR, Sim.G.TOUCH, Sim.G.REACH, Sim.G.BOW]:
				gain += 0.45
			sus[i] += gain * skill
	# Missions reveal a place and a time: everyone close is a suspect.
	for e in sim.events:
		if e.t == "mission":
			for i in sim.guests.size():
				var g := sim.guests[i]
				var d := Vector2(g.x - int(e.x), g.y - int(e.y)).length()
				if d < 3.0 * 256:
					# Only what was being watched is remembered well; the rest is a vague impression.
					var watched := Vector2(g.x, g.y).distance_to(_focus) < radius
					sus[i] += (3.0 if watched else 0.9 * skill) * (1.0 - d / (3.0 * 256))
			_focus_target = Vector2(int(e.x), int(e.y))
	# Shoot when one guest stands out, or when time and missions run out.
	var top := _top()
	if top >= 0:
		var second := 0.0
		for i in sus.size():
			if i != top and sus[i] > second:
				second = sus[i]
		var urgent := sim.done.size() >= sim.need - 1 and sim.frame > sim.limit * 0.4
		var confident := sus[top] > 7.0 / maxf(0.5, skill) and sus[top] > second * 1.8
		var last_call := sim.done.size() >= sim.need - 1 and sim.frame > sim.limit * 0.75
		if confident or (urgent and sus[top] > 4.5 and sus[top] > second * 1.4) or (last_call and sus[top] > second * 1.15):
			_patience += 1
			var g := sim.guests[top]
			aim = Vector2i(g.x, g.y)
			_focus = Vector2(g.x, g.y)
			if _patience > 40:
				_fire = true
		else:
			_patience = 0
	return [aim, _fire]

func _top() -> int:
	var best := -1
	var bv := 0.0
	for i in sus.size():
		if sim.guests[i].st != Sim.St.DEAD and sus[i] > bv:
			bv = sus[i]
			best = i
	return best
