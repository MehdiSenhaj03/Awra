class_name Fx
## Particle effects built in code: smoke, steam, dust puffs, fire, sparks, embers. Soft round sprites, billboarded.

static var _soft: Texture2D = null

static func soft() -> Texture2D:
	if _soft == null:
		var g := Gradient.new()
		g.set_color(0, Color(1, 1, 1, 1))
		g.set_color(1, Color(1, 1, 1, 0))
		g.add_point(0.45, Color(1, 1, 1, 0.55))
		var t := GradientTexture2D.new()
		t.gradient = g
		t.fill = GradientTexture2D.FILL_RADIAL
		t.fill_from = Vector2(0.5, 0.5)
		t.fill_to = Vector2(1.0, 0.5)
		t.width = 64
		t.height = 64
		_soft = t
	return _soft

static func _mat(additive: bool) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.blend_mode = BaseMaterial3D.BLEND_MODE_ADD if additive else BaseMaterial3D.BLEND_MODE_MIX
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	m.vertex_color_use_as_albedo = true
	m.albedo_texture = soft()
	m.depth_draw_mode = BaseMaterial3D.DEPTH_DRAW_DISABLED
	return m

static func _ramp(cols: Array) -> GradientTexture1D:
	var g := Gradient.new()
	g.offsets = PackedFloat32Array()
	g.colors = PackedColorArray()
	for i in cols.size():
		g.add_point(float(i) / (cols.size() - 1), cols[i])
	var t := GradientTexture1D.new()
	t.gradient = g
	return t

static func _curve(pts: Array) -> CurveTexture:
	var c := Curve.new()
	for p: Vector2 in pts:
		c.add_point(p)
	var t := CurveTexture.new()
	t.curve = c
	return t

## A generic emitter. dir: initial direction, spread in degrees, v: speed range, g: gravity (m/s²),
## size: sprite size range (m), grow: size curve end, cols: colour ramp over life.
static func make(amount: int, life: float, dir: Vector3, spread: float, v: Vector2, g: Vector3, size: Vector2, grow: float, cols: Array, additive := false, box := Vector3.ZERO) -> GPUParticles3D:
	var p := GPUParticles3D.new()
	p.amount = amount
	p.lifetime = life
	p.local_coords = false
	var pm := ParticleProcessMaterial.new()
	pm.direction = dir
	pm.spread = spread
	pm.initial_velocity_min = v.x
	pm.initial_velocity_max = v.y
	pm.gravity = g
	pm.scale_min = size.x
	pm.scale_max = size.y
	pm.scale_curve = _curve([Vector2(0, 0.4), Vector2(1, grow)])
	pm.color_ramp = _ramp(cols)
	pm.damping_min = 0.5
	pm.damping_max = 1.5
	if box != Vector3.ZERO:
		pm.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
		pm.emission_box_extents = box
	p.process_material = pm
	var q := QuadMesh.new()
	q.size = Vector2(1, 1)
	q.material = _mat(additive)
	p.draw_pass_1 = q
	p.visibility_aabb = AABB(Vector3(-200, -50, -200), Vector3(400, 200, 400))
	return p

static func smoke(amount := 40, size := 3.0) -> GPUParticles3D:
	return make(amount, 5.0, Vector3(0, 1, 0), 12, Vector2(3, 5), Vector3(0, 0.6, 5.0), Vector2(size * 0.6, size), 3.0,
		[Color(0.3, 0.27, 0.25, 0.0), Color(0.32, 0.29, 0.27, 0.55), Color(0.45, 0.42, 0.4, 0.0)])

static func steam(amount := 30) -> GPUParticles3D:
	return make(amount, 1.6, Vector3(0, 1, 0), 18, Vector2(8, 12), Vector3(0, 1.0, 3.0), Vector2(1.0, 1.6), 3.5,
		[Color(1, 1, 1, 0.0), Color(0.95, 0.95, 0.95, 0.6), Color(1, 1, 1, 0.0)])

static func fire(amount := 24) -> GPUParticles3D:
	return make(amount, 0.8, Vector3(0, 1, 0), 15, Vector2(1.5, 3), Vector3(0, 2, 1.5), Vector2(0.8, 1.4), 0.2,
		[Color(1, 0.9, 0.5, 0.0), Color(1, 0.6, 0.15, 0.9), Color(0.8, 0.2, 0.05, 0.5), Color(0.2, 0.2, 0.2, 0.0)], true)

static func dust(amount := 30, size := 6.0) -> GPUParticles3D:
	var p := make(amount, 2.5, Vector3(0, 0.4, 0), 80, Vector2(4, 10), Vector3(0, -0.5, 0), Vector2(size * 0.5, size), 2.5,
		[Color(0.6, 0.55, 0.48, 0.0), Color(0.62, 0.56, 0.5, 0.55), Color(0.6, 0.55, 0.5, 0.0)])
	p.one_shot = true
	p.explosiveness = 0.9
	p.emitting = false
	return p

static func sparks(amount := 40) -> GPUParticles3D:
	var p := make(amount, 0.9, Vector3(0, 1, 0), 70, Vector2(8, 22), Vector3(0, -14, 0), Vector2(0.25, 0.45), 0.2,
		[Color(1, 0.9, 0.6, 1), Color(1, 0.5, 0.1, 0.8), Color(1, 0.3, 0.05, 0.0)], true)
	p.one_shot = true
	p.explosiveness = 0.95
	p.emitting = false
	return p

## Fires a one-shot effect at a world position (it frees itself).
static func burst(parent: Node3D, p: GPUParticles3D, pos: Vector3) -> void:
	parent.add_child(p)
	p.global_position = pos
	p.restart()
	p.emitting = true
	var t := parent.get_tree().create_timer(p.lifetime + 0.5)
	t.timeout.connect(p.queue_free)
