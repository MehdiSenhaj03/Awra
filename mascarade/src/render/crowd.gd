class_name Crowd
extends Node3D
## Every guest of the ball, drawn with a handful of MultiMeshes (bodies, heads, masks, arms, hats, props, contact
## shadows) and animated on the CPU between simulation frames: walk with a bob and swinging arms, waltz, and the
## gestures the sniper watches for (a sip, a bow, a hand on a statue, a pen on paper...).

const COATS := [Color(0.5, 0.03, 0.07), Color(0.03, 0.24, 0.14), Color(0.05, 0.07, 0.24), Color(0.6, 0.44, 0.12), Color(0.82, 0.76, 0.62), Color(0.04, 0.035, 0.04), Color(0.26, 0.06, 0.3)]
const HAIR := [Color(0.05, 0.03, 0.02), Color(0.22, 0.12, 0.05), Color(0.45, 0.3, 0.14), Color(0.62, 0.6, 0.55), Color(0.12, 0.05, 0.03)]
const SCALE := 1.12
const SHOULDER_M := Vector3(0.215, 1.25, 0.0)
const SHOULDER_W := Vector3(0.175, 1.22, 0.0)

var sim: Sim
var n := 0
var _bodies_m: MultiMesh
var _bodies_w: MultiMesh
var _heads: MultiMesh
var _masks: MultiMesh
var _arms: MultiMesh
var _hats: Array[MultiMesh] = []
var _glass: MultiMesh
var _blobs: MultiMesh
var _blood: MultiMesh

# Per-guest render state.
var _px := PackedFloat32Array()     # previous sim position (tiles)
var _pz := PackedFloat32Array()
var _cx := PackedFloat32Array()     # current sim position
var _cz := PackedFloat32Array()
var _yaw := PackedFloat32Array()
var _phase := PackedFloat32Array()
var _lr := PackedFloat32Array()     # right / left arm lift, inward swing, body lean
var _ll := PackedFloat32Array()
var _sr := PackedFloat32Array()
var _sl := PackedFloat32Array()
var _lean := PackedFloat32Array()
var _fall := PackedFloat32Array()
var _flash := PackedFloat32Array()
var _look := PackedByteArray()      # disguise key (mask, coat, hat) last applied
var _blood_t := PackedFloat32Array()
var _t := 0.0

func _init(s: Sim) -> void:
	sim = s
	n = s.guests.size()
	for arr: PackedFloat32Array in [_px, _pz, _cx, _cz, _yaw, _phase, _lr, _ll, _sr, _sl, _lean, _fall, _flash, _blood_t]:
		arr.resize(n)
	_look.resize(n * 3)
	_look.fill(255)
	var body := ShaderMaterial.new()
	body.shader = preload("res://src/render/shaders/guest.gdshader")
	var gown := body.duplicate() as ShaderMaterial
	gown.set_shader_parameter("kind", 1)
	var sleeve := body.duplicate() as ShaderMaterial
	sleeve.set_shader_parameter("kind", 2)
	var plain := body.duplicate() as ShaderMaterial
	plain.set_shader_parameter("kind", 3)
	_bodies_m = _mm(Shapes.cached("coat", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.12, 0.0), Vector2(0.115, 0.5), Vector2(0.2, 0.54), Vector2(0.185, 0.8), Vector2(0.165, 1.0), Vector2(0.205, 1.2), Vector2(0.2, 1.28), Vector2(0.065, 1.32), Vector2(0.055, 1.37)]), 14, 0.72)), body, n)
	_bodies_w = _mm(Shapes.cached("gown", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.37, 0.0), Vector2(0.355, 0.07), Vector2(0.3, 0.32), Vector2(0.2, 0.68), Vector2(0.125, 0.92), Vector2(0.135, 1.04), Vector2(0.17, 1.18), Vector2(0.165, 1.25), Vector2(0.055, 1.29), Vector2(0.05, 1.34)]), 16, 0.8)), gown, n)
	var head_m := ShaderMaterial.new()
	head_m.shader = preload("res://src/render/shaders/head.gdshader")
	_heads = _mm(Shapes.cached("head", func() -> Mesh: return Shapes.sphere(0.125, 14, 8)), head_m, n)
	var mask_m := ShaderMaterial.new()
	mask_m.shader = preload("res://src/render/shaders/mask.gdshader")
	mask_m.set_shader_parameter("atlas", load("res://assets/art/masks.png"))
	_masks = _mm(Shapes.cached("maskq", func() -> Mesh: return Shapes.quad(Vector2(0.3, 0.3))), mask_m, n, true)
	_arms = _mm(Shapes.cached("arm", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.0, -0.53), Vector2(0.035, -0.5), Vector2(0.04, -0.42), Vector2(0.045, -0.38), Vector2(0.05, -0.05), Vector2(0.0, 0.02)]), 8)), sleeve, n * 2)
	var tricorne := Shapes.cached("tricorne", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.0, 0.0), Vector2(0.21, 0.0), Vector2(0.22, 0.08), Vector2(0.12, 0.06), Vector2(0.11, 0.14), Vector2(0.0, 0.15)]), 3))
	var tophat := Shapes.cached("tophat", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.0, 0.0), Vector2(0.21, 0.0), Vector2(0.21, 0.02), Vector2(0.115, 0.03), Vector2(0.12, 0.25), Vector2(0.13, 0.28), Vector2(0.0, 0.28)]), 14))
	var plume := Shapes.cached("plume", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.0, 0.0), Vector2(0.05, 0.08), Vector2(0.045, 0.3), Vector2(0.0, 0.42)]), 8, 0.35))
	var bun := Shapes.cached("bun", func() -> Mesh: return Shapes.sphere(0.08, 10, 6))
	for m: Mesh in [tricorne, tophat, plume, bun]:
		_hats.append(_mm(m, plain, n))
	_glass = _mm(Shapes.cached("flute", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.03, 0.0), Vector2(0.004, 0.01), Vector2(0.004, 0.06), Vector2(0.025, 0.08), Vector2(0.03, 0.16)]), 8, 1.0, false)), Shapes.mat(Color(1.0, 0.9, 0.6), 0.05, 0.0, Color(1.0, 0.8, 0.4), 0.6), n)
	var blob_m := ShaderMaterial.new()
	blob_m.shader = preload("res://src/render/shaders/blob.gdshader")
	var flat := Shapes.cached("blobq", func() -> Mesh:
		var pm := PlaneMesh.new()
		pm.size = Vector2(1, 1)
		return pm)
	_blobs = _mm(flat, blob_m, n, false, true)
	_blood = _mm(flat, blob_m.duplicate(), n, false, true)
	for i in n:
		var g := sim.guests[i]
		_cx[i] = g.x / 256.0
		_cz[i] = g.y / 256.0
		_px[i] = _cx[i]
		_pz[i] = _cz[i]
		_yaw[i] = _yaw_of(g.face)
		_phase[i] = randf() * TAU
		_blobs.set_instance_color(i, Color(0, 0, 0, 0.55))
		_blood.set_instance_color(i, Color(0.25, 0.0, 0.02, 0.0))

func _mm(mesh: Mesh, m: Material, count: int, custom: bool = true, _colors: bool = true) -> MultiMesh:
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.use_colors = true
	mm.use_custom_data = custom
	mm.mesh = mesh
	mm.instance_count = count
	var mi := MultiMeshInstance3D.new()
	mi.multimesh = mm
	mi.material_override = m
	mi.custom_aabb = AABB(Vector3(-5, -5, -5), Vector3(80, 20, 80))
	add_child(mi)
	return mm

static func _yaw_of(face: int) -> float:
	var th := face * TAU / 256.0
	return atan2(cos(th), sin(th))

## After each simulation step.
func snap() -> void:
	for i in n:
		var g := sim.guests[i]
		_px[i] = _cx[i]
		_pz[i] = _cz[i]
		_cx[i] = g.x / 256.0
		_cz[i] = g.y / 256.0
		# Teleports (respawns) do not interpolate.
		if absf(_cx[i] - _px[i]) + absf(_cz[i] - _pz[i]) > 1.5:
			_px[i] = _cx[i]
			_pz[i] = _cz[i]

func flash(i: int, k: float = 1.0) -> void:
	_flash[i] = k

func _apply_look(i: int, g: Sim.Guest) -> void:
	if _look[i * 3] == g.mask and _look[i * 3 + 1] == g.coat and _look[i * 3 + 2] == g.hat:
		return
	_look[i * 3] = g.mask
	_look[i * 3 + 1] = g.coat
	_look[i * 3 + 2] = g.hat
	var coat: Color = COATS[g.coat % COATS.size()]
	var hair: Color = HAIR[(i * 7 + g.mask) % HAIR.size()]
	var sheen := float((i * 13) % 10) / 10.0
	_bodies_m.set_instance_color(i, coat)
	_bodies_w.set_instance_color(i, coat)
	_bodies_m.set_instance_custom_data(i, Color(0, 0, sheen, 0))
	_bodies_w.set_instance_custom_data(i, Color(0, 0, sheen, 0))
	_heads.set_instance_color(i, hair)
	_arms.set_instance_color(i * 2, coat)
	_arms.set_instance_color(i * 2 + 1, coat)
	_hats[0].set_instance_color(i, Color(0.04, 0.03, 0.03))
	_hats[1].set_instance_color(i, Color(0.03, 0.03, 0.035))
	_hats[2].set_instance_color(i, coat.lightened(0.25))
	_hats[3].set_instance_color(i, hair)

## Per rendered frame; `alpha` is how far we are between the last two simulation frames.
func draw(alpha: float, dt: float) -> void:
	_t += dt
	var zero := Transform3D(Basis().scaled(Vector3.ZERO), Vector3(0, -10, 0))
	var k := 1.0 - exp(-dt * 10.0)
	for i in n:
		var g := sim.guests[i]
		_apply_look(i, g)
		if g.st == Sim.St.OUT:
			for mm: MultiMesh in [_bodies_m, _bodies_w, _heads, _masks, _glass, _blobs]:
				mm.set_instance_transform(i, zero)
			for h in _hats:
				h.set_instance_transform(i, zero)
			_arms.set_instance_transform(i * 2, zero)
			_arms.set_instance_transform(i * 2 + 1, zero)
			continue
		var x := lerpf(_px[i], _cx[i], alpha)
		var z := lerpf(_pz[i], _cz[i], alpha)
		var woman := g.hat >= 2
		var dead := g.st == Sim.St.DEAD
		# Facing: smooth turn.
		var want := _yaw_of(g.face)
		_yaw[i] = lerp_angle(_yaw[i], want, 1.0 - exp(-dt * 9.0))
		var speed := Vector2(_cx[i] - _px[i], _cz[i] - _pz[i]).length() * 60.0
		var moving := g.moving and speed > 0.2
		if moving:
			_phase[i] += dt * speed * 5.5
		# Arm and body targets.
		var tr := 0.0
		var tl := 0.0
		var ts_r := 0.12
		var ts_l := 0.12
		var tlean := 0.0
		var glass := false
		var swing := sin(_phase[i]) * (0.45 if moving else 0.0)
		match g.gesture:
			Sim.G.SIP:
				tr = 2.2; ts_r = 0.45; glass = true
			Sim.G.ADMIRE:
				tr = -0.35; tl = -0.35; ts_r = 0.3; ts_l = 0.3; tlean = -0.06
			Sim.G.TALK:
				tr = 0.8 + 0.35 * sin(_t * 5.0 + i); ts_r = 0.1
			Sim.G.WRITE:
				tr = 0.95 + 0.06 * sin(_t * 14.0 + i); tlean = 0.32; ts_r = 0.25
			Sim.G.POUR:
				tr = 1.35; tlean = 0.18; ts_r = 0.3; glass = true
			Sim.G.TOUCH:
				tr = 1.45; ts_r = 0.05; tlean = 0.1
			Sim.G.REACH:
				tr = 0.65; ts_r = 0.35; tlean = 0.22
			Sim.G.STAB:
				tr = 1.65; ts_r = 0.15; tlean = 0.25
			Sim.G.BOW:
				tr = 0.7; ts_r = 0.75; tl = -0.25; tlean = 0.62
			_:
				tr = swing
				tl = -swing
				if g.st == Sim.St.DANCE:
					tr = 1.25; ts_r = -0.35; tl = 1.0; ts_l = 0.55; tlean = -0.05
				elif g.st == Sim.St.ACT and not moving and g.poi >= 0 and sim.venue.pois[g.poi].kind == "buffet":
					tr = 1.1 + 0.2 * sin(_t * 0.7 + i); ts_r = 0.3; glass = true
		_lr[i] = lerpf(_lr[i], tr, k)
		_ll[i] = lerpf(_ll[i], tl, k)
		_sr[i] = lerpf(_sr[i], ts_r, k)
		_sl[i] = lerpf(_sl[i], ts_l, k)
		_lean[i] = lerpf(_lean[i], tlean, k)
		_fall[i] = minf(1.0, _fall[i] + dt * 2.2) if dead else 0.0
		_flash[i] = maxf(0.0, _flash[i] - dt * 0.6)
		# Body frame: position, yaw, a bob when walking, a sway when dancing, lean (bow) from the hips, fall.
		var bob := absf(sin(_phase[i])) * 0.035 if moving else 0.0
		var yaw_b := Basis(Vector3.UP, _yaw[i])
		var sway := sin(_t * 3.1 + i) * 0.12 if g.st == Sim.St.DANCE else 0.0
		var lean_b := Basis(Vector3.RIGHT, _lean[i]) * Basis(Vector3.FORWARD, sway)
		var hip := Vector3(0, 0.62, 0)
		var body := Transform3D(yaw_b.scaled(Vector3(SCALE, SCALE, SCALE)), Vector3(x, bob, z))
		body = body * Transform3D(Basis(), hip) * Transform3D(lean_b, Vector3.ZERO) * Transform3D(Basis(), -hip)
		if _fall[i] > 0.0:
			var f := ease(_fall[i], 0.4)
			body = Transform3D(yaw_b.scaled(Vector3(SCALE, SCALE, SCALE)), Vector3(x, 0.0, z)) * Transform3D(Basis(Vector3.RIGHT, -f * PI * 0.5), Vector3(0, f * 0.18, 0))
		var hide := zero
		_bodies_m.set_instance_transform(i, body if not woman else hide)
		_bodies_w.set_instance_transform(i, body if woman else hide)
		var head_y := 1.47 if not woman else 1.43
		var look := sin(_t * 0.6 + i * 2.1) * 0.35 if g.st == Sim.St.ACT else 0.0
		var head := body * Transform3D(Basis(Vector3.UP, look), Vector3(0, head_y, 0.0))
		_heads.set_instance_transform(i, head)
		_masks.set_instance_transform(i, head * Transform3D(Basis(), Vector3(0, 0.0, 0.118)))
		var fl := _flash[i]
		_masks.set_instance_custom_data(i, Color(g.mask, fl, 0, 0))
		_bodies_m.set_instance_custom_data(i, Color(fl, 0, 0.5, 0))
		_bodies_w.set_instance_custom_data(i, Color(fl, 0, 0.5, 0))
		for h in 4:
			var ht := hide
			if h == g.hat:
				match h:
					0: ht = head * Transform3D(Basis(Vector3.UP, -PI * 0.5), Vector3(0, 0.07, -0.01))
					1: ht = head * Transform3D(Basis(), Vector3(0, 0.08, 0))
					2: ht = head * Transform3D(Basis(Vector3.RIGHT, -0.7), Vector3(0.06, 0.08, -0.04))
					3: ht = head * Transform3D(Basis(), Vector3(0, 0.08, -0.08))
			_hats[h].set_instance_transform(i, ht)
		# Arms from the shoulders: lift forward (positive) / back, swing inward.
		var sh := SHOULDER_W if woman else SHOULDER_M
		var ar := body * Transform3D(Basis(Vector3.FORWARD, -_sr[i]) * Basis(Vector3.RIGHT, -_lr[i]), Vector3(-sh.x, sh.y, 0))
		var al := body * Transform3D(Basis(Vector3.FORWARD, _sl[i]) * Basis(Vector3.RIGHT, -_ll[i]), Vector3(sh.x, sh.y, 0))
		_arms.set_instance_transform(i * 2, ar)
		_arms.set_instance_transform(i * 2 + 1, al)
		if glass and not dead:
			_glass.set_instance_transform(i, ar * Transform3D(Basis(Vector3.RIGHT, _lr[i]), Vector3(0, -0.5, 0.02)))
		else:
			_glass.set_instance_transform(i, hide)
		_blobs.set_instance_transform(i, Transform3D(Basis().scaled(Vector3(0.75, 1, 0.75) * (1.3 if woman else 1.0)), Vector3(x, 0.012, z)))
		if dead:
			_blood_t[i] = minf(1.0, _blood_t[i] + dt * 0.25)
			var r := 0.4 + _blood_t[i] * 1.0
			var fwd := yaw_b * Vector3(0, 0, 0.7)
			_blood.set_instance_transform(i, Transform3D(Basis().scaled(Vector3(r, 1, r)), Vector3(x + fwd.x, 0.014, z + fwd.z)))
			_blood.set_instance_color(i, Color(0.22, 0.0, 0.02, 0.9))
		else:
			_blood.set_instance_transform(i, hide)

## World position of a guest's chest (for the laser, the markers).
func chest(i: int) -> Vector3:
	return Vector3(_cx[i], 1.1, _cz[i])
