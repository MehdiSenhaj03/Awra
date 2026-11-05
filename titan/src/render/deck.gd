class_name Deck
extends Node3D
## Ama's village on her back: the pilot's saddle and reins at the neck, the two harpoon turrets on the shoulders,
## the heart forge in the middle (its glow follows the heat, its chimney smokes, it vents steam), the lookout tower,
## huts, masts and sails, ropes and bunting, crates; and the people on it (crew, raiders), fires.
## A child of the titan's body: deck points (sim cm, x forward, y left) sit on the back's surface.

const LINEAGE := [Color(0.62, 0.2, 0.14), Color(0.75, 0.16, 0.12), Color(0.78, 0.6, 0.18), Color(0.3, 0.42, 0.55)]
const ROBES := [Color(0.42, 0.36, 0.3), Color(0.35, 0.3, 0.27), Color(0.4, 0.33, 0.25), Color(0.38, 0.36, 0.33)]

var wood_m := Shapes.mat(Color(0.38, 0.27, 0.18), 0.9)
var dark_wood_m := Shapes.mat(Color(0.24, 0.17, 0.12), 0.9)
var copper_m := Shapes.mat(Color(0.72, 0.42, 0.22), 0.45, 0.7)
var rope_m := Shapes.mat(Color(0.55, 0.47, 0.35), 0.95)
var forge_m: StandardMaterial3D
var forge_light: OmniLight3D
var smoke: GPUParticles3D
var steam: GPUParticles3D
var turrets: Array[Node3D] = []          # yaw nodes, left then right
var crew: Array[Figure] = []
var raiders: Dictionary = {}             # boarder id -> Figure
var fire_nodes: Array[Node3D] = []
var _heat := 0.5
var _vent := 0.0

## A deck point (cm) on the back's surface, in body coordinates (metres).
static func at(x: int, y: int, lift: float = 0.0) -> Vector3:
	var bx := -y / 100.0
	var bz := -x / 100.0
	return Vector3(bx, Colossus.surface(bx, bz) + lift - 0.25, bz)

func _init() -> void:
	_saddle()
	for i in 2:
		_turret(March.POSTS[1 + i], i)
	_forge()
	_tower()
	_village()
	for i in 4:
		var f := Figure.new(ROBES[i], LINEAGE[i], Color(0.62, 0.45, 0.33), i == 3)
		add_child(f)
		crew.append(f)

func _cloth(c: Color, stripe := Color(0.6, 0.2, 0.1), stripes := 0.0, seed := 0.0) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = preload("res://src/render/shaders/cloth.gdshader")
	m.set_shader_parameter("col", c)
	m.set_shader_parameter("stripe", stripe)
	m.set_shader_parameter("stripes", stripes)
	m.set_shader_parameter("seed", seed)
	return m

func _rope(a: Vector3, b: Vector3, sag: float = 0.8, r: float = 0.05) -> void:
	var prev := a
	for i in range(1, 7):
		var t := i / 6.0
		var p := a.lerp(b, t) + Vector3(0, -sag * 4.0 * t * (1.0 - t), 0)
		var seg := Shapes.put(self, Shapes.cached("rope", func() -> Mesh: return Shapes.cyl(1.0, 1.0, 1.0, 4)), rope_m, Vector3.ZERO)
		Shapes.span(seg, prev, p, r)
		seg.position = (prev + p) * 0.5     # the cylinder mesh is centred
		prev = p

func _bunting(a: Vector3, b: Vector3, seed: float) -> void:
	_rope(a, b, 0.9, 0.04)
	var cols := [Color(0.75, 0.2, 0.12), Color(0.85, 0.65, 0.2), Color(0.2, 0.45, 0.5), Color(0.85, 0.8, 0.7)]
	for i in range(1, 9):
		var t := i / 9.0
		var p := a.lerp(b, t) + Vector3(0, -0.9 * 4.0 * t * (1.0 - t) - 0.35, 0)
		var f := Shapes.put(self, Shapes.cached("flag", func() -> Mesh: return Shapes.quad(Vector2(0.5, 0.7))), _cloth(cols[i % 4], Color.BLACK, 0.0, seed + i), p)
		f.look_at_from_position(p, p + (b - a).cross(Vector3.UP), Vector3.UP)

func _saddle() -> void:
	var p := at(1800, 0)
	var base := Shapes.put(self, Shapes.box(Vector3(3.2, 0.5, 2.6)), wood_m, p + Vector3(0, 0.2, 0))
	base.rotation.x = 0.25
	Shapes.put(self, Shapes.box(Vector3(0.3, 1.4, 0.3)), dark_wood_m, p + Vector3(-1.4, 0.9, -1.0))
	Shapes.put(self, Shapes.box(Vector3(0.3, 1.4, 0.3)), dark_wood_m, p + Vector3(1.4, 0.9, -1.0))
	Shapes.put(self, Shapes.box(Vector3(3.2, 0.2, 0.3)), dark_wood_m, p + Vector3(0, 1.6, -1.0))
	var canopy := Shapes.put(self, Shapes.quad(Vector2(3.6, 2.6)), _cloth(Color(0.82, 0.72, 0.55), Color(0.62, 0.25, 0.15), 6.0, 1.0), p + Vector3(0, 2.6, 0.2))
	canopy.rotation.x = -PI / 2 + 0.2
	# Reins to the head.
	for sd in [-1, 1]:
		_rope(p + Vector3(sd * 1.2, 1.4, -1.0), Vector3(sd * 4.2, 6.0, -31.0), 2.5, 0.07)

func _turret(post: Vector2i, i: int) -> void:
	var p := at(post.x, post.y)
	Shapes.put(self, Shapes.cyl(2.2, 2.6, 1.2, 10), dark_wood_m, p + Vector3(0, 0.4, 0))
	var yaw := Node3D.new()
	yaw.position = p + Vector3(0, 1.4, 0)
	add_child(yaw)
	var pitch := Node3D.new()
	pitch.name = "pitch"
	pitch.position.y = 0.8
	yaw.add_child(pitch)
	Shapes.put(yaw, Shapes.box(Vector3(0.5, 1.2, 0.5)), wood_m, Vector3(0, 0.3, 0))
	Shapes.put(pitch, Shapes.box(Vector3(0.6, 0.5, 4.2)), wood_m, Vector3(0, 0, -0.6))
	var bow := Shapes.put(pitch, Shapes.box(Vector3(5.2, 0.3, 0.35)), dark_wood_m, Vector3(0, 0, -2.4))
	bow.rotation.y = 0.0
	for sd in [-1, 1]:
		var tip := Shapes.put(pitch, Shapes.box(Vector3(0.3, 0.3, 1.0)), dark_wood_m, Vector3(sd * 2.6, 0, -2.1))
		tip.rotation.y = sd * 0.5
	var harpoon := Shapes.put(pitch, Shapes.cyl(0.08, 0.08, 4.0, 6), Shapes.mat(Color(0.5, 0.5, 0.52), 0.4, 0.8), Vector3(0, 0.3, -1.4))
	harpoon.rotation.x = PI / 2
	harpoon.name = "harpoon"
	var head := Shapes.put(harpoon, Shapes.cyl(0.0, 0.25, 0.7, 6), Shapes.mat(Color(0.55, 0.55, 0.58), 0.35, 0.9), Vector3(0, -2.2, 0))
	head.rotation.x = PI
	# Shield boards.
	var sh := Shapes.put(yaw, Shapes.box(Vector3(2.6, 1.3, 0.15)), wood_m, Vector3(0, 0.5, -1.2))
	sh.rotation.x = -0.2
	turrets.append(yaw)

## Points a turret at a target (body coordinates); reload 0..1 (the harpoon slides back while reloading).
func aim_turret(i: int, target: Vector3, reload: float) -> void:
	var yaw := turrets[i]
	var d := target - yaw.position
	yaw.rotation.y = lerp_angle(yaw.rotation.y, atan2(-d.x, -d.z), 0.25)
	var pitch := yaw.get_node("pitch") as Node3D
	pitch.rotation.x = lerpf(pitch.rotation.x, atan2(d.y, Vector2(d.x, d.z).length()), 0.25)
	var h := pitch.get_node("harpoon") as Node3D
	h.visible = reload < 0.6
	h.position.z = -1.4 + reload * 1.5

func _forge() -> void:
	var p := at(0, 0)
	var stone := Shapes.mat(Color(0.4, 0.37, 0.34), 0.95)
	Shapes.put(self, Shapes.cyl(3.0, 3.6, 2.8, 12), stone, p + Vector3(0, 1.0, 0))
	Shapes.put(self, Shapes.cyl(2.4, 3.0, 1.2, 12), stone, p + Vector3(0, 2.9, 0))
	var chim := Shapes.put(self, Shapes.cyl(0.8, 1.1, 6.0, 8), copper_m, p + Vector3(0.6, 6.0, 0.8))
	chim.rotation.z = -0.06
	forge_m = Shapes.mat(Color(1, 0.5, 0.15), 0.5, 0.0, Color(1.0, 0.45, 0.1), 4.0)
	Shapes.put(self, Shapes.box(Vector3(1.6, 1.2, 0.3)), forge_m, p + Vector3(0, 1.3, -3.25))
	Shapes.put(self, Shapes.box(Vector3(0.3, 1.2, 1.6)), forge_m, p + Vector3(3.25, 1.3, 0))
	# Copper pipes down into the body, a valve wheel.
	for k in 4:
		var a := k * TAU / 4 + 0.4
		var pipe := Shapes.put(self, Shapes.cyl(0.22, 0.22, 3.2, 6), copper_m, p + Vector3(cos(a) * 3.4, 0.6, sin(a) * 3.4))
		pipe.rotation = Vector3(0.0, 0.0, 0.0)
	Shapes.put(self, Shapes.torus(0.5, 0.7, 12), copper_m, p + Vector3(-3.6, 1.8, -0.6)).rotation.z = PI / 2
	forge_light = OmniLight3D.new()
	forge_light.light_color = Color(1, 0.55, 0.2)
	forge_light.omni_range = 16.0
	forge_light.light_energy = 3.0
	forge_light.position = p + Vector3(0, 2.0, -4.0)
	add_child(forge_light)
	smoke = Fx.smoke(46, 3.4)
	smoke.position = p + Vector3(0.8, 9.2, 1.0)
	add_child(smoke)
	steam = Fx.steam(40)
	steam.position = p + Vector3(-3.6, 2.6, -0.6)
	steam.emitting = false
	add_child(steam)

func _tower() -> void:
	var post: Vector2i = March.POSTS[March.P.LOOKOUT]
	var p := at(post.x, post.y)
	for sx in [-1, 1]:
		for sz in [-1, 1]:
			var leg := Shapes.put(self, Shapes.cyl(0.18, 0.22, 1.0, 6), dark_wood_m, Vector3.ZERO)
			Shapes.span(leg, p + Vector3(sx * 1.6, 0, sz * 1.6), p + Vector3(sx * 0.9, 11.5, sz * 0.9), 1.0)
			leg.position = (p + Vector3(sx * 1.6, 0, sz * 1.6) + p + Vector3(sx * 0.9, 11.5, sz * 0.9)) * 0.5
	Shapes.put(self, Shapes.box(Vector3(3.2, 0.3, 3.2)), wood_m, p + Vector3(0, 11.5, 0))
	Shapes.put(self, Shapes.box(Vector3(3.2, 0.9, 0.15)), wood_m, p + Vector3(0, 12.1, 1.55))
	Shapes.put(self, Shapes.box(Vector3(3.2, 0.9, 0.15)), wood_m, p + Vector3(0, 12.1, -1.55))
	Shapes.put(self, Shapes.box(Vector3(0.15, 0.9, 3.2)), wood_m, p + Vector3(1.55, 12.1, 0))
	var roof := Shapes.put(self, Shapes.cyl(0.0, 2.6, 1.8, 4), _cloth(Color(0.7, 0.55, 0.38)), p + Vector3(0, 14.3, 0))
	roof.rotation.y = PI / 4
	Shapes.put(self, Shapes.cyl(0.06, 0.06, 4.0, 4), dark_wood_m, p + Vector3(0, 16.5, 0))
	var flag := Shapes.put(self, Shapes.quad(Vector2(2.2, 1.0)), _cloth(Color(0.2, 0.42, 0.5), Color(0.85, 0.8, 0.6), 2.0, 3.0), p + Vector3(0, 18.0, 1.1))
	flag.rotation.y = PI / 2
	# A ladder.
	var lad := Shapes.put(self, Shapes.box(Vector3(0.8, 11.0, 0.1)), dark_wood_m, p + Vector3(0, 5.6, -1.8))
	lad.rotation.x = 0.08

func _village() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 42
	var huts := [Vector2i(-300, 750), Vector2i(-500, -700), Vector2i(-1400, -250), Vector2i(-1650, 380), Vector2i(1150, -480), Vector2i(-850, 1050), Vector2i(-1150, -900)]
	var cols := [Color(0.78, 0.66, 0.5), Color(0.66, 0.42, 0.3), Color(0.55, 0.6, 0.58), Color(0.82, 0.74, 0.6)]
	for i in huts.size():
		var h: Vector2i = huts[i]
		var p := at(h.x, h.y)
		var w := rng.randf_range(2.6, 3.6)
		var d := rng.randf_range(2.4, 3.2)
		var ht := rng.randf_range(2.0, 2.6)
		var root := Node3D.new()
		root.position = p
		root.rotation.y = rng.randf_range(-0.5, 0.5)
		add_child(root)
		Shapes.put(root, Shapes.box(Vector3(w, ht, d)), wood_m if i % 2 == 0 else dark_wood_m, Vector3(0, ht / 2, 0))
		var roof := Shapes.put(root, Shapes.cyl(0.0, maxf(w, d) * 0.85, 1.6, 4), _cloth(cols[i % 4], Color(0.6, 0.25, 0.15), 4.0 if i % 3 == 0 else 0.0, i), Vector3(0, ht + 0.8, 0))
		roof.rotation.y = PI / 4
		Shapes.put(root, Shapes.box(Vector3(0.8, 1.4, 0.05)), Shapes.mat(Color(0.1, 0.08, 0.06)), Vector3(0, 0.7, -d / 2 - 0.03))
		# Crates and barrels.
		for k in 2:
			var c := Shapes.put(root, Shapes.box(Vector3(0.8, 0.8, 0.8)), wood_m, Vector3(w / 2 + 0.6, 0.4, k * 0.9 - 0.4))
			c.rotation.y = rng.randf()
		Shapes.put(root, Shapes.cyl(0.4, 0.4, 1.0, 8), dark_wood_m, Vector3(-w / 2 - 0.5, 0.5, 0.3))
	# Masts and sails.
	var masts := [Vector2i(-1500, -650), Vector2i(600, 350)]
	var tops: Array[Vector3] = []
	for i in masts.size():
		var m: Vector2i = masts[i]
		var p := at(m.x, m.y)
		var hgt := 13.0 - i * 2.5
		Shapes.put(self, Shapes.cyl(0.25, 0.35, hgt, 8), dark_wood_m, p + Vector3(0, hgt / 2, 0))
		Shapes.put(self, Shapes.cyl(0.15, 0.15, 7.0, 6), dark_wood_m, p + Vector3(0, hgt - 1.5, 0)).rotation.z = PI / 2
		var sail := Shapes.put(self, Shapes.quad(Vector2(6.6, hgt * 0.55)), _cloth(Color(0.86, 0.78, 0.62), Color(0.7, 0.3, 0.18), 3.0 if i == 0 else 0.0, 7.0 + i), p + Vector3(0, hgt - 1.6 - hgt * 0.275, 0.3))
		sail.rotation.y = 0.0
		tops.append(p + Vector3(0, hgt, 0))
	var tower_p := at(March.POSTS[4].x, March.POSTS[4].y) + Vector3(0, 13, 0)
	_bunting(tops[0], tower_p + Vector3(0, -2.0, 0), 1.0)
	_bunting(tops[0], tops[1], 2.0)
	_bunting(tops[1], at(1150, -480) + Vector3(0, 3.5, 0), 3.0)
	_bunting(tops[0], at(-1650, 380) + Vector3(0, 3.5, 0), 4.0)

func update_forge(heat: float, venting: bool, dt: float) -> void:
	_heat = lerpf(_heat, heat, minf(1.0, dt * 3.0))
	var flick := 0.85 + 0.15 * sin(Time.get_ticks_msec() * 0.013) * sin(Time.get_ticks_msec() * 0.007)
	forge_m.emission_energy_multiplier = (0.6 + 5.0 * _heat) * flick
	forge_light.light_energy = (0.5 + 4.0 * _heat) * flick
	smoke.amount_ratio = clampf(0.2 + _heat, 0.2, 1.0)
	steam.emitting = venting
