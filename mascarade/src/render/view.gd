class_name BallView
extends Node3D
## The 3D scene of a match: the palace, its crowd, the light, the camera looking down from the south gallery (where
## the sniper stands), the sniper's red laser sweeping the crowd, the shot and its aftermath.

var sim: Sim
var palace: Palace
var crowd: Crowd
var cam: Camera3D
var env: Environment
var sun: DirectionalLight3D
var _laser: MeshInstance3D
var _dot: MeshInstance3D
var _dot_light: OmniLight3D
var _ring: MeshInstance3D
var _muzzle: OmniLight3D
var _tracer: MeshInstance3D
var _tracer_t := 0.0
var gallery := Vector3.ZERO
var aim_world := Vector3.ZERO
var laser_on := true
var mark_guest := -1         # the local spy's own guest (a faint ring only they see)
var zoom := 0.0              # 0..1, the sniper's scope
var _base_pos := Vector3.ZERO
var _base_fov := 40.0
var _focus := Vector3.ZERO
var _shake := 0.0
var _t := 0.0
var cine := 0.0              # 0..1 dramatic zoom on `cine_target` after the shot
var cine_target := Vector3.ZERO

func _init(s: Sim, venue_id: String) -> void:
	sim = s
	palace = Palace.new(s.venue, venue_id)
	add_child(palace)
	crowd = Crowd.new(s)
	add_child(crowd)
	var we := WorldEnvironment.new()
	env = Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.02, 0.012, 0.012)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.55, 0.36, 0.28)
	env.ambient_light_energy = 0.38
	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	env.tonemap_exposure = 1.05
	env.tonemap_white = 6.0
	env.glow_enabled = true
	env.glow_intensity = 0.7
	env.glow_bloom = 0.04
	env.glow_hdr_threshold = 1.1
	env.glow_blend_mode = Environment.GLOW_BLEND_MODE_SOFTLIGHT
	env.adjustment_enabled = true
	env.adjustment_saturation = 1.08
	env.adjustment_contrast = 1.06
	we.environment = env
	add_child(we)
	sun = DirectionalLight3D.new()
	sun.light_color = Color(1.0, 0.82, 0.62)
	sun.light_energy = 0.55
	sun.shadow_enabled = true
	sun.shadow_blur = 1.5
	sun.directional_shadow_max_distance = 70.0
	sun.rotation = Vector3(deg_to_rad(-62), deg_to_rad(18), 0)
	add_child(sun)
	cam = Camera3D.new()
	cam.fov = _base_fov
	cam.near = 0.3
	cam.far = 200.0
	add_child(cam)
	_frame_camera()
	gallery = Vector3(s.venue.w * 0.5 + 7.0, 7.5, s.venue.h + 2.0)
	# Laser beam and dot.
	var beam_m := StandardMaterial3D.new()
	beam_m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	beam_m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	beam_m.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	beam_m.albedo_color = Color(1.0, 0.08, 0.05, 0.35)
	_laser = Shapes.put(self, Shapes.cyl(0.008, 0.008, 1.0, 6), beam_m, Vector3.ZERO)
	var dot_m := StandardMaterial3D.new()
	dot_m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	dot_m.albedo_color = Color(1.0, 0.15, 0.1)
	dot_m.emission_enabled = true
	dot_m.emission = Color(1.0, 0.1, 0.05)
	dot_m.emission_energy_multiplier = 6.0
	_dot = Shapes.put(self, Shapes.sphere(0.03, 8, 4), dot_m, Vector3.ZERO)
	_dot_light = OmniLight3D.new()
	_dot_light.light_color = Color(1.0, 0.1, 0.05)
	_dot_light.light_energy = 0.8
	_dot_light.omni_range = 0.9
	add_child(_dot_light)
	# The spy's own marker.
	var ring_m := StandardMaterial3D.new()
	ring_m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	ring_m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	ring_m.albedo_color = Color(1.0, 0.85, 0.5, 0.55)
	_ring = Shapes.put(self, Shapes.torus(0.36, 0.41, 32), ring_m, Vector3.ZERO)
	_ring.scale = Vector3(1, 0.1, 1)
	_ring.visible = false
	_muzzle = OmniLight3D.new()
	_muzzle.light_color = Color(1.0, 0.8, 0.5)
	_muzzle.light_energy = 0.0
	_muzzle.omni_range = 14.0
	_muzzle.position = gallery
	add_child(_muzzle)
	var tr_m := beam_m.duplicate() as StandardMaterial3D
	tr_m.albedo_color = Color(1.0, 0.85, 0.5, 0.9)
	_tracer = Shapes.put(self, Shapes.cyl(0.02, 0.02, 1.0, 6), tr_m, Vector3.ZERO)
	_tracer.visible = false

func set_quality(shadows: bool, ssr: bool, ssao: bool) -> void:
	sun.shadow_enabled = shadows
	env.ssr_enabled = ssr
	env.ssr_max_steps = 48
	env.ssr_fade_in = 0.2
	env.ssr_depth_tolerance = 0.3
	env.ssao_enabled = ssao
	env.ssao_radius = 0.6
	env.ssao_intensity = 1.2

## Places the camera on the gallery side so the whole floor fits the screen.
func _frame_camera() -> void:
	var w := float(sim.venue.w)
	var h := float(sim.venue.h)
	var pitch := deg_to_rad(-54.0)
	_focus = Vector3(w * 0.5, 0.0, h * 0.5 + 0.2)
	var aspect := 16.0 / 9.0
	var vp := get_viewport()
	if vp:
		var sz := vp.get_visible_rect().size
		if sz.y > 0:
			aspect = sz.x / sz.y
	var dist := 8.0
	for it in 200:
		cam.position = _focus + Vector3(0, -sin(pitch), cos(pitch)) * dist
		cam.rotation = Vector3(pitch, 0, 0)
		# Fit the four floor corners (plus head height at the far wall) in the frustum.
		var ok := true
		var tan_v := tan(deg_to_rad(_base_fov) * 0.5)
		var tan_h := tan_v * aspect
		for c in [Vector3(0.6, 0, 0.6), Vector3(w - 0.6, 0, 0.6), Vector3(0.6, 0, h - 0.4), Vector3(w - 0.6, 0, h - 0.4), Vector3(w * 0.5, 3.2, 0.6)]:
			var lp: Vector3 = cam.transform.affine_inverse() * c
			if lp.z > -0.1 or absf(lp.x / -lp.z) > tan_h * 0.97 or absf(lp.y / -lp.z) > tan_v * 0.95:
				ok = false
				break
		if ok:
			break
		dist += 0.25
	_base_pos = cam.position

## Floor point (tiles, at chest height) under a screen position.
func screen_to_floor(p: Vector2) -> Vector2:
	var o := cam.project_ray_origin(p)
	var d := cam.project_ray_normal(p)
	if absf(d.y) < 1e-4:
		return Vector2(-1, -1)
	var t := (1.0 - o.y) / d.y
	var q := o + d * t
	return Vector2(q.x, q.z)

func floor_to_screen(x: float, z: float, y: float = 1.0) -> Vector2:
	return cam.unproject_position(Vector3(x, y, z))

## The sniper's aim (tiles) — the laser follows it.
func set_aim(a: Vector2) -> void:
	aim_world = Vector3(a.x, 0.0, a.y)

func shot(target: int, hit: Vector2) -> void:
	_muzzle.light_energy = 12.0
	_shake = 0.6
	_tracer.visible = true
	_tracer_t = 0.25
	var to := Vector3(hit.x, 1.1, hit.y)
	_place_beam(_tracer, gallery, to)
	if target >= 0:
		crowd.flash(target, 1.0)
		cine_target = Vector3(crowd._cx[target], 0.6, crowd._cz[target])
	else:
		cine_target = to

## A golden ring that spreads on the floor where a mission was just done (the candle flickers).
func pulse(p: Vector2, col: Color = Color(1.0, 0.75, 0.35)) -> void:
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	m.albedo_color = col
	var r := Shapes.put(self, Shapes.torus(0.9, 1.0, 40), m, Vector3(p.x, 0.04, p.y))
	r.scale = Vector3(0.2, 0.05, 0.2)
	var l := OmniLight3D.new()
	l.light_color = col
	l.light_energy = 3.0
	l.omni_range = 3.5
	l.position = Vector3(p.x, 1.2, p.y)
	add_child(l)
	var tw := create_tween().set_parallel()
	tw.tween_property(r, "scale", Vector3(2.2, 0.05, 2.2), 2.2).set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
	tw.tween_property(m, "albedo_color:a", 0.0, 2.2)
	tw.tween_property(l, "light_energy", 0.0, 2.2)
	tw.chain().tween_callback(func() -> void:
		r.queue_free()
		l.queue_free())

func _place_beam(m: MeshInstance3D, a: Vector3, b: Vector3) -> void:
	var d := b - a
	var l := d.length()
	if l < 0.001:
		return
	m.position = (a + b) * 0.5
	var y := d / l
	var x := y.cross(Vector3.FORWARD if absf(y.z) < 0.9 else Vector3.RIGHT).normalized()
	var z := x.cross(y)
	m.basis = Basis(x, y * l, z)

func _process(dt: float) -> void:
	_t += dt
	# Laser: from the gallery to the first guest along the line, or the floor.
	_laser.visible = laser_on
	_dot.visible = laser_on
	_dot_light.visible = laser_on
	if laser_on:
		var tgt := aim_world + Vector3(0, 1.05, 0)
		# Rest the dot on the nearest guest under the aim (it lights their coat), else on the floor.
		var best := -1
		var bd := 0.45 * 0.45
		for i in crowd.n:
			var g := sim.guests[i]
			if g.st == Sim.St.OUT:
				continue
			var dx := crowd._cx[i] - aim_world.x
			var dz := crowd._cz[i] - aim_world.z
			if dx * dx + dz * dz < bd:
				bd = dx * dx + dz * dz
				best = i
		if best < 0:
			tgt = Vector3(aim_world.x, 0.02, aim_world.z)
		else:
			var dir := (tgt - gallery).normalized()
			tgt -= dir * 0.18
		_place_beam(_laser, gallery, tgt)
		_dot.position = tgt
		_dot_light.position = tgt - (tgt - gallery).normalized() * 0.15
	if mark_guest >= 0 and sim.guests[mark_guest].st != Sim.St.DEAD:
		_ring.visible = true
		_ring.position = Vector3(lerpf(crowd._px[mark_guest], crowd._cx[mark_guest], 0.5), 0.03, lerpf(crowd._pz[mark_guest], crowd._cz[mark_guest], 0.5))
		var pulse := 0.85 + 0.15 * sin(_t * 3.0)
		_ring.scale = Vector3(pulse, 0.1, pulse)
	else:
		_ring.visible = false
	_muzzle.light_energy = maxf(0.0, _muzzle.light_energy - dt * 60.0)
	if _tracer_t > 0.0:
		_tracer_t -= dt
		if _tracer_t <= 0.0:
			_tracer.visible = false
	# Camera: scope zoom towards the aim, the dramatic close-up after the shot, a little shake.
	var pos := _base_pos
	var fov := _base_fov
	var look := _focus
	if zoom > 0.001:
		var z := ease(zoom, 0.6)
		look = _focus.lerp(aim_world, z * 0.85)
		fov = lerpf(_base_fov, 16.0, z)
	if cine > 0.001:
		var c := ease(cine, -2.0)
		look = look.lerp(cine_target, c)
		fov = lerpf(fov, 14.0, c)
	var off := pos - _focus
	cam.position = look + off
	cam.fov = fov
	if _shake > 0.0:
		_shake = maxf(0.0, _shake - dt * 1.8)
		cam.position += Vector3(randf_range(-1, 1), randf_range(-1, 1), 0) * _shake * 0.12
