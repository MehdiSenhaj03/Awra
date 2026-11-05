class_name View
extends Node3D
## Draws a vault and the heist in it: marble and brass, the echoes with their trails, guards with their
## flashlights, every device, vision cones and noises. Reads the simulation, never changes it.

const GMAX := 4000
const ST_ORB := 0.0
const ST_RING := 2.0
const ST_BEAM := 3.0
const ST_WARN := 4.0
const ST_DISC := 6.0
const ST_CONE := 7.0
const ST_DOT := 8.0

var camera := Camera3D.new()
var env := Environment.new()
var _we := WorldEnvironment.new()
var _root := Node3D.new()
var _floor := MeshInstance3D.new()
var _floor_mat := ShaderMaterial.new()
var _tiles_img: Image
var _dark_img: Image
var _dark_tex: ImageTexture
var _walls := MultiMesh.new()
var _vision := ImmediateMesh.new()
var _vision_mat := ShaderMaterial.new()
var _glows := MultiMesh.new()
var _glows_mat := ShaderMaterial.new()
var _g := PackedFloat32Array()
var _gn := 0
var _echo_live: Array[Node3D] = []
var _echo_ghost: Array[Node3D] = []
var _echo_light: Array[OmniLight3D] = []
var _carry: Array[Node3D] = []
var _guards: Array[Node3D] = []
var _guard_light: Array[SpotLight3D] = []
var _guard_mark: Array[Label3D] = []
var _devs: Array[Node3D] = []
var _dropped: Dictionary = {}
var _prev: PackedInt32Array = PackedInt32Array()
var _rings: Array[Dictionary] = []
var _level: Level
var time := 0.0
var _center := Vector3.ZERO
var _fit := 10.0
var zoom := 1.0
var focus := Vector3.ZERO
var follow := 0.0
## Screen space taken by the interface (in 1080p pixels): the vault is framed in what remains.
var margin_top := 120.0
var margin_bottom := 230.0
var _shake := 0.0
var flash := 0.0
var flash_color := Color(1, 0.2, 0.25)
var post: ShaderMaterial
var shake_scale := 1.0
## Planning: the trajectory each past echo recorded (Run.trace), drawn on the floor.
var traces: Array = []
var trace_frame := 0

func _init() -> void:
	add_child(_root)
	_build_env()
	add_child(camera)
	camera.fov = 34.0
	camera.near = 0.3
	camera.far = 200.0
	camera.current = true
	var moon := DirectionalLight3D.new()
	moon.light_color = Color("#8fa8ff")
	moon.light_energy = 0.35
	add_child(moon)
	moon.look_at_from_position(Vector3(-10, 30, 10), Vector3.ZERO)
	_glows.transform_format = MultiMesh.TRANSFORM_3D
	_glows.mesh = Geo.quad_y(2.0)
	_glows.instance_count = GMAX
	_glows.visible_instance_count = 0
	_g.resize(GMAX * 12)
	_glows_mat.shader = preload("res://src/render/shaders/glows.gdshader")
	var gi := MultiMeshInstance3D.new()
	gi.multimesh = _glows
	gi.material_override = _glows_mat
	gi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	gi.custom_aabb = AABB(Vector3(-100, -10, -100), Vector3(300, 30, 300))
	add_child(gi)
	_vision_mat.shader = preload("res://src/render/shaders/vision.gdshader")
	var vmi := MeshInstance3D.new()
	vmi.mesh = _vision
	vmi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	vmi.custom_aabb = AABB(Vector3(-100, -10, -100), Vector3(300, 30, 300))
	add_child(vmi)

func _build_env() -> void:
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color("#020309")
	var sky := Sky.new()
	var sm := ShaderMaterial.new()
	sm.shader = preload("res://src/render/shaders/sky.gdshader")
	sky.sky_material = sm
	sky.radiance_size = Sky.RADIANCE_SIZE_128
	env.sky = sky
	env.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color("#3a3550")
	env.ambient_light_energy = 0.6
	env.tonemap_mode = Environment.TONE_MAPPER_AGX
	env.tonemap_exposure = 0.95
	env.glow_enabled = true
	for i in 7:
		env.set_glow_level(i, [0.0, 1.0, 1.0, 0.9, 0.7, 0.4, 0.2][i])
	env.glow_intensity = 0.32
	env.glow_hdr_threshold = 0.9
	env.glow_blend_mode = Environment.GLOW_BLEND_MODE_ADDITIVE
	env.adjustment_enabled = true
	env.adjustment_contrast = 1.08
	env.adjustment_saturation = 1.08
	env.ssr_enabled = false
	_we.environment = env
	add_child(_we)

func set_quality(q: int) -> void:
	env.ssr_enabled = q >= 2
	env.ssr_max_steps = 40
	env.ssr_fade_in = 0.1
	env.ssr_depth_tolerance = 0.3
	var vp := get_viewport()
	if vp:
		vp.msaa_3d = Viewport.MSAA_4X if q >= 2 else (Viewport.MSAA_2X if q == 1 else Viewport.MSAA_DISABLED)
		vp.scaling_3d_scale = 1.0 if q >= 1 else 0.75

static func at(tx: int, ty: int, y: float = 0.0) -> Vector3:
	return Vector3(tx + 0.5, y, ty + 0.5)

static func pos(ux: int, uy: int, y: float = 0.0) -> Vector3:
	return Vector3(ux / 256.0, y, uy / 256.0)

# ---- Building -----------------------------------------------------------------------------------------------------

func build(lv: Level, sim: Sim) -> void:
	_level = lv
	for c in _root.get_children():
		c.queue_free()
	_echo_live.clear()
	_echo_ghost.clear()
	_echo_light.clear()
	_carry.clear()
	_guards.clear()
	_guard_light.clear()
	_guard_mark.clear()
	_devs.clear()
	_dropped.clear()
	_rings.clear()
	var w := lv.w
	var h := lv.h
	# Floor data textures.
	_tiles_img = Image.create(w, h, false, Image.FORMAT_R8)
	for y in h:
		for x in w:
			_tiles_img.set_pixel(x, y, Color(sim.tile_at(x, y) * 40 / 255.0, 0, 0))
	_dark_img = Image.create(w, h, false, Image.FORMAT_R8)
	_dark_tex = ImageTexture.create_from_image(_dark_img)
	var pm := PlaneMesh.new()
	pm.size = Vector2(w, h)
	_floor = MeshInstance3D.new()
	_floor.mesh = pm
	_floor.position = Vector3(w / 2.0, 0, h / 2.0)
	_floor_mat = ShaderMaterial.new()
	_floor_mat.shader = preload("res://src/render/shaders/floor.gdshader")
	_floor_mat.set_shader_parameter("u_tiles", ImageTexture.create_from_image(_tiles_img))
	_floor_mat.set_shader_parameter("u_dark", _dark_tex)
	_floor_mat.set_shader_parameter("u_size", Vector2(w, h))
	_floor.material_override = _floor_mat
	_root.add_child(_floor)
	# Walls.
	var walls: Array[Vector3] = []
	for y in h:
		for x in w:
			if sim.tile_at(x, y) == Sim.T_WALL:
				walls.append(at(x, y, 0.55))
	_walls = MultiMesh.new()
	_walls.transform_format = MultiMesh.TRANSFORM_3D
	_walls.use_custom_data = true
	_walls.mesh = Geo.box(1.0, 1.1, 1.0)
	_walls.instance_count = walls.size()
	for i in walls.size():
		_walls.set_instance_transform(i, Transform3D(Basis.IDENTITY, walls[i]))
		var wx := int(walls[i].x)
		var wz := int(walls[i].z)
		var mask := 0
		if sim.tile_at(wx + 1, wz) == Sim.T_WALL: mask |= 1
		if sim.tile_at(wx - 1, wz) == Sim.T_WALL: mask |= 2
		if sim.tile_at(wx, wz + 1) == Sim.T_WALL: mask |= 4
		if sim.tile_at(wx, wz - 1) == Sim.T_WALL: mask |= 8
		_walls.set_instance_custom_data(i, Color(mask / 15.0, 0, 0, 0))
	var wmi := MultiMeshInstance3D.new()
	wmi.multimesh = _walls
	var wm := ShaderMaterial.new()
	wm.shader = preload("res://src/render/shaders/wall.gdshader")
	wmi.material_override = wm
	wmi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_root.add_child(wmi)
	# Lamps: warm pools of light across the lit parts of the vault.
	for gy in range(2, h, 5):
		for gx in range(2, w, 6):
			if sim.tile_at(gx, gy) != Sim.T_WALL and sim.tile_at(gx, gy) != Sim.T_VOID and not sim.dark_at(gx * 256 + 128, gy * 256 + 128):
				var l := OmniLight3D.new()
				l.position = at(gx, gy, 2.6)
				l.omni_range = 6.0
				l.light_energy = 0.55
				l.light_specular = 0.25
				l.light_color = Color("#ffd29a")
				_root.add_child(l)
	# Devices.
	for v in sim.devs:
		var n: Node3D
		match v.t:
			"door":
				var along := sim.tile_at(v.x - 1, v.y) == Sim.T_WALL or sim.tile_at(v.x + 1, v.y) == Sim.T_WALL
				n = Props.door(v.k, v.ch, v.color, along)
			"plate": n = Props.plate(v.ch)
			"button": n = Props.button(v.ch)
			"lever": n = Props.lever(v.ch, false)
			"vlever": n = Props.lever(v.ch, true)
			"term": n = Props.terminal(v.ch)
			"safe": n = Props.safe()
			"card": n = Props.card(v.color)
			"gem": n = Props.gem()
			"loot": n = Props.loot_bag()
			"cam": n = Props.camera()
			"laser":
				n = Node3D.new()
				var col: int = 0xff4060
				var e0 := Props.emitter(col)
				n.add_child(e0)
				var e1 := Props.emitter(col)
				e1.position = Vector3(v.len - 1, 0, 0) if v.dir == 0 else Vector3(0, 0, v.len - 1)
				n.add_child(e1)
			_: n = Node3D.new()
		n.position = at(v.x, v.y)
		_root.add_child(n)
		_devs.append(n)
	# Spawn marker.
	var sp: Array = lv.data.spawn
	var mk := Props.mesh(_root, Geo.torus(0.4, 0.03, 6, 32), Mats.glow(0x2ef2ff, 1.5), at(int(sp[0]), int(sp[1]), 0.02))
	mk.rotation.x = PI / 2
	# Guards.
	for gi in sim.guards.size():
		var g := Props.guard()
		g.scale = Vector3.ONE * 1.25
		_root.add_child(g)
		_guards.append(g)
		var sl := SpotLight3D.new()
		sl.spot_range = 7.5
		sl.spot_angle = 42.0
		sl.light_energy = 3.0
		sl.light_color = Color("#fff0c8")
		sl.spot_attenuation = 1.2
		_root.add_child(sl)
		_guard_light.append(sl)
		var lb := Label3D.new()
		lb.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		lb.font_size = 96
		lb.pixel_size = 0.006
		lb.outline_size = 18
		lb.no_depth_test = true
		lb.font = Kit.font("display", 400, 0.0, 96)
		_root.add_child(lb)
		_guard_mark.append(lb)
	# Echo pools.
	for i in 8:
		var col: int = Props.ECHO_COLORS[i % Props.ECHO_COLORS.size()]
		var live := Props.figure(col, false)
		var ghost := Props.figure(col, true)
		live.scale = Vector3.ONE * 1.25
		ghost.scale = Vector3.ONE * 1.25
		live.visible = false
		ghost.visible = false
		_root.add_child(live)
		_root.add_child(ghost)
		_echo_live.append(live)
		_echo_ghost.append(ghost)
		var ol := OmniLight3D.new()
		ol.light_color = Mats.srgb(col)
		ol.omni_range = 2.6
		ol.light_energy = 0.0
		_root.add_child(ol)
		_echo_light.append(ol)
		var bag := Props.loot_bag()
		bag.visible = false
		_root.add_child(bag)
		_carry.append(bag)
	_center = Vector3(w / 2.0, 0, h / 2.0)
	focus = _center
	var vs := get_viewport().get_visible_rect().size if is_inside_tree() else Vector2(1920, 1080)
	var aspect := vs.x / maxf(1.0, vs.y)
	_fit = maxf(h * 1.08, w / aspect * 1.04 * (1080.0 - margin_top - margin_bottom) / 1080.0)
	_prev.clear()

# ---- Per step ---------------------------------------------------------------------------------------------------------

## Positions before a simulation step, for interpolation.
func capture(sim: Sim) -> void:
	var n := sim.echoes.size() * 3 + sim.guards.size() * 3
	if _prev.size() != n:
		_prev.resize(n)
	var k := 0
	for e in sim.echoes:
		_prev[k] = e.x; _prev[k + 1] = e.y; _prev[k + 2] = e.face
		k += 3
	for g in sim.guards:
		_prev[k] = g.x; _prev[k + 1] = g.y; _prev[k + 2] = g.face
		k += 3

func handle(events: Array[Dictionary]) -> void:
	for e in events:
		match e.t:
			"noise":
				_rings.append({"x": e.x / 256.0, "z": e.y / 256.0, "r": e.r / 256.0, "t": 0.0, "life": 0.6, "c": Color(1, 0.85, 0.5)})
			"coin":
				_rings.append({"x": e.x / 256.0, "z": e.y / 256.0, "r": 5.0, "t": 0.0, "life": 0.8, "c": Color(1, 0.8, 0.3)})
			"alarm":
				flash = 0.6
				flash_color = Color(1, 0.15, 0.2)
				_shake = 0.6
				_rings.append({"x": e.x / 256.0, "z": e.y / 256.0, "r": 3.0, "t": 0.0, "life": 0.7, "c": Color(1, 0.2, 0.25)})
			"takedown":
				_shake = maxf(_shake, 0.25)
				_rings.append({"x": e.x / 256.0, "z": e.y / 256.0, "r": 1.4, "t": 0.0, "life": 0.4, "c": Color(0.6, 0.8, 1.0)})
			"crack", "hack":
				_rings.append({"x": e.x / 256.0, "z": e.y / 256.0, "r": 1.8, "t": 0.0, "life": 0.5, "c": Color(1, 0.8, 0.35)})
			"win":
				flash = 0.5
				flash_color = Color(1, 0.85, 0.5)
			"pick", "gem":
				_rings.append({"x": e.x / 256.0, "z": e.y / 256.0, "r": 1.0, "t": 0.0, "life": 0.35, "c": Color(0.7, 1.0, 1.0)})

# ---- Frame --------------------------------------------------------------------------------------------------------

func render(sim: Sim, run: Run, alpha: float, dt: float, live_slots: Array = []) -> void:
	time += dt
	_floor_mat.set_shader_parameter("u_time", time)
	_update_dark(sim)
	_camera(dt)
	# Echoes.
	var k := 0
	for i in _echo_live.size():
		var on := i < sim.echoes.size()
		var live := on and live_slots.has(i)
		_echo_live[i].visible = live
		_echo_ghost[i].visible = on and not live
		_echo_light[i].light_energy = 0.0
		_carry[i].visible = false
		if not on:
			continue
		var e: Sim.Echo = sim.echoes[i]
		var x := float(e.x)
		var y := float(e.y)
		var f := e.face
		if k + 2 < _prev.size():
			x = lerpf(_prev[k], e.x, alpha)
			y = lerpf(_prev[k + 1], e.y, alpha)
		k += 3
		var node: Node3D = _echo_live[i] if live else _echo_ghost[i]
		node.position = Vector3(x / 256.0, 0, y / 256.0)
		node.rotation.y = -f * TAU / 256.0
		var body: Node3D = node.get_child(0)
		body.position.y = absf(sin(time * 12.0 + i)) * 0.05 if e.moving else 0.0
		body.rotation.z = 0.0
		body.rotation.x = 0.0
		if e.caught:
			node.visible = false
		_echo_light[i].position = node.position + Vector3(0, 0.9, 0)
		_echo_light[i].light_energy = 1.2 if live else 0.5
		if e.desync:
			node.visible = node.visible and fmod(time * 10.0, 1.0) > 0.25
		if e.item != "":
			_carry[i].visible = e.item == "loot"
			_carry[i].position = node.position + Vector3(0, 1.45 + sin(time * 4.0) * 0.04, 0)
			_carry[i].rotation.y = time
	# Guards.
	for gi in _guards.size():
		var g: Sim.Guard = sim.guards[gi]
		var x := float(g.x)
		var y := float(g.y)
		var fa := float(g.face)
		if k + 2 < _prev.size():
			x = lerpf(_prev[k], g.x, alpha)
			y = lerpf(_prev[k + 1], g.y, alpha)
			fa = _prev[k + 2] + Fx.adiff(_prev[k + 2], g.face) * alpha
		k += 3
		var n := _guards[gi]
		n.position = Vector3(x / 256.0, 0, y / 256.0)
		n.rotation.y = -fa * TAU / 256.0
		var body: Node3D = n.get_child(0)
		var down := g.mode == 3
		body.rotation.z = -PI / 2 if down else 0.0
		body.position.y = 0.15 if down else (absf(sin(time * 9.0 + gi)) * 0.03 if g.route.size() > 0 else 0.0)
		var sl := _guard_light[gi]
		sl.visible = not down
		var ang := fa * TAU / 256.0
		var dir := Vector3(cos(ang), 0, sin(ang))
		sl.position = n.position + Vector3(0, 1.0, 0) + dir * 0.3
		sl.look_at(n.position + dir * 4.0, Vector3.UP)
		var mk := _guard_mark[gi]
		mk.position = n.position + Vector3(0, 1.75, 0)
		if down:
			mk.text = "z"
			mk.modulate = Color(0.6, 0.7, 1.0)
		elif g.sus > 0:
			mk.text = "!" if g.sus > Sim.SUS_ALARM / 2 else "?"
			mk.modulate = Color(1, 0.25, 0.2) if g.sus > Sim.SUS_ALARM / 2 else Color(1, 0.85, 0.3)
		elif g.mode == 1 or g.mode == 2:
			mk.text = "?"
			mk.modulate = Color(1, 0.85, 0.3)
		else:
			mk.text = ""
	# Devices.
	for vi in mini(_devs.size(), sim.devs.size()):
		_device(sim, sim.devs[vi], _devs[vi], dt)
	for vi in range(_devs.size(), sim.devs.size()):
		# Dropped items (created at run time).
		var v: Sim.Dev = sim.devs[vi]
		var n: Node3D = _dropped.get(vi)
		if n == null:
			n = Props.card(v.color) if v.t == "card" else Props.loot_bag()
			_root.add_child(n)
			_dropped[vi] = n
		n.position = at(v.x, v.y, 0.4 + sin(time * 3.0) * 0.05)
		n.rotation.y = time
		n.visible = not v.gone
	for vi in _dropped.keys():
		if vi >= sim.devs.size():
			(_dropped[vi] as Node3D).queue_free()
			_dropped.erase(vi)
	_draw_glows(sim, run, dt, live_slots)
	_screen(dt)

func _update_dark(sim: Sim) -> void:
	var changed := false
	for y in sim.h:
		for x in sim.w:
			var d := 1.0 if sim.dark_at(x * 256 + 128, y * 256 + 128) else 0.0
			if _dark_img.get_pixel(x, y).r != d:
				_dark_img.set_pixel(x, y, Color(d, 0, 0))
				changed = true
	if changed:
		_dark_tex.update(_dark_img)

func _device(sim: Sim, v: Sim.Dev, n: Node3D, dt: float) -> void:
	match v.t:
		"door":
			var p: Node3D = n.get_node("panel")
			var target := -1.02 if v.on else 0.0
			p.position.y = move_toward(p.position.y, target, dt * 4.0)
		"plate":
			var top: Node3D = n.get_node("top")
			top.position.y = 0.01 if v.on else 0.03
		"button":
			var c: Node3D = n.get_node("cap")
			c.scale = Vector3(1, 0.3 if v.timer > 0 else 0.6, 1)
		"lever", "vlever":
			var arm: Node3D = n.get_node("arm")
			var on := v.on if v.t == "lever" else v.pulled > 0
			arm.rotation.z = move_toward(arm.rotation.z, -0.6 if on else 0.6, dt * 8.0)
		"term":
			var scr: GeometryInstance3D = n.get_node("screen")
			scr.set_instance_shader_parameter(&"gain", 2.0 if v.timer > 0 else 1.0)
		"safe":
			(n.get_node("loot") as Node3D).visible = not v.gone
			(n.get_node("dial") as Node3D).rotation.z += dt * (6.0 if not v.gone else 0.0) * 0.1
		"card", "gem", "loot":
			n.visible = not v.gone
			n.position.y = 0.45 + sin(time * 3.0 + v.x) * 0.06
			n.rotation.y = time * 1.5
		"cam":
			var hd: Node3D = n.get_node("head")
			hd.rotation.y = -float(v.ang) / 16.0 * TAU / 256.0
			var off := v.ch > 0 and sim.chan[v.ch] == 1
			(hd.get_node("eye") as GeometryInstance3D).set_instance_shader_parameter(&"gain", 0.15 if off else 1.0)

func _camera(dt: float) -> void:
	var vs := get_viewport().get_visible_rect().size
	var aspect := vs.x / maxf(1.0, vs.y)
	var free := (1080.0 - margin_top - margin_bottom) / 1080.0
	var vis := _fit / zoom / free
	var target := _center.lerp(focus, follow)
	target.z += (margin_bottom - margin_top) / 1080.0 * vis * 0.5 / sin(deg_to_rad(58.0))
	var dist := vis / (2.0 * tan(deg_to_rad(camera.fov) / 2.0))
	var pitch := deg_to_rad(58.0)
	_shake = maxf(0.0, _shake - dt * 1.5)
	var s := _shake * _shake * shake_scale
	var o := Vector3(sin(time * 53.0), 0, cos(time * 47.0)) * s * 0.25
	var eye := target + Vector3(0, sin(pitch), cos(pitch)) * dist + o
	camera.look_at_from_position(eye, target + Vector3(0, 0, 0.6) + o * 0.5)
	var _a := aspect

func _screen(dt: float) -> void:
	flash = maxf(0.0, flash - dt * 1.6)
	if post:
		post.set_shader_parameter("u_flash", Vector4(flash_color.r, flash_color.g, flash_color.b, flash * 0.5))
		post.set_shader_parameter("u_time", time)
		post.set_shader_parameter("u_chroma", 0.0005 + flash * 0.004)
		var vs := get_viewport().get_visible_rect().size
		post.set_shader_parameter("u_aspect", vs.x / maxf(1.0, vs.y))

# ---- Glows ---------------------------------------------------------------------------------------------------------

func _glow(st: float, x: float, z: float, y: float, a: float, ln: float, wd: float, c: Color, k: float, prm: float = 0.0) -> void:
	if _gn >= GMAX:
		return
	var o := _gn * 12
	_g[o] = st; _g[o + 4] = a; _g[o + 8] = ln
	_g[o + 1] = wd; _g[o + 5] = prm
	_g[o + 2] = c.r * k; _g[o + 6] = c.g * k; _g[o + 10] = c.b * k
	_g[o + 3] = x; _g[o + 7] = y; _g[o + 11] = z
	_gn += 1

## Distance (tiles) a ray travels before a wall or a closed door (grid traversal).
static func ray(sim: Sim, ox: float, oz: float, dx: float, dz: float, maxd: float) -> float:
	var tx := int(floorf(ox))
	var tz := int(floorf(oz))
	var stx := 1 if dx > 0 else -1
	var stz := 1 if dz > 0 else -1
	var tdx := absf(1.0 / dx) if absf(dx) > 1e-6 else 1e9
	var tdz := absf(1.0 / dz) if absf(dz) > 1e-6 else 1e9
	var mx := ((tx + 1 - ox) if dx > 0 else (ox - tx)) * tdx
	var mz := ((tz + 1 - oz) if dz > 0 else (oz - tz)) * tdz
	var d := 0.0
	while d < maxd:
		if mx < mz:
			d = mx
			mx += tdx
			tx += stx
		else:
			d = mz
			mz += tdz
			tz += stz
		if d >= maxd:
			return maxd
		if sim.blocks_sight(tx, tz):
			return d
	return maxd

func _cone(sim: Sim, ox: float, oz: float, ang: float, r: float, half: float, c: Color, k: float, n: int) -> void:
	if not _vision_open:
		_vision.surface_begin(Mesh.PRIMITIVE_TRIANGLES, _vision_mat)
		_vision_open = true
	var prev := Vector3.ZERO
	var prev_d := 0.0
	var col := Color(c.r, c.g, c.b, k)
	for i in n + 1:
		var a := ang - half + 2.0 * half * i / n
		var dx := cos(a)
		var dz := sin(a)
		var d := ray(sim, ox, oz, dx, dz, r)
		var p := Vector3(ox + dx * d, 0.045, oz + dz * d)
		if i > 0:
			_vision.surface_set_color(col)
			_vision.surface_set_uv(Vector2(0, 0))
			_vision.surface_add_vertex(Vector3(ox, 0.045, oz))
			_vision.surface_set_uv(Vector2(prev_d / r, 0))
			_vision.surface_add_vertex(prev)
			_vision.surface_set_uv(Vector2(d / r, 0))
			_vision.surface_add_vertex(p)
		prev = p
		prev_d = d

var _vision_open := false

func _draw_glows(sim: Sim, run: Run, dt: float, live_slots: Array) -> void:
	_vision.clear_surfaces()
	_vision_open = false
	# Trails: where each past echo walked when recorded; brighter ahead of the present.
	if run:
		for s in mini(run.trace.size(), sim.echoes.size()):
			if live_slots.has(s):
				continue
			var tr: PackedInt32Array = run.trace[s]
			var col := Mats.lin(Props.ECHO_COLORS[s % 6])
			var now := sim.frame / Run.TRACE_STEP
			var n := tr.size() / 2
			for j in range(1, n, 2):
				if tr[j * 2] == 0 and tr[j * 2 + 1] == 0:
					continue
				var ahead := j >= now
				_glow(ST_DOT, tr[j * 2] / 256.0, tr[j * 2 + 1] / 256.0, 0.03, 0, 0.06 if ahead else 0.04, 0.06 if ahead else 0.04, col, 1.6 if ahead else 0.35)
	# Pools of light under the echoes.
	for i in sim.echoes.size():
		var e: Sim.Echo = sim.echoes[i]
		if e.caught:
			continue
		var col := Mats.lin(Props.ECHO_COLORS[i % 6])
		var live := live_slots.has(i)
		_glow(ST_DISC, e.x / 256.0, e.y / 256.0, 0.04, 0, 0.6 if live else 0.45, 0.6 if live else 0.45, Color(1, 0.2, 0.2) if e.desync else col, 1.2 if live else 0.6)
		if e.hold > 0:
			var need := Sim.HACK_FRAMES if e.hold_target >= 0 and sim.devs[e.hold_target].t == "term" else Sim.CRACK_FRAMES
			var k := clampf(float(e.hold) / need, 0.0, 1.0)
			_glow(ST_RING, e.x / 256.0, e.y / 256.0, 0.06, 0, 0.5 + k * 0.4, 0.5 + k * 0.4, Color(1, 0.8, 0.35), 2.5 * k, 0.15)
	# Guards' vision cones and suspicion.
	for gi in sim.guards.size():
		var g: Sim.Guard = sim.guards[gi]
		if g.mode == 3:
			continue
		var ang := g.face * TAU / 256.0
		var r := float(g.range_t)
		var k := clampf(float(g.sus) / Sim.SUS_ALARM, 0.0, 1.0)
		var c := Color(1.0, 0.8, 0.45).lerp(Color(1, 0.15, 0.15), k)
		if g.mode == 1 or g.mode == 2:
			c = c.lerp(Color(1, 0.75, 0.2), 0.6)
		var gx := g.x / 256.0
		var gz := g.y / 256.0
		_cone(sim, gx, gz, ang, r, 0.785, c, 0.42 + k * 1.4, 22)
		if k > 0.0:
			_glow(ST_RING, gx, gz, 0.07, 0, 0.55, 0.55, Color(1, 0.3, 0.2), 1.5 + k * 2.0, 0.1 + k * 0.3)
	# Cameras and lasers.
	for v in sim.devs:
		if v.t == "cam":
			var off := v.ch > 0 and sim.chan[v.ch] == 1
			if off:
				continue
			var ang := float(v.ang) / 16.0 * TAU / 256.0
			var k := clampf(float(v.sus) / Sim.CAM_SUS_ALARM, 0.0, 1.0)
			_cone(sim, v.x + 0.5, v.y + 0.5, ang, v.rng, 0.524, Color(1, 0.3, 0.35).lerp(Color(1, 0.05, 0.05), k), 0.45 + k * 1.3, 14)
		elif v.t == "laser":
			if sim.laser_on(v):
				var l := float(v.len)
				var cx := v.x + (l / 2.0 if v.dir == 0 else 0.5)
				var cz := v.y + (0.5 if v.dir == 0 else l / 2.0)
				_glow(ST_BEAM, cx, cz, 0.55, 0.0 if v.dir == 0 else PI / 2, l / 2.0, 0.08, Color(1, 0.15, 0.2), 3.0)
				_glow(ST_BEAM, cx, cz, 0.02, 0.0 if v.dir == 0 else PI / 2, l / 2.0, 0.25, Color(1, 0.15, 0.2), 0.4)
		elif v.t == "plate" and v.on:
			_glow(ST_DISC, v.x + 0.5, v.y + 0.5, 0.06, 0, 0.6, 0.6, Mats.lin(Props.CHAN_COLORS[v.ch]), 1.2)
	# Coins in flight.
	for c in sim.coins:
		var t := 1.0 - c.t / 14.0
		var x := lerpf(c.x, c.tx, t) / 256.0
		var z := lerpf(c.y, c.ty, t) / 256.0
		_glow(ST_ORB, x, z, 0.3 + sin(t * PI) * 1.2, 0, 0.12, 0.12, Color(1, 0.8, 0.3), 3.0)
	# Rings.
	var i := _rings.size() - 1
	while i >= 0:
		var r: Dictionary = _rings[i]
		r.t += dt
		var k: float = r.t / r.life
		if k >= 1.0:
			_rings.remove_at(i)
		else:
			var rad: float = r.r * (0.2 + 0.8 * sqrt(k))
			_glow(ST_RING, r.x, r.z, 0.08, 0, rad, rad, r.c, 2.5 * (1.0 - k), 0.05)
		i -= 1
	if _vision_open:
		_vision.surface_end()
	_glows_mat.set_shader_parameter("u_time", time)
	RenderingServer.multimesh_set_buffer(_glows.get_rid(), _g)
	_glows.visible_instance_count = _gn
	_gn = 0

func project(p: Vector3) -> Vector2:
	return camera.unproject_position(p)
