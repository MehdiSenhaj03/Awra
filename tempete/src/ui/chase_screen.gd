class_name ChaseScreen
extends Screen
## A chase: the simulation at 60 Hz, the 3D plains and weather, the chase camera and the photographer's camera,
## the radar, Eli on the radio, the HUD; local crews (one player does everything, or two share the roles:
## a gamepad drives, the mouse shoots, drops probes and plans), online crews and rival teams.
##
## cfg: {day, day_id, mode: "solo"|"duo"|"net_coop"|"net_versus"|"versus_ai", host: bool, upgrades: {}}

signal finished(r: Dictionary)

var cfg: Dictionary
var chase: Chase
var world: World
var cam: Camera3D
var pcam: Camera3D                # the photographer's camera (the main one, or a picture-in-picture for a duo)
var _pip: SubViewport
var _pip_box: SubViewportContainer
var radar: Radar
var copilot: Copilot
var rival: ChaserAI
var online: Node = null
var mode := "solo"
var team := 0                     # the local team's vehicle
var can_drive := true
var can_shoot := true
var photo := false                # in the viewfinder
var yaw := 0.0                    # photo camera yaw/pitch (radians, world)
var pitch := 0.12
var zoom := 1.0
var look_storm := false
var _steer := 0.0
var _cam_yaw := 0.0
var _queue: Array = []            # pending local inputs (photo, probe, waypoint) for the next frame
var _shots: Array = []            # images waiting for their score: [{img}]
var album: Array = []             # [{img: Image, cat, score}]
var _over_t := -1.0
var _sent := false
var _wet_t := 0.0
var _alert_t := 0.0
# HUD.
var _hud: Control
var _clock: Label
var _score: Label
var _speed: Label
var _surface: Label
var _alert: Label
var _radio: VBoxContainer
var _vf: Control
var _flash_rect: ColorRect
var _hint: RichTextLabel
var _thumbs: HBoxContainer

func _init(c: Dictionary, world_root: Node3D) -> void:
	super()
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	cfg = c
	mode = c.get("mode", "solo")
	var teams := 2 if mode in ["net_versus", "versus_ai"] else 1
	chase = Chase.new(c.day, teams)
	var up: Dictionary = c.get("upgrades", {})
	for v in chase.vehicles:
		v.armor = int(up.get("armor", 100))
		v.engine = int(up.get("engine", 100))
		v.probes = int(up.get("probes", 3))
	if mode == "net_versus":
		team = 0 if c.get("host", true) else 1
	if mode == "net_coop":
		var host: bool = c.get("host", true)
		can_drive = host
		can_shoot = not host
	if mode == "versus_ai":
		rival = ChaserAI.new(chase, 1, float(c.get("rival_skill", 0.9)), int(c.day.get("seed", 1)) * 3)
	world = World.new(chase)
	world_root.add_child(world)
	cam = Camera3D.new()
	cam.far = 45000.0
	cam.near = 0.2
	cam.fov = 62.0
	world.add_child(cam)
	cam.make_current()
	pcam = cam
	copilot = Copilot.new(chase, team)
	copilot.say.connect(_radio_say)
	_build_hud()
	if mode == "duo":
		# The mouse player's viewfinder in a window of its own; the driver keeps the road.
		_pip_box = SubViewportContainer.new()
		_pip_box.stretch = true
		_pip_box.position = Vector2(40, Kit.H - 560)
		_pip_box.size = Vector2(768, 432)
		_pip_box.mouse_filter = Control.MOUSE_FILTER_IGNORE
		_pip = SubViewport.new()
		_pip.size = Vector2i(768, 432)
		_pip_box.add_child(_pip)
		pcam = Camera3D.new()
		pcam.far = 45000.0
		pcam.fov = 62.0
		_pip.add_child(pcam)
		add_child(_pip_box)
		move_child(_pip_box, 0)
		_pip_box.visible = false
	var v: Chase.Vehicle = chase.vehicles[team]
	_cam_yaw = float(v.a) / Trig.TURN * TAU

func _exit_tree() -> void:
	if is_instance_valid(world):
		world.queue_free()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func set_quality(q: int) -> void:
	world.set_quality(q)

# ---- HUD -----------------------------------------------------------------------------------------------------------------

func _build_hud() -> void:
	_vf = Control.new()
	Kit.full(_vf)
	_vf.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_vf.draw.connect(_draw_viewfinder)
	add_child(_vf)
	_hud = Control.new()
	Kit.full(_hud)
	_hud.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_hud)
	_hud.draw.connect(_draw_hud)
	_clock = Kit.label(_hud, "", 30, Kit.IVORY, "display", 700, 0.06, Color(0, 0, 0, 0.8), 8)
	_clock.position = Vector2(40, 24)
	var sub := Kit.label(_hud, I18n.t("day.%s.name" % cfg.get("day_id", "free")) if cfg.has("day_id") else I18n.t("chase.free"), 16, Kit.BRASS, "display", 500, 0.2, Color(0, 0, 0, 0.8), 6)
	sub.position = Vector2(42, 66)
	_score = Kit.label(_hud, "", 30, Kit.GOLD, "display", 700, 0.06, Color(0, 0, 0, 0.8), 8)
	_score.position = Vector2(Kit.W - 560, 24)
	_score.size = Vector2(520, 40)
	_score.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_speed = Kit.label(_hud, "", 54, Kit.IVORY, "display", 700, 0.0, Color(0, 0, 0, 0.8), 10)
	_speed.position = Vector2(48, Kit.H - 170)
	_surface = Kit.label(_hud, "", 16, Kit.SOFT, "display", 500, 0.2, Color(0, 0, 0, 0.8), 6)
	_surface.position = Vector2(52, Kit.H - 100)
	_alert = Kit.label(_hud, "", 40, Color("#ff4a3a"), "display", 700, 0.12, Color(0, 0, 0, 0.9), 14)
	_alert.position = Vector2(0, 130)
	_alert.size = Vector2(Kit.W, 50)
	_alert.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_radio = Kit.vbox(_hud, 8)
	_radio.position = Vector2(40, 140)
	_radio.custom_minimum_size.x = 560
	radar = Radar.new(chase)
	radar.team = team
	radar.position = Vector2(Kit.W - 500, Kit.H - 500)
	radar.size = Vector2(460, 460)
	radar.clickable = can_shoot
	radar.waypoint.connect(func(p: Vector2i) -> void:
		_queue.append({"waypoint": p})
		radar.waypoint_pos = p
		Sound.sfx("ui", 1.2))
	_hud.add_child(radar)
	_thumbs = Kit.hbox(_hud, 10)
	_thumbs.position = Vector2(Kit.W - 1060, Kit.H - 130)
	_hint = Kit.rich(_hud, "", 15, Kit.SOFT)
	_hint.position = Vector2(40, Kit.H - 44)
	_hint.size = Vector2(Kit.W - 600, 30)
	_flash_rect = ColorRect.new()
	_flash_rect.color = Color(1, 1, 1, 0)
	_flash_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_flash_rect)
	Kit.full(_flash_rect)
	_update_hint()

func _update_hint() -> void:
	var t := ""
	if photo:
		t = I18n.t("hud.keys_photo", {"shoot": Keys.key_name("shoot"), "camera": Keys.key_name("camera")})
	elif can_drive:
		t = I18n.t("hud.keys_drive", {"camera": Keys.key_name("camera"), "probe": Keys.key_name("probe"), "radar": Keys.key_name("radar"), "look": Keys.key_name("look")})
	else:
		t = I18n.t("hud.keys_crew", {"camera": Keys.key_name("camera"), "probe": Keys.key_name("probe")})
	_hint.text = Kit.html(t)

func _radio_say(text: String, urgent: bool) -> void:
	var p := Kit.panel(_radio)
	var m := Kit.margin(p, 14, 10, 18, 12)
	var h := Kit.hbox(m, 12)
	var pt := Kit.image(h, "pt_eli", false)
	pt.custom_minimum_size = Vector2(54, 54)
	var l := Kit.label(h, text, 17, Color("#ffd0a0") if urgent else Kit.IVORY, "body", 600)
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.custom_minimum_size.x = 460
	p.modulate.a = 0.0
	var tw := p.create_tween()
	tw.tween_property(p, "modulate:a", 1.0, 0.25)
	tw.tween_interval(7.0 if urgent else 5.5)
	tw.tween_property(p, "modulate:a", 0.0, 0.6)
	tw.tween_callback(p.queue_free)
	if _radio.get_child_count() > 3:
		_radio.get_child(0).queue_free()
	Sound.sfx("radio" if not urgent else "alert")

# ---- Inputs ------------------------------------------------------------------------------------------------------------------

func _input(e: InputEvent) -> void:
	if not photo or _over_t >= 0.0:
		return
	if e is InputEventMouseMotion:
		var mm := e as InputEventMouseMotion
		var k := 0.0025 / zoom
		yaw -= mm.relative.x * k
		pitch = clampf(pitch - mm.relative.y * k, -0.9, 1.2)
	elif e is InputEventMouseButton and e.pressed:
		var mb := e as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_WHEEL_UP:
			zoom = minf(_max_zoom(), zoom * 1.15)
		elif mb.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			zoom = maxf(1.0, zoom / 1.15)

func _max_zoom() -> float:
	return float(cfg.get("upgrades", {}).get("zoom", 4))

## This machine's input dictionary for the next simulation frame.
func local_input() -> Dictionary:
	var inp := {"team": team}
	if can_drive and not (photo and mode == "solo" and Input.get_connected_joypads().is_empty()):
		var target := Input.get_action_strength("steer_left") - Input.get_action_strength("steer_right")
		_steer = move_toward(_steer, target, 0.12) if absf(target) > 0.01 else move_toward(_steer, 0.0, 0.2)
		inp.steer = int(roundf(_steer * 8.0))
		inp.throttle = Input.get_action_strength("throttle") > 0.3
		inp.brake = Input.get_action_strength("brake") > 0.3
		inp.horn = Keys.down("horn")
	while _queue.size() > 0:
		var q: Dictionary = _queue.pop_front()
		inp.merge(q, true)
	return inp

## Lockstep: this machine's input, packed into two ints.
func local_net_input() -> Array:
	return NetCode.pack(local_input())

func step_net(host_in: Array, guest_in: Array) -> void:
	var h := NetCode.unpack(0, int(host_in[0]), int(host_in[1]))
	var g := NetCode.unpack(1 if mode == "net_versus" else 0, int(guest_in[0]), int(guest_in[1]))
	if mode == "net_coop":
		# Host drives, guest shoots: merge into one vehicle's input.
		var m := h.duplicate()
		for k in ["photo", "probe", "waypoint"]:
			if g.has(k) and (k != "probe" or g.probe):
				m[k] = g[k]
		_tick([m])
	else:
		_tick([h, g])

func step() -> void:
	var inputs: Array = [local_input()]
	if rival:
		inputs.append(rival.update())
	_tick(inputs)

func _tick(inputs: Array) -> void:
	if chase.over:
		return
	chase.step(inputs)
	copilot.update(1.0 / 60.0)
	radar.route = copilot.route
	_events()

func _events() -> void:
	for e in chase.events:
		match e.t:
			"strike":
				world.strike(int(e.x), int(e.y))
				var v: Chase.Vehicle = chase.vehicles[team]
				var d := Vector2(int(e.x) - v.x, int(e.y) - v.y).length() / 1000.0
				Sound.thunder(d)
			"photo":
				if int(e.team) == team:
					_photo_scored(e)
			"probe_drop":
				world.add_probe(int(e.x), int(e.y))
				Sound.sfx("probe")
			"probe_hit":
				Sound.sfx("score")
			"tornado":
				Sound.sfx("siren")
			"hurt":
				if int(e.team) == team:
					Sound.sfx("hit_%s" % e.why if e.why in ["hail", "debris", "lightning"] else "hit_debris")
			"wreck":
				if int(e.team) == team:
					Sound.sfx("wreck")
			"bank":
				if int(e.team) == team:
					Sound.sfx("hit_debris", 0.7)
			"over":
				_finish()

# ---- Photos -------------------------------------------------------------------------------------------------------------

func _shoot() -> void:
	var v: Chase.Vehicle = chase.vehicles[team]
	if v.film <= 0:
		_radio_say(I18n.t("hud.no_film"), false)
		return
	var f := -pcam.global_transform.basis.z
	var bearing := int(roundf(fposmod(atan2(-f.z, f.x), TAU) / TAU * Trig.TURN))
	var pt := int(roundf(asin(clampf(f.y, -1.0, 1.0)) / TAU * Trig.TURN))
	_queue.append({"photo": {"yaw": bearing, "pitch": pt, "zoom": int(zoom * 100.0)}})
	Sound.sfx("shutter")
	# The picture itself: hide the HUD for one frame and grab the screen.
	_hud.visible = false
	_vf.visible = false
	await RenderingServer.frame_post_draw
	var img := (_pip.get_texture() if _pip else get_viewport().get_texture()).get_image()
	img.resize(480, 270, Image.INTERPOLATE_BILINEAR)
	_shots.append(img)
	_hud.visible = true
	_vf.visible = true
	_flash_rect.color.a = 0.85

func _photo_scored(e: Dictionary) -> void:
	var img: Image = _shots.pop_front() if _shots.size() > 0 else null
	var top: int = e.top
	var scores: Array = e.scores
	var score: int = int(scores[top]) if top >= 0 else 0
	album.append({"img": img, "cat": top, "score": score})
	# A polaroid on the HUD.
	if img:
		var card := Control.new()
		card.custom_minimum_size = Vector2(170, 120)
		card.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var bg := Kit.rect(card, Color(0.95, 0.94, 0.9), Vector2.ZERO, Vector2(170, 124))
		bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var tr := TextureRect.new()
		tr.texture = ImageTexture.create_from_image(img)
		tr.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		tr.position = Vector2(6, 6)
		tr.size = Vector2(158, 89)
		card.add_child(tr)
		var l := Kit.label(card, "%s  %d" % [I18n.t("cat.%s" % Chase.CATS[top]) if top >= 0 else I18n.t("cat.none"), score], 14, Color(0.1, 0.1, 0.1), "display", 700)
		l.position = Vector2(8, 98)
		_thumbs.add_child(card)
		card.rotation = randf_range(-0.06, 0.06)
		if _thumbs.get_child_count() > 5:
			_thumbs.get_child(0).queue_free()
	if top < 0:
		_radio_say(I18n.t("hud.photo_nothing"), false)

# ---- Frame -------------------------------------------------------------------------------------------------------------------

func frame(dt: float) -> void:
	var v: Chase.Vehicle = chase.vehicles[team]
	# Photo mode toggles.
	if can_shoot and Keys.pressed("camera") and _over_t < 0.0:
		photo = not photo
		if _pip_box:
			_pip_box.visible = photo
		if photo:
			var f := -cam.global_transform.basis.z
			yaw = atan2(-f.x, -f.z)
			pitch = 0.15
			zoom = 1.0
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
		else:
			Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		_update_hint()
		Sound.sfx("ui")
	if photo and Keys.pressed("shoot"):
		_shoot()
	if Keys.pressed("probe") and v.probes > 0:
		if absi(v.v) < 3 * Chase.MS:
			_queue.append({"probe": true})
		else:
			_radio_say(I18n.t("hud.probe_stop"), false)
	if Keys.pressed("radar"):
		radar.velocity = not radar.velocity
	if Keys.pressed("look"):
		look_storm = not look_storm
	# Pad photo controls (right stick look, triggers zoom).
	for dv in Input.get_connected_joypads():
		if photo and mode != "duo":
			var st := Vector2(Input.get_joy_axis(dv, JOY_AXIS_RIGHT_X), Input.get_joy_axis(dv, JOY_AXIS_RIGHT_Y))
			if st.length() > 0.15:
				yaw -= st.x * dt * 1.6 / zoom
				pitch = clampf(pitch - st.y * dt * 1.2 / zoom, -0.9, 1.2)
			zoom = clampf(zoom + (Input.get_joy_axis(dv, JOY_AXIS_TRIGGER_RIGHT) - Input.get_joy_axis(dv, JOY_AXIS_TRIGGER_LEFT)) * dt * 2.0, 1.0, _max_zoom())
	_place_camera(dt, v)
	world.frame(dt, cam.global_position)
	_wet_t -= dt
	if _wet_t <= 0.0:
		_wet_t = 1.0
		world.update_wet()
	# Trucks.
	for i in chase.vehicles.size():
		var cv: Chase.Vehicle = chase.vehicles[i]
		var tr: Truck = world.trucks[i]
		tr.visible = cv.alive or true
		var alpha := Engine.get_physics_interpolation_fraction()
		var p := World.p3(cv.x, cv.y)
		tr.position = tr.position.lerp(p, 0.5) if tr.position.distance_to(p) < 20.0 else p
		tr.rotation.y = lerp_angle(tr.rotation.y, float(cv.a) / Trig.TURN * TAU, 0.5)
		var w := chase.wind_at(cv.x, cv.y)
		var speed := float(cv.v) / Chase.MS
		tr.animate(dt, speed, Vector2(w).length(), _steer if i == team else 0.0, 0.2 if cv.surface == Plains.S.PAVED else 1.0)
		tr.set_probes(cv.probes)
		if not cv.alive:
			tr.rotation.z = lerpf(tr.rotation.z, 1.4, dt)
		alpha = alpha
	# HUD numbers.
	var left := maxi(0, chase.length - chase.frame)
	_clock.text = "%d:%02d" % [left / 3600, (left / 60) % 60]
	var dos := chase.dossier(team)
	_score.text = I18n.t("hud.score", {"n": int(dos.total), "film": v.film, "probes": v.probes})
	_speed.text = "%d km/h" % absi(int(v.v * 3.6 / Chase.MS))
	var surf: String = ["field", "dirt", "paved", "water", "town"][v.surface]
	var wet := chase.plains.wet_at(v.x, v.y) > 300 and v.surface in [Plains.S.DIRT, Plains.S.FIELD]
	_surface.text = I18n.t("surface.%s" % surf) + ("  ·  " + I18n.t("surface.mud") if wet else "")
	_alert_t += dt
	var al := ""
	for tn in chase.tornadoes:
		if not tn.dead:
			al = I18n.t("hud.tornado_warning")
	if chase.hail_at(v.x, v.y) > 25:
		al = I18n.t("hud.hail")
	if not v.alive:
		al = I18n.t("hud.wrecked")
	_alert.text = al
	_alert.modulate.a = 0.55 + 0.45 * sin(_alert_t * 8.0) if al != "" else 0.0
	_flash_rect.color.a = maxf(0.0, _flash_rect.color.a - dt * 4.0)
	Sound.engine(absf(float(v.v) / Chase.MS), v.alive)
	Sound.weather(chase.rain_at(v.x, v.y), chase.hail_at(v.x, v.y), Vector2(chase.wind_at(v.x, v.y)).length())
	var close := 0.0
	for tn in chase.tornadoes:
		if not tn.dead:
			close = maxf(close, 1.0 - Vector2(tn.x - v.x, tn.y - v.y).length() / 8_000_000.0)
	Sound.tension(clampf(close, 0.0, 1.0))
	# In the viewfinder (alone at the screen), keep the frame clear.
	var full_photo := photo and _pip_box == null
	radar.visible = not full_photo
	_speed.visible = not full_photo
	_surface.visible = not full_photo
	_thumbs.visible = not full_photo
	_hud.queue_redraw()
	_vf.queue_redraw()
	if _over_t >= 0.0:
		_over_t += dt
		if _over_t > 3.0 and not _sent:
			_sent = true
			finished.emit(_result())

func _place_camera(dt: float, v: Chase.Vehicle) -> void:
	var p := World.p3(v.x, v.y)
	if photo:
		pcam.global_position = p + Vector3(0, 2.9, 0)
		pcam.rotation = Vector3(pitch, yaw, 0)
		pcam.fov = 62.0 / zoom
		if pcam == cam:
			return
	cam.fov = lerpf(cam.fov, 62.0, 1.0 - exp(-dt * 6.0))
	var want := float(v.td) / Trig.TURN * TAU
	if v.v < -Chase.MS:
		want += PI
	_cam_yaw = lerp_angle(_cam_yaw, want, 1.0 - exp(-dt * 3.0))
	var back := Vector3(cos(_cam_yaw), 0, -sin(_cam_yaw))
	cam.global_position = cam.global_position.lerp(p - back * 13.5 + Vector3(0, 5.2, 0), 1.0 - exp(-dt * 8.0))
	var look := p + back * 30.0 + Vector3(0, 6, 0)
	if look_storm:
		# Keep the nearest storm (or tornado) in view.
		var best := Vector3.ZERO
		var bd := 1e12
		for tn in chase.tornadoes:
			if not tn.dead:
				var tp := World.p3(tn.x, tn.y, 400.0)
				if tp.distance_to(p) < bd:
					bd = tp.distance_to(p)
					best = tp
		if bd > 1e11:
			for st in chase.storms:
				if not st.dead:
					var sp := World.p3(st.x, st.y, 900.0)
					if sp.distance_to(p) < bd:
						bd = sp.distance_to(p)
						best = sp
		if bd < 1e11:
			var to := (best - p)
			to.y = 0
			to = to.normalized()
			cam.global_position = cam.global_position.lerp(p - to * 13.5 + Vector3(0, 5.2, 0), 1.0 - exp(-dt * 4.0))
			look = best
	cam.look_at(look)

func _draw_hud() -> void:
	var v: Chase.Vehicle = chase.vehicles[team]
	if photo and _pip_box == null:
		return
	# Damage bar.
	var r := Rect2(52, Kit.H - 66, 300, 12)
	_hud.draw_rect(r.grow(2), Color(0, 0, 0, 0.6))
	var k := float(v.hp) / Chase.HP
	_hud.draw_rect(Rect2(r.position, Vector2(r.size.x * k, r.size.y)), Color(0.3, 0.9, 0.4).lerp(Color(1.0, 0.25, 0.2), 1.0 - k))
	_hud.draw_rect(r.grow(2), Color(1, 0.7, 0.3, 0.6), false, 1.5)
	# Compass strip: storms, tornadoes, waypoint.
	var cx := Kit.W / 2.0
	var heading := fposmod(float(v.a) / Trig.TURN * TAU, TAU) if not photo else fposmod(atan2(-(-cam.global_transform.basis.z).z, (-cam.global_transform.basis.z).x), TAU)
	_hud.draw_rect(Rect2(cx - 360, 24, 720, 34), Color(0, 0, 0, 0.45))
	var font := Kit.font("display", 700, 0.0, 18)
	for i in 8:
		var a := i * TAU / 8.0
		var d := wrapf(a - heading, -PI, PI)
		if absf(d) < PI / 2:
			var x := cx - d / (PI / 2) * 360
			var names := ["E", "NE", "N", "NO", "O", "SO", "S", "SE"]
			_hud.draw_string(font, Vector2(x - 20, 48), I18n.t("dir.%s" % names[i]), HORIZONTAL_ALIGNMENT_CENTER, 40, 18, Kit.IVORY)
	for st in chase.storms:
		if st.dead:
			continue
		var a := atan2(st.y - v.y, st.x - v.x)
		var d := wrapf(a - heading, -PI, PI)
		if absf(d) < PI / 2:
			_hud.draw_circle(Vector2(cx - d / (PI / 2) * 360, 62), 5, Color(0.6, 0.85, 1.0))
	for tn in chase.tornadoes:
		if tn.dead:
			continue
		var a := atan2(tn.y - v.y, tn.x - v.x)
		var d := wrapf(a - heading, -PI, PI)
		if absf(d) < PI / 2:
			var x := cx - d / (PI / 2) * 360
			_hud.draw_colored_polygon(PackedVector2Array([Vector2(x - 8, 58), Vector2(x + 8, 58), Vector2(x, 72)]), Color(1, 0.2, 0.15))
	if radar.waypoint_pos.x >= 0:
		var a := atan2(radar.waypoint_pos.y - v.y, radar.waypoint_pos.x - v.x)
		var d := wrapf(a - heading, -PI, PI)
		if absf(d) < PI / 2:
			_hud.draw_circle(Vector2(cx - d / (PI / 2) * 360, 62), 6, Color(0.3, 0.9, 1.0))

func _draw_viewfinder() -> void:
	if not photo:
		return
	var w := Kit.W
	var h := Kit.H
	var c := Color(1, 1, 1, 0.7)
	var m := 90.0
	if _pip_box:
		var r := Rect2(_pip_box.position, _pip_box.size)
		_vf.draw_rect(r.grow(4), Color(1, 0.7, 0.3, 0.8), false, 3.0)
		var font0 := Kit.font("mono", 600, 0.0, 18)
		var v0: Chase.Vehicle = chase.vehicles[team]
		_vf.draw_string(font0, r.position + Vector2(12, r.size.y - 14), "×%.1f   %d/%d" % [zoom, v0.film, Chase.FILM], HORIZONTAL_ALIGNMENT_LEFT, -1, 18, c)
		_vf.draw_arc(r.get_center(), 20, 0, TAU, 24, c, 1.5)
		return
	for corner in [Vector2(m, m), Vector2(w - m, m), Vector2(m, h - m), Vector2(w - m, h - m)]:
		var sx := 1.0 if corner.x < w / 2 else -1.0
		var sy := 1.0 if corner.y < h / 2 else -1.0
		_vf.draw_line(corner, corner + Vector2(60 * sx, 0), c, 3.0)
		_vf.draw_line(corner, corner + Vector2(0, 60 * sy), c, 3.0)
	_vf.draw_arc(Vector2(w / 2, h / 2), 30, 0, TAU, 32, c, 1.5)
	_vf.draw_line(Vector2(w / 2 - 50, h / 2), Vector2(w / 2 - 34, h / 2), c, 1.5)
	_vf.draw_line(Vector2(w / 2 + 34, h / 2), Vector2(w / 2 + 50, h / 2), c, 1.5)
	var font := Kit.font("mono", 600, 0.0, 22)
	var v: Chase.Vehicle = chase.vehicles[team]
	_vf.draw_string(font, Vector2(m + 10, h - m - 20), "×%.1f   %d/%d" % [zoom, v.film, Chase.FILM], HORIZONTAL_ALIGNMENT_LEFT, -1, 22, c)

# ---- End ------------------------------------------------------------------------------------------------------------------------

func _finish() -> void:
	_over_t = 0.0
	photo = false
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func _result() -> Dictionary:
	var dos := chase.dossier(team)
	# The best image of each category for the album.
	var best_imgs := {}
	for a: Dictionary in album:
		var c: int = a.cat
		if c < 0 or a.img == null:
			continue
		if not best_imgs.has(c) or int(a.score) > int((best_imgs[c] as Dictionary).score):
			best_imgs[c] = a
	var r := {"dossier": dos, "album": best_imgs, "day_id": cfg.get("day_id", ""), "mode": mode, "team": team}
	if chase.vehicles.size() > 1:
		r["rival"] = chase.dossier(1 - team)
	return r
