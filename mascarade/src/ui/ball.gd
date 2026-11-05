class_name Ball
extends Screen
## A match: the simulation at 60 Hz, its 3D view, the computer opponents, the local controls (spy, sniper or
## party guests), the network lockstep, and the HUD of each role.
##
## cfg: {venue, mode: "contract"|"party", role: "spy"|"hunter" (local, contract), skill, missions, need, seed,
##       net: bool, host_role: "spy"|"hunter" (net), sources: ["kb", "kb2", "pad0", ...] (party), night: int}

signal finished(r: Dictionary)

const MISSION_ICON := {"letter": "✒", "poison": "⚗", "bug": "◉", "contact": "✉", "pocket": "✋", "countess": "♛"}
const PLAYER_COLORS := [Color("#f1cf7f"), Color("#e0475a"), Color("#5fd39a"), Color("#7fb4ff")]

var cfg: Dictionary
var sim: Sim
var view: BallView
var mode := "contract"
var role := "spy"
var net := false
var online: Node = null
var spy_ai: SpyAI
var hunter_ai: HunterAI
var aim := Vector2.ZERO           # tiles
var scoped := 0.0
var marks: Dictionary = {}
var over_t := -1.0
var result: Dictionary = {}
var _fire_req := false
var _ai_aim := Vector2.ZERO
var _prev_dev: Dictionary = {}
var _sent := false
var _flash_exposed: Array[int] = []
var _tolled := false

# HUD.
var _top: Label
var _sub: Label
var _missions_box: VBoxContainer
var _mission_rows: Dictionary = {}
var _feed: VBoxContainer
var _hint: RichTextLabel
var _banner: Label
var _banner_sub: Label
var _ov: Control
var _scope: ColorRect
var _scope_m: ShaderMaterial
var _cards: Array[Dictionary] = []
var _ctx: Label

func _init(c: Dictionary, world: Node3D) -> void:
	super()
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	cfg = c
	mode = c.get("mode", "contract")
	role = c.get("role", "spy")
	net = c.get("net", false)
	var vid: String = c.get("venue", "salon")
	var vd: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://venues/%s.json" % vid))
	var v := Venue.new(vd)
	var srcs: Array = c.get("sources", [])
	sim = Sim.new(v, mode, int(c.get("seed", 1)), int(vd.guests), maxi(1, srcs.size()), c.get("missions", []))
	if mode == "contract":
		sim.need = mini(int(c.get("need", 3)), sim.missions.size())
	view = BallView.new(sim, vid)
	world.add_child(view)
	if mode == "contract":
		var sk: float = c.get("skill", 1.0)
		if not net:
			if role == "spy":
				hunter_ai = HunterAI.new(sim, sk, int(c.get("seed", 1)) * 7 + 3)
				view.mark_guest = sim.spy
			else:
				spy_ai = SpyAI.new(sim, sk, int(c.get("seed", 1)) * 5 + 1)
		elif role == "spy":
			view.mark_guest = sim.spy
	aim = Vector2(v.w * 0.5, v.h * 0.5)
	_ai_aim = aim
	view.set_aim(aim)
	_build_hud()

func _exit_tree() -> void:
	if is_instance_valid(view):
		view.queue_free()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func set_quality(q: int) -> void:
	view.set_quality(q >= 1, q >= 2, q >= 2)

# ---- HUD -------------------------------------------------------------------------------------------------------------------

func _build_hud() -> void:
	_scope = ColorRect.new()
	Kit.full(_scope)
	_scope.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_scope_m = ShaderMaterial.new()
	_scope_m.shader = Shader.new()
	_scope_m.shader.code = """
shader_type canvas_item;
uniform float k = 0.0;
uniform vec2 c = vec2(0.5);
uniform float aspect = 1.7778;
void fragment() {
	vec2 d = (UV - c) * vec2(aspect, 1.0);
	float r = length(d);
	float hole = 0.43;
	float a = smoothstep(hole - 0.01, hole + 0.02, r);
	float ring = smoothstep(0.012, 0.0, abs(r - hole)) * 0.8;
	vec3 col = mix(vec3(0.0), vec3(0.85, 0.62, 0.3), ring);
	// Fine cross hairs inside the glass.
	float cross = (step(abs(d.x), 0.0012) + step(abs(d.y), 0.0012)) * step(r, hole) * step(0.02, r) * 0.35;
	COLOR = vec4(col + vec3(cross) * vec3(0.1, 0.0, 0.0), clamp(max(a * 0.97, ring + cross), 0.0, 1.0) * k);
}
"""
	_scope.material = _scope_m
	add_child(_scope)
	_ov = Control.new()
	Kit.full(_ov)
	_ov.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_ov.draw.connect(_draw_overlay)
	add_child(_ov)
	# Top: clock and what is at stake.
	var top := Kit.vbox(self, 0)
	top.position = Vector2(0, 22)
	top.size = Vector2(Kit.W, 90)
	_top = Kit.label(top, "3:30", 40, Kit.IVORY, "display", 700, 0.08, Color(0, 0, 0, 0.8), 12)
	_top.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_sub = Kit.label(top, "", 15, Kit.BRASS, "display", 400, 0.3, Color(0, 0, 0, 0.8), 8)
	_sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	# Notifications feed (top right).
	_feed = Kit.vbox(self, 8)
	_feed.position = Vector2(Kit.W - 620, 30)
	_feed.size = Vector2(590, 300)
	_feed.alignment = BoxContainer.ALIGNMENT_BEGIN
	# Controls hint at the bottom; context line above it.
	_hint = Kit.rich(self, "", 15, Kit.SOFT)
	_hint.position = Vector2(60, Kit.H - 54)
	_hint.size = Vector2(Kit.W - 120, 40)
	_hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_ctx = Kit.label(self, "", 22, Kit.GOLD, "display", 700, 0.12, Color(0, 0, 0, 0.9), 10)
	_ctx.position = Vector2(0, Kit.H - 120)
	_ctx.size = Vector2(Kit.W, 40)
	_ctx.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	# Banner (start and end).
	_banner = Kit.label(self, "", 76, Kit.IVORY, "display", 700, 0.14, Color(0.6, 0.05, 0.08, 0.7), 26)
	_banner.position = Vector2(0, Kit.H * 0.36)
	_banner.size = Vector2(Kit.W, 110)
	_banner.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_banner_sub = Kit.label(self, "", 24, Kit.SOFT, "body", 600, 0.04, Color(0, 0, 0, 0.9), 10)
	_banner_sub.position = Vector2(0, Kit.H * 0.36 + 112)
	_banner_sub.size = Vector2(Kit.W, 40)
	_banner_sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	if mode == "contract":
		_build_missions()
		var who := I18n.t("ball.you_spy") if role == "spy" else I18n.t("ball.you_hunter")
		_show_banner(who, I18n.t("ball.goal_spy", {"n": sim.need}) if role == "spy" else I18n.t("ball.goal_hunter"), 3.2)
		if role == "spy":
			var mv := Keys.key_name("up") + Keys.key_name("left") + Keys.key_name("down") + Keys.key_name("right")
			_hint.text = Kit.html(I18n.t("ball.keys_spy", {"move": mv, "act": Keys.key_name("act"), "hurry": Keys.key_name("hurry")}))
		else:
			_hint.text = Kit.html(I18n.t("ball.keys_hunter", {"scope": Keys.key_name("scope"), "fire": Keys.key_name("fire"), "mark": Keys.key_name("mark")}))
			Input.mouse_mode = Input.MOUSE_MODE_HIDDEN
	else:
		_build_cards()
		_show_banner(I18n.t("ball.party_title"), I18n.t("ball.party_goal", {"n": sim.party_goal}), 3.5)
		_hint.text = Kit.html(I18n.t("ball.keys_party"))

func _build_missions() -> void:
	var p := Kit.panel(self)
	p.position = Vector2(34, 30)
	var m := Kit.margin(p, 26, 18, 26, 20)
	_missions_box = Kit.vbox(m, 10)
	Kit.label(_missions_box, I18n.t("ball.missions") if role == "spy" else I18n.t("ball.missions_hunter"), 13, Kit.BRASS, "display", 700, 0.3)
	for mid: String in sim.missions:
		var row := Kit.vbox(_missions_box, 3)
		var l := Kit.label(row, "%s  %s" % [MISSION_ICON.get(mid, "•"), I18n.t("mission.%s.name" % mid)], 19, Kit.IVORY, "body", 700)
		var where := Kit.label(row, I18n.t("mission.%s.where" % mid), 14, Kit.MUTED, "body", 400)
		var bar := Kit.rect(row, Color(1, 1, 1, 0.08), Vector2.ZERO, Vector2(300, 4))
		bar.custom_minimum_size = Vector2(300, 4)
		var fill := Kit.rect(bar, Kit.GOLD, Vector2.ZERO, Vector2(0, 4))
		_mission_rows[mid] = {"label": l, "where": where, "fill": fill, "bar": bar}
		if role != "spy":
			bar.visible = false

func _build_cards() -> void:
	var n := sim.players.size()
	var corners := [Vector2(34, 30), Vector2(Kit.W - 334, 30), Vector2(34, Kit.H - 190), Vector2(Kit.W - 334, Kit.H - 190)]
	for i in n:
		var p := Kit.panel(self)
		p.position = corners[i]
		p.custom_minimum_size = Vector2(300, 0)
		var m := Kit.margin(p, 22, 14, 22, 16)
		var v := Kit.vbox(m, 6)
		var src: String = cfg.sources[i]
		var name := Kit.label(v, I18n.t("ball.player", {"n": i + 1}) + "  ·  " + I18n.t("ball.dev_%s" % src.substr(0, 3)), 16, PLAYER_COLORS[i], "display", 700, 0.15)
		var score := Kit.label(v, "", 26, Kit.IVORY, "display", 700, 0.2)
		var state := Kit.label(v, "", 14, Kit.MUTED, "body", 600)
		_cards.append({"name": name, "score": score, "state": state, "panel": p})

func _show_banner(a: String, b: String, hold: float) -> void:
	_banner.text = a
	_banner_sub.text = b
	_banner.modulate.a = 0.0
	_banner_sub.modulate.a = 0.0
	var tw := create_tween()
	tw.tween_property(_banner, "modulate:a", 1.0, 0.4)
	tw.parallel().tween_property(_banner_sub, "modulate:a", 1.0, 0.6)
	if hold > 0.0:
		tw.tween_interval(hold)
		tw.tween_property(_banner, "modulate:a", 0.0, 0.6)
		tw.parallel().tween_property(_banner_sub, "modulate:a", 0.0, 0.6)

func notify(text: String, col: Color = Kit.IVORY) -> void:
	var p := Kit.panel(_feed)
	var m := Kit.margin(p, 20, 10, 20, 12)
	var l := Kit.label(m, text, 17, col, "body", 700)
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.custom_minimum_size.x = 540
	p.modulate.a = 0.0
	var tw := p.create_tween()
	tw.tween_property(p, "modulate:a", 1.0, 0.3)
	tw.tween_interval(6.0)
	tw.tween_property(p, "modulate:a", 0.0, 0.8)
	tw.tween_callback(p.queue_free)
	if _feed.get_child_count() > 4:
		_feed.get_child(0).queue_free()

# ---- Simulation ----------------------------------------------------------------------------------------------------------

## One fixed step (local play).
func step() -> void:
	if sim.over != "":
		return
	if mode == "party":
		var bytes := PackedByteArray()
		for src: String in cfg.sources:
			bytes.append(Keys.device_byte(src, _prev_dev))
		sim.step(bytes)
	else:
		var sb := Keys.solo_byte() if role == "spy" else spy_ai.input()
		var a := Vector2i(aim * 256.0)
		var f := false
		if role == "hunter":
			f = _fire_req
			_fire_req = false
		else:
			var h: Array = hunter_ai.update()
			a = h[0]
			f = h[1]
		sim.step(PackedByteArray([sb]), a, f)
	_after_step()

## Lockstep: this machine's input for a future frame.
func local_net_input() -> Array:
	if role == "spy":
		return [Keys.solo_byte(), 0]
	var a := Vector2i(aim * 256.0)
	var f := 1 if _fire_req else 0
	_fire_req = false
	return [(clampi(a.x, 0, 65535) << 16) | clampi(a.y, 0, 65535), f]

func step_net(host_in: Array, guest_in: Array) -> void:
	if sim.over != "":
		return
	var host_role: String = cfg.get("host_role", "spy")
	var spy_in: Array = host_in if host_role == "spy" else guest_in
	var hun_in: Array = guest_in if host_role == "spy" else host_in
	var packed := int(hun_in[0])
	sim.step(PackedByteArray([int(spy_in[0]) & 255]), Vector2i(packed >> 16, packed & 65535), int(hun_in[1]) != 0)
	_after_step()

func _after_step() -> void:
	view.crowd.snap()
	for e in sim.events:
		match e.t:
			"mission":
				view.pulse(Vector2(int(e.x) / 256.0, int(e.y) / 256.0))
				var nm := I18n.t("mission.%s.name" % e.m)
				if mode == "contract" and role == "spy":
					notify("✔  " + I18n.t("ball.mission_ok", {"m": nm}), Kit.GOLD)
				else:
					notify("✦  " + I18n.t("ball.mission_seen", {"m": nm}), Color("#ffb08a"))
				Sound.sfx("mission")
			"shot":
				view.shot(int(e.guest), Vector2(int(e.x) / 256.0, int(e.y) / 256.0))
				Sound.sfx("shot")
			"over":
				_on_over(e)
			"kill":
				notify(I18n.t("ball.kill", {"a": int(e.by) + 1, "b": int(e.victim) + 1}), PLAYER_COLORS[int(e.by)])
				view.pulse(Vector2(int(e.x) / 256.0, int(e.y) / 256.0), Color(1.0, 0.2, 0.2))
				Sound.sfx("stab")
			"innocent":
				notify(I18n.t("ball.innocent", {"a": int(e.by) + 1}), Color("#ff8080"))
				Sound.sfx("stab")
			"respawn":
				notify(I18n.t("ball.respawn", {"a": int(e.player) + 1}), Kit.MUTED)

func _on_over(e: Dictionary) -> void:
	over_t = 0.0
	Sound.tension(0.0)
	var winner: String = e.winner
	var why: String = e.why
	var won := false
	if mode == "contract":
		won = winner == role
		# Everyone sees who the spy was.
		view.mark_guest = sim.spy
		view.crowd.flash(sim.spy, 1.5)
		view.laser_on = false
		var head := I18n.t("ball.won") if won else I18n.t("ball.lost")
		_show_banner(head, I18n.t("ball.why_%s_%s" % [winner, why]), 0.0)
		Sound.sfx("win" if won else "lose")
		if sim.shot:
			view.cine_target = Vector3(view.crowd._cx[maxi(0, sim.shot_at)], 0.6, view.crowd._cz[maxi(0, sim.shot_at)])
		else:
			view.cine_target = Vector3(view.crowd._cx[sim.spy], 0.6, view.crowd._cz[sim.spy])
	else:
		var pi := int(winner.substr(1))
		_show_banner(I18n.t("ball.party_win", {"n": pi + 1}), I18n.t("ball.party_why_%s" % why), 0.0)
		_banner.label_settings.font_color = PLAYER_COLORS[pi]
		view.cine_target = Vector3(view.crowd._cx[int(sim.players[pi].guest)], 0.6, view.crowd._cz[int(sim.players[pi].guest)])
		view.mark_guest = int(sim.players[pi].guest)
		Sound.sfx("win")
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	result = {"won": won, "winner": winner, "why": why, "frame": sim.frame, "missions": sim.done.size(), "need": sim.need,
		"role": role, "mode": mode, "shot": sim.shot, "night": cfg.get("night", 0), "scores": _scores()}

func _scores() -> Array:
	var out: Array = []
	for p in sim.players:
		out.append(int(p.score))
	return out

# ---- Per frame -----------------------------------------------------------------------------------------------------------

func _input(e: InputEvent) -> void:
	if mode != "contract" or role != "hunter" or over_t >= 0.0:
		return
	if e is InputEventMouseMotion and scoped > 0.5:
		var mm := e as InputEventMouseMotion
		var k := 0.0045 * (1.0 - 0.6 * scoped) * float(app.settings.get("sens", 1.0) if app else 1.0)
		aim += mm.relative * k * 3.0

func frame(dt: float) -> void:
	var alpha := Engine.get_physics_interpolation_fraction()
	view.crowd.draw(alpha, dt)
	if over_t >= 0.0:
		over_t += dt
		view.cine = clampf((over_t - 0.4) / 1.4, 0.0, 1.0)
		view.zoom = maxf(0.0, view.zoom - dt * 2.0)
		_scope_m.set_shader_parameter("k", maxf(0.0, scoped - over_t * 3.0))
		if over_t > 4.2 and not _sent:
			_sent = true
			finished.emit(result)
		_ov.queue_redraw()
		return
	if mode == "contract":
		_frame_contract(dt)
	else:
		_frame_party(dt)
	_ov.queue_redraw()

func _frame_contract(dt: float) -> void:
	var left := maxi(0, sim.limit - sim.frame)
	if left <= 1800 and not _tolled:
		_tolled = true
		Sound.sfx("midnight")
	var tens := maxf(float(sim.done.size()) / maxf(1.0, sim.need) * 0.75, clampf(1.0 - left / 3600.0, 0.0, 1.0))
	if role == "spy":
		var g := sim.guests[sim.spy]
		var d := Vector2(sim.cursor).distance_to(Vector2(g.x, g.y)) / 256.0
		tens = maxf(tens, clampf(1.0 - (d - 1.0) / 3.0, 0.0, 1.0) * 0.8)
	Sound.tension(tens)
	_top.text = "%d:%02d" % [left / 3600, (left / 60) % 60]
	_top.label_settings.font_color = Kit.IVORY if left > 1800 else Color("#ff6a5a")
	_sub.text = I18n.t("ball.progress", {"n": sim.done.size(), "need": sim.need})
	for mid: String in _mission_rows:
		var r: Dictionary = _mission_rows[mid]
		var done := sim.done.has(mid)
		var l: Label = r.label
		l.label_settings.font_color = Kit.GOLD if done else Kit.IVORY
		l.modulate.a = 0.55 if done else 1.0
		var fill: ColorRect = r.fill
		var frames: int = Sim.MISSION[mid].frames
		fill.size.x = 300.0 * (1.0 if done else clampf(float(sim.progress.get(mid, 0)) / frames, 0.0, 1.0))
	if role == "spy":
		var here := sim.mission_here(sim.guests[sim.spy])
		_ctx.text = I18n.t("ball.hold", {"key": Keys.key_name("act"), "m": I18n.t("mission.%s.name" % here)}) if here != "" else ""
		# The AI sniper's laser.
		_ai_aim = _ai_aim.lerp(Vector2(sim.cursor) / 256.0, 1.0 - exp(-dt * 12.0))
		view.set_aim(_ai_aim)
	else:
		_hunter_controls(dt)
		if spy_ai == null and net:
			pass
	if net and role == "spy":
		view.set_aim(Vector2(sim.cursor) / 256.0)

func _hunter_controls(dt: float) -> void:
	var want := 1.0 if Keys.down("scope") else 0.0
	var was := scoped
	if want > 0.5 and was <= 0.0:
		Sound.sfx("scope")
	scoped = move_toward(scoped, want, dt * 5.0)
	if scoped <= 0.5:
		if Input.mouse_mode != Input.MOUSE_MODE_HIDDEN:
			Input.mouse_mode = Input.MOUSE_MODE_HIDDEN
			if was > 0.5:
				Input.warp_mouse(view.floor_to_screen(aim.x, aim.y) * _screen_scale())
		var mp := get_viewport().get_mouse_position()
		var f := view.screen_to_floor(mp)
		if f.x > -0.5:
			aim = f
	elif Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	# Gamepad: right stick.
	for dv in Input.get_connected_joypads():
		var st := Vector2(Input.get_joy_axis(dv, JOY_AXIS_RIGHT_X), Input.get_joy_axis(dv, JOY_AXIS_RIGHT_Y))
		if st.length() > 0.2:
			aim += st * dt * (9.0 - 6.0 * scoped)
	aim = aim.clamp(Vector2(1.2, 1.2), Vector2(sim.venue.w - 1.2, sim.venue.h - 1.2))
	view.zoom = scoped
	view.set_aim(aim)
	_scope_m.set_shader_parameter("k", scoped)
	var vs := get_viewport().get_visible_rect().size
	_scope_m.set_shader_parameter("aspect", vs.x / maxf(1.0, vs.y))
	var c := view.floor_to_screen(aim.x, aim.y) / vs
	_scope_m.set_shader_parameter("c", c)
	if Keys.pressed("fire"):
		if scoped > 0.7 and not sim.shot:
			_fire_req = true
		elif not sim.shot:
			_ctx.text = I18n.t("ball.scope_first", {"key": Keys.key_name("scope")})
			get_tree().create_timer(1.6).timeout.connect(func() -> void:
				if is_instance_valid(_ctx):
					_ctx.text = "")
	if Keys.pressed("mark"):
		var g := _guest_at(aim, 0.6)
		if g >= 0:
			if marks.has(g):
				marks.erase(g)
			else:
				marks[g] = marks.size() + 1
			Sound.sfx("ui", 1.3)

func _screen_scale() -> Vector2:
	var vs := get_viewport().get_visible_rect().size
	var ws := Vector2(DisplayServer.window_get_size())
	return ws / vs

func _guest_at(p: Vector2, r: float) -> int:
	var best := -1
	var bd := r * r
	for i in view.crowd.n:
		if sim.guests[i].st == Sim.St.DEAD or sim.guests[i].st == Sim.St.OUT:
			continue
		var d := Vector2(view.crowd._cx[i], view.crowd._cz[i]).distance_squared_to(p)
		if d < bd:
			bd = d
			best = i
	return best

func _frame_party(_dt: float) -> void:
	var left := maxi(0, sim.limit - sim.frame)
	var best := 0
	for p in sim.players:
		best = maxi(best, int(p.score))
	Sound.tension(maxf(float(best) / sim.party_goal, clampf(1.0 - left / 3600.0, 0.0, 1.0)))
	_top.text = "%d:%02d" % [left / 3600, (left / 60) % 60]
	_sub.text = I18n.t("ball.party_sub", {"n": sim.party_goal})
	for i in _cards.size():
		var p: Dictionary = sim.players[i]
		var c: Dictionary = _cards[i]
		var score: Label = c.score
		score.text = "✦ ".repeat(int(p.score)) + "· ".repeat(maxi(0, sim.party_goal - int(p.score)))
		var state: Label = c.state
		if int(p.respawn) > 0:
			state.text = I18n.t("ball.respawning", {"s": int(ceil(int(p.respawn) / 60.0))})
			state.label_settings.font_color = Kit.MUTED
		elif int(p.exposed) > 0:
			state.text = I18n.t("ball.exposed")
			state.label_settings.font_color = Color("#ff6a5a")
			if Engine.get_frames_drawn() % 20 == 0:
				view.crowd.flash(int(p.guest), 0.8)
		else:
			state.text = I18n.t("ball.hidden")
			state.label_settings.font_color = Kit.SOFT

# ---- Overlay drawing (world-anchored icons) ------------------------------------------------------------------------------------

func _draw_overlay() -> void:
	var font := Kit.font("display", 700, 0.0, 22)
	var body := Kit.font("body", 700, 0.0, 16)
	if mode == "contract" and over_t < 0.0:
		# Mission spots: where they can happen (everyone knows the palace); people missions only for the spy.
		for mid: String in sim.missions:
			if sim.done.has(mid):
				continue
			var p := Vector3.ZERO
			var show := true
			match mid:
				"contact": p = Vector3(sim.guests[sim.contact].x / 256.0, 2.0, sim.guests[sim.contact].y / 256.0); show = role == "spy"
				"countess": p = Vector3(sim.guests[sim.countess].x / 256.0, 2.0, sim.guests[sim.countess].y / 256.0); show = role == "spy"
				"pocket": p = Vector3(sim.guests[sim.ambassador].x / 256.0, 2.0, sim.guests[sim.ambassador].y / 256.0); show = role == "spy"
				_:
					var q: Dictionary = sim.venue.pois[sim.venue.missions[mid]]
					p = Vector3(int(q.x) + 0.5, 1.6, int(q.y) + 0.5)
			if not show:
				continue
			var s := view.cam.unproject_position(p)
			var col := Color(1.0, 0.82, 0.45, 0.85)
			_ov.draw_circle(s, 17, Color(0.08, 0.03, 0.03, 0.75))
			_ov.draw_arc(s, 17, 0, TAU, 32, col, 2.0, true)
			var ic: String = MISSION_ICON.get(mid, "•")
			_ov.draw_string(font, s + Vector2(-9, 8), ic, HORIZONTAL_ALIGNMENT_CENTER, 18, 20, col)
		if role == "hunter":
			# Marks over suspects; the reticle.
			for g: int in marks:
				if sim.guests[g].st == Sim.St.DEAD:
					continue
				var s := view.cam.unproject_position(Vector3(view.crowd._cx[g], 2.15, view.crowd._cz[g]))
				var pts := PackedVector2Array([s + Vector2(-9, -16), s + Vector2(9, -16), s])
				_ov.draw_colored_polygon(pts, Color(0.9, 0.12, 0.1, 0.9))
				_ov.draw_string(body, s + Vector2(-5, -20), str(marks[g]), HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color(1, 0.8, 0.7))
			var c := view.floor_to_screen(aim.x, aim.y)
			var r := lerpf(26.0, 14.0, scoped)
			var rc := Color(1.0, 0.3, 0.22, 0.95)
			_ov.draw_arc(c, r, 0, TAU, 40, rc, 2.0, true)
			for d: Vector2 in [Vector2(1, 0), Vector2(-1, 0), Vector2(0, 1), Vector2(0, -1)]:
				_ov.draw_line(c + d * (r + 4), c + d * (r + 16), rc, 2.0, true)
			if sim.shot:
				_ov.draw_string(body, c + Vector2(22, -14), I18n.t("ball.no_bullet"), HORIZONTAL_ALIGNMENT_LEFT, -1, 15, rc)
		elif role == "spy":
			# The spy's own guest: a small gold chevron above the head.
			var g := sim.guests[sim.spy]
			if g.st != Sim.St.DEAD:
				var s := view.cam.unproject_position(Vector3(view.crowd._cx[sim.spy], 2.2, view.crowd._cz[sim.spy]))
				var pts := PackedVector2Array([s + Vector2(-8, -12), s + Vector2(8, -12), s])
				_ov.draw_colored_polygon(pts, Color(1.0, 0.85, 0.45, 0.85))
