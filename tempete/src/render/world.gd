class_name World
extends Node3D
## The plains in 3D and the weather over them: fields, roads, the river and its bridges, farms and towns, power
## lines; the sky; each storm's tower, anvil, striated base, wall cloud, rain and hail shafts; tornadoes; lightning;
## rain and hail around the camera. Metres: X east, Z = -north.

var chase: Chase
var plains: Plains
var env: Environment
var sun: DirectionalLight3D
var sky_m: ShaderMaterial
var ground_m: ShaderMaterial
var wet_img: Image
var wet_tex: ImageTexture
var water_m: ShaderMaterial
var trucks: Array[Truck] = []
var _storms: Dictionary = {}       # storm id -> {root, base, tower, anvil, wall, rain, hail, mats}
var _tornadoes: Dictionary = {}    # storm id -> {root, funnel, debris, mat}
var _bolts: Array = []             # {mesh, light, life}
var _probes: Array[Node3D] = []
var rain_p: GPUParticles3D
var hail_p: GPUParticles3D
var _t := 0.0
var _flash := 0.0
var cam_pos := Vector3.ZERO

func _init(c: Chase) -> void:
	chase = c
	plains = c.plains
	_environment()
	_ground()
	_roads()
	_river()
	_props()
	_precip()
	for v in chase.vehicles:
		var tr := Truck.new() if v.team == 0 else Truck.new(Color(0.08, 0.08, 0.09), Color(0.75, 0.08, 0.08))
		add_child(tr)
		trucks.append(tr)

static func p3(x: int, y: int, h: float = 0.0) -> Vector3:
	return Vector3(x / 1000.0, h, -y / 1000.0)

# ---- Environment ---------------------------------------------------------------------------------------------------------

func _environment() -> void:
	var we := WorldEnvironment.new()
	env = Environment.new()
	var sky := Sky.new()
	sky_m = ShaderMaterial.new()
	sky_m.shader = preload("res://src/render/shaders/sky.gdshader")
	sky.sky_material = sky_m
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_energy = 0.7
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.tonemap_exposure = 1.1
	env.glow_enabled = true
	env.glow_intensity = 0.5
	env.glow_hdr_threshold = 1.2
	env.fog_enabled = true
	env.fog_light_color = Color(0.45, 0.48, 0.48)
	env.fog_density = 0.00004
	env.fog_sky_affect = 0.3
	env.adjustment_enabled = true
	env.adjustment_contrast = 1.08
	env.adjustment_saturation = 1.05
	we.environment = env
	add_child(we)
	sun = DirectionalLight3D.new()
	sun.light_color = Color(1.0, 0.82, 0.58)
	sun.light_energy = 1.2
	sun.rotation = Vector3(deg_to_rad(-14), deg_to_rad(-75), 0)
	sun.shadow_enabled = true
	sun.directional_shadow_max_distance = 250.0
	add_child(sun)

func set_quality(q: int) -> void:
	sun.shadow_enabled = q >= 1
	env.ssao_enabled = q >= 2
	env.ssao_radius = 2.0

# ---- Ground --------------------------------------------------------------------------------------------------------------

func _ground() -> void:
	var img := Image.create(Plains.FN, Plains.FN, false, Image.FORMAT_R8)
	for fy in Plains.FN:
		for fx in Plains.FN:
			img.set_pixel(fx, fy, Color(plains.fields[fy * Plains.FN + fx] / 255.0, 0, 0))
	wet_img = Image.create(Plains.FN, Plains.FN, false, Image.FORMAT_R8)
	wet_tex = ImageTexture.create_from_image(wet_img)
	ground_m = ShaderMaterial.new()
	ground_m.shader = preload("res://src/render/shaders/ground.gdshader")
	ground_m.set_shader_parameter("fields", ImageTexture.create_from_image(img))
	ground_m.set_shader_parameter("wet", wet_tex)
	var pm := PlaneMesh.new()
	pm.size = Vector2(36000, 36000)
	pm.subdivide_width = 0
	Shapes.put(self, pm, ground_m, Vector3(6000, 0, -6000))

func update_wet() -> void:
	for fy in Plains.FN:
		for fx in Plains.FN:
			wet_img.set_pixel(fx, fy, Color(plains.wet[fy * Plains.FN + fx] / 1000.0, 0, 0))
	wet_tex.update(wet_img)

func _roads() -> void:
	for kind in [1, 2]:
		var st := SurfaceTool.new()
		st.begin(Mesh.PRIMITIVE_TRIANGLES)
		var hw := 5.0 if kind == 2 else 4.0
		for k in range(1, Plains.N):
			for j in Plains.N:
				if int(plains.vroad[k][j]) == kind:
					_quad(st, Vector2(k * 1000.0, j * 1000.0), Vector2(k * 1000.0, (j + 1) * 1000.0), hw)
				if int(plains.hroad[k][j]) == kind:
					_quad(st, Vector2(j * 1000.0, k * 1000.0), Vector2((j + 1) * 1000.0, k * 1000.0), hw)
		st.generate_normals()
		var m := ShaderMaterial.new()
		m.shader = preload("res://src/render/shaders/road.gdshader")
		m.set_shader_parameter("paved", 1.0 if kind == 2 else 0.0)
		m.set_shader_parameter("wet", wet_tex)
		var mi := MeshInstance3D.new()
		mi.mesh = st.commit()
		mi.material_override = m
		add_child(mi)

## A road quad between two map points (metres, y north), half-width hw, slightly above the ground.
func _quad(st: SurfaceTool, a: Vector2, b: Vector2, hw: float) -> void:
	var d := (b - a).normalized()
	var n := Vector2(-d.y, d.x) * hw
	var len := a.distance_to(b)
	var pts := [a - n, a + n, b + n, b - n]
	var uvs := [Vector2(0, 0), Vector2(1, 0), Vector2(1, len), Vector2(0, len)]
	for i in [0, 1, 2, 0, 2, 3]:
		var p: Vector2 = pts[i]
		st.set_uv(uvs[i])
		st.set_normal(Vector3.UP)
		st.add_vertex(Vector3(p.x, 0.06, -p.y))

func _river() -> void:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var r := plains.river
	var hw := plains.river_w / 2000.0
	var acc := 0.0
	for i in range(0, r.size() - 2, 2):
		var a := Vector2(r[i] / 1000.0, r[i + 1] / 1000.0)
		var b := Vector2(r[i + 2] / 1000.0, r[i + 3] / 1000.0)
		var d := (b - a).normalized()
		var n := Vector2(-d.y, d.x) * hw
		var len := a.distance_to(b)
		var pts := [a - n, a + n, b + n, b - n]
		var uvs := [Vector2(0, acc), Vector2(1, acc), Vector2(1, acc + len), Vector2(0, acc + len)]
		for k in [0, 1, 2, 0, 2, 3]:
			var p: Vector2 = pts[k]
			st.set_uv(uvs[k] / Vector2(1, 1000))
			st.set_normal(Vector3.UP)
			st.add_vertex(Vector3(p.x, 0.04, -p.y))
		acc += len
	water_m = ShaderMaterial.new()
	water_m.shader = preload("res://src/render/shaders/water.gdshader")
	var mi := MeshInstance3D.new()
	mi.mesh = st.commit()
	mi.material_override = water_m
	add_child(mi)
	# Bridges.
	var deck := Shapes.mat(Color(0.45, 0.44, 0.42), 0.8)
	var rail := Shapes.mat(Color(0.6, 0.6, 0.62), 0.4, 0.6)
	for b: Dictionary in plains.bridges:
		var p := p3(int(b.x), int(b.y), 0.3)
		var vert: bool = b.v
		var size := Vector3(10, 0.5, 60) if vert else Vector3(60, 0.5, 10)
		Shapes.put(self, Shapes.box(size), deck, p)
		for s in [-1, 1]:
			var off := Vector3(5.0 * s, 0.8, 0) if vert else Vector3(0, 0.8, 5.0 * s)
			Shapes.put(self, Shapes.box(Vector3(0.2, 0.8, 60) if vert else Vector3(60, 0.8, 0.2)), rail, p + off)

# ---- Farms, towns, power lines -----------------------------------------------------------------------------------------------

func _mm(mesh: Mesh, m: Material, xf: Array) -> void:
	if xf.is_empty():
		return
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = mesh
	mm.instance_count = xf.size()
	for i in xf.size():
		mm.set_instance_transform(i, xf[i])
	var mi := MultiMeshInstance3D.new()
	mi.multimesh = mm
	mi.material_override = m
	add_child(mi)

func _props() -> void:
	var rng := Rng.new(plains.seed_v + 999)
	var houses: Array = []
	var roofs: Array = []
	var barns: Array = []
	var barn_roofs: Array = []
	var silos: Array = []
	var trees: Array = []
	var poles: Array = []
	var bars: Array = []
	var roof_mesh := Shapes.cached("roof", func() -> Mesh:
		var pm := PrismMesh.new()
		pm.size = Vector3(1, 1, 1)
		return pm)
	for f: Array in plains.farms:
		var c := p3(int(f[0]), int(f[1]))
		var a := rng.rint(0, 3) * PI / 2
		var b := Basis(Vector3.UP, a)
		houses.append(Transform3D(b.scaled(Vector3(11, 6, 8)), c + Vector3(0, 3, 0)))
		roofs.append(Transform3D(b.scaled(Vector3(12, 3.5, 9)), c + Vector3(0, 7.7, 0)))
		var bp := c + b * Vector3(24, 0, 10)
		barns.append(Transform3D(b.scaled(Vector3(16, 9, 12)), bp + Vector3(0, 4.5, 0)))
		barn_roofs.append(Transform3D(b.scaled(Vector3(17, 5, 13)), bp + Vector3(0, 11.5, 0)))
		if int(f[2]) % 2 == 0:
			for s in 1 + int(f[2]) / 2:
				silos.append(Transform3D(Basis().scaled(Vector3(4.5, 16, 4.5)), c + b * Vector3(-14 - s * 6, 8, -12)))
		for k in rng.rint(4, 9):
			var tp := c + Vector3(rng.rint(-45, 45), 0, rng.rint(-45, 45))
			var sc := rng.rint(7, 13)
			trees.append(Transform3D(Basis().scaled(Vector3(sc, sc * 1.1, sc)), tp + Vector3(0, sc * 0.7, 0)))
	for t: Dictionary in plains.towns:
		for h: Array in t.houses:
			var c := p3(int(h[0]), int(h[1]))
			var b := Basis(Vector3.UP, rng.rint(0, 1) * PI / 2)
			houses.append(Transform3D(b.scaled(Vector3(12, 7, 10)), c + Vector3(0, 3.5, 0)))
			roofs.append(Transform3D(b.scaled(Vector3(13, 4, 11)), c + Vector3(0, 9, 0)))
			if rng.chance(500):
				trees.append(Transform3D(Basis().scaled(Vector3(9, 10, 9)), c + Vector3(14, 6, 6)))
		var tc := p3(int(t.x), int(t.y))
		# The grain elevator and the water tower.
		for k in 4:
			silos.append(Transform3D(Basis().scaled(Vector3(7, 34, 7)), tc + Vector3(60 + k * 7.5, 17, 45)))
		houses.append(Transform3D(Basis().scaled(Vector3(14, 46, 10)), tc + Vector3(76, 23, 56)))
		var wt := Node3D.new()
		wt.position = tc + Vector3(-80, 0, -70)
		add_child(wt)
		var steel := Shapes.mat(Color(0.75, 0.78, 0.8), 0.4, 0.6)
		for k in 4:
			var leg := Shapes.put(wt, Shapes.cyl(0.4, 0.4, 30, 6), steel, Vector3(cos(k * PI / 2) * 5, 15, sin(k * PI / 2) * 5))
			leg.rotation.z = 0.0
		Shapes.put(wt, Shapes.sphere(8.0, 16, 10), steel, Vector3(0, 34, 0))
	# Power lines along the paved roads.
	for k in range(1, Plains.N):
		for j in Plains.N:
			if int(plains.vroad[k][j]) == 2:
				for s in 20:
					poles.append(Transform3D(Basis().scaled(Vector3(0.3, 10, 0.3)), Vector3(k * 1000.0 + 11, 5, -(j * 1000.0 + s * 50.0))))
					bars.append(Transform3D(Basis().scaled(Vector3(3.0, 0.2, 0.2)), Vector3(k * 1000.0 + 11, 9.6, -(j * 1000.0 + s * 50.0))))
			if int(plains.hroad[k][j]) == 2:
				for s in 20:
					poles.append(Transform3D(Basis().scaled(Vector3(0.3, 10, 0.3)), Vector3(j * 1000.0 + s * 50.0, 5, -(k * 1000.0 + 11))))
					bars.append(Transform3D(Basis().scaled(Vector3(0.2, 0.2, 3.0)), Vector3(j * 1000.0 + s * 50.0, 9.6, -(k * 1000.0 + 11))))
	# Windbreaks: rows of trees along some patch edges.
	for i in 40:
		var fx := rng.rint(1, Plains.FN - 2)
		var fy := rng.rint(1, Plains.FN - 2)
		var vert := rng.chance(500)
		for k in 18:
			var x := fx * 250.0 + (0.0 if vert else k * 14.0)
			var y := fy * 250.0 + (k * 14.0 if vert else 0.0)
			var sc := rng.rint(8, 12)
			trees.append(Transform3D(Basis().scaled(Vector3(sc, sc * 1.2, sc)), Vector3(x, sc * 0.8, -y)))
	_mm(Shapes.box(Vector3.ONE), Shapes.mat(Color(0.88, 0.86, 0.8), 0.8), houses)
	_mm(roof_mesh, Shapes.mat(Color(0.25, 0.22, 0.22), 0.7), roofs)
	_mm(Shapes.box(Vector3.ONE), Shapes.mat(Color(0.55, 0.12, 0.08), 0.8), barns)
	_mm(roof_mesh, Shapes.mat(Color(0.3, 0.3, 0.32), 0.6, 0.3), barn_roofs)
	_mm(Shapes.cyl(0.5, 0.5, 1.0, 14), Shapes.mat(Color(0.7, 0.72, 0.74), 0.35, 0.6), silos)
	_mm(Shapes.sphere(0.5, 8, 5), Shapes.mat(Color(0.14, 0.24, 0.1), 0.95), trees)
	_mm(Shapes.cyl(0.5, 0.5, 1.0, 5), Shapes.mat(Color(0.3, 0.22, 0.14), 0.9), poles)
	_mm(Shapes.box(Vector3.ONE), Shapes.mat(Color(0.3, 0.22, 0.14), 0.9), bars)

# ---- Precipitation around the camera -------------------------------------------------------------------------------------------

func _precip() -> void:
	rain_p = GPUParticles3D.new()
	rain_p.amount = 6000
	rain_p.lifetime = 0.9
	rain_p.visibility_aabb = AABB(Vector3(-60, -40, -60), Vector3(120, 80, 120))
	var pm := ParticleProcessMaterial.new()
	pm.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
	pm.emission_box_extents = Vector3(45, 1, 45)
	pm.direction = Vector3(0.15, -1, 0)
	pm.spread = 3.0
	pm.initial_velocity_min = 38.0
	pm.initial_velocity_max = 46.0
	pm.gravity = Vector3(0, -9.8, 0)
	rain_p.process_material = pm
	var streak := QuadMesh.new()
	streak.size = Vector2(0.03, 1.1)
	var sm := StandardMaterial3D.new()
	sm.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	sm.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	sm.albedo_color = Color(0.75, 0.8, 0.85, 0.35)
	sm.billboard_mode = BaseMaterial3D.BILLBOARD_FIXED_Y
	streak.material = sm
	rain_p.draw_pass_1 = streak
	rain_p.amount_ratio = 0.0
	add_child(rain_p)
	hail_p = GPUParticles3D.new()
	hail_p.amount = 1500
	hail_p.lifetime = 1.4
	hail_p.visibility_aabb = AABB(Vector3(-60, -40, -60), Vector3(120, 80, 120))
	var hm := ParticleProcessMaterial.new()
	hm.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
	hm.emission_box_extents = Vector3(35, 1, 35)
	hm.direction = Vector3(0, -1, 0)
	hm.initial_velocity_min = 25.0
	hm.initial_velocity_max = 32.0
	hm.gravity = Vector3(0, -9.8, 0)
	hm.collision_mode = ParticleProcessMaterial.COLLISION_RIGID
	hm.collision_bounce = 0.4
	hm.collision_friction = 0.2
	hail_p.process_material = hm
	var stone := SphereMesh.new()
	stone.radius = 0.04
	stone.height = 0.08
	stone.radial_segments = 6
	stone.rings = 3
	stone.material = Shapes.mat(Color(0.92, 0.95, 0.95), 0.2)
	hail_p.draw_pass_1 = stone
	hail_p.amount_ratio = 0.0
	add_child(hail_p)
	var col := GPUParticlesCollisionHeightField3D.new()
	col.size = Vector3(80, 40, 80)
	col.resolution = GPUParticlesCollisionHeightField3D.RESOLUTION_256
	col.follow_camera_enabled = true
	add_child(col)

# ---- Weather ----------------------------------------------------------------------------------------------------------------------

func _cloud_mat(lit: Color, shade: Color, alpha: float, bands: float, bumps: float, scale: float) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = preload("res://src/render/shaders/cloud.gdshader")
	m.set_shader_parameter("lit", lit)
	m.set_shader_parameter("shade", shade)
	m.set_shader_parameter("alpha", alpha)
	m.set_shader_parameter("bands", bands)
	m.set_shader_parameter("bumps", bumps)
	m.set_shader_parameter("scale", scale)
	return m

func _storm_view(st: Chase.Storm) -> Dictionary:
	var root := Node3D.new()
	add_child(root)
	var base_m := _cloud_mat(Color(0.35, 0.4, 0.42), Color(0.07, 0.09, 0.1), 0.96, 1.0, 0.04, 0.8)
	var base := Shapes.put(root, Shapes.lathe(PackedVector2Array([Vector2(0.0, 0.0), Vector2(0.55, 0.02), Vector2(0.92, 0.12), Vector2(1.0, 0.35), Vector2(0.94, 0.62), Vector2(0.7, 0.9), Vector2(0.0, 1.0)]), 96, 1.0, false), base_m, Vector3.ZERO)
	var tower_m := _cloud_mat(Color(0.95, 0.9, 0.82), Color(0.18, 0.2, 0.22), 0.97, 0.0, 0.1, 1.5)
	var tower := Shapes.put(root, Shapes.lathe(PackedVector2Array([Vector2(0.75, 0.0), Vector2(0.9, 0.2), Vector2(0.8, 0.5), Vector2(0.95, 0.75), Vector2(0.7, 0.95), Vector2(0.0, 1.0)]), 28, 1.0, false), tower_m, Vector3.ZERO)
	var anvil_m := _cloud_mat(Color(0.92, 0.9, 0.86), Color(0.35, 0.38, 0.42), 0.92, 0.0, 0.05, 0.6)
	var anvil := Shapes.put(root, Shapes.sphere(1.0, 32, 12), anvil_m, Vector3.ZERO)
	var wall_m := _cloud_mat(Color(0.25, 0.28, 0.3), Color(0.05, 0.06, 0.07), 0.97, 0.6, 0.1, 2.0)
	var wall := Shapes.put(root, Shapes.lathe(PackedVector2Array([Vector2(0.7, 0.0), Vector2(0.95, 0.5), Vector2(1.0, 1.0)]), 28, 1.0, false), wall_m, Vector3.ZERO)
	var rain_m := ShaderMaterial.new()
	rain_m.shader = preload("res://src/render/shaders/rain.gdshader")
	var shaft := Shapes.cached("shaft", func() -> Mesh: return Shapes.lathe(PackedVector2Array([Vector2(1.0, -0.5), Vector2(1.0, 0.5)]), 48, 1.0, false))
	var rain := Shapes.put(self, shaft, rain_m, Vector3.ZERO)
	var hail_m := ShaderMaterial.new()
	hail_m.shader = preload("res://src/render/shaders/rain.gdshader")
	hail_m.set_shader_parameter("color", Color(0.75, 0.88, 0.82))
	var hail := Shapes.put(self, shaft, hail_m, Vector3.ZERO)
	return {"root": root, "base": base, "tower": tower, "anvil": anvil, "wall": wall, "rain": rain, "hail": hail,
		"mats": [base_m, tower_m, anvil_m, wall_m, rain_m, hail_m]}

func _tornado_view() -> Dictionary:
	var root := Node3D.new()
	add_child(root)
	var m := ShaderMaterial.new()
	m.shader = preload("res://src/render/shaders/tornado.gdshader")
	var funnel := Shapes.put(root, Shapes.lathe(PackedVector2Array([Vector2(0.5, 0.0), Vector2(0.42, 0.08), Vector2(0.45, 0.3), Vector2(0.6, 0.55), Vector2(0.85, 0.8), Vector2(1.3, 0.95), Vector2(1.8, 1.0)]), 32, 1.0, false), m, Vector3.ZERO)
	var dm := ShaderMaterial.new()
	dm.shader = preload("res://src/render/shaders/tornado.gdshader")
	dm.set_shader_parameter("dust", 1.0)
	dm.set_shader_parameter("alpha", 0.55)
	var debris := Shapes.put(root, Shapes.lathe(PackedVector2Array([Vector2(0.4, 0.0), Vector2(1.0, 0.25), Vector2(0.9, 0.7), Vector2(0.3, 1.0)]), 24, 1.0, false), dm, Vector3.ZERO)
	return {"root": root, "funnel": funnel, "debris": debris, "mat": m, "dmat": dm}

func strike(x: int, y: int) -> void:
	var top := p3(x, y, 1100.0)
	var pts: Array[Vector3] = [top]
	var p := top
	var bot := p3(x, y, 0.0)
	var n := 18
	for i in range(1, n):
		var k := float(i) / n
		p = top.lerp(bot, k) + Vector3(randf_range(-60, 60), 0, randf_range(-60, 60))
		pts.append(p)
	pts.append(bot)
	var im := ImmediateMesh.new()
	im.surface_begin(Mesh.PRIMITIVE_LINES)
	for i in range(pts.size() - 1):
		im.surface_add_vertex(pts[i])
		im.surface_add_vertex(pts[i + 1])
		# Branches.
		if i % 4 == 2:
			var b := pts[i] + Vector3(randf_range(-150, 150), randf_range(-150, -60), randf_range(-150, 150))
			im.surface_add_vertex(pts[i])
			im.surface_add_vertex(b)
	im.surface_end()
	var mi := MeshInstance3D.new()
	mi.mesh = im
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.albedo_color = Color(0.85, 0.9, 1.0)
	m.emission_enabled = true
	m.emission = Color(0.8, 0.85, 1.0)
	m.emission_energy_multiplier = 12.0
	mi.material_override = m
	add_child(mi)
	var l := OmniLight3D.new()
	l.position = p3(x, y, 500.0)
	l.omni_range = 4000.0
	l.light_energy = 6.0
	l.light_color = Color(0.8, 0.85, 1.0)
	add_child(l)
	_bolts.append({"mesh": mi, "light": l, "life": 0.35})
	var d := cam_pos.distance_to(p3(x, y, 0))
	_flash = maxf(_flash, clampf(1.0 - d / 12000.0, 0.15, 1.0))

func add_probe(x: int, y: int) -> void:
	var dome := Shapes.lathe(PackedVector2Array([Vector2(0.45, 0), Vector2(0.44, 0.08), Vector2(0.3, 0.2), Vector2(0.0, 0.25)]), 16)
	var n := Shapes.put(self, dome, Shapes.mat(Color(0.8, 0.8, 0.82), 0.3, 0.8), p3(x, y, 0.0))
	var beacon := Shapes.put(n, Shapes.sphere(0.06, 6, 4), Shapes.mat(Color(1, 0.3, 0.1), 0.2, 0.0, Color(1, 0.3, 0.1), 6.0), Vector3(0, 0.3, 0))
	beacon.set_meta("blink", true)
	_probes.append(n)

## Per rendered frame: follow the simulation (positions interpolated by alpha are close enough at these scales).
func frame(dt: float, cam: Vector3) -> void:
	_t += dt
	cam_pos = cam
	water_m.set_shader_parameter("t", _t)
	# Storms.
	var darkest := 0.0
	var dir := Vector3(-1, 0, 0)
	for st in chase.storms:
		if st.dead:
			if _storms.has(st.id):
				var v: Dictionary = _storms[st.id]
				(v.root as Node3D).queue_free()
				(v.rain as Node3D).queue_free()
				(v.hail as Node3D).queue_free()
				_storms.erase(st.id)
			continue
		if not _storms.has(st.id):
			_storms[st.id] = _storm_view(st)
		var v: Dictionary = _storms[st.id]
		var k := st.alive_k() / 1000.0
		var rot := st.rot() / 1000.0
		var c := p3(st.x, st.y)
		var root: Node3D = v.root
		root.position = c
		var m := float(st.motion()) / Trig.TURN * TAU
		var base: Node3D = v.base
		base.position = Vector3(0, 1100.0 - 300.0 * k, 0)
		base.scale = Vector3(6000 * k + 1500, 700 * k + 200, 6000 * k + 1500)
		base.rotation.y += dt * 0.05 * rot
		var tower: Node3D = v.tower
		tower.position = Vector3(800 * cos(m), 1200, -800 * sin(m))
		tower.scale = Vector3(4200 * k + 800, 8800 * k + 600, 4200 * k + 800)
		var anvil: Node3D = v.anvil
		anvil.position = Vector3(9000 * cos(m), 10200 * k + 400, -9000 * sin(m))
		anvil.scale = Vector3(15000 * k + 1000, 1300 * k + 100, 11000 * k + 1000)
		anvil.rotation.y = m
		var wall: Node3D = v.wall
		var h := chase.hook(st)
		wall.position = (p3(h.x, h.y) - c) * 0.35 + Vector3(0, 650.0 + 300.0 * (1.0 - rot), 0)
		wall.scale = Vector3(1500 * rot + 300, 500, 1500 * rot + 300)
		wall.visible = rot > 0.35
		(v.mats[0] as ShaderMaterial).set_shader_parameter("spin", _t * 0.04 * rot)
		for mm: ShaderMaterial in v.mats:
			mm.set_shader_parameter("t", _t)
		var core := chase.core_center(st)
		var rain: Node3D = v.rain
		rain.position = p3(core.x, core.y, 550.0)
		rain.scale = Vector3(3600 * k + 400, 1100, 2600 * k + 300)
		rain.rotation.y = m
		(v.mats[4] as ShaderMaterial).set_shader_parameter("density", 0.75 * k)
		var hc := chase.hail_center(st)
		var hail: Node3D = v.hail
		hail.position = p3(hc.x, hc.y, 550.0)
		var hr := (700 + st.sev * 260) * k
		hail.scale = Vector3(hr, 1100, hr)
		hail.visible = st.sev >= 2 and st.t >= st.dev * 2 / 3
		(v.mats[5] as ShaderMaterial).set_shader_parameter("density", 0.5 * k)
		var to := (c - cam)
		var dist := Vector2(to.x, to.z).length()
		var dk := clampf(1.0 - (dist - 4000.0) / 20000.0, 0.0, 1.0) * k
		if dk > darkest:
			darkest = dk
			dir = Vector3(to.x, 0, to.z).normalized()
	sky_m.set_shader_parameter("darkness", 0.25 + 0.65 * darkest)
	sky_m.set_shader_parameter("storm_dir", dir)
	sun.light_energy = lerpf(1.3, 0.35, darkest)
	env.ambient_light_energy = lerpf(0.75, 0.35, darkest)
	# Tornadoes.
	var seen := {}
	for tn in chase.tornadoes:
		if tn.dead:
			continue
		seen[tn.storm] = true
		if not _tornadoes.has(tn.storm):
			_tornadoes[tn.storm] = _tornado_view()
		var v: Dictionary = _tornadoes[tn.storm]
		var root: Node3D = v.root
		root.position = p3(tn.x, tn.y)
		var w := maxf(30.0, tn.width / 1000.0)
		var s := float(chase._strength(tn)) / 1000.0
		var funnel: Node3D = v.funnel
		funnel.scale = Vector3(w, 1100.0, w)
		funnel.rotation.y -= dt * 3.0
		(v.mat as ShaderMaterial).set_shader_parameter("t", _t)
		(v.mat as ShaderMaterial).set_shader_parameter("alpha", 0.35 + 0.6 * s)
		var debris: Node3D = v.debris
		debris.scale = Vector3(w * 1.6, 60.0 + w * 0.3, w * 1.6)
		(v.dmat as ShaderMaterial).set_shader_parameter("t", _t)
	for id: int in _tornadoes.keys():
		if not seen.has(id):
			((_tornadoes[id] as Dictionary).root as Node3D).queue_free()
			_tornadoes.erase(id)
	# Lightning.
	for b: Dictionary in _bolts:
		b.life = float(b.life) - dt
		var l: OmniLight3D = b.light
		l.light_energy = maxf(0.0, float(b.life) * 20.0) * (0.5 + 0.5 * float(randi() % 2))
		(b.mesh as Node3D).visible = randi() % 3 != 0
		if float(b.life) <= 0.0:
			(b.mesh as Node3D).queue_free()
			l.queue_free()
	_bolts = _bolts.filter(func(b: Dictionary) -> bool: return float(b.life) > 0.0)
	_flash = maxf(0.0, _flash - dt * 4.0)
	sky_m.set_shader_parameter("flash", _flash * 0.6)
	# Rain and hail where the camera is; fog thickens in the core.
	var vx := int(cam.x * 1000.0)
	var vy := int(-cam.z * 1000.0)
	var r := chase.rain_at(vx, vy)
	var hl := chase.hail_at(vx, vy)
	rain_p.global_position = cam + Vector3(0, 30, 0)
	hail_p.global_position = cam + Vector3(0, 25, 0)
	rain_p.amount_ratio = clampf(r / 80.0, 0.0, 1.0)
	hail_p.amount_ratio = clampf(hl / 60.0, 0.0, 1.0)
	env.fog_density = 0.00004 + r * 0.0000045
	env.fog_light_color = Color(0.42, 0.45, 0.45).lerp(Color(0.1, 0.13, 0.13), clampf(r / 80.0, 0.0, 1.0)) * lerpf(1.0, 0.6, darkest)
	for p in _probes:
		for c in p.get_children():
			if c.has_meta("blink"):
				(c as Node3D).visible = fmod(_t, 1.0) < 0.5
