class_name Colossus
extends Node3D
## A titan of rock: a long body carried on two hind legs and two great arms that walk on their knuckles and fight,
## a head with calm (or burning) eyes, a tail. Ama is grey stone under moss and lichen; a wild titan is red hide
## with lava in its cracks, ram horns, a ridge of bone and ribs showing. Animated: the walk (a gait with planted
## feet and footfalls that shake the world), the fight's poses (wind-up, strike, parry, grab, held, stagger, fall)
## and the hits. Metres, forward = -Z, right = +X.

signal footfall(pos: Vector3, strength: float)

const H := 23.0                    # height of the body's centre
const S := 15.0                    # stride (m)
const DUTY := 0.68                 # share of the cycle a foot is planted
# The torso (a superellipsoid) and its top: the deck's surface.
const BX := 12.5
const CY_T := 8.0
const CY_B := 7.0
const CZ_F := 23.0
const CZ_B := 20.0
const PW := 2.4
const LEAN := 0.07                 # the body leans back a little: the shoulders stand higher than the hips

var wild := false
var body: Node3D
var head: Node3D
var skin: ShaderMaterial
var eye_m: StandardMaterial3D
var limbs: Array[Dictionary] = []
var phase := 0.0
var moving := 0.0
var fight := false
var state := 0
var st_t := 0
var last_verb := ""                # what the last RECOVER follows: "strike", "parried", "grab_fail", ""
var _hit := 0.0
var _lunge := 0.0
var _pitch := 0.0
var _yaw := 0.0
var _roll := 0.0
var _drop := 0.0
var _t := 0.0

func _init(is_wild := false) -> void:
	wild = is_wild
	skin = ShaderMaterial.new()
	skin.shader = preload("res://src/render/shaders/stone.gdshader")
	if wild:
		skin.set_shader_parameter("rock", Color(0.5, 0.2, 0.14))
		skin.set_shader_parameter("rock_dark", Color(0.2, 0.06, 0.05))
		skin.set_shader_parameter("moss_amt", 0.0)
		skin.set_shader_parameter("crack_glow", 1.0)
		skin.set_shader_parameter("bump", 0.7)
		skin.set_shader_parameter("seed", 3.0)
		eye_m = Shapes.mat(Color(1, 0.5, 0.1), 0.3, 0.0, Color(1.0, 0.45, 0.08), 6.0)
	else:
		eye_m = Shapes.mat(Color(0.3, 0.8, 0.85), 0.3, 0.0, Color(0.3, 0.9, 1.0), 3.0)
	body = Node3D.new()
	body.position.y = H
	add_child(body)
	_build()

## The deck's height (above the body's centre) at a point of the back (body coordinates, metres).
static func surface(x: float, z: float) -> float:
	var cz := CZ_F if z < 0.0 else CZ_B
	var k := 1.0 - pow(absf(x) / BX, PW) - pow(absf(z) / cz, PW)
	return CY_T * pow(maxf(k, 0.0), 1.0 / PW)

func _part(mesh: Mesh, pos: Vector3, parent: Node3D = null, m: Material = null) -> MeshInstance3D:
	return Shapes.put(parent if parent else body, mesh, m if m else skin, pos)

func _build() -> void:
	var k := 1.12 if wild else 1.0
	_part(Shapes.blob(BX, CY_T, CY_B, CZ_F, CZ_B, PW, 40, 22), Vector3.ZERO)
	# Belly and chest mass, shoulders, haunches.
	_part(Shapes.blob(9.5, 4, 8, 17, 13, 2.2), Vector3(0, -3, -5))
	# The chest rises toward the shoulders.
	_part(Shapes.blob(11, 7, 7, 9, 8, 2.3), Vector3(0, 1.5, -13))
	for sd in [-1, 1]:
		_part(Shapes.blob(5.0 * k, 5.5, 7, 6.5, 6, 2.0), Vector3(sd * 10.0, -1.0, -13))
		_part(Shapes.blob(5.0, 5.0, 6.5, 6, 6.5, 2.0), Vector3(sd * 8.5, -2.5, 13))
	# Neck and head.
	head = Node3D.new()
	head.position = Vector3(0, 8.5 if not wild else 3.0, -31)
	body.add_child(head)
	var neck := _part(Shapes.bone(6.0, 4.0), Vector3.ZERO)
	Shapes.span(neck, Vector3(0, 2.0, -18), head.position + Vector3(0, -1.0, 2.5))
	_part(Shapes.blob(5.2, 4.8, 3.8, 7.5, 5.0, 2.3), Vector3.ZERO, head)
	_part(Shapes.blob(5.6, 1.7, 1.3, 2.2, 3.0, 1.8), Vector3(0, 2.6, -4.6), head)          # brow
	_part(Shapes.blob(3.6, 2.2, 2.4, 3.6, 3.0, 2.0), Vector3(0, -0.6, -6.8), head)         # muzzle
	_part(Shapes.blob(4.0, 1.8, 2.4, 5.0, 3.5, 2.2), Vector3(0, -3.0, -3.0), head)         # jaw
	for sd in [-1, 1]:
		Shapes.put(head, Shapes.sphere(0.65, 12, 6), eye_m, Vector3(sd * 2.6, 1.3, -6.9))
	# Tail.
	var tail := _part(Shapes.bone(4.2, 0.9), Vector3.ZERO)
	Shapes.span(tail, Vector3(0, -1, 19), Vector3(0, -9, 36))
	# Rock outcrops on the back and flanks.
	var rng := RandomNumberGenerator.new()
	rng.seed = 7 if not wild else 11
	for i in 22:
		var ph := rng.randf() * TAU
		var th := rng.randf_range(0.05, 0.9)
		var p := _on_torso(ph, th)
		var r := rng.randf_range(1.2, 3.4)
		var rock := _part(Shapes.blob(r, r * 0.9, r * 0.5, r * 1.3, r, 1.3, 7, 4), p)
		rock.rotation = Vector3(rng.randf_range(-0.3, 0.3), rng.randf() * TAU, rng.randf_range(-0.3, 0.3))
	if wild:
		_wild_parts(rng)
	# Limbs: arms in front (elbows back), legs behind (knees forward).
	for sd in [-1, 1]:
		_limb(Vector3(sd * 10.5, -1.0, -13), Vector3(sd * 14.0, 0, -17), 13.5 * k, 13.0 * k, 3.0 * k, 2.3 * k, sd, true, 0.25 if sd < 0 else 0.75)
		_limb(Vector3(sd * 8.8, -3.0, 13), Vector3(sd * 11.0, 0, 15.5), 11.5, 11.5, 3.2, 2.1, sd, false, 0.0 if sd < 0 else 0.5)

## A point on the torso's surface (longitude ph, latitude th from 0 at the equator to 1 at the top).
func _on_torso(ph: float, th: float) -> Vector3:
	var e := 2.0 / PW
	var a := th * PI / 2
	var cx := Shapes._spow(cos(a), e)
	var z := cx * Shapes._spow(sin(ph), e)
	return Vector3(BX * cx * Shapes._spow(cos(ph), e), CY_T * Shapes._spow(sin(a), e), z * (CZ_B if z > 0.0 else CZ_F)) * 0.97

func _wild_parts(rng: RandomNumberGenerator) -> void:
	var bonem := Shapes.mat(Color(0.86, 0.78, 0.62), 0.7)
	# Ram horns: spirals of shrinking spheres.
	for sd in [-1, 1]:
		for i in 14:
			var a := i * 0.42
			var r := 1.9 - i * 0.11
			var p := Vector3(sd * (3.5 + sin(a) * 3.2 + i * 0.25), 3.0 + cos(a) * 3.4 - 1.0, -1.5 + sin(a * 0.5) * 2.0 - i * 0.12)
			Shapes.put(head, Shapes.sphere(r, 10, 6), bonem, p)
	# Teeth.
	for i in 6:
		var t := Shapes.put(head, Shapes.cyl(0.35, 0.0, 1.6, 6), bonem, Vector3(-2.0 + i * 0.8, -1.6, -6.2))
		t.rotation.x = PI
	# A ridge of bone along the spine.
	for i in 11:
		var z := -16.0 + i * 3.4
		var y := surface(0, z)
		var sp := Shapes.put(body, Shapes.cyl(1.3, 0.0, 4.5 - absf(i - 5) * 0.3, 6), bonem, Vector3(0, y + 1.6, z))
		sp.rotation.x = 0.35
	# Ribs on the flanks.
	for sd in [-1, 1]:
		for i in 5:
			var z := -8.0 + i * 4.0
			var rb := Shapes.put(body, Shapes.bone(0.7, 0.5, 8), bonem, Vector3.ZERO)
			Shapes.span(rb, Vector3(sd * 6.0, 7.0, z), Vector3(sd * 13.6, -2.0, z + 1.0))
	# Tail spikes.
	for i in 4:
		var t := Shapes.put(body, Shapes.cyl(0.8, 0.0, 3.0, 6), bonem, Vector3(0, -2.0 - i * 1.9 + 3.0, 22.0 + i * 3.6))
		t.rotation.x = 0.6

func _limb(joint: Vector3, foot: Vector3, l1: float, l2: float, r0: float, r1: float, side: int, front: bool, off: float) -> void:
	var l := {"joint": joint, "neutral": foot, "foot": foot, "pose": foot, "l1": l1, "l2": l2, "side": side, "front": front, "off": off, "u": 0.0}
	l.up = Shapes.put(self, Shapes.bone(r0, r0 * 0.85), skin, Vector3.ZERO)
	l.lo = Shapes.put(self, Shapes.bone(r1 * 1.1, r1 * 0.95), skin, Vector3.ZERO)
	l.j1 = Shapes.put(self, Shapes.sphere(maxf(r0 * 0.9, r1 * 1.15), 14, 8), skin, Vector3.ZERO)
	l.end = Shapes.put(self, Shapes.blob(r1 * 1.5, r1 * 1.0, r1 * 0.45, r1 * 1.9, r1 * 1.1, 2.0, 14, 8), skin, Vector3.ZERO)
	if wild:
		var bonem := Shapes.mat(Color(0.86, 0.78, 0.62), 0.7)
		for c in 3:
			var cl := Shapes.put(l.end, Shapes.cyl(0.45, 0.0, 2.2, 6), bonem, Vector3((c - 1) * 1.3, -0.2, -r1 * 1.5))
			cl.rotation.x = -PI / 2 - 0.4
	limbs.append(l)

func hit(k: float = 1.0) -> void:
	_hit = maxf(_hit, k)

## The titan's state in a fight (March.T), with the frames left in it.
func set_fight(s: int, t: int) -> void:
	fight = true
	state = s
	st_t = t

## Advances the animation: v is the walking speed (m/s).
func animate(dt: float, v: float) -> void:
	_t += dt
	moving = move_toward(moving, 1.0 if v > 0.3 and not fight else 0.0, dt * 0.9)
	var prev_phase := phase
	if not fight:
		phase += v * DUTY / S * dt
	# Fight pose targets.
	var pose := _pose()
	var rate: float = pose.rate
	var a := 1.0 - exp(-rate * dt)
	_lunge = lerpf(_lunge, pose.lunge, a)
	_pitch = lerpf(_pitch, pose.pitch, a)
	_yaw = lerpf(_yaw, pose.yaw, a)
	_roll = lerpf(_roll, pose.roll, minf(1.0, a * 0.6))
	_drop = lerpf(_drop, pose.drop, minf(1.0, a * 0.6))
	var bob := sin(phase * TAU * 2.0) * 0.45 * moving
	var shake := Vector3.ZERO
	if fight and state == March.T.HELD:
		shake = Vector3(sin(_t * 31.0), 0, cos(_t * 27.0)) * 0.35
	body.position = Vector3(0, H + bob - _drop, _lunge) + shake
	body.rotation = Vector3(LEAN + _pitch + sin(phase * TAU * 2.0 + 1.0) * 0.008 * moving, _yaw, _roll + sin(phase * TAU) * 0.018 * moving)
	head.rotation = Vector3(sin(_t * 0.4) * 0.03 - _pitch * 0.4, sin(_t * 0.23) * 0.06 * (1.0 - moving * 0.5), 0)
	var bt := body.transform
	for l: Dictionary in limbs:
		var n: Vector3 = l.neutral
		var tgt := n
		if fight and l.front:
			tgt = pose.l if int(l.side) < 0 else pose.r
		elif not fight or not l.front:
			var u := fposmod(phase + float(l.off), 1.0)
			var g := n
			if u < DUTY:
				g.z = n.z - S * 0.5 + S * (u / DUTY)
			else:
				var s := (u - DUTY) / (1.0 - DUTY)
				g.z = n.z + S * 0.5 - S * s
				g.y = 3.2 * sin(PI * s)
			# A footfall when the foot comes down.
			var pu: float = fposmod(prev_phase + float(l.off), 1.0)
			if moving > 0.2 and pu >= DUTY and u < DUTY and phase != prev_phase:
				footfall.emit(to_global(Vector3(g.x, 0, g.z)), moving * (1.3 if l.front else 1.0))
			tgt = n.lerp(g, moving)
			if fight and state == March.T.FALLEN:
				tgt = n + Vector3(int(l.side) * 6.0, 0, 0)
		var cur: Vector3 = l.pose
		var k := 1.0 - exp(-(rate if l.front and fight else 30.0) * dt)
		cur = cur.lerp(tgt, k)
		l.pose = cur
		var j: Vector3 = bt * (l.joint as Vector3)
		var pole: Vector3 = j + (Vector3(int(l.side) * 3.0, 0, 12.0) if l.front else Vector3(int(l.side) * 2.0, 0, -12.0))
		var e := Shapes.ik(j, cur, l.l1, l.l2, pole - j)
		var reach: Vector3 = j + (cur - j).limit_length(float(l.l1) + float(l.l2) - 0.02)
		Shapes.span(l.up, j, e)
		Shapes.span(l.lo, e, reach)
		(l.j1 as Node3D).position = e
		var hand := l.end as Node3D
		hand.position = reach + Vector3(0, 1.0, 0)
		hand.rotation = Vector3(0, 0, 0) if not (fight and l.front) else Vector3(-0.8, 0, 0)
	_hit = maxf(0.0, _hit - dt * 3.0)
	skin.set_shader_parameter("hit", _hit)

## Where the arms, body and head go for the current fight state.
func _pose() -> Dictionary:
	var p := {"lunge": 0.0, "pitch": 0.0, "yaw": 0.0, "roll": 0.0, "drop": 0.0, "rate": 5.0,
		"l": Vector3(-9, 9, -27), "r": Vector3(9, 9, -27)}
	if not fight:
		return p
	var br := sin(_t * 1.6) * 0.6
	p.pitch = 0.3
	p.l = Vector3(-9, 17 + br, -26)
	p.r = Vector3(9, 17 - br, -26)
	match state:
		March.T.WINDUP:
			var k := 1.0 - clampf(st_t / 40.0, 0.0, 1.0)
			k = k * k * (3.0 - 2.0 * k)
			p.r = Vector3(9, 17, -26).lerp(Vector3(12, 36, -4), k)
			p.l = Vector3(-10, 18, -28)
			p.pitch = 0.3 + 0.1 * k
			p.yaw = -0.18 * k
			p.lunge = 2.5 * k
			p.rate = 9.0
		March.T.RECOVER:
			if last_verb == "strike" and st_t > 18:
				p.r = Vector3(2, 24, -52)
				p.l = Vector3(-10, 15, -30)
				p.lunge = -8.0
				p.yaw = 0.2
				p.pitch = 0.2
				p.rate = 26.0
			elif last_verb == "parried":
				p.r = Vector3(13, 30, -20)
				p.pitch = 0.42
				p.lunge = 4.0
				p.rate = 12.0
			elif last_verb == "grab_fail":
				p.l = Vector3(-16, 6, -34)
				p.r = Vector3(16, 6, -34)
				p.pitch = 0.05
				p.lunge = -3.0
		March.T.PARRY:
			p.l = Vector3(-2.5, 30, -30)
			p.r = Vector3(2.5, 27, -32)
			p.pitch = 0.36
			p.rate = 14.0
		March.T.GRAB_WINDUP:
			p.l = Vector3(-23, 23, -28)
			p.r = Vector3(23, 23, -28)
			p.lunge = -2.0
			p.rate = 8.0
		March.T.HOLDING:
			p.l = Vector3(-7.5, 20, -52)
			p.r = Vector3(7.5, 20, -52)
			p.lunge = -9.0
			p.pitch = 0.18
			p.rate = 10.0
		March.T.HELD:
			p.l = Vector3(-11, 23, -32)
			p.r = Vector3(11, 23, -32)
			p.lunge = 3.0
			p.pitch = 0.4
		March.T.STAGGER:
			p.l = Vector3(-12, 6, -21)
			p.r = Vector3(12, 6, -21)
			p.lunge = 5.0
			p.pitch = 0.45
			p.roll = 0.06
			p.rate = 8.0
		March.T.FALLEN:
			p.l = Vector3(-17, 1, -23)
			p.r = Vector3(16, 1, -20)
			p.drop = 8.0
			p.roll = 0.75
			p.pitch = -0.04
			p.rate = 3.0
	return p
