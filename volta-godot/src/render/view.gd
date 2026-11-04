class_name View
extends Node3D
## Renders a World: arena, actors, bullets and every effect. Reads the simulation, never changes it.
## Ported from the web version (same camera maths, same effects), rebuilt around the engine's strengths: one
## MultiMesh draw for every glow, GPU-simulated sparks and debris, shared materials with per-instance state, pooled
## models, screen-space reflections on the glass.

const GMAX := 6000
const ST_ORB := 0.0
const ST_STREAK := 1.0
const ST_RING := 2.0
const ST_BEAM := 3.0
const ST_WARN := 4.0
const ST_ZONE := 5.0
const ST_DISC := 6.0
const SPARKS_MAX := 6000
const SHARDS_MAX := 2600
const EMIT_FLAGS := GPUParticles3D.EMIT_FLAG_POSITION | GPUParticles3D.EMIT_FLAG_VELOCITY | GPUParticles3D.EMIT_FLAG_COLOR | GPUParticles3D.EMIT_FLAG_CUSTOM

var camera := Camera3D.new()
var env := Environment.new()
var _we := WorldEnvironment.new()
var _key := DirectionalLight3D.new()
var _rim := DirectionalLight3D.new()
var _player_light := OmniLight3D.new()
var _lights: Array[OmniLight3D] = []
var _light_i := 0
var _li: PackedFloat32Array = PackedFloat32Array([0, 0, 0, 0])

var _floor := MeshInstance3D.new()
var _floor_mat := ShaderMaterial.new()
var _ripples := PackedVector4Array()
var _ripple_i := 0
var _backdrop := MeshInstance3D.new()
var _backdrop_mat := ShaderMaterial.new()
var _debris := MultiMesh.new()
var _debris_p := PackedFloat32Array()
var _debris_r := PackedFloat32Array()
var _debris_buf := PackedFloat32Array()
var _stage := Node3D.new()
var _mirror_vp := SubViewport.new()
var _mirror_cam := Camera3D.new()
var _mirror_env := Environment.new()
var refl_scale := 0.5

var _glows := MultiMesh.new()
var _glows_mi := MultiMeshInstance3D.new()
var _glows_mat := ShaderMaterial.new()
var _g := PackedFloat32Array()
var _gn := 0

var _sparks := GPUParticles3D.new()
var _shards_dark := GPUParticles3D.new()
var _shards_lit := GPUParticles3D.new()

var _player: VModel
var _models: Dictionary = {}
var _pool: Dictionary = {}
var _seen: Dictionary = {}
var _warm: Node3D = null

# Transient effects, in flat arrays (no allocation per effect).
var _rings := PackedFloat32Array()   # x z r t life R G B w
var _rn := 0
var _flashes := PackedFloat32Array() # x z r t life R G B
var _fn := 0
var _arcs := PackedFloat32Array()    # x1 z1 x2 z2 t
var _an := 0
var _after := PackedFloat32Array()   # x z t
var _afn := 0
var _trail := PackedVector2Array()
var _trail_head := 0
var _trail_n := 0

## Text popups (score, PERFECT) drawn by the interface at projected positions: {x, z, text, cls, t}.
var popups: Array[Dictionary] = []

# Camera and screen state.
var _cam_x := 0.0
var _cam_z := 0.0
var _trauma := 0.0
var _kick_x := 0.0
var _kick_z := 0.0
var _zoom := 1.0
var zoom_target := 1.0
var _fov_kick := 0.0
var _chroma_k := 0.0
var _flash_k := 0.0
var _flash_c := Color.WHITE
var _od_k := 0.0
var _beat_pulse := 0.0
var time := 0.0
var shake_scale := 1.0
var flash_scale := 1.0
## Music level 0..1, for reactive visuals.
var level := 0.0
var quality := 2
var accent := 0x2ef2ff
var accent2 := 0xff2fb9

# Post-processing (screen shader).
var post: ShaderMaterial
var _shocks := PackedVector4Array([Vector4(0, 0, -1, 0), Vector4(0, 0, -1, 0), Vector4(0, 0, -1, 0)])
var _shock_world: Array[Vector3] = [Vector3.ZERO, Vector3.ZERO, Vector3.ZERO]
var _shock_i := 0

func _init() -> void:
	name = "View"
	_build_environment()
	add_child(camera)
	camera.fov = 36.0
	camera.near = 0.5
	camera.far = 600.0
	camera.current = true
	camera.cull_mask = 1 | 2
	_build_mirror()
	_key.light_color = Color(0xbcd4ffff)
	_key.light_energy = 1.6
	_key.position = Vector3(-20, 40, 10)
	add_child(_key)
	_key.look_at_from_position(Vector3(-20, 40, 10), Vector3.ZERO)
	_rim.light_color = Color(0xff4fc8ff)
	_rim.light_energy = 0.7
	add_child(_rim)
	_rim.look_at_from_position(Vector3(25, 12, -30), Vector3.ZERO)
	_player_light.omni_range = 16.0
	_player_light.omni_attenuation = 1.6
	_player_light.light_color = Color(0x7ff6ffff)
	add_child(_player_light)
	for i in 4:
		var l := OmniLight3D.new()
		l.omni_range = 14.0
		l.omni_attenuation = 1.8
		l.light_energy = 0.0
		l.visible = false
		_lights.append(l)
		add_child(l)
	# Floor.
	_floor_mat.shader = preload("res://src/render/shaders/floor.gdshader")
	_floor.material_override = _floor_mat
	_floor.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_floor.layers = 2
	_floor_mat.render_priority = -1
	_ripples.resize(16)
	for i in 16:
		_ripples[i] = Vector4(0, 0, -100, 0)
	add_child(_floor)
	# Abyss.
	var bp := PlaneMesh.new()
	bp.size = Vector2(900, 600)
	_backdrop.mesh = bp
	_backdrop_mat.shader = preload("res://src/render/shaders/backdrop.gdshader")
	_backdrop.material_override = _backdrop_mat
	_backdrop.position.y = -120.0
	_backdrop.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_backdrop.layers = 2
	add_child(_backdrop)
	_build_debris()
	add_child(_stage)
	_build_glows()
	_build_particles()
	_player = Models.player()
	add_child(_player.root)
	_trail.resize(18)

func _build_environment() -> void:
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0x010104ff)
	var sky := Sky.new()
	var sm := ShaderMaterial.new()
	sm.shader = preload("res://src/render/shaders/sky.gdshader")
	sky.sky_material = sm
	sky.radiance_size = Sky.RADIANCE_SIZE_128
	env.sky = sky
	env.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0x30365aff)
	env.ambient_light_energy = 0.55
	env.tonemap_mode = Environment.TONE_MAPPER_AGX
	env.tonemap_exposure = 1.0
	env.glow_enabled = true
	env.glow_normalized = false
	for i in 7:
		env.set_glow_level(i, [0.0, 1.0, 1.0, 1.0, 0.8, 0.6, 0.4][i])
	env.glow_intensity = 0.38
	env.glow_strength = 1.0
	env.glow_bloom = 0.0
	env.glow_hdr_threshold = 0.75
	env.glow_hdr_scale = 2.0
	env.glow_blend_mode = Environment.GLOW_BLEND_MODE_ADDITIVE
	env.adjustment_enabled = true
	env.adjustment_contrast = 1.08
	env.adjustment_saturation = 1.12
	env.fog_enabled = true
	env.fog_mode = Environment.FOG_MODE_DEPTH
	env.fog_density = 1.0
	env.fog_depth_begin = 60.0
	env.fog_depth_end = 220.0
	env.fog_depth_curve = 1.0
	env.fog_sky_affect = 0.0
	_we.environment = env
	add_child(_we)

## The floor mirror: a second camera below the glass, rendering the actors (not the floor, the abyss or the
## particles) at reduced resolution, without post-processing.
func _build_mirror() -> void:
	_mirror_env.background_mode = Environment.BG_COLOR
	_mirror_env.background_color = Color(0, 0, 0)
	_mirror_env.sky = env.sky
	_mirror_env.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	_mirror_env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	_mirror_env.ambient_light_color = env.ambient_light_color
	_mirror_env.ambient_light_energy = env.ambient_light_energy
	_mirror_env.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	_mirror_cam.environment = _mirror_env
	_mirror_cam.cull_mask = 1
	_mirror_cam.fov = 36.0
	_mirror_cam.near = 0.5
	_mirror_cam.far = 300.0
	_mirror_vp.use_hdr_2d = true
	_mirror_vp.msaa_3d = Viewport.MSAA_DISABLED
	_mirror_vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_mirror_vp.positional_shadow_atlas_size = 0
	_mirror_vp.size = Vector2i(640, 360)
	_mirror_vp.add_child(_mirror_cam)
	add_child(_mirror_vp)
	_floor_mat.set_shader_parameter("u_refl", _mirror_vp.get_texture())

func update_mirror() -> void:
	if refl_scale <= 0.0:
		return
	var vs := get_viewport().get_visible_rect().size
	var want := Vector2i(maxi(16, int(vs.x * refl_scale)), maxi(16, int(vs.y * refl_scale)))
	if _mirror_vp.size != want:
		_mirror_vp.size = want
	var ct := camera.global_transform
	var o := ct.origin
	var fwd := -ct.basis.z
	var up := ct.basis.y
	var mo := Vector3(o.x, -o.y, o.z)
	var mf := Vector3(fwd.x, -fwd.y, fwd.z)
	var mu := Vector3(up.x, -up.y, up.z)
	_mirror_cam.fov = camera.fov
	_mirror_cam.look_at_from_position(mo, mo + mf, mu)
	var proj := _mirror_cam.get_camera_projection()
	var viewm := Projection(_mirror_cam.global_transform.affine_inverse())
	_floor_mat.set_shader_parameter("u_mirror", proj * viewm)

func _build_debris() -> void:
	_debris.transform_format = MultiMesh.TRANSFORM_3D
	_debris.mesh = Geo.tetra()
	_debris.instance_count = 70
	_debris_p.resize(70 * 4)
	_debris_r.resize(70 * 6)
	_debris_buf.resize(70 * 12)
	for i in 70:
		var a := randf() * TAU
		var d := randf_range(30, 140)
		_debris_p[i * 4] = cos(a) * d
		_debris_p[i * 4 + 1] = randf_range(-90, -12)
		_debris_p[i * 4 + 2] = sin(a) * d * 0.8
		_debris_p[i * 4 + 3] = randf_range(1, 7) * (1.0 - _debris_p[i * 4 + 1] / 60.0)
		for j in 3:
			_debris_r[i * 6 + j] = randf() * TAU
			_debris_r[i * 6 + 3 + j] = randf_range(-0.15, 0.15)
	var mi := MultiMeshInstance3D.new()
	mi.multimesh = _debris
	mi.material_override = Mats.obsidian(0x07080e, 0.15, 0.95)
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mi.custom_aabb = AABB(Vector3(-200, -150, -200), Vector3(400, 160, 400))
	mi.layers = 2
	add_child(mi)

func _build_glows() -> void:
	_glows.transform_format = MultiMesh.TRANSFORM_3D
	_glows.mesh = Geo.quad_y(2.0)
	_glows.instance_count = GMAX
	_glows.visible_instance_count = 0
	_g.resize(GMAX * 12)
	_glows_mat.shader = preload("res://src/render/shaders/glows.gdshader")
	_glows_mi.multimesh = _glows
	_glows_mi.material_override = _glows_mat
	_glows_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_glows_mi.custom_aabb = AABB(Vector3(-500, -10, -500), Vector3(1000, 20, 1000))
	_glows_mi.sorting_offset = 5.0
	add_child(_glows_mi)

func _particles(p: GPUParticles3D, amount: int, process: Shader, draw: Mesh, mat: Material, lifetime: float) -> void:
	p.amount = amount
	p.lifetime = lifetime
	p.emitting = false
	p.one_shot = false
	p.local_coords = false
	p.explosiveness = 0.0
	p.fixed_fps = 0
	p.interpolate = false
	p.visibility_aabb = AABB(Vector3(-300, -50, -300), Vector3(600, 120, 600))
	var pm := ShaderMaterial.new()
	pm.shader = process
	p.process_material = pm
	p.draw_pass_1 = draw
	p.material_override = mat
	p.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	p.layers = 2
	add_child(p)

func _build_particles() -> void:
	var sm := ShaderMaterial.new()
	sm.shader = preload("res://src/render/shaders/spark_draw.gdshader")
	var q := QuadMesh.new()
	q.size = Vector2(0.585, 0.585)
	_particles(_sparks, SPARKS_MAX, preload("res://src/render/shaders/spark_process.gdshader"), q, sm, 3.0)
	var dm := ShaderMaterial.new()
	dm.shader = preload("res://src/render/shaders/shard_draw.gdshader")
	_particles(_shards_dark, SHARDS_MAX, preload("res://src/render/shaders/shard_process.gdshader"), Geo.tetra(), dm, 3.0)
	_particles(_shards_lit, SHARDS_MAX / 2, preload("res://src/render/shaders/shard_process.gdshader"), Geo.octa(), dm, 3.0)

func set_post(m: ShaderMaterial) -> void:
	post = m

# ---- Quality -------------------------------------------------------------------------------------------------------

## 0 low, 1 medium, 2 high.
func set_quality(q: int) -> void:
	quality = q
	refl_scale = 0.5 if q >= 2 else (0.35 if q == 1 else 0.0)
	_mirror_vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS if refl_scale > 0.0 else SubViewport.UPDATE_DISABLED
	_floor_mat.set_shader_parameter("u_has_refl", 1.0 if refl_scale > 0.0 else 0.0)
	var vp := get_viewport()
	if vp:
		vp.msaa_3d = Viewport.MSAA_4X if q >= 2 else (Viewport.MSAA_2X if q == 1 else Viewport.MSAA_DISABLED)
		vp.scaling_3d_scale = 1.0 if q >= 1 else 0.75
		vp.scaling_3d_mode = Viewport.SCALING_3D_MODE_BILINEAR if q >= 1 else Viewport.SCALING_3D_MODE_FSR
	_sparks.amount_ratio = 1.0 if q >= 1 else 0.5

# ---- Stage -----------------------------------------------------------------------------------------------------------

## Builds the arena of a new stage.
func set_stage(w: World, accent_: int, accent2_: int, fog: int, painting: Texture2D) -> void:
	accent = accent_
	accent2 = accent2_
	for c in _stage.get_children():
		c.queue_free()
	for id in _models:
		_release(_models[id])
	_models.clear()
	var a := w.arena
	var size := a.extent * 2.0 + 8.0
	var pm := PlaneMesh.new()
	pm.size = Vector2(size, size)
	_floor.mesh = pm
	_floor_mat.set_shader_parameter("u_accent", Mats.srgb(accent))
	_floor_mat.set_shader_parameter("u_accent2", Mats.srgb(accent2))
	_floor_mat.set_shader_parameter("u_shape", 0.0 if a.shape == "circle" else 1.0)
	_floor_mat.set_shader_parameter("u_size", Vector3(a.r, a.w, a.h))
	var pil := PackedVector3Array()
	pil.resize(8)
	for i in 8:
		pil[i] = a.pillars[i] if i < a.pillars.size() else Vector3.ZERO
	_floor_mat.set_shader_parameter("u_pillars", pil)
	for i in 16:
		_ripples[i] = Vector4(0, 0, -100, 0)
	_floor_mat.set_shader_parameter("u_ripples", _ripples)
	_backdrop_mat.set_shader_parameter("u_tex", painting)
	_backdrop_mat.set_shader_parameter("u_has", 1.0 if painting else 0.0)
	_backdrop_mat.set_shader_parameter("u_a", Mats.srgb(accent) * 0.5)
	_backdrop_mat.set_shader_parameter("u_b", Mats.srgb(accent2) * 0.4)
	env.fog_light_color = Mats.srgb(fog)
	# Pillars: obsidian columns with glowing bands.
	for p: Vector3 in a.pillars:
		var g := Node3D.new()
		g.position = Vector3(p.x, 0, p.y)
		var c := Models.mesh(g, Geo.cylinder(p.z, p.z * 1.1, 4, 8), Mats.obsidian(0x07080d, 0.14, 0.95), Vector3(0, 2, 0))
		c.set_instance_shader_parameter(&"rim_color", Vector3.ZERO)
		Models.mesh(g, Geo.cylinder(p.z * 1.02, p.z * 1.02, 0.12, 8, true), Mats.glow(accent, 4), Vector3(0, 3.2, 0))
		_stage.add_child(g)
	_sparks.restart()
	_shards_dark.restart()
	_shards_lit.restart()
	_rn = 0
	_fn = 0
	_an = 0
	_afn = 0
	_trail_n = 0
	popups.clear()
	_cam_x = w.player.x
	_cam_z = w.player.y
	_prewarm()

## Every model the fight can need, drawn once (tiny, under the camera) so no pipeline is compiled mid-fight.
func _prewarm() -> void:
	if _warm == null:
		_warm = Node3D.new()
		for look: String in Models.LOOKS:
			_warm.add_child(Models.enemy(look, 1.0, 0xff2fb9, look == "mote").root)
		add_child(_warm)
	_warm.visible = true
	_warm.scale = Vector3.ONE * 0.001
	_warm.position = Vector3(_cam_x, 1, _cam_z)
	_warm_frames = 2

var _warm_frames := 0

# ---- Effects ---------------------------------------------------------------------------------------------------------

func _light(x: float, z: float, color: int, intensity: float) -> void:
	var l := _lights[_light_i]
	_light_i = (_light_i + 1) % _lights.size()
	l.position = Vector3(x, 1.6, z)
	l.light_color = Mats.srgb(color)
	l.light_energy = intensity / 14.0
	l.visible = true

func _shock(x: float, z: float, amp: float = 0.06, size: float = 0.22) -> void:
	var i := _shock_i
	_shock_i = (_shock_i + 1) % 3
	_shock_world[i] = Vector3(x, 0.5, z)
	_shocks[i] = Vector4(0, 0, 0.0, amp * (0.6 + size))

func ring(x: float, z: float, r: float, c: int, life: float = 0.45, w: float = 0.12) -> void:
	if _rn >= 256:
		return
	if _rings.size() < 256 * 9:
		_rings.resize(256 * 9)
	var o := _rn * 9
	var l := Mats.lin(c)
	_rings[o] = x; _rings[o + 1] = z; _rings[o + 2] = r; _rings[o + 3] = 0.0; _rings[o + 4] = life
	_rings[o + 5] = l.r; _rings[o + 6] = l.g; _rings[o + 7] = l.b; _rings[o + 8] = w
	_rn += 1

func _flash(x: float, z: float, r: float, life: float, c: int) -> void:
	if _fn >= 256:
		return
	if _flashes.size() < 256 * 8:
		_flashes.resize(256 * 8)
	var o := _fn * 8
	var l := Mats.lin(c)
	_flashes[o] = x; _flashes[o + 1] = z; _flashes[o + 2] = r; _flashes[o + 3] = 0.0; _flashes[o + 4] = life
	_flashes[o + 5] = l.r; _flashes[o + 6] = l.g; _flashes[o + 7] = l.b
	_fn += 1

func _arc(x1: float, z1: float, x2: float, z2: float) -> void:
	if _an >= 64:
		return
	if _arcs.size() < 64 * 5:
		_arcs.resize(64 * 5)
	var o := _an * 5
	_arcs[o] = x1; _arcs[o + 1] = z1; _arcs[o + 2] = x2; _arcs[o + 3] = z2; _arcs[o + 4] = 0.0
	_an += 1

func _popup(x: float, z: float, text: String, cls: String) -> void:
	popups.append({"x": x, "y": 1.5, "z": z, "text": text, "cls": cls, "t": 0.0})
	if popups.size() > 12:
		popups.pop_front()

func ripple(x: float, z: float, strength: float) -> void:
	_ripples[_ripple_i] = Vector4(x, z, time, strength)
	_ripple_i = (_ripple_i + 1) % 16
	_floor_mat.set_shader_parameter("u_ripples", _ripples)

## Sparks: hot additive points with drag and optional gravity (negative = rising).
func sparks(x: float, y: float, z: float, count: int, color: int, speed: float, life: float = 0.6, size: float = 0.35, up: float = 0.3, gravity: float = 0.0, drag: float = 2.5, dir: float = NAN, spread: float = 0.5) -> void:
	var l := Mats.lin(color)
	var col := Color(l.r * 3.0, l.g * 3.0, l.b * 3.0)
	var packed := roundf(drag * 10.0) * 1000.0 + gravity + 100.0
	var xf := Transform3D(Basis.IDENTITY, Vector3(x, y, z))
	var has_dir := not is_nan(dir)
	for c in count:
		var a := (dir + randf_range(-1.0, 1.0) * spread) if has_dir else randf() * TAU
		var sp := speed * randf_range(0.3, 1.0)
		var u := up * randf_range(-0.2, 1.0)
		_sparks.emit_particle(xf, Vector3(cos(a) * sp, u * sp, sin(a) * sp), col, Color(size * randf_range(0.6, 1.3), 0.0, life * randf_range(0.5, 1.2), packed), EMIT_FLAGS)

## Debris: obsidian chips and glowing splinters thrown by a kill.
func shards(x: float, z: float, y: float, count: int, size: float, speed: float, color: int, lit_ratio: float = 0.4) -> void:
	var l := Mats.lin(color)
	for k in count:
		var a := randf() * TAU
		var upk := randf_range(0.2, 1.0)
		var sp := speed * randf_range(0.35, 1.1)
		var xf := Transform3D(Basis.IDENTITY, Vector3(x + randf_range(-0.2, 0.2) * size, y + randf_range(-0.3, 0.3) * size, z + randf_range(-0.2, 0.2) * size))
		var v := Vector3(cos(a) * sp, upk * sp * 0.9 + 2.0, sin(a) * sp)
		var s := size * randf_range(0.08, 0.32)
		var life := randf_range(1.2, 2.6)
		if randf() < lit_ratio:
			var hot := randf_range(2.0, 5.0)
			var c := Color(minf(8.0, l.r * hot + 0.3), minf(8.0, l.g * hot + 0.3), minf(8.0, l.b * hot + 0.3))
			_shards_lit.emit_particle(xf, v, c, Color(s, 0, life, 1), EMIT_FLAGS)
		else:
			_shards_dark.emit_particle(xf, v, Color.WHITE, Color(s, 0, life, 0), EMIT_FLAGS)

func kick(x: float, z: float) -> void:
	_kick_x += x
	_kick_z += z

func on_beat(strong: bool) -> void:
	_beat_pulse = 1.0 if strong else 0.6

func handle(events: Array[Dictionary], w: World) -> void:
	for e in events:
		var t: String = e.t
		match t:
			"shot":
				var wp: String = e.weapon
				if wp == "arcLink":
					_arc(e.x1, e.y1, e.x, e.y)
					sparks(e.x, 1, e.y, 8, 0x9ff8ff, 8, 0.3, 0.3)
				elif wp == "arcMiss":
					sparks(e.x, 1, e.y, 4, 0x9ff8ff, 4, 0.2)
				else:
					var big: bool = e.accent
					var perfect := wp.contains("Perfect")
					sparks(e.x, 0.9, e.y, 7 if big else 3, 0xffffff if perfect else 0x9ff8ff, 10 if big else 6, 0.18, 0.28, 0.3, 0.0, 2.5, e.a, 0.35)
					_flash(e.x, e.y, 1.4 if big else 0.8, 0.08, 0x9ff8ff)
					if perfect:
						ring(e.x, e.y, 2.2, 0xffffff, 0.3, 0.15)
						_light(e.x, e.y, 0xbff8ff, 50)
			"hit":
				var big: bool = e.big
				sparks(e.x, 0.9, e.y, 14 if big else 5, e.color, 12 if big else 7, 0.3, 0.3, 0.6)
				if big:
					_flash(e.x, e.y, 1.4, 0.1, e.color)
			"kill":
				var s: float = e.size
				var heavy: bool = e.heavy
				shards(e.x, e.y, 0.9, int(U.jround(18 + s * 26)), s * 1.6, 9 + s * 6, e.color, 0.45)
				sparks(e.x, 0.9, e.y, int(U.jround(40 + s * 50)), e.color, 14 + s * 8, 0.7, 0.4, 0.8, 0.0, 2.0)
				sparks(e.x, 0.9, e.y, 16, 0xffffff, 20, 0.25, 0.5)
				ring(e.x, e.y, 2.5 + s * 2.5, e.color, 0.4, 0.1)
				ring(e.x, e.y, 1.2 + s, 0xffffff, 0.2, 0.25)
				_flash(e.x, e.y, 2.5 + s * 2, 0.18, e.color)
				_light(e.x, e.y, e.color, 160 if heavy else 70)
				ripple(e.x, e.y, 1.6 if heavy else 0.8)
				_trauma = minf(1.0, _trauma + (0.35 if heavy else 0.12))
				if heavy:
					_shock(e.x, e.y, 0.05, 0.25)
					_chroma_k = maxf(_chroma_k, 0.5)
			"boom":
				var r: float = e.r
				sparks(e.x, 0.6, e.y, int(U.jround(30 + r * 20)), e.color, 6 + r * 6, 0.5, 0.45)
				ring(e.x, e.y, r, e.color, 0.35, 0.2)
				ring(e.x, e.y, r * 0.6, 0xffffff, 0.18, 0.4)
				_flash(e.x, e.y, r * 1.3, 0.2, e.color)
				_light(e.x, e.y, e.color, 90)
				ripple(e.x, e.y, minf(2.0, r * 0.4))
			"ring":
				ring(e.x, e.y, e.r, e.color, float(e.get("life", 20)) / 60.0, 0.15)
			"spawn":
				ring(e.x, e.y, 2.4, e.color, w.beat.spb, 0.08)
				sparks(e.x, 0.3, e.y, 20, e.color, 3, 0.8, 0.3, 2.0, -4.0)
			"dash":
				var perf: bool = e.judge == "perfect"
				_fov_kick = 1.0 if perf else 0.6
				sparks(e.x, 0.8, e.y, 30 if perf else 14, 0xffffff if perf else 0x2ef2ff, 10, 0.35, 0.35, 0.3, 0.0, 2.5, e.a + PI, 0.6)
				ring(e.x, e.y, 2.4 if perf else 1.5, 0xffffff if perf else 0x2ef2ff, 0.25, 0.2)
			"pulse":
				var perf: bool = e.judge == "perfect"
				ring(e.x, e.y, e.r, 0xffffff if perf else 0x2ef2ff, 0.25, 0.25)
				ring(e.x, e.y, e.r * 1.25, 0x2ef2ff, 0.4, 0.06)
				if perf:
					_shock(e.x, e.y, 0.05, 0.18)
				_light(e.x, e.y, 0x7ff6ff, 120 if perf else 50)
			"parry":
				var n: int = e.n
				_flash_k = maxf(_flash_k, 0.25)
				_flash_c = Mats.srgb(0x9ff8ff)
				sparks(e.x, 1, e.y, 30 + n * 6, 0xffffff, 14, 0.4)
				_popup(e.x, e.y, ("PARADE ×%d" % n) if n > 1 else "PARADE", "parry")
				_trauma = minf(1.0, _trauma + 0.2)
			"graze":
				sparks(e.x, 0.8, e.y, 5, 0xffffff, 5, 0.2, 0.22)
			"hurt":
				_flash_k = 0.32
				_flash_c = Mats.srgb(0xff2040)
				_chroma_k = 1.4
				ring(e.x, e.y, 4.5, 0xff3050, 0.4, 0.15)
				_shock(e.x, e.y, 0.09, 0.3)
				sparks(e.x, 1, e.y, 40, 0xff3050, 14, 0.5)
			"die":
				shards(e.x, e.y, 1, 80, 1.6, 16, 0x2ef2ff, 0.8)
				sparks(e.x, 1, e.y, 200, 0x9ff8ff, 22, 1.2, 0.5)
				_flash_k = 1.0
				_flash_c = Color.WHITE
				_shock(e.x, e.y, 0.12, 0.4)
			"judge":
				if e.judge == "perfect":
					_popup(e.x, e.y, "PARFAIT", "perfect")
					sparks(e.x, 1.2, e.y, 12, 0xffd890, 6, 0.4, 0.3, 1.5)
				else:
					_popup(e.x, e.y, "BIEN", "great")
			"score":
				var v: int = e.value
				if v >= 400:
					_popup(e.x, e.y, "+%d" % v, "big" if v >= 1000 else "score")
			"note":
				sparks(e.x, 0.8, e.y, 4, 0xffc34a, 3, 0.25, 0.25)
			"heal":
				ring(e.x, e.y, 2, 0x40ffb0, 0.4, 0.2)
				_popup(e.x, e.y, "+1", "heal")
			"overdrive":
				if e.on:
					_shock(w.player.x, w.player.y, 0.14, 0.5)
					ring(w.player.x, w.player.y, 12, 0xffd890, 0.7, 0.05)
					_chroma_k = 1.0
			"shake":
				_trauma = minf(1.0, _trauma + float(e.a))
			"flash":
				_flash_k = maxf(_flash_k, float(e.a))
				_flash_c = Mats.srgb(e.color)
			"chroma":
				_chroma_k = maxf(_chroma_k, float(e.a))
			"bossDown":
				_shock(e.x, e.y, 0.15, 0.6)
				_flash_k = 1.0
				_flash_c = Color.WHITE

# ---- Frame -------------------------------------------------------------------------------------------------------------

func render(w: World, alpha: float, dt: float) -> void:
	time += dt
	if _warm_frames > 0:
		_warm_frames -= 1
		if _warm_frames == 0:
			_warm.visible = false
	var p := w.player
	var px := p.px + (p.x - p.px) * alpha
	var pz := p.py + (p.y - p.py) * alpha
	_od_k += ((1.0 if p.is_overdrive() else 0.0) - _od_k) * U.damp(4.0, dt)
	_beat_pulse *= exp(-dt * 5.0)
	# Camera: follows the hero with a slight lead towards the aim, frames bosses wider.
	var lead := 3.2
	var tx := px + cos(p.aim) * lead
	var tz := pz + sin(p.aim) * lead * 0.8
	var boss: Enemy = null
	for e in w.enemies:
		if e.boss and not e.dead:
			boss = e
			break
	if boss:
		tx = px + (boss.x - px) * 0.38
		tz = pz + (boss.y - pz) * 0.38
		zoom_target = 1.32
	else:
		zoom_target = 1.0
	_cam_x += (tx - _cam_x) * U.damp(5.0, dt)
	_cam_z += (tz - _cam_z) * U.damp(5.0, dt)
	var ext := w.arena.extent
	_cam_x = clampf(_cam_x, -ext + 8, ext - 8)
	_cam_z = clampf(_cam_z, -ext + 6, ext - 4)
	_zoom += (zoom_target - _zoom) * U.damp(2.0, dt)
	_trauma = maxf(0.0, _trauma - dt * 1.6)
	var sh := _trauma * _trauma * shake_scale
	var t := time
	var ox := (sin(t * 61) + sin(t * 37.3)) * 0.5 * sh * 0.9 + _kick_x
	var oz := (sin(t * 53.1) + sin(t * 29.7)) * 0.5 * sh * 0.9 + _kick_z
	var height := 19.5 * _zoom
	var back := 11.5 * _zoom
	camera.look_at_from_position(Vector3(_cam_x + ox, height, _cam_z + back + oz), Vector3(_cam_x + ox * 0.5, 0, _cam_z + oz * 0.5))
	camera.rotate_object_local(Vector3.FORWARD, -sin(t * 47) * sh * 0.012)
	_fov_kick *= exp(-dt * 6.0)
	camera.fov = 36.0 + _fov_kick * 3.0 + _od_k * 4.0
	update_mirror()
	_update_player(w, px, pz, dt)
	_update_enemies(w, alpha, dt)
	_draw_glows(w, alpha, dt, px, pz)
	for l in _lights:
		if l.visible:
			l.light_energy *= exp(-dt * 9.0)
			if l.light_energy < 0.02:
				l.visible = false
	# Floor and abyss.
	_floor_mat.set_shader_parameter("u_time", t)
	_floor_mat.set_shader_parameter("u_beat", w.beat.beat)
	_floor_mat.set_shader_parameter("u_pulse", _beat_pulse)
	_floor_mat.set_shader_parameter("u_level", level)
	_floor_mat.set_shader_parameter("u_player", Vector2(px, pz))
	_floor_mat.set_shader_parameter("u_overdrive", _od_k)
	_backdrop_mat.set_shader_parameter("u_time", t)
	_backdrop_mat.set_shader_parameter("u_level", level)
	_update_debris(t, dt)
	# Screen.
	_chroma_k *= exp(-dt * 4.0)
	env.glow_intensity = 0.38 + _beat_pulse * 0.07 + _od_k * 0.17
	env.adjustment_saturation = 1.12 + _od_k * 0.15
	env.adjustment_brightness = 1.0 + _od_k * 0.04
	_flash_k *= exp(-dt * 10.0)
	_kick_x *= exp(-dt * 12.0)
	_kick_z *= exp(-dt * 12.0)
	if post:
		var ca := 0.0006 + _chroma_k * 0.006 * flash_scale + _od_k * 0.0015
		post.set_shader_parameter("u_chroma", ca)
		post.set_shader_parameter("u_time", t)
		var fc := _flash_c
		post.set_shader_parameter("u_flash", Vector4(fc.r, fc.g, fc.b, minf(0.85, _flash_k * flash_scale)))
		var vp := get_viewport().get_visible_rect().size
		post.set_shader_parameter("u_aspect", vp.x / maxf(1.0, vp.y))
		for i in 3:
			var s := _shocks[i]
			if s.z >= 0.0 and s.z <= 1.0:
				var sp := camera.unproject_position(_shock_world[i])
				s.x = sp.x / vp.x
				s.y = sp.y / vp.y
				s.z += dt / 0.375
				_shocks[i] = s
		post.set_shader_parameter("u_shocks", _shocks)
	for q in popups:
		q.t += dt

func _update_debris(t: float, dt: float) -> void:
	var buf := _debris_buf
	var p := _debris_p
	var r := _debris_r
	for i in 70:
		var o := i * 6
		r[o] += r[o + 3] * dt
		r[o + 1] += r[o + 4] * dt
		r[o + 2] += r[o + 5] * dt
		var s := p[i * 4 + 3]
		var b: Basis = Basis.from_euler(Vector3(r[o], r[o + 1], r[o + 2]), EULER_ORDER_XYZ) * Basis.from_scale(Vector3(s, s * 1.8, s))
		var q := i * 12
		buf[q] = b.x.x; buf[q + 1] = b.y.x; buf[q + 2] = b.z.x; buf[q + 3] = p[i * 4]
		buf[q + 4] = b.x.y; buf[q + 5] = b.y.y; buf[q + 6] = b.z.y; buf[q + 7] = p[i * 4 + 1] + sin(t * 0.2 + i) * 0.8
		buf[q + 8] = b.x.z; buf[q + 9] = b.y.z; buf[q + 10] = b.z.z; buf[q + 11] = p[i * 4 + 2]
	RenderingServer.multimesh_set_buffer(_debris.get_rid(), buf)

func _update_player(w: World, px: float, pz: float, dt: float) -> void:
	var p := w.player
	var m := _player
	m.root.position = Vector3(px, 0, pz)
	m.root.scale = Vector3.ONE * 1.6
	var body: Node3D = m.parts.body
	body.position.y = sin(time * 3.0) * 0.06
	(m.parts.aim as Node3D).rotation.y = -p.aim
	var halo: Node3D = m.parts.halo
	halo.rotation.x = -0.9
	halo.scale = Vector3.ONE * (1.0 + _beat_pulse * 0.3)
	(m.parts.shards as Node3D).rotation.y += dt * (2.5 + _od_k * 5.0)
	body.rotation.z = -p.vx * 0.012
	body.rotation.x = p.vy * 0.012
	m.root.visible = not p.dead and not (p.inv > 0 and (p.inv / 4) % 2 == 0)
	_player_light.position = Vector3(px, 2.2, pz)
	_player_light.light_energy = (55.0 + _beat_pulse * 30.0 + _od_k * 60.0) / 14.0
	_player_light.light_color = Mats.srgb(0xffe0a0 if p.is_overdrive() else 0x7ff6ff)
	# Ribbon trail (ring buffer, newest first) and dash afterimages.
	_trail_head = (_trail_head + 17) % 18
	_trail[_trail_head] = Vector2(px, pz)
	_trail_n = mini(18, _trail_n + 1)
	if p.is_dashing() and w.frame % 2 == 0 and _afn < 64:
		if _after.size() < 64 * 3:
			_after.resize(64 * 3)
		_after[_afn * 3] = px
		_after[_afn * 3 + 1] = pz
		_after[_afn * 3 + 2] = 0.0
		_afn += 1
	if p.is_overdrive() and w.frame % 3 == 0:
		sparks(px, 1.2, pz, 2, 0xffd890, 3, 0.6, 0.3, 2.0, -3.0)

func _model_key(e: Enemy, r: float) -> String:
	return "%s|%s|%d|%s" % [e.look, snappedf(r, 0.001), e.color, e.elite]

func _release(m: VModel) -> void:
	m.root.visible = false
	var list: Array = _pool.get(m.key, [])
	list.append(m)
	_pool[m.key] = list

func _update_enemies(w: World, alpha: float, dt: float) -> void:
	_seen.clear()
	for e in w.enemies:
		_seen[e.id] = true
		var m: VModel = _models.get(e.id)
		if m == null:
			var r := e.r * (1.0 if e.boss else 1.35)
			var key := _model_key(e, r)
			var list: Array = _pool.get(key, [])
			if list.size() > 0:
				m = list.pop_back()
				m.reset()
			else:
				m = Models.enemy(e.look, r, e.color, e.elite)
				m.key = key
				add_child(m.root)
			_models[e.id] = m
		_pose_enemy(e, m, alpha, dt, w)
	for id in _models.keys():
		if not _seen.has(id):
			_release(_models[id])
			_models.erase(id)

func _pose_enemy(e: Enemy, m: VModel, alpha: float, dt: float, w: World) -> void:
	var x := e.px + (e.x - e.px) * alpha
	var z := e.py + (e.y - e.py) * alpha
	m.root.position = Vector3(x, 0, z)
	var spawn_k := (1.0 - e.spawn_t / maxf(1.0, w.beat.spb * 60.0)) if e.spawn_t > 0 else 1.0
	m.root.scale = Vector3.ONE * (spawn_k * spawn_k if e.spawn_t > 0 else 1.0)
	m.root.visible = true
	if e.spawn_t > 0 and w.frame % 3 == 0:
		sparks(x, 0.2, z, 2, e.color, 2, 0.5, 0.3, 4.0, -6.0)
	var gain := 1.0
	if e is Mirror:
		gain = 2.0 if e.spin > 0.5 else 0.4
	m.set_state(maxf(0.0, e.flash), 1.0 if e.shielded > 0 else 0.0, gain)
	if not m.fade.is_empty() and e.look == "phantom":
		m.set_alpha(e.vis)
	var spin: Node3D = m.parts.get("spin")
	if spin:
		spin.rotation.y += dt * (1.2 + e.spin * 0.5)
		if e.look == "mote":
			spin.rotation.x += dt * 3.0
	var face: Node3D = m.parts.get("face")
	if face:
		face.rotation.y = -e.rot
	if e is Bulwark:
		var b := e as Bulwark
		var shield: Node3D = m.parts.shield
		face.rotation.y = -b.shield_a
		shield.visible = b.broken == 0
		shield.scale = Vector3.ONE * (0.9 + (b.guard / 6.0) * 0.1)
	_pose_boss(e, m, dt)
	var crown: Node3D = m.parts.get("crown")
	if crown:
		crown.rotation.y += dt * 2.0
	var rg: Node3D = m.parts.get("ring")
	if rg:
		rg.rotation.z += dt
	m.root.rotation.y = sin(time * 40.0) * 0.1 if e.stun > 0 else 0.0
	if e is Choir:
		var lc := Mats.lin3(0x40ffb0, 1.6)
		for id in (e as Choir).links:
			var o := w.enemy_by_id(id)
			if o == null or not o.is_active():
				continue
			var dx := o.x - x
			var dz := o.y - z
			var ln := sqrt(dx * dx + dz * dz)
			_glow(ST_BEAM, x + dx / 2, z + dz / 2, 1.0, atan2(dz, dx), ln / 2, 0.18, lc.x, lc.y, lc.z)

func _pose_boss(e: Enemy, m: VModel, dt: float) -> void:
	var t := time
	match e.look:
		"ostinato":
			var al: float = (e as Ostinato).align
			var rings: Array[Node3D] = m.lists.rings
			for i in rings.size():
				var rg := rings[i]
				var sp := (1.0 - al) * (0.6 + i * 0.35)
				rg.rotation.x = PI / 2 + (1.0 - al) * sin(t * sp + i * 2) * 1.1
				rg.rotation.y = (1.0 - al) * cos(t * sp * 0.8 + i) * 0.9
				rg.rotation.z += dt * (0.2 + i * 0.15) * (-1.0 if i % 2 == 1 else 1.0)
			var heart: Node3D = m.parts.heart
			heart.rotation.y += dt * 1.5
			heart.scale = Vector3(0.8 + al * 0.5, 1.4 + al * 0.6, 0.8 + al * 0.5) * (e.r * 0.4)
			(m.parts.body as Node3D).position.y = 2.2 + sin(t * 1.3) * 0.25
		"crescendo":
			(m.parts.spin as Node3D).rotation.y += dt * 0.2
			var heart: Node3D = m.parts.heart
			heart.rotation.y += dt
			heart.scale = Vector3.ONE * (1.0 + sin(t * 4.0) * 0.05)
		"crystal":
			(m.parts.body as Node3D).scale = Vector3.ONE * (0.5 + (e as Crystal).grow * 0.7)
		"fugue":
			var arms: Array[Node3D] = m.lists.arms
			for i in arms.size():
				arms[i].rotation.x = sin(t * 3.0 + i * PI) * 0.4
			(m.parts.body as Node3D).position.y = absf(sin(t * 2.2)) * 0.15
			(m.parts.face as Node3D).rotation.y = -e.rot + PI / 2
		"rubato":
			m.set_alpha(e.vis)
			m.root.position.y = (e.vis - 1.0) * 3.0
		"segment":
			m.root.position.y = (e.vis - 1.0) * 3.0
			m.root.visible = e.vis > 0.05
		"staccato":
			var blades: Array[Node3D] = m.lists.blades
			for i in blades.size():
				blades[i].rotation.z = (1.0 if i == 1 else -1.0) * (0.35 + absf(sin(t * 4.0)) * 0.25)
			(m.parts.body as Node3D).rotation.y += 0.004
		"fermata":
			(m.parts.spin as Node3D).rotation.y += dt * 0.25
			(m.parts.body as Node3D).position.y = 3.0 + sin(t * 0.8) * 0.2

# ---- Glows -------------------------------------------------------------------------------------------------------------

func _glow(st: float, x: float, z: float, y: float, a: float, ln: float, wd: float, r: float, g: float, b: float, prm: float = 0.0) -> void:
	if _gn >= GMAX:
		return
	var o := _gn * 12
	var buf := _g
	buf[o] = st; buf[o + 4] = a; buf[o + 8] = ln
	buf[o + 1] = wd; buf[o + 5] = prm
	buf[o + 2] = r; buf[o + 6] = g; buf[o + 10] = b
	buf[o + 3] = x; buf[o + 7] = y; buf[o + 11] = z
	_gn += 1

func _draw_glows(w: World, alpha: float, dt: float, px: float, pz: float) -> void:
	var buf := _g
	var t := time
	# Enemy bullets (inlined: the hot loop).
	var bl := w.bullets
	var ex := (1.0 - alpha) / 60.0 * w.k
	var n := mini(bl.n, GMAX - 600)
	var bx := bl.x
	var by := bl.y
	var bvx := bl.vx
	var bvy := bl.vy
	var br := bl.r
	var bc := bl.color
	var bk := bl.kind
	var bf := bl.flags
	var bw := bl.wait
	var wob := 0.8 + sin(t * 20.0) * 0.1
	var o := _gn * 12
	var last_c := -1
	var lc := Color()
	for i in n:
		var c := bc[i]
		if c != last_c:
			lc = Mats.lin(c)
			last_c = c
		var kk := 2.5 if bf[i] == 2 else 2.0
		var rad := br[i]
		buf[o + 2] = lc.r * kk; buf[o + 6] = lc.g * kk; buf[o + 10] = lc.b * kk
		buf[o + 3] = bx[i] - bvx[i] * ex; buf[o + 7] = 0.7; buf[o + 11] = by[i] - bvy[i] * ex
		buf[o + 5] = 0.0
		if bk[i] == EnemyBullets.SHARD:
			buf[o] = ST_STREAK; buf[o + 4] = atan2(bvy[i], bvx[i]); buf[o + 8] = rad * 3.0; buf[o + 1] = rad * 1.6
		else:
			var s := rad * (2.4 if bk[i] == EnemyBullets.BIG else 2.2) * (wob if bw[i] > 0 else 1.0)
			buf[o] = ST_ORB; buf[o + 4] = 0.0; buf[o + 8] = s; buf[o + 1] = s
		o += 12
	_gn += n
	# Soft pools of light under the foes.
	for e in w.enemies:
		if e.dead:
			continue
		var l := Mats.lin3(e.color, 0.35 * e.vis)
		_glow(ST_DISC, e.x, e.y, 0.05, 0, e.r * 2.6, e.r * 2.6, l.x, l.y, l.z)
	# Player shots.
	var s := w.shots
	var sex := (1.0 - alpha) / 60.0
	for i in s.n:
		var x := s.x[i] - s.vx[i] * sex
		var z := s.y[i] - s.vy[i] * sex
		var a := atan2(s.vy[i], s.vx[i])
		match s.kind[i]:
			Shots.LANCE: _glow(ST_BEAM, x, z, 0.9, a, 2.4, 0.5, 2.5, 4, 5)
			Shots.PERFECT: _glow(ST_STREAK, x, z, 0.9, a, 1.3, 0.36, 4, 4, 4)
			Shots.ACCENT: _glow(ST_STREAK, x, z, 0.9, a, 1.2, 0.34, 1.2, 3.6, 4)
			Shots.MISSILE: _glow(ST_ORB, x, z, 0.9, 0, 0.5, 0.5, 4, 2.2, 0.6)
			Shots.PELLET: _glow(ST_STREAK, x, z, 0.9, a, 0.5, 0.2, 0.8, 3, 3.6)
			_: _glow(ST_STREAK, x, z, 0.9, a, 0.85, 0.22, 0.6, 2.6, 3.2)
	# Hazards.
	for h in w.hazards:
		var l := Mats.lin(h.color)
		if h.kind == "zone":
			_glow(ST_ZONE, h.x, h.y, 0.08, 0, h.r, h.r, l.r * 2, l.g * 2, l.b * 2, h.k)
		else:
			var dx := h.x2 - h.x
			var dz := h.y2 - h.y
			var ln := sqrt(dx * dx + dz * dz) / 2.0
			var a := atan2(dz, dx)
			if h.kind == "beam" and h.live:
				_glow(ST_BEAM, h.x + dx / 2, h.y + dz / 2, 0.9, a, ln, h.r * 2.4, l.r * 3, l.g * 3, l.b * 3)
			else:
				_glow(ST_WARN, h.x + dx / 2, h.y + dz / 2, 0.15, a, ln, maxf(0.2, h.r * 1.6), l.r * 2, l.g * 2, l.b * 2, h.k)
	# Pickups.
	for q in w.pickups:
		var kind: String = q.kind
		var sz := 0.35 if kind == "note" else 0.8
		var y := 0.6 + sin(t * 6.0 + float(q.t)) * 0.1
		if kind == "heal":
			_glow(ST_ORB, q.x, q.y, y, 0, sz, sz, 0.6, 4, 2)
		elif kind == "fragment":
			_glow(ST_ORB, q.x, q.y, y, 0, sz, sz, 5, 4, 2)
		else:
			_glow(ST_ORB, q.x, q.y, y, 0, sz, sz, 3, 2, 0.6)
	# Rings, flashes, lightning.
	var i := _rn - 1
	while i >= 0:
		var ro := i * 9
		_rings[ro + 3] += dt
		var k := _rings[ro + 3] / _rings[ro + 4]
		if k >= 1.0:
			_rn -= 1
			var last := _rn * 9
			for j in 9:
				_rings[ro + j] = _rings[last + j]
		else:
			var rad := _rings[ro + 2] * (0.3 + 0.7 * sqrt(k))
			var f := 3.0 * (1.0 - k)
			_glow(ST_RING, _rings[ro], _rings[ro + 1], 0.12, 0, rad, rad, _rings[ro + 5] * f, _rings[ro + 6] * f, _rings[ro + 7] * f, _rings[ro + 8])
		i -= 1
	i = _fn - 1
	while i >= 0:
		var fo := i * 8
		_flashes[fo + 3] += dt
		var k := _flashes[fo + 3] / _flashes[fo + 4]
		if k >= 1.0:
			_fn -= 1
			var last := _fn * 8
			for j in 8:
				_flashes[fo + j] = _flashes[last + j]
		else:
			var f := 3.0 * (1.0 - k)
			_glow(ST_DISC, _flashes[fo], _flashes[fo + 1], 0.5, 0, _flashes[fo + 2], _flashes[fo + 2], _flashes[fo + 5] * f, _flashes[fo + 6] * f, _flashes[fo + 7] * f)
		i -= 1
	i = _an - 1
	while i >= 0:
		var ao := i * 5
		_arcs[ao + 4] += dt
		if _arcs[ao + 4] > 0.12:
			_an -= 1
			var last := _an * 5
			for j in 5:
				_arcs[ao + j] = _arcs[last + j]
		else:
			var x1 := _arcs[ao]
			var z1 := _arcs[ao + 1]
			var x2 := _arcs[ao + 2]
			var z2 := _arcs[ao + 3]
			var x0 := x1
			var z0 := z1
			for k in range(1, 4):
				var tk := k / 3.0
				var xa := x1 + (x2 - x1) * tk + (randf_range(-0.5, 0.5) if k < 3 else 0.0)
				var za := z1 + (z2 - z1) * tk + (randf_range(-0.5, 0.5) if k < 3 else 0.0)
				var dx := xa - x0
				var dz := za - z0
				_glow(ST_BEAM, x0 + dx / 2, z0 + dz / 2, 1, atan2(dz, dx), sqrt(dx * dx + dz * dz) / 2, 0.22, 1.5, 3.5, 5)
				x0 = xa
				z0 = za
		i -= 1
	# The hero: ribbon of light, dash afterimages, beat ring, dash pips, charge and pulse rings, aim line.
	var p := w.player
	if not p.dead:
		for j in range(1, _trail_n):
			var a := _trail[(_trail_head + j - 1) % 18]
			var c := _trail[(_trail_head + j) % 18]
			var dx := c.x - a.x
			var dz := c.y - a.y
			var ln := sqrt(dx * dx + dz * dz)
			if ln < 0.01:
				continue
			var k := 1.0 - float(j) / _trail_n
			_glow(ST_STREAK, a.x + dx / 2, a.y + dz / 2, 1.1, atan2(dz, dx), ln / 2 + 0.2, 0.25 * k + 0.05, 0.4 * k, 2.4 * k, 3 * k)
		i = _afn - 1
		while i >= 0:
			_after[i * 3 + 2] += dt
			if _after[i * 3 + 2] > 0.3:
				_afn -= 1
				_after[i * 3] = _after[_afn * 3]
				_after[i * 3 + 1] = _after[_afn * 3 + 1]
				_after[i * 3 + 2] = _after[_afn * 3 + 2]
			else:
				var k := 1.0 - _after[i * 3 + 2] / 0.3
				_glow(ST_DISC, _after[i * 3], _after[i * 3 + 1], 1, 0, 1.1, 1.1, 0.5 * k, 2.5 * k, 3 * k)
			i -= 1
		var ph := w.beat.phase()
		var ring_r := 1.15 + (1.0 - ph) * 1.6
		var near := 1.0 - minf(1.0, absf(w.beat.offset_sec()) / 0.12)
		_glow(ST_RING, px, pz, 0.1, 0, ring_r, ring_r, 0.15 + near * 2, 0.5 + near * 2, 0.7 + near * 2, 0.04 + near * 0.06)
		for j in p.max_dash:
			var a := PI / 2 + (j - (p.max_dash - 1) / 2.0) * 0.35
			var on := j < p.dash_charges
			_glow(ST_ORB, px + cos(a) * 1.5, pz + sin(a) * 1.5, 0.1, 0, 0.16, 0.16, 1.0 if on else 0.15, 3.0 if on else 0.3, 3.5 if on else 0.4)
		if p.charge > 0:
			var c := float(p.charge)
			_glow(ST_RING, px, pz, 0.15, 0, 1.6 - c * 0.15, 1.6 - c * 0.15, 3, 3, 4, 0.08 + c * 0.03)
		if p.pulse_t > 0:
			_glow(ST_RING, px, pz, 0.2, 0, p.pulse_r, p.pulse_r, 1, 3.5, 4, 0.12)
		_glow(ST_WARN, px + cos(p.aim) * 4, pz + sin(p.aim) * 4, 0.12, p.aim, 2.6, 0.12, 0.3, 0.8, 1, 0.2)
	_glows_mat.set_shader_parameter("u_time", t)
	RenderingServer.multimesh_set_buffer(_glows.get_rid(), buf)
	_glows.visible_instance_count = _gn
	_gn = 0

# ---- Projection --------------------------------------------------------------------------------------------------------

func project(x: float, y: float, z: float) -> Vector2:
	return camera.unproject_position(Vector3(x, y, z))

## World point under a screen position, on the plane y = 0.9.
func pick(sp: Vector2) -> Variant:
	var o := camera.project_ray_origin(sp)
	var d := camera.project_ray_normal(sp)
	if absf(d.y) < 1e-5:
		return null
	var k := (0.9 - o.y) / d.y
	if k < 0:
		return null
	var h := o + d * k
	return Vector2(h.x, h.z)
