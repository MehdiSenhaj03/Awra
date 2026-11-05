class_name World
extends Node3D
## The Ash Sea in 3D around Ama: the sky and its dust, the dunes scrolling under her feet, the scenery of each
## land (bones and dead trees, sunken towers, petrified forests, an oasis), the hazards on the route (chasms,
## rockfalls in gorges, geysers, ash storms, wrecks to salvage), the scavengers, the raiders' sand skiffs and the
## boarders, fires on the deck, harpoons and their ropes, and the wild titan in a fight. Metres; Ama stands at the
## origin facing -Z; the land moves.

const KINDS := {
	"ash": {"sun": Color(1.0, 0.86, 0.7), "sun_e": 2.0, "hz": Color(0.9, 0.66, 0.45), "zen": Color(0.42, 0.47, 0.52), "fog": Color(0.72, 0.6, 0.5), "tint": Color(0.9, 0.9, 0.9), "elev": -22.0, "az": -140.0},
	"ruins": {"sun": Color(0.92, 0.9, 0.85), "sun_e": 1.6, "hz": Color(0.72, 0.68, 0.62), "zen": Color(0.38, 0.45, 0.55), "fog": Color(0.62, 0.6, 0.58), "tint": Color(0.92, 0.93, 0.96), "elev": -35.0, "az": -60.0},
	"forest": {"sun": Color(1.0, 0.78, 0.7), "sun_e": 1.8, "hz": Color(0.95, 0.68, 0.6), "zen": Color(0.4, 0.42, 0.55), "fog": Color(0.78, 0.64, 0.6), "tint": Color(0.95, 0.92, 0.95), "elev": -12.0, "az": 150.0},
	"storm": {"sun": Color(0.85, 0.7, 0.6), "sun_e": 1.2, "hz": Color(0.6, 0.45, 0.34), "zen": Color(0.25, 0.24, 0.26), "fog": Color(0.45, 0.36, 0.3), "tint": Color(0.8, 0.76, 0.74), "elev": -25.0, "az": -120.0},
	"oasis": {"sun": Color(1.0, 0.94, 0.82), "sun_e": 2.2, "hz": Color(0.95, 0.78, 0.55), "zen": Color(0.35, 0.5, 0.62), "fog": Color(0.8, 0.7, 0.55), "tint": Color(1, 0.98, 0.94), "elev": -48.0, "az": -100.0},
}
const CHUNK := 10_000              # cm of route per scenery chunk

var m: March
var kind := "ash"
var env: Environment
var sun: DirectionalLight3D
var sky_m: ShaderMaterial
var ground_m: ShaderMaterial
var ama: Colossus
var deck: Deck
var foe: Colossus = null
var land: Node3D
var shake := 0.0
var _chunks: Dictionary = {}       # chunk index -> Node3D
var _hz: Dictionary = {}           # hazard index -> Node3D
var _birds: Dictionary = {}        # id -> {node, wings, prev}
var _skiffs: Dictionary = {}
var _shots: Dictionary = {}        # Shot -> {harpoon, rope}
var _fires: Array[Node3D] = []
var _weak: Array[MeshInstance3D] = []
var _dust_wall: Node3D = null
var _storm_p: GPUParticles3D
var _flash := 0.0
var _t := 0.0
var _storm_k := 0.0
var _base: Dictionary
var _mats: Dictionary = {}

func _init(march: March, k: String) -> void:
	m = march
	kind = k if KINDS.has(k) else "ash"
	_base = KINDS[kind]
	_environment()
	_ground()
	land = Node3D.new()
	add_child(land)
	ama = Colossus.new(false)
	add_child(ama)
	deck = Deck.new()
	ama.body.add_child(deck)
	ama.footfall.connect(_on_footfall)
	_storm_p = Fx.make(160, 1.4, Vector3(0, 0, 1), 10, Vector2(40, 60), Vector3.ZERO, Vector2(1.5, 3.0), 2.0,
		[Color(0.5, 0.4, 0.32, 0.0), Color(0.45, 0.36, 0.3, 0.45), Color(0.4, 0.33, 0.28, 0.0)], false, Vector3(60, 25, 40))
	_storm_p.emitting = false
	add_child(_storm_p)
	_mats.bone = Shapes.mat(Color(0.86, 0.82, 0.72), 0.85)
	_mats.rock = _rock_mat(Color(0.5, 0.47, 0.43), 0.0)
	_mats.dark = _rock_mat(Color(0.32, 0.3, 0.29), 0.0)
	_mats.white = _rock_mat(Color(0.86, 0.84, 0.8), 0.0)
	_mats.ruin = _rock_mat(Color(0.58, 0.55, 0.5), 0.15)
	_mats.wood = Shapes.mat(Color(0.33, 0.24, 0.17), 0.9)
	_mats.leaf = Shapes.mat(Color(0.3, 0.5, 0.18), 0.8)

func _rock_mat(c: Color, moss: float) -> ShaderMaterial:
	var sm := ShaderMaterial.new()
	sm.shader = preload("res://src/render/shaders/stone.gdshader")
	sm.set_shader_parameter("rock", c)
	sm.set_shader_parameter("rock_dark", c * 0.55)
	sm.set_shader_parameter("moss_amt", moss)
	sm.set_shader_parameter("bump", 1.0)
	return sm

## A titan-relative point (sim cm: x forward, y left, z up) in metres.
static func p3(x: int, y: int, z: int = 0) -> Vector3:
	return Vector3(-y / 100.0, z / 100.0, -x / 100.0)

# ---- Sky, light, ground ----------------------------------------------------------------------------------------------------

func _environment() -> void:
	var we := WorldEnvironment.new()
	env = Environment.new()
	var sky := Sky.new()
	sky_m = ShaderMaterial.new()
	sky_m.shader = preload("res://src/render/shaders/sky.gdshader")
	sky_m.set_shader_parameter("horizon", _base.hz)
	sky_m.set_shader_parameter("zenith", _base.zen)
	sky.sky_material = sky_m
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_energy = 0.35
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.tonemap_exposure = 0.92
	env.tonemap_white = 6.0
	env.glow_enabled = true
	env.glow_intensity = 0.6
	env.glow_hdr_threshold = 1.1
	env.fog_enabled = true
	env.fog_light_color = _base.fog
	env.fog_density = 0.0011
	env.fog_sky_affect = 0.12
	env.adjustment_enabled = true
	env.adjustment_contrast = 1.12
	env.adjustment_saturation = 1.0
	we.environment = env
	add_child(we)
	sun = DirectionalLight3D.new()
	sun.light_color = _base.sun
	sun.light_energy = _base.sun_e
	sun.rotation = Vector3(deg_to_rad(_base.elev), deg_to_rad(_base.az), 0)
	sun.shadow_enabled = true
	sun.directional_shadow_max_distance = 420.0
	add_child(sun)

func set_quality(q: int) -> void:
	sun.shadow_enabled = q >= 1
	env.ssao_enabled = q >= 2
	env.ssao_radius = 3.0
	env.ssao_intensity = 1.5

func _ground() -> void:
	ground_m = ShaderMaterial.new()
	ground_m.shader = preload("res://src/render/shaders/ash.gdshader")
	ground_m.set_shader_parameter("tint", _base.tint)
	var pm := PlaneMesh.new()
	pm.size = Vector2(5000, 5000)
	pm.subdivide_width = 160
	pm.subdivide_depth = 160
	var g := Shapes.put(self, pm, ground_m, Vector3(0, 0, -900))
	g.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF

# ---- Scenery ----------------------------------------------------------------------------------------------------------------

func _chunk(k: int) -> Node3D:
	var root := Node3D.new()
	land.add_child(root)
	var rng := RandomNumberGenerator.new()
	rng.seed = k * 7919 + kind.hash()
	var n := 9 if kind != "ruins" else 12
	for i in n:
		var wx := k * CHUNK + rng.randi_range(0, CHUNK)
		var side := -1 if rng.randf() < 0.5 else 1
		var wy := side * rng.randi_range(7000, 70000)
		var pos := Vector3(-wy / 100.0, 0, -wx / 100.0)
		var r := rng.randf()
		match kind:
			"ruins":
				if r < 0.45: _tower(root, pos, rng)
				elif r < 0.65: _arch(root, pos, rng)
				else: _rock(root, pos, rng, 3.0, 9.0)
			"forest":
				if r < 0.7: _petrified(root, pos, rng)
				else: _rock(root, pos, rng, 2.0, 6.0)
			"oasis":
				if r < 0.3: _deadtree(root, pos, rng)
				elif r < 0.5: _bones(root, pos, rng)
				else: _rock(root, pos, rng, 2.0, 7.0)
			"storm":
				if r < 0.25: _bones(root, pos, rng)
				elif r < 0.45: _deadtree(root, pos, rng)
				else: _rock(root, pos, rng, 3.0, 12.0)
			_:
				if r < 0.3: _bones(root, pos, rng)
				elif r < 0.5: _deadtree(root, pos, rng)
				elif r < 0.62: _tower(root, pos, rng)
				else: _rock(root, pos, rng, 2.0, 8.0)
	return root

func _rock(p: Node3D, pos: Vector3, rng: RandomNumberGenerator, a: float, b: float) -> void:
	var s := rng.randf_range(a, b)
	var mi := Shapes.put(p, Shapes.cached("rock%d" % rng.randi_range(0, 3), func() -> Mesh: return Shapes.blob(1, 0.8, 0.3, 1.2, 1.0, 1.4, 9, 6)), _mats.rock if rng.randf() < 0.7 else _mats.dark, pos)
	mi.scale = Vector3(s * rng.randf_range(0.8, 1.4), s * rng.randf_range(0.5, 1.0), s)
	mi.rotation.y = rng.randf() * TAU

func _bones(p: Node3D, pos: Vector3, rng: RandomNumberGenerator) -> void:
	# The rib cage of something huge, half buried.
	var root := Node3D.new()
	root.position = pos
	root.rotation.y = rng.randf() * TAU
	p.add_child(root)
	var s := rng.randf_range(0.8, 1.6)
	for i in 6:
		for sd in [-1, 1]:
			var rib := Shapes.put(root, Shapes.cached("rib", func() -> Mesh: return Shapes.bone(0.6, 0.25, 6)), _mats.bone, Vector3.ZERO)
			Shapes.span(rib, Vector3(sd * 1.0, 0, i * 4.0) * s, Vector3(sd * (7.0 - absf(i - 2.5)), 9.0 - absf(i - 2.5) * 1.2, i * 4.0 + 2.0) * s)
	var spine := Shapes.put(root, Shapes.cached("spine", func() -> Mesh: return Shapes.bone(0.9, 0.5, 6)), _mats.bone, Vector3.ZERO)
	Shapes.span(spine, Vector3(0, 0.3, -4) * s, Vector3(0, 0.5, 26) * s)

func _deadtree(p: Node3D, pos: Vector3, rng: RandomNumberGenerator) -> void:
	var root := Node3D.new()
	root.position = pos
	root.rotation.y = rng.randf() * TAU
	p.add_child(root)
	var h := rng.randf_range(6, 12)
	var t := Shapes.put(root, Shapes.cached("trunk", func() -> Mesh: return Shapes.bone(0.5, 0.2, 6)), _mats.wood, Vector3.ZERO)
	Shapes.span(t, Vector3.ZERO, Vector3(rng.randf_range(-1, 1), h, 0))
	for i in 4:
		var a := rng.randf() * TAU
		var y0 := h * rng.randf_range(0.45, 0.85)
		var br := Shapes.put(root, Shapes.cached("branch", func() -> Mesh: return Shapes.bone(0.2, 0.06, 5)), _mats.wood, Vector3.ZERO)
		Shapes.span(br, Vector3(0, y0, 0), Vector3(cos(a) * 3.5, y0 + rng.randf_range(1, 3), sin(a) * 3.5))

func _petrified(p: Node3D, pos: Vector3, rng: RandomNumberGenerator) -> void:
	# A white stone tree, enormous.
	var root := Node3D.new()
	root.position = pos
	p.add_child(root)
	var h := rng.randf_range(35, 70)
	var t := Shapes.put(root, Shapes.cached("ptrunk", func() -> Mesh: return Shapes.bone(3.0, 1.2, 8)), _mats.white, Vector3.ZERO)
	Shapes.span(t, Vector3(0, -2, 0), Vector3(rng.randf_range(-3, 3), h, rng.randf_range(-3, 3)))
	for i in 5:
		var a := rng.randf() * TAU
		var y0 := h * rng.randf_range(0.5, 0.9)
		var br := Shapes.put(root, Shapes.cached("pbranch", func() -> Mesh: return Shapes.bone(1.1, 0.3, 6)), _mats.white, Vector3.ZERO)
		var l := rng.randf_range(8, 16)
		Shapes.span(br, Vector3(0, y0, 0), Vector3(cos(a) * l, y0 + rng.randf_range(4, 10), sin(a) * l))
	for i in 3:
		var a := rng.randf() * TAU
		var rt := Shapes.put(root, Shapes.cached("proot", func() -> Mesh: return Shapes.bone(1.4, 0.3, 6)), _mats.white, Vector3.ZERO)
		Shapes.span(rt, Vector3(0, 3, 0), Vector3(cos(a) * 7, -0.5, sin(a) * 7))

func _tower(p: Node3D, pos: Vector3, rng: RandomNumberGenerator) -> void:
	# A sunken tower: a leaning shaft with a broken top and windows.
	var root := Node3D.new()
	root.position = pos
	root.rotation = Vector3(rng.randf_range(-0.12, 0.12), rng.randf() * TAU, rng.randf_range(-0.12, 0.12))
	p.add_child(root)
	var w := rng.randf_range(6, 12)
	var h := rng.randf_range(20, 55)
	Shapes.put(root, Shapes.cyl(w * 0.5, w * 0.6, h, 8), _mats.ruin, Vector3(0, h / 2 - 3, 0))
	for i in 3:
		var c := Shapes.put(root, Shapes.box(Vector3(w * 0.4, rng.randf_range(2, 7), w * 0.3)), _mats.ruin, Vector3(rng.randf_range(-w, w) * 0.3, h - 3 + 1.5, rng.randf_range(-w, w) * 0.3))
		c.rotation.y = rng.randf() * TAU
	for i in int(h / 8):
		Shapes.put(root, Shapes.box(Vector3(1.2, 2.2, 0.4)), Shapes.mat(Color(0.08, 0.07, 0.06)), Vector3(0, 4 + i * 7.0, -w * 0.5))

func _arch(p: Node3D, pos: Vector3, rng: RandomNumberGenerator) -> void:
	var root := Node3D.new()
	root.position = pos
	root.rotation.y = rng.randf() * TAU
	p.add_child(root)
	var h := rng.randf_range(14, 24)
	for sd in [-1, 1]:
		Shapes.put(root, Shapes.box(Vector3(3, h, 3)), _mats.ruin, Vector3(sd * 7, h / 2 - 1, 0))
	Shapes.put(root, Shapes.box(Vector3(17, 3, 3.4)), _mats.ruin, Vector3(0, h, 0)).rotation.z = rng.randf_range(-0.1, 0.1)

# ---- Hazards ------------------------------------------------------------------------------------------------------------------

func _hazard(h: March.Hazard) -> Node3D:
	var root := Node3D.new()
	root.position = Vector3(-h.y / 100.0, 0, -h.at / 100.0)
	land.add_child(root)
	var rng := RandomNumberGenerator.new()
	rng.seed = h.at
	match h.kind:
		March.H.CHASM:
			var w := h.r / 100.0 * 2.0 + 14.0
			var q := PlaneMesh.new()
			q.size = Vector2(w, 16)
			var cm := ShaderMaterial.new()
			cm.shader = preload("res://src/render/shaders/chasm.gdshader")
			cm.set_shader_parameter("seed", float(h.at % 97))
			Shapes.put(root, q, cm, Vector3(0, 0.08, 0))
			for i in 10:
				var x := -w / 2 + i * w / 9.0
				for sd in [-1, 1]:
					var r := Shapes.put(root, Shapes.cached("rock1", func() -> Mesh: return Shapes.blob(1, 0.8, 0.3, 1.2, 1.0, 1.4, 9, 6)), _mats.dark, Vector3(x + rng.randf_range(-2, 2), 0, sd * rng.randf_range(7, 9)))
					r.scale = Vector3.ONE * rng.randf_range(1.0, 2.2)
			var glow := OmniLight3D.new()
			glow.light_color = Color(1, 0.4, 0.1)
			glow.omni_range = w * 0.6
			glow.light_energy = 1.5
			glow.position = Vector3(0, -2, 0)
			root.add_child(glow)
		March.H.ROCKS:
			# A gorge: cliffs on both sides, boulders ready to fall.
			for sd in [-1, 1]:
				for i in 7:
					var r := Shapes.put(root, Shapes.cached("rock2", func() -> Mesh: return Shapes.blob(1, 0.8, 0.3, 1.2, 1.0, 1.4, 9, 6)), _mats.rock if i % 2 == 0 else _mats.dark, Vector3(sd * rng.randf_range(48, 60), 0, -42 + i * 14))
					r.scale = Vector3(rng.randf_range(12, 18), rng.randf_range(30, 46), rng.randf_range(10, 14))
					r.rotation.y = rng.randf() * TAU
			var boulders := Node3D.new()
			boulders.name = "boulders"
			root.add_child(boulders)
			for i in 5:
				var b := Shapes.put(boulders, Shapes.cached("rock3", func() -> Mesh: return Shapes.blob(1, 0.9, 0.8, 1.1, 1.0, 1.6, 9, 6)), _mats.dark, Vector3.ZERO)
				b.scale = Vector3.ONE * rng.randf_range(3.0, 5.5)
				b.set_meta("side", -1 if i % 2 == 0 else 1)
				b.set_meta("k", float(i))
		March.H.GEYSER:
			var ring := Shapes.put(root, Shapes.torus(h.r / 100.0 * 0.5, h.r / 100.0 * 0.7, 16), _mats.dark, Vector3(0, 0.3, 0))
			ring.scale.y = 0.5
			var pool := Shapes.put(root, Shapes.cyl(h.r / 100.0 * 0.55, h.r / 100.0 * 0.55, 0.2, 16), Shapes.mat(Color(1, 0.55, 0.2), 0.5, 0.0, Color(1, 0.45, 0.12), 2.5), Vector3(0, 0.1, 0))
			pool.name = "pool"
			var st := Fx.make(70, 3.5, Vector3(0, 1, 0), 8, Vector2(22, 32), Vector3(0, -2, 4), Vector2(4, 7), 3.0,
				[Color(1, 1, 1, 0.0), Color(0.95, 0.93, 0.9, 0.6), Color(0.9, 0.88, 0.85, 0.0)])
			st.name = "steam"
			root.add_child(st)
		March.H.OASIS:
			var water := ShaderMaterial.new()
			water.shader = preload("res://src/render/shaders/water.gdshader")
			var pond := Shapes.put(root, Shapes.cyl(26, 26, 0.3, 32), water, Vector3(-45, 0.15, 0))
			pond.scale.z = 0.6
			for i in 22:
				var a := rng.randf() * TAU
				var d := rng.randf_range(28, 70)
				_palm(root, Vector3(-45 + cos(a) * d, 0, sin(a) * d * 0.7), rng)
			ground_m.set_shader_parameter("green", 1.0)
		March.H.STORM:
			# The wall of the storm: tall dust billows across the route at its leading edge.
			var wall := Node3D.new()
			wall.position.z = h.r / 100.0
			root.add_child(wall)
			var dm := ShaderMaterial.new()
			dm.shader = preload("res://src/render/shaders/dust.gdshader")
			dm.set_shader_parameter("col", Color(0.35, 0.28, 0.24))
			dm.set_shader_parameter("alpha", 0.85)
			dm.set_shader_parameter("soft", 6.0)
			for i in 26:
				var b := Shapes.put(wall, Shapes.cached("dustq", func() -> Mesh: return Shapes.quad(Vector2(1, 1))), dm, Vector3(rng.randf_range(-500, 500), rng.randf_range(20, 140), rng.randf_range(-20, 20)))
				b.scale = Vector3.ONE * rng.randf_range(120, 220)
		March.H.WRECK:
			var hull := Node3D.new()
			hull.name = "hull"
			hull.rotation = Vector3(rng.randf_range(-0.2, 0.2), rng.randf() * TAU, rng.randf_range(0.2, 0.45))
			root.add_child(hull)
			var hp := PackedVector2Array([Vector2(0.1, -7.5), Vector2(1.6, -5), Vector2(2.4, -1), Vector2(2.4, 3), Vector2(1.8, 6), Vector2(0.2, 7.5)])
			var hm := Shapes.put(hull, Shapes.cached("hull", func() -> Mesh: return Shapes.lathe(hp, 12, 0.7)), _mats.wood, Vector3(0, 0.5, 0))
			hm.rotation.x = PI / 2
			Shapes.put(hull, Shapes.cyl(0.2, 0.25, 9, 6), _mats.wood, Vector3(0, 4.5, -1)).rotation.z = 0.3
			var sail := Shapes.put(hull, Shapes.quad(Vector2(4, 4)), deck._cloth(Color(0.7, 0.6, 0.48), Color(0.5, 0.2, 0.1), 3.0, float(h.at % 13)), Vector3(1.2, 5.5, -1))
			sail.rotation.y = PI / 2
			for i in 4:
				Shapes.put(hull, Shapes.box(Vector3(0.9, 0.9, 0.9)), _mats.wood, Vector3(rng.randf_range(-3, 3), 0.4, rng.randf_range(-5, 5)))
			# A beacon so the gunners can find it.
			var beacon := Shapes.put(root, Shapes.cyl(0.05, 0.05, 14, 4), Shapes.mat(Color(1, 0.8, 0.4), 0.5, 0, Color(1, 0.7, 0.3), 1.5), Vector3(0, 7, 0))
			beacon.name = "beacon"
	return root

func _palm(p: Node3D, pos: Vector3, rng: RandomNumberGenerator) -> void:
	var root := Node3D.new()
	root.position = pos
	p.add_child(root)
	var h := rng.randf_range(8, 15)
	var top := Vector3(rng.randf_range(-2, 2), h, rng.randf_range(-2, 2))
	var t := Shapes.put(root, Shapes.cached("palmt", func() -> Mesh: return Shapes.bone(0.45, 0.3, 6)), _mats.wood, Vector3.ZERO)
	Shapes.span(t, Vector3.ZERO, top)
	for i in 7:
		var a := i * TAU / 7 + rng.randf() * 0.4
		var leaf := Shapes.put(root, Shapes.cached("frond", func() -> Mesh: return Shapes.box(Vector3(1.2, 0.08, 5.0))), _mats.leaf, top + Vector3(cos(a) * 2.2, -0.6, sin(a) * 2.2))
		leaf.rotation = Vector3(0.45, -a + PI / 2, 0)

# ---- Frame ------------------------------------------------------------------------------------------------------------------------

func sync(dt: float, v_mps: float) -> void:
	_t += dt
	var prog := m.progress
	land.position = Vector3(m.lateral / 100.0, 0, prog / 100.0)
	ground_m.set_shader_parameter("offset", Vector2(m.lateral / 100.0, prog / 100.0))
	# Scenery chunks around.
	var k0 := prog / CHUNK - 2
	var k1 := (prog + 140_000) / CHUNK
	for k in range(k0, k1 + 1):
		if not _chunks.has(k):
			_chunks[k] = _chunk(k)
	for k: int in _chunks.keys():
		if k < k0 or k > k1:
			(_chunks[k] as Node3D).queue_free()
			_chunks.erase(k)
	# Hazards.
	var green := 0.0
	for i in m.hazards.size():
		var h: March.Hazard = m.hazards[i]
		var ahead := h.at - prog
		var near := ahead < 150_000 and ahead > -(30_000 + h.r)
		if near and not _hz.has(i):
			_hz[i] = _hazard(h)
		elif not near and _hz.has(i):
			(_hz[i] as Node3D).queue_free()
			_hz.erase(i)
		if _hz.has(i):
			_animate_hazard(h, _hz[i], ahead)
			if h.kind == March.H.OASIS:
				green = 1.0
				ground_m.set_shader_parameter("oasis", Vector2(-h.y / 100.0 - 45.0, -h.at / 100.0))
	ground_m.set_shader_parameter("green", green)
	# Storm.
	_storm_k = move_toward(_storm_k, 1.0 if m.in_storm else 0.0, dt * 0.4)
	sky_m.set_shader_parameter("dust", _storm_k * 0.85)
	env.fog_density = lerpf(clampf(60.0 / (m.vision / 100.0) * 0.009, 0.0005, 0.003), 0.011, _storm_k)
	env.fog_light_color = (_base.fog as Color).lerp(Color(0.32, 0.25, 0.2), _storm_k)
	sun.light_energy = lerpf(_base.sun_e, 0.35, _storm_k)
	_storm_p.emitting = _storm_k > 0.3
	if _storm_k > 0.5 and randf() < dt * 0.25:
		_flash = 1.0
	_flash = maxf(0.0, _flash - dt * 4.0)
	sky_m.set_shader_parameter("flash", _flash * _storm_k)
	# Ama walking; her heading follows the steering.
	ama.rotation.y = lerp_angle(ama.rotation.y, m.steer / 8.0 * 0.1 * clampf(v_mps / 10.0, 0.0, 1.0), dt * 2.0)
	ama.animate(dt, v_mps)
	_sync_threats(dt)
	_sync_fight(dt)
	shake = maxf(0.0, shake - dt * 2.5)

func _animate_hazard(h: March.Hazard, n: Node3D, ahead: int) -> void:
	match h.kind:
		March.H.ROCKS:
			var bs := n.get_node_or_null("boulders")
			if bs == null:
				return
			var f := clampf(1.0 - ahead / 6000.0, 0.0, 1.4)
			for b: Node3D in bs.get_children():
				var sd: int = b.get_meta("side")
				var k: float = b.get_meta("k")
				var fk := clampf(f - k * 0.06, 0.0, 1.3)
				var top := Vector3(sd * 46.0, 55.0 + k * 3.0, -10.0 + k * 6.0)
				var hit := Vector3(-(m.lateral - h.y) / 100.0 + sd * 12.0, 2.0, k * 4.0 - 6.0)
				var pos := top.lerp(hit, minf(fk, 1.0))
				pos.y += sin(minf(fk, 1.0) * PI) * 10.0
				if fk > 1.0:
					pos.y = lerpf(2.0, -6.0, (fk - 1.0) / 0.3)
				b.position = pos
				b.rotation.x += 0.05 * fk
				b.visible = h.seen or ahead < 9000
		March.H.GEYSER:
			var st := n.get_node("steam") as GPUParticles3D
			st.emitting = fmod(_t + h.at * 0.001, 9.0) < 4.0
		March.H.WRECK:
			var hull := n.get_node_or_null("hull") as Node3D
			if hull and h.done and ahead > -15_000:
				hull.position.y = lerpf(hull.position.y, -8.0, 0.02)
				(n.get_node("beacon") as Node3D).visible = false

func _on_footfall(pos: Vector3, k: float) -> void:
	var d := Fx.dust(26, 7.0)
	Fx.burst(self, d, pos + Vector3(0, 1.0, 0))
	shake = maxf(shake, 0.25 * k)

# ---- Threats -----------------------------------------------------------------------------------------------------------------

func _bird() -> Dictionary:
	var root := Node3D.new()
	add_child(root)
	var dark := Shapes.mat(Color(0.18, 0.15, 0.14), 0.9)
	var red := Shapes.mat(Color(0.55, 0.2, 0.15), 0.8)
	var b := Shapes.put(root, Shapes.cached("bbody", func() -> Mesh: return Shapes.blob(0.8, 0.7, 0.6, 2.2, 2.0, 2.0, 10, 6)), dark, Vector3.ZERO)
	b.scale = Vector3.ONE
	Shapes.put(root, Shapes.cached("bhead", func() -> Mesh: return Shapes.sphere(0.45, 8, 5)), red, Vector3(0, 0.3, -2.3))
	var beak := Shapes.put(root, Shapes.cached("beak", func() -> Mesh: return Shapes.cyl(0.0, 0.18, 1.1, 5)), Shapes.mat(Color(0.85, 0.8, 0.6)), Vector3(0, 0.15, -3.0))
	beak.rotation.x = -PI / 2
	var wings: Array[Node3D] = []
	for sd in [-1, 1]:
		var w := Node3D.new()
		w.position = Vector3(sd * 0.6, 0.2, -0.3)
		root.add_child(w)
		var q := Shapes.put(w, Shapes.cached("wing", func() -> Mesh: return Shapes.box(Vector3(4.2, 0.08, 1.8))), dark, Vector3(sd * 2.1, 0, 0.2))
		q.rotation.y = sd * -0.15
		wings.append(w)
	return {"node": root, "wings": wings, "prev": Vector3.ZERO}

func _skiff() -> Dictionary:
	var root := Node3D.new()
	add_child(root)
	var hp := PackedVector2Array([Vector2(0.1, -5), Vector2(1.0, -3), Vector2(1.5, 0), Vector2(1.3, 3), Vector2(0.2, 4.5)])
	var hm := Shapes.put(root, Shapes.cached("skiffhull", func() -> Mesh: return Shapes.lathe(hp, 10, 0.6)), Shapes.mat(Color(0.25, 0.16, 0.12)), Vector3(0, 1.0, 0))
	hm.rotation.x = PI / 2
	hm.scale.y = 1.0
	Shapes.put(root, Shapes.cyl(0.12, 0.15, 9, 5), _mats.wood, Vector3(0, 5.5, -0.5))
	var sail := Shapes.put(root, Shapes.quad(Vector2(5, 6.5)), deck._cloth(Color(0.55, 0.12, 0.1), Color(0.1, 0.08, 0.06), 2.0, randf() * 9.0), Vector3(0, 5.8, 0.2))
	sail.rotation.y = 0.0
	# Runners and the dust trail.
	var trail := Fx.make(30, 2.0, Vector3(0, 0.5, 1), 30, Vector2(2, 5), Vector3(0, -0.4, 0), Vector2(2.0, 3.5), 2.5,
		[Color(0.6, 0.55, 0.5, 0.0), Color(0.6, 0.55, 0.48, 0.45), Color(0.6, 0.55, 0.5, 0.0)])
	trail.position = Vector3(0, 0.5, 3.5)
	root.add_child(trail)
	for sd in [-1, 1]:
		Shapes.put(root, Shapes.box(Vector3(0.15, 0.15, 9)), _mats.wood, Vector3(sd * 1.3, 0.2, 0))
	return {"node": root}

func _sync_threats(dt: float) -> void:
	# Scavengers.
	var seen := {}
	for b in m.birds:
		seen[b.id] = true
		if not _birds.has(b.id):
			_birds[b.id] = _bird()
		var d: Dictionary = _birds[b.id]
		var n := d.node as Node3D
		var p := p3(b.x, b.y, b.z)
		var prev: Vector3 = d.prev
		var vel := (p - prev) / maxf(dt, 0.001)
		if prev != Vector3.ZERO and vel.length() > 0.5:
			var fwd := vel.normalized()
			n.look_at_from_position(p, p + fwd, Vector3.UP)
		else:
			n.position = p
		d.prev = p
		var flap := sin(_t * (9.0 if b.mode != 1 else 2.0) + b.id) * (0.6 if b.mode != 1 else 0.15)
		var wings: Array = d.wings
		(wings[0] as Node3D).rotation.z = -flap - (0.5 if b.mode == 1 else 0.0)
		(wings[1] as Node3D).rotation.z = flap + (0.5 if b.mode == 1 else 0.0)
	for id: int in _birds.keys():
		if not seen.has(id):
			(_birds[id].node as Node3D).queue_free()
			_birds.erase(id)
	# Skiffs.
	seen.clear()
	for s in m.skiffs:
		seen[s.id] = true
		if not _skiffs.has(s.id):
			_skiffs[s.id] = _skiff()
		var n := _skiffs[s.id].node as Node3D
		n.position = p3(s.x, s.y, 0)
		n.rotation.y = PI / 2 if s.y > 0 else -PI / 2
		n.rotation.z = sin(_t * 3.0 + s.id) * 0.04
	for id: int in _skiffs.keys():
		if not seen.has(id):
			var n := _skiffs[id].node as Node3D
			Fx.burst(self, Fx.dust(30, 8.0), n.position)
			n.queue_free()
			_skiffs.erase(id)
	# Boarders on the deck.
	seen.clear()
	for b in m.boarders:
		seen[b.id] = true
		if not deck.raiders.has(b.id):
			var f := Figure.new(Color(0.2, 0.16, 0.14), Color(0.6, 0.1, 0.08), Color(0.5, 0.36, 0.28), true, true)
			deck.add_child(f)
			deck.raiders[b.id] = f
		var f: Figure = deck.raiders[b.id]
		var np := Deck.at(b.x, b.y)
		f.walk = clampf((np - f.position).length() * 20.0, 0.0, 1.0)
		if (np - f.position).length() > 0.01:
			f.rotation.y = atan2(-(np - f.position).x, -(np - f.position).z)
		f.position = np
		f.work = 1.0 if f.walk < 0.2 else 0.0
		f.animate(dt)
	for id: int in deck.raiders.keys():
		if not seen.has(id):
			(deck.raiders[id] as Node3D).queue_free()
			deck.raiders.erase(id)
	# Fires.
	while _fires.size() < m.fires.size():
		var fr := Fx.fire(30)
		fr.local_coords = true
		deck.add_child(fr)
		var l := OmniLight3D.new()
		l.light_color = Color(1, 0.5, 0.15)
		l.omni_range = 8.0
		l.light_energy = 2.0
		fr.add_child(l)
		_fires.append(fr)
	while _fires.size() > m.fires.size():
		_fires.pop_back().queue_free()
	for i in m.fires.size():
		_fires[i].position = Deck.at(m.fires[i].x, m.fires[i].y, 0.3)
	# Harpoons in flight and their ropes.
	var live := {}
	for s in m.shots:
		live[s] = true
		if not _shots.has(s):
			var hn := Shapes.put(self, Shapes.cached("harp", func() -> Mesh: return Shapes.cyl(0.1, 0.1, 4.0, 5)), Shapes.mat(Color(0.6, 0.6, 0.62), 0.4, 0.8), Vector3.ZERO)
			var rope := Shapes.put(self, Shapes.cached("hrope", func() -> Mesh: return Shapes.cyl(1, 1, 1, 4)), deck.rope_m, Vector3.ZERO)
			_shots[s] = {"h": hn, "rope": rope, "from": turret_world(1 if s.gun == March.P.GUN_R else 0)}
		var d: Dictionary = _shots[s]
		var hp := p3(s.x, s.y, s.z)
		var tp := p3(s.tx, s.ty, s.tz)
		var hn := d.h as Node3D
		hn.position = hp
		if (tp - hp).length() > 0.1:
			Shapes.span(hn, hp, hp + (tp - hp).normalized() * 4.0)
			hn.position = hp + (tp - hp).normalized() * 2.0
		var rope := d.rope as Node3D
		var from: Vector3 = d.from
		Shapes.span(rope, from, hp, 0.06)
		rope.position = (from + hp) * 0.5
	for s: Object in _shots.keys():
		if not live.has(s):
			(_shots[s].h as Node3D).queue_free()
			(_shots[s].rope as Node3D).queue_free()
			_shots.erase(s)

## Where a turret's harpoon leaves from, in world coordinates.
func turret_world(i: int) -> Vector3:
	return (deck.turrets[i] as Node3D).global_position + Vector3(0, 1.2, 0)

# ---- The fight --------------------------------------------------------------------------------------------------------------

func _sync_fight(dt: float) -> void:
	if m.foe == null:
		return
	if foe == null:
		foe = Colossus.new(true)
		foe.position = p3(March.FOE_X, 0, 0)
		foe.rotation.y = PI
		match String(m.leg.get("boss_id", "")):
			"fournaise":
				# An old lava titan: blacker hide, brighter cracks.
				foe.skin.set_shader_parameter("rock", Color(0.32, 0.16, 0.12))
				foe.skin.set_shader_parameter("rock_dark", Color(0.1, 0.04, 0.03))
				foe.skin.set_shader_parameter("crack_glow", 1.8)
				foe.scale = Vector3.ONE * 1.08
			"roi":
				# The Ash King: ash-grey basalt veined with fire, the biggest of all.
				foe.skin.set_shader_parameter("rock", Color(0.36, 0.34, 0.36))
				foe.skin.set_shader_parameter("rock_dark", Color(0.12, 0.11, 0.12))
				foe.skin.set_shader_parameter("crack_col", Color(1.0, 0.55, 0.15))
				foe.skin.set_shader_parameter("crack_glow", 1.4)
				foe.scale = Vector3.ONE * 1.2
		add_child(foe)
		for i in m.foe_weak.size():
			var wm := Shapes.mat(Color(1, 0.4, 0.15), 0.3, 0.0, Color(1, 0.35, 0.1), 3.0)
			var w := Shapes.put(self, Shapes.sphere(1.6, 10, 6), wm, Vector3.ZERO)
			_weak.append(w)
		ama.fight = true
	ama.set_fight(m.titan.state, m.titan.t)
	foe.set_fight(m.foe.state, m.foe.t)
	foe.animate(dt, 0.0)
	for i in _weak.size():
		var w := _weak[i]
		var wp := m.weak_world(i)
		w.position = w.position.lerp(p3(wp.x, wp.y, wp.z), minf(1.0, dt * 6.0)) if w.position != Vector3.ZERO else p3(wp.x, wp.y, wp.z)
		var mat := w.material_override as StandardMaterial3D
		var open := m.foe.state == March.T.HELD or m.foe.state == March.T.FALLEN
		mat.emission_energy_multiplier = (6.0 if m.weak_seen else 1.2) * (1.6 if open else 1.0) * (0.75 + 0.25 * sin(_t * 6.0 + i))
		w.scale = Vector3.ONE * (1.4 if m.weak_seen else 0.8)

## Visual reactions to the simulation's events.
func on_event(e: Dictionary) -> void:
	match e.t:
		"strike":
			var target: Colossus = foe if int(e.who) == 0 else ama
			var src: Colossus = ama if int(e.who) == 0 else foe
			if src:
				src.last_verb = "strike"
			if target:
				target.hit(1.0)
				var cp := target.head.global_position
				Fx.burst(self, Fx.sparks(60), cp)
				Fx.burst(self, Fx.dust(30, 9.0), cp)
			shake = maxf(shake, 1.3)
		"parried":
			var src: Colossus = ama if int(e.who) == 0 else foe
			if src:
				src.last_verb = "parried"
			var other: Colossus = foe if int(e.who) == 0 else ama
			if other:
				Fx.burst(self, Fx.sparks(80), other.head.global_position + Vector3(0, 6, 0))
			shake = maxf(shake, 0.8)
		"grab_fail":
			var src: Colossus = ama if int(e.who) == 0 else foe
			if src:
				src.last_verb = "grab_fail"
		"verb":
			var src: Colossus = ama if int(e.who) == 0 else foe
			if src:
				src.last_verb = ""
		"grab":
			shake = maxf(shake, 0.7)
		"fall":
			var who: Colossus = ama if int(e.who) == 0 else foe
			if who:
				Fx.burst(self, Fx.dust(60, 18.0), who.global_position + Vector3(0, 2, 0))
			shake = maxf(shake, 2.0)
		"weak_hit":
			if foe:
				foe.hit(0.8)
			var wp := m.weak_world(int(e.i))
			Fx.burst(self, Fx.sparks(50), p3(wp.x, wp.y, wp.z))
		"foe_hit":
			if foe:
				foe.hit(0.3)
		"bird_hit":
			var feathers := Fx.make(30, 1.5, Vector3(0, 1, 0), 180, Vector2(4, 10), Vector3(0, -6, 0), Vector2(0.4, 0.8), 0.5,
				[Color(0.15, 0.12, 0.1, 1), Color(0.2, 0.15, 0.12, 0.0)])
			feathers.one_shot = true
			feathers.explosiveness = 0.95
			Fx.burst(self, feathers, p3(int(e.x), int(e.y), int(e.z)))
		"skiff_hit":
			Fx.burst(self, Fx.sparks(30), p3(int(e.x), int(e.y), 200))
		"salvage":
			Fx.burst(self, Fx.dust(40, 10.0), p3(int(e.x), int(e.y), 0))
		"miss":
			if int(e.z) < 500:
				Fx.burst(self, Fx.dust(12, 4.0), p3(int(e.x), int(e.y), 0))
		"chasm":
			ama.hit(0.6)
			Fx.burst(self, Fx.dust(60, 16.0), Vector3(0, 1, -8))
			shake = maxf(shake, 2.2)
		"rocks":
			ama.hit(0.8)
			Fx.burst(self, Fx.dust(50, 12.0), Vector3(10, 25, 0))
			shake = maxf(shake, 1.8)
		"braced":
			Fx.burst(self, Fx.dust(40, 10.0), Vector3(10, 25, 0))
			shake = maxf(shake, 0.8)
		"burst":
			Fx.burst(self, Fx.sparks(80), deck.global_transform * Deck.at(0, 0, 4.0))
			shake = maxf(shake, 1.0)
		"peck":
			Fx.burst(self, Fx.sparks(14), ama.body.global_transform * Deck.at(int(e.x), int(e.y), 1.0))
