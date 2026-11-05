class_name GameScreen
extends Screen
## A leg on Ama's back (or a duel of titans): the simulation at 60 Hz, the 3D world, a camera per local player
## (split screen for two), what each device does at each post, the harpoons' aim with its assist, the HUD (Ama's
## vitals, the posts and who holds them, the journey, the fight's bars and telegraphs), the crew's voices, tips.
##
## cfg: {leg: {...March leg}, mode: "solo"|"coop"|"net_coop"|"duel_ai"|"duel_local"|"net_duel", host: bool,
##   crew: [...] (Meta.crew_info), gen: int, names: [4], title: String, tips: bool}

signal finished(r: Dictionary)

const POST_KEYS := ["post1", "post2", "post3", "post4", "post5"]
const P2_POST_KEYS := ["p2_post1", "p2_post2", "p2_post3", "p2_post4", "p2_post5"]
const PLAYER_COL := [Color("#f2c46d"), Color("#6fd3c8")]

class Player:
	var idx := 0
	var dev := "kbm"
	var pads: Array[int] = []
	var crew := 0
	var titan := 0
	var remote := false
	var steer := 0.0
	var yaw := 0.0
	var pitch := 0.05
	var box: SubViewportContainer
	var vp: SubViewport
	var cam: Camera3D
	var hud: Control
	var rect := Rect2()
	var hint: RichTextLabel
	var view := 0
	var prev := {}
	var nudge := 0
	var take := -1
	var lock_ok := false
	var lock := Vector3.ZERO
	var zoom := false
	var cpos := Vector3(40, 50, 60)
	var clook := Vector3(0, 20, -20)
	var fov := 62.0
	var last_post := -2
	var mouse := Vector2.ZERO

var cfg: Dictionary
var m: March
var world: World
var players: Array[Player] = []
var online: Node = null
var mode := "solo"
var gen := 0
var names: Array = ["", "", "", ""]
var _over_t := -1.0
var _sent := false
var _bird_prev: Dictionary = {}
var _stats := {"birds": 0, "skiffs": 0, "salvage": 0, "boarders": 0, "fires": 0, "weak": 0}
var _shared: Control
var _voices: VBoxContainer
var _banner: Label
var _banner_t := 0.0
var _tele: Label
var _tips: Array = []
var _tip_t := 0.0
var _tip_box: PanelContainer
var _tip_label: RichTextLabel
var _warn_t := {}
var _font: Font
var _font_b: Font
var _t := 0.0
var _foe_windup := false

func _init(c: Dictionary, world_root: Node3D) -> void:
	super()
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	cfg = c
	mode = c.get("mode", "solo")
	gen = int(c.get("gen", 0))
	names = c.get("names", ["Ysa", "Kader", "Mira", "Pip"])
	var leg: Dictionary = c.leg.duplicate(true)
	if mode.begins_with("duel") or mode == "net_duel":
		leg.duel = true
		leg.foe_ai = mode == "duel_ai"
	m = March.new(leg, c.get("crew", []))
	world = World.new(m, String(leg.get("kind", "ash")))
	world_root.add_child(world)
	world.ama.footfall.connect(func(_pos: Vector3, k: float) -> void: Sound.sfx("footfall", randf_range(0.9, 1.05), 0.55 * k))
	_font = Kit.font("display", 700, 0.06, 16)
	_font_b = Kit.font("body", 600, 0.0, 15)
	_make_players()
	_build_hud()
	if c.get("tips", false):
		_tips = I18n.tl("tips.leg")
		_tip_t = 3.0

func _exit_tree() -> void:
	if is_instance_valid(world):
		world.queue_free()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func set_quality(q: int) -> void:
	world.set_quality(q)
	for p in players:
		if p.box:
			p.box.stretch_shrink = 2 if q == 0 and app and app.get("software") else 1

# ---- Players, devices, viewports -------------------------------------------------------------------------------------------

func _make_players() -> void:
	var pads := Input.get_connected_joypads()
	var host: bool = cfg.get("host", true)
	var local_two := mode in ["coop", "duel_local"]
	var p1 := Player.new()
	p1.dev = "kbm"
	if not local_two:
		for j in pads:
			p1.pads.append(j)
	elif pads.size() > 1:
		p1.pads.append(pads[1])
	players.append(p1)
	if local_two:
		var p2 := Player.new()
		p2.idx = 1
		p2.dev = "kb2"
		if pads.size() > 0:
			p2.pads.append(pads[0])
		players.append(p2)
	# Online: this machine's player is index 0 (host) or 1 (guest); the other is remote.
	if mode in ["net_coop", "net_duel"]:
		p1.idx = 0 if host else 1
		var r := Player.new()
		r.idx = 1 if host else 0
		r.remote = true
		players.append(r)
		players.sort_custom(func(a: Player, b: Player) -> bool: return a.idx < b.idx)
	# Who starts where.
	for p in players:
		p.crew = _crew_of_role(March.P.PILOT) if p.idx == 0 else _crew_of_role(March.P.GUN_L)
		if mode.contains("duel"):
			p.crew = _crew_of_role(March.P.PILOT)
			p.titan = 1 if p.idx == 1 else 0
	# Viewports: one per local player (one shared for a local duel).
	var locals: Array[Player] = []
	for p in players:
		if not p.remote:
			locals.append(p)
	var split := locals.size() == 2 and mode != "duel_local"
	for i in locals.size():
		var p := locals[i]
		if i == 1 and not split:
			p.rect = locals[0].rect
			p.hud = locals[0].hud
			continue
		p.rect = Rect2(Vector2(Kit.W / 2.0 * i, 0), Vector2(Kit.W / 2.0, Kit.H)) if split else Rect2(Vector2.ZERO, Vector2(Kit.W, Kit.H))
		p.box = SubViewportContainer.new()
		p.box.stretch = true
		p.box.position = p.rect.position
		p.box.size = p.rect.size
		p.box.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(p.box)
		p.vp = SubViewport.new()
		p.vp.size = Vector2i(p.rect.size)
		p.vp.msaa_3d = Viewport.MSAA_2X
		p.box.add_child(p.vp)
		p.cam = Camera3D.new()
		p.cam.far = 3000.0
		p.cam.near = 0.3
		p.cam.fov = 62.0
		p.vp.add_child(p.cam)
		p.cam.make_current()
		p.hud = Control.new()
		p.hud.position = p.rect.position
		p.hud.size = p.rect.size
		p.hud.clip_contents = true
		p.hud.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(p.hud)
		p.hud.draw.connect(_draw_player.bind(p))
	for p in locals:
		p.hint = Kit.rich(p.hud, "", 16, Kit.IVORY)
		p.hint.position = Vector2(28, p.rect.size.y - 44 - (34 if p.idx == 1 and not split else 0))
		p.hint.size = Vector2(p.rect.size.x - 56, 30)

func _crew_of_role(r: int) -> int:
	for c in m.crew:
		if c.role == r:
			return c.id
	return 0

func _local() -> Array[Player]:
	var out: Array[Player] = []
	for p in players:
		if not p.remote:
			out.append(p)
	return out

## Held state of a logical button for a player, across its devices.
func _held(p: Player, b: String) -> bool:
	if p.dev == "kbm":
		var a: String = {"act": "act", "alt": "alt", "grab": "grab", "view": "view"}.get(b, "")
		if a != "" and Input.is_action_pressed(a):
			return true
		if b.begins_with("post") and Input.is_action_pressed(POST_KEYS[int(b.substr(4)) - 1]):
			return true
	elif p.dev == "kb2":
		var a: String = {"act": "p2_act", "alt": "p2_alt", "grab": "p2_grab"}.get(b, "")
		if a != "" and Input.is_action_pressed(a):
			return true
		if b.begins_with("post") and Input.is_action_pressed(P2_POST_KEYS[int(b.substr(4)) - 1]):
			return true
	for j in p.pads:
		var jb: int = {"act": JOY_BUTTON_A, "alt": JOY_BUTTON_B, "grab": JOY_BUTTON_X, "view": JOY_BUTTON_BACK,
			"post1": JOY_BUTTON_DPAD_UP, "post2": JOY_BUTTON_DPAD_LEFT, "post3": JOY_BUTTON_DPAD_RIGHT, "post4": JOY_BUTTON_DPAD_DOWN, "post5": JOY_BUTTON_Y}.get(b, -1)
		if jb >= 0 and Input.is_joy_button_pressed(j, jb as JoyButton):
			return true
		if b == "act" and Input.get_joy_axis(j, JOY_AXIS_TRIGGER_RIGHT) > 0.5:
			return true
		if b == "alt" and Input.get_joy_axis(j, JOY_AXIS_TRIGGER_LEFT) > 0.5:
			return true
	return false

func _hit(p: Player, b: String) -> bool:
	var now := _held(p, b)
	var was: bool = p.prev.get(b, false)
	p.prev[b] = now
	return now and not was

func _move(p: Player) -> Vector2:
	var v := Vector2.ZERO
	if p.dev == "kbm":
		v = Vector2(Input.get_action_strength("mv_right") - Input.get_action_strength("mv_left"), Input.get_action_strength("mv_down") - Input.get_action_strength("mv_up"))
	elif p.dev == "kb2":
		v = Vector2(Input.get_action_strength("p2_right") - Input.get_action_strength("p2_left"), Input.get_action_strength("p2_down") - Input.get_action_strength("p2_up"))
	for j in p.pads:
		var s := Vector2(Input.get_joy_axis(j, JOY_AXIS_LEFT_X), Input.get_joy_axis(j, JOY_AXIS_LEFT_Y))
		if s.length() > 0.25:
			v += s
	return v.limit_length(1.0)

func _look(p: Player) -> Vector2:
	var v := Vector2.ZERO
	for j in p.pads:
		var s := Vector2(Input.get_joy_axis(j, JOY_AXIS_RIGHT_X), Input.get_joy_axis(j, JOY_AXIS_RIGHT_Y))
		if s.length() > 0.2:
			v += s
	return v

func _input(e: InputEvent) -> void:
	if e is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		for p in players:
			if p.dev == "kbm" and not p.remote:
				p.mouse += (e as InputEventMouseMotion).relative

# ---- Inputs per frame ---------------------------------------------------------------------------------------------------------

## What a local player asks of the simulation this frame.
func _read(p: Player, dt: float) -> Dictionary:
	var inp := {"crew": p.crew}
	if p.titan == 1:
		inp = {"titan": 1, "crew": -1}
		if _hit(p, "act"):
			inp.verb = "strike"
		elif _hit(p, "alt"):
			inp.verb = "parry"
		elif _hit(p, "grab"):
			inp.verb = "grab"
		inp.anchor = _move(p).y > 0.5
		return inp
	var c: March.Crew = m.crew[p.crew]
	# Taken down: wake up in someone else.
	if c.down > 0:
		for o in m.crew:
			if o.down == 0 and not _taken(o.id, p):
				p.crew = o.id
				inp.crew = o.id
				c = o
				break
	# Post keys: take over whoever holds the post, or walk there; the same key again steps off.
	if not mode.contains("duel"):
		for k in 5:
			if _hit(p, "post%d" % (k + 1)):
				_choose(p, k)
				inp.crew = p.crew
				c = m.crew[p.crew]
	if p.take >= 0:
		inp.take = p.take
		p.take = -1
	var mv := _move(p)
	var post := c.post
	if p.nudge > 0:
		p.nudge -= 1
		inp.move = Vector2i(-1, 0)
		return inp
	match post:
		March.P.PILOT:
			var target := -mv.x * 8.0
			p.steer = move_toward(p.steer, target, dt * (28.0 if absf(target) > absf(p.steer) else 40.0))
			inp.steer = int(roundf(p.steer))
			if m.fight:
				inp.anchor = mv.y > 0.5
				if _hit(p, "act"):
					inp.verb = "strike"
				elif _hit(p, "alt"):
					inp.verb = "parry"
				elif _hit(p, "grab"):
					inp.verb = "grab"
			else:
				var up := mv.y < -0.5
				var down := mv.y > 0.5
				if up and not p.prev.get("_up", false):
					inp.pace = 1
				elif down and not p.prev.get("_down", false):
					inp.pace = -1
				p.prev["_up"] = up
				p.prev["_down"] = down
				inp.anchor = _held(p, "act")
				_hit(p, "alt")
				_hit(p, "grab")
		March.P.GUN_L, March.P.GUN_R:
			_aim(p, dt, mv)
			inp.aim = _aim_point(p)
			inp.fire = _hit(p, "act")
		March.P.HEART:
			inp.stoke = _held(p, "act")
			inp.vent = _held(p, "alt")
		March.P.LOOKOUT:
			if _hit(p, "act"):
				p.zoom = not p.zoom
				Sound.sfx("ui", 1.3)
			p.yaw = wrapf(p.yaw - mv.x * dt * 1.2 - _look(p).x * dt * 1.6, -PI, PI)
			p.pitch = clampf(p.pitch - mv.y * dt * 0.6, -0.5, 0.4)
		_:
			# On foot: walk the deck (forward is the titan's forward), beat out fires, fight raiders.
			inp.move = Vector2i(-signi(roundi(mv.y * 1.4)) if absf(mv.y) > 0.35 else 0, -signi(roundi(mv.x * 1.4)) if absf(mv.x) > 0.35 else 0)
			inp.use = _held(p, "act")
	if _hit(p, "view"):
		p.view = (p.view + 1) % 2
	return inp

func _taken(crew_id: int, me: Player) -> bool:
	for o in players:
		if o != me and o.crew == crew_id and o.titan == 0:
			return true
	return m.crew[crew_id].ctrl >= 0 and m.crew[crew_id].ctrl != me.idx

func _choose(p: Player, k: int) -> void:
	var me: March.Crew = m.crew[p.crew]
	if me.post == k:
		p.nudge = 2
		Sound.sfx("step", 1.0, 0.6)
		return
	for o in m.crew:
		if o.id != p.crew and (o.post == k or o.goal == k) and o.down == 0 and not _taken(o.id, p):
			p.crew = o.id
			Sound.sfx("ui", 1.1)
			return
	p.take = k
	Sound.sfx("ui", 0.9)

# ---- The harpoon's aim ---------------------------------------------------------------------------------------------------------

func _gun_index(post: int) -> int:
	return 1 if post == March.P.GUN_R else 0

func _aim(p: Player, dt: float, mv: Vector2) -> void:
	var sens: float = app.settings.get("mouse", 1.0) if app else 1.0
	if p.dev == "kbm":
		p.yaw -= p.mouse.x * 0.0022 * sens
		p.pitch -= p.mouse.y * 0.0022 * sens
	p.mouse = Vector2.ZERO
	var lk := _look(p)
	var kb := mv if p.dev != "kbm" else Vector2.ZERO
	p.yaw -= (lk.x * 1.9 + kb.x * 1.3) * dt
	p.pitch -= (lk.y * 1.3 + kb.y * 0.9) * dt
	p.pitch = clampf(p.pitch, -0.6, 0.75)
	p.yaw = wrapf(p.yaw, -PI, PI)

func _aim_dir(p: Player) -> Vector3:
	return (Basis(Vector3.UP, p.yaw) * Basis(Vector3.RIGHT, p.pitch)) * Vector3.FORWARD

func _to_sim(v: Vector3) -> Vector3i:
	return Vector3i(int(-v.z * 100.0), int(-v.x * 100.0), int(maxf(v.y, 0.0) * 100.0))

## The point the harpoon flies to: the best target near the line of aim (scavengers led by their speed), else the
## end of the rope or the ground.
func _aim_point(p: Player) -> Vector3i:
	var c: March.Crew = m.crew[p.crew]
	var from := world.turret_world(_gun_index(c.post))
	var dir := _aim_dir(p)
	var best := Vector3.ZERO
	var best_a := deg_to_rad(3.5 if p.dev == "kbm" else 8.0)
	var cands: Array[Vector3] = []
	for b in m.birds:
		var pos := World.p3(b.x, b.y, b.z)
		var prev: Vector3i = _bird_prev.get(b.id, Vector3i(b.x, b.y, b.z))
		var vel := Vector3i(b.x, b.y, b.z) - prev
		var dist := (pos - from).length() * 100.0
		var t := dist / March.HARPOON_V
		cands.append(World.p3(b.x + int(vel.x * t), b.y + int(vel.y * t), b.z + int(vel.z * t)))
	for s in m.skiffs:
		cands.append(World.p3(s.x, s.y, 150))
	for h in m.hazards:
		if h.kind == March.H.WRECK and not h.done:
			var wx := h.at - m.progress
			if wx > -6000 and wx < 12000:
				cands.append(World.p3(wx, h.y - m.lateral, 200))
	if m.fight and m.foe:
		for i in m.foe_weak.size():
			var w := m.weak_world(i)
			cands.append(World.p3(w.x, w.y, w.z))
		cands.append(World.p3(March.FOE_X, 0, 3000))
	for q in cands:
		var to := q - from
		if to.length() * 100.0 > March.GUN_RANGE * 1.05:
			continue
		var a := dir.angle_to(to)
		if a < best_a:
			best_a = a
			best = q
	p.lock_ok = best != Vector3.ZERO
	if p.lock_ok:
		p.lock = best
		return _to_sim(best)
	var end := from + dir * (March.GUN_RANGE / 100.0)
	if dir.y < -0.01:
		var tg := -from.y / dir.y
		if tg < March.GUN_RANGE / 100.0:
			end = from + dir * tg
	p.lock = end
	return _to_sim(end)

# ---- Steps -----------------------------------------------------------------------------------------------------------------------

## A local frame: every local player's input, the simulation, the effects.
func step() -> void:
	var dt := 1.0 / 60.0
	var inputs: Array = []
	for p in players:
		if p.remote:
			continue
		while inputs.size() <= p.idx:
			inputs.append({})
		inputs[p.idx] = NetCode.normalize(_read(p, dt))
	_tick(inputs)

func local_net_input() -> Array:
	for p in players:
		if not p.remote:
			return NetCode.pack(_read(p, 1.0 / 60.0))
	return [0, 0]

func step_net(host_in: Array, guest_in: Array) -> void:
	var a := NetCode.unpack(host_in[0], host_in[1])
	var b := NetCode.unpack(guest_in[0], guest_in[1])
	for p in players:
		if p.remote:
			var d: Dictionary = a if p.idx == 0 else b
			if d.get("crew", -1) >= 0:
				p.crew = int(d.crew)
	_tick([a, b])

func _tick(inputs: Array) -> void:
	if _over_t >= 0.0:
		return
	for b in m.birds:
		_bird_prev[b.id] = Vector3i(b.x, b.y, b.z)
	m.step(inputs)
	for e in m.events:
		_event(e)
		world.on_event(e)
	if m.over != "" and _over_t < 0.0:
		_over_t = 0.0

# ---- Events: sounds, voices, banners ----------------------------------------------------------------------------------------------

func _say(k: int, key: String, args: Dictionary = {}, urgent := false) -> void:
	var p := PanelContainer.new()
	var sb := StyleBoxFlat.new()
	sb.bg_color = Color(0.08, 0.06, 0.05, 0.86)
	sb.border_color = Deck.LINEAGE[k].lightened(0.2)
	sb.border_width_left = 3
	sb.set_corner_radius_all(4)
	p.add_theme_stylebox_override("panel", sb)
	p.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_voices.add_child(p)
	var mg := Kit.margin(p, 12, 8, 18, 10)
	var h := Kit.hbox(mg, 12)
	var pt := Kit.image(h, Meta.crew_pt(gen, k), true)
	pt.custom_minimum_size = Vector2(58, 58)
	var v := Kit.vbox(h, 2)
	Kit.label(v, names[k], 13, Deck.LINEAGE[k].lightened(0.35), "display", 700, 0.12)
	var l := Kit.label(v, I18n.t(key, args), 17, Color("#ffd2a8") if urgent else Kit.IVORY, "body", 600)
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.custom_minimum_size.x = 430
	p.modulate.a = 0.0
	var tw := p.create_tween()
	tw.tween_property(p, "modulate:a", 1.0, 0.25)
	tw.tween_interval(6.0 if urgent else 4.5)
	tw.tween_property(p, "modulate:a", 0.0, 0.6)
	tw.tween_callback(p.queue_free)
	if _voices.get_child_count() > 3:
		_voices.get_child(0).queue_free()
	Sound.sfx("voice" if not urgent else "alert", 0.9 + k * 0.08, 0.7)

func _banner_show(text: String, col: Color = Kit.GOLD) -> void:
	_banner.text = text
	_banner.label_settings.font_color = col
	_banner_t = 3.0

func _side(y: int) -> String:
	if absi(y) < 1500:
		return I18n.t("side.ahead")
	return I18n.t("side.port") if y > 0 else I18n.t("side.star")

func _event(e: Dictionary) -> void:
	var L := 3
	match e.t:
		"seen":
			var dist := (int(e.at) - m.progress) / 100
			var k: int = e.kind
			var rel := int(e.y) - m.lateral
			match k:
				March.H.CHASM: _say(L, "voice.chasm", {"d": dist, "side": _side(rel)}, true)
				March.H.ROCKS: _say(L, "voice.rocks", {"d": dist}, true)
				March.H.GEYSER: _say(L, "voice.geyser", {"d": dist, "side": _side(rel)})
				March.H.OASIS: _say(L, "voice.oasis_seen", {"d": dist})
				March.H.STORM: _say(L, "voice.storm_seen", {"d": dist}, true)
				March.H.WRECK: _say(1, "voice.wreck", {"side": _side(rel)})
			Sound.sfx("horn" if k in [March.H.CHASM, March.H.ROCKS, March.H.STORM] else "bell", 1.0, 0.8)
		"birds":
			_say(1, "voice.birds", {"side": _side(int(e.side) * 5000)}, true)
			Sound.sfx("caw", 1.0, 0.9)
		"dive":
			Sound.sfx("caw", 1.25, 0.7)
		"peck":
			Sound.sfx("peck")
		"bird_hit":
			_stats.birds += 1
			Sound.sfx("hit_bird")
		"bird_gone":
			pass
		"skiffs":
			_say(1, "voice.skiffs", {"side": _side(int(e.side) * 5000)}, true)
			Sound.sfx("horn", 0.8, 0.8)
		"docked":
			_say(0, "voice.docked", {}, true)
		"boarder":
			_say(2, "voice.boarder", {}, true)
			Sound.sfx("shout")
		"boarder_down":
			_stats.boarders += 1
			Sound.sfx("shout", 0.7, 0.6)
		"fire":
			Sound.sfx("harpoon")
		"miss":
			Sound.sfx("thud", 1.0, 0.4)
		"salvage":
			_stats.salvage += 1
			_say(1, "voice.salvage")
			Sound.sfx("salvage")
		"skiff_hit":
			Sound.sfx("crack")
		"skiff_sunk":
			_stats.skiffs += 1
			Sound.sfx("wreck")
		"chasm":
			_say(0, "voice.chasm_hit", {}, true)
			Sound.sfx("quake")
		"dodged":
			_say(0, "voice.dodged")
		"rocks":
			_say(0, "voice.rocks_hit", {}, true)
			Sound.sfx("rockfall")
		"braced":
			_say(0, "voice.braced")
			Sound.sfx("rockfall", 1.1, 0.6)
		"geyser":
			_say(2, "voice.geyser_in")
			Sound.sfx("steam", 0.7)
		"oasis":
			_say(0, "voice.oasis")
		"storm":
			_say(L, "voice.storm_out")
		"burst":
			_say(2, "voice.burst", {}, true)
			Sound.sfx("burst")
		"overheat":
			_say(2, "voice.overheat", {}, true)
		"fire_start":
			Sound.sfx("ignite")
		"fire_out":
			_stats.fires += 1
			Sound.sfx("steam", 1.3, 0.5)
		"crew_down":
			var k: int = e.crew
			_banner_show(I18n.t("hud.down", {"name": names[k]}), Color("#ff7a5a"))
			Sound.sfx("hurt")
		"up":
			pass
		"pace":
			Sound.sfx("drum", 1.0 + int(e.pace) * 0.1, 0.7)
		"fight":
			_banner_show(I18n.t("hud.fight"), Color("#ff6a3a"))
			_say(L, "voice.fight", {}, true)
			Sound.music("fight")
			Sound.sfx("roar")
		"verb":
			if int(e.who) == 1 and e.verb == "strike":
				Sound.sfx("roar", 1.15, 0.7)
		"strike":
			Sound.sfx("impact")
		"parried":
			Sound.sfx("clang")
			if int(e.who) == 1:
				_banner_show(I18n.t("hud.parried"), Kit.GOLD)
		"grab":
			Sound.sfx("grab")
			if int(e.who) == 1:
				_say(0, "voice.grabbed", {"key": _key_for("down")}, true)
			else:
				_banner_show(I18n.t("hud.holding"), Kit.GOLD)
		"grab_fail":
			Sound.sfx("thud")
		"break_free":
			Sound.sfx("grab", 0.8)
		"fall":
			Sound.sfx("fall")
			if int(e.who) == 1:
				_banner_show(I18n.t("hud.foe_down"), Kit.GOLD)
			else:
				_banner_show(I18n.t("hud.ama_down"), Color("#ff6a3a"))
		"rise":
			Sound.sfx("roar", 0.9, 0.6)
		"weak_hit":
			_stats.weak += 1
			Sound.sfx("crack", 0.8)
		"foe_hit":
			Sound.sfx("thud", 0.8, 0.6)
		"over":
			pass

func _key_for(what: String) -> String:
	match what:
		"down": return Keys.key_name("mv_down")
		"act": return Keys.key_name("act")
		"alt": return Keys.key_name("alt")
		"grab": return Keys.key_name("grab")
	return "?"

# ---- HUD -------------------------------------------------------------------------------------------------------------------------

func _build_hud() -> void:
	_shared = Control.new()
	Kit.full(_shared)
	_shared.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_shared)
	_shared.draw.connect(_draw_shared)
	_voices = Kit.vbox(_shared, 8)
	_voices.position = Vector2(28, 150)
	_banner = Kit.label(_shared, "", 44, Kit.GOLD, "display", 700, 0.12, Color(0, 0, 0, 0.9), 16)
	_banner.position = Vector2(0, 300)
	_banner.size = Vector2(Kit.W, 60)
	_banner.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_tele = Kit.label(_shared, "", 30, Color("#ff5a3a"), "display", 700, 0.16, Color(0, 0, 0, 0.9), 14)
	_tele.position = Vector2(0, 205)
	_tele.size = Vector2(Kit.W, 44)
	_tele.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_tip_box = Kit.panel(_shared)
	var mg := Kit.margin(_tip_box, 22, 14, 22, 14)
	_tip_label = Kit.rich(mg, "", 17, Kit.IVORY)
	_tip_label.custom_minimum_size.x = 620
	_tip_box.visible = false

func _post_name(k: int) -> String:
	return I18n.t("post.%d" % k)

func _update_hints() -> void:
	for p in _local():
		if p.hint == null:
			continue
		var t := ""
		var a := "act" if p.dev == "kbm" else "p2_act"
		var al := "alt" if p.dev == "kbm" else "p2_alt"
		var gr := "grab" if p.dev == "kbm" else "p2_grab"
		var ka := {"act": Keys.key_name(a), "alt": Keys.key_name(al), "grab": Keys.key_name(gr),
			"move": "ZQSD" if Keys.key_name("mv_up") == "Z" else "WASD", "posts": "1-5" if p.dev == "kbm" else "6-0"}
		if p.dev == "kb2":
			ka.move = "↑←↓→"
		if p.titan == 1:
			t = I18n.t("hint.titan", ka)
		else:
			var c: March.Crew = m.crew[p.crew]
			match c.post:
				March.P.PILOT: t = I18n.t("hint.fight" if m.fight else "hint.pilot", ka)
				March.P.GUN_L, March.P.GUN_R: t = I18n.t("hint.gun_mouse" if p.dev == "kbm" else "hint.gun", ka)
				March.P.HEART: t = I18n.t("hint.heart", ka)
				March.P.LOOKOUT: t = I18n.t("hint.lookout", ka)
				_: t = I18n.t("hint.deck", ka)
			if not mode.contains("duel"):
				t += "   " + I18n.t("hint.posts", ka)
		p.hint.text = Kit.html(t)

func frame(dt: float) -> void:
	_t += dt
	var v := m.speed() * 0.6 if not m.fight else 0.0
	world.sync(dt, v)
	_sync_crew(dt)
	for p in _local():
		_camera(p, dt)
		if p.hud:
			p.hud.queue_redraw()
	# The mouse is captured while player 1 aims a harpoon.
	var want_capture := false
	for p in _local():
		if p.dev == "kbm" and p.titan == 0:
			var c: March.Crew = m.crew[p.crew]
			want_capture = (c.post == March.P.GUN_L or c.post == March.P.GUN_R) and _over_t < 0.0
	var mm := Input.MOUSE_MODE_CAPTURED if want_capture else Input.MOUSE_MODE_VISIBLE
	if Input.mouse_mode != mm and (app == null or app.call("top_is_game")):
		Input.mouse_mode = mm
	_update_hints()
	_shared.queue_redraw()
	_banner_t -= dt
	_banner.modulate.a = clampf(_banner_t, 0.0, 1.0)
	_telegraph()
	_tips_tick(dt)
	_warnings(dt)
	Sound.ambience(m, world)
	if _over_t >= 0.0:
		_over_t += dt
		if _over_t > 3.5 and not _sent:
			_sent = true
			Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
			finished.emit({"over": m.over, "hp": m.titan.hp, "supplies": m.supplies, "foe_hp": m.foe.hp if m.foe else 0,
				"time": m.frame / 60, "stats": _stats, "mode": mode})

func _telegraph() -> void:
	_tele.text = ""
	if not m.fight or m.foe == null:
		return
	var f := m.foe
	if f.state == March.T.WINDUP:
		_tele.text = I18n.t("tele.strike")
		_tele.modulate.a = 0.6 + 0.4 * sin(_t * 20.0)
	elif f.state == March.T.GRAB_WINDUP:
		_tele.text = I18n.t("tele.grab")
		_tele.modulate.a = 0.6 + 0.4 * sin(_t * 20.0)
	elif m.titan.state == March.T.HELD:
		_tele.text = I18n.t("tele.held")
		_tele.modulate.a = 1.0

func _tips_tick(dt: float) -> void:
	if _tips.is_empty() and _tip_t <= 0.0:
		_tip_box.visible = false
		return
	_tip_t -= dt
	if _tip_t <= 0.0:
		if _tips.is_empty():
			_tip_box.visible = false
			return
		var line: String = _tips.pop_front()
		_tip_label.text = Kit.html(line.format({"act": Keys.key_name("act"), "alt": Keys.key_name("alt"), "grab": Keys.key_name("grab"), "view": Keys.key_name("view")}))
		_tip_box.visible = true
		_tip_box.reset_size()
		_tip_box.position = Vector2((Kit.W - _tip_box.size.x) / 2.0, Kit.H - 290)
		_tip_t = 9.0
		Sound.sfx("ui", 1.4, 0.5)

func _warnings(_dt: float) -> void:
	var now := _t
	var warn := func(key: String, k: int, cond: bool) -> void:
		if cond and now - float(_warn_t.get(key, -100.0)) > 25.0:
			_warn_t[key] = now
			_say(k, "voice." + key, {}, true)
	warn.call("cold", 2, m.titan.heat < 160 and not m.fight)
	warn.call("pressure", 2, m.titan.pressure > 860)
	warn.call("hungry", 2, m.supplies < 80)
	warn.call("hurt", 0, m.titan.hp < 2500)

## The crew figures follow their simulated selves.
func _sync_crew(dt: float) -> void:
	for c in m.crew:
		var f: Figure = world.deck.crew[c.id]
		var np := Deck.at(c.x, c.y)
		var d := np - f.position
		f.walk = clampf(d.length() * 25.0, 0.0, 1.0)
		if d.length() > 0.005:
			f.rotation.y = lerp_angle(f.rotation.y, atan2(-d.x, -d.z), minf(1.0, dt * 10.0))
		elif c.post >= 0:
			f.rotation.y = lerp_angle(f.rotation.y, 0.0, minf(1.0, dt * 4.0))
		f.position = np
		f.down = c.down > 0
		f.work = 1.0 if c.act > 0 or (c.post == March.P.HEART and (m.titan.heat < 500)) else (0.4 if c.post >= 0 else 0.0)
		f.animate(dt)
	# Turrets follow their gunners' aim.
	for gi in 2:
		var post := March.P.GUN_L if gi == 0 else March.P.GUN_R
		var aim := Vector3.ZERO
		var rl := 0.0
		for c in m.crew:
			if c.post == post:
				aim = World.p3(c.aim.x, c.aim.y, c.aim.z)
				rl = float(c.reload) / March.RELOAD
		var yawn := world.deck.turrets[gi]
		if aim != Vector3.ZERO:
			var local_t := world.ama.body.global_transform.affine_inverse() * aim
			world.deck.aim_turret(gi, local_t, rl)
		else:
			yawn.rotation.y = lerp_angle(yawn.rotation.y, 0.0, 0.05)
	world.deck.update_forge(m.titan.heat / 1000.0, _venting(), dt)

func _venting() -> bool:
	for p in players:
		if p.remote or p.titan == 1:
			continue
		var c: March.Crew = m.crew[p.crew]
		if c.post == March.P.HEART and _held(p, "alt"):
			return true
	return m.titan.pressure > 720 and not _player_at(March.P.HEART)

func _player_at(post: int) -> bool:
	for p in players:
		if p.titan == 0 and m.crew[p.crew].post == post:
			return true
	return false

# ---- Cameras ----------------------------------------------------------------------------------------------------------------------

func _camera(p: Player, dt: float) -> void:
	if p.cam == null:
		return
	var pos := Vector3.ZERO
	var look := Vector3.ZERO
	var fov := 62.0
	var rate := 4.0
	var c: March.Crew = m.crew[p.crew]
	var body := world.ama.body.global_transform
	var fig := world.deck.crew[p.crew].global_position
	var post := c.post if p.titan == 0 else -9
	var fight_view := m.fight and (post == March.P.PILOT or post == -9 or mode.contains("duel"))
	if p.view == 1 and not fight_view:
		pos = Vector3(62, 64, 58)
		look = Vector3(0, 22, -22)
		fov = 55.0
	elif fight_view:
		var sway := sin(_t * 0.2) * 6.0
		pos = Vector3(132 + sway, 46, -36)
		look = Vector3(0, 24, -46)
		fov = 52.0
		rate = 2.5
	else:
		match post:
			March.P.PILOT:
				var head := world.ama.head.global_position
				pos = head + Vector3(0, 21, 42)
				look = head + Vector3(-m.steer * 2.0, -14, -170)
			March.P.GUN_L, March.P.GUN_R:
				var tw := world.turret_world(_gun_index(post))
				var dir := _aim_dir(p)
				var right := dir.cross(Vector3.UP).normalized()
				var out := Vector3(-1.0 if post == March.P.GUN_L else 1.0, 0, 0)
				pos = tw - dir * 3.2 + Vector3(0, 2.4, 0) + out * 1.5 + right * 0.4
				look = tw + dir * 60.0
				rate = 14.0
				fov = 58.0
			March.P.HEART:
				var fg := body * Deck.at(0, 0)
				pos = fg + Vector3(12, 21, 17)
				look = fg + Vector3(0, 0, -2)
			March.P.LOOKOUT:
				var tp := body * (Deck.at(March.POSTS[4].x, March.POSTS[4].y) + Vector3(0, 13.4, 0))
				var dir := (Basis(Vector3.UP, p.yaw) * Basis(Vector3.RIGHT, p.pitch - 0.05)) * Vector3.FORWARD
				if m.fight and m.foe and p.yaw == 0.0:
					dir = (World.p3(March.FOE_X, 0, 3000) - tp).normalized()
				pos = tp
				look = tp + dir * 100.0
				fov = 16.0 if p.zoom else 42.0
				rate = 10.0
			_:
				pos = fig + Vector3(0, 8.5, 15)
				look = fig + Vector3(0, 0.5, -10)
				rate = 5.0
	var k := 1.0 - exp(-rate * dt)
	if p.last_post != post:
		k = maxf(k, 0.12)
		p.last_post = post
	p.cpos = p.cpos.lerp(pos, k)
	p.clook = p.clook.lerp(look, k)
	p.fov = lerpf(p.fov, fov, k)
	var sh: float = world.shake * float(app.settings.get("shake", 1.0) if app else 1.0)
	var jit := Vector3(sin(_t * 37.0), sin(_t * 41.0 + 1.0), sin(_t * 29.0 + 2.0)) * sh * 0.6
	p.cam.fov = p.fov
	if (p.clook - p.cpos).length() > 0.01:
		p.cam.look_at_from_position(p.cpos + jit, p.clook + jit * 0.5, Vector3.UP)

## Projects a world point into a player's HUD; returns (-1,-1) behind the camera.
func _proj(p: Player, w: Vector3) -> Vector2:
	if p.cam == null or p.cam.is_position_behind(w):
		return Vector2(-1, -1)
	var s := p.cam.unproject_position(w)
	return s * (p.rect.size / Vector2(p.vp.size)) if p.vp else s

# ---- Drawing --------------------------------------------------------------------------------------------------------------------

func _bar(ci: CanvasItem, r: Rect2, k: float, col: Color, back := Color(0, 0, 0, 0.55)) -> void:
	ci.draw_rect(r.grow(2), back)
	ci.draw_rect(Rect2(r.position, Vector2(r.size.x * clampf(k, 0.0, 1.0), r.size.y)), col)
	ci.draw_rect(r.grow(2), Color(1, 0.85, 0.6, 0.25), false, 1.0)

func _txt(ci: CanvasItem, pos: Vector2, s: String, size: int, col: Color, f: Font = null, align := HORIZONTAL_ALIGNMENT_LEFT, w := -1.0) -> void:
	var font := f if f else _font
	# An anchor at pos.x for right or centred text.
	if align == HORIZONTAL_ALIGNMENT_RIGHT and w <= 0.0:
		pos.x -= 800.0
		w = 800.0
	elif align == HORIZONTAL_ALIGNMENT_CENTER and w <= 0.0:
		pos.x -= 400.0
		w = 800.0
	ci.draw_string_outline(font, pos, s, align, w, size, 6, Color(0, 0, 0, 0.75))
	ci.draw_string(font, pos, s, align, w, size, col)

func _draw_shared() -> void:
	var ci := _shared
	var t := m.titan
	# Title and journey (top left).
	_txt(ci, Vector2(30, 46), String(cfg.get("title", "")), 24, Kit.GOLD)
	_txt(ci, Vector2(30, 74), String(cfg.get("subtitle", "")), 14, Kit.BRASS, _font_b)
	if not m.fight and not m.duel:
		var r := Rect2(30, 92, 420, 8)
		_bar(ci, r, float(m.progress) / maxf(1.0, m.length), Color("#e9b872"))
		for h in m.hazards:
			if not h.seen or h.done:
				continue
			var x := r.position.x + r.size.x * clampf(float(h.at) / m.length, 0.0, 1.0)
			ci.draw_circle(Vector2(x, r.position.y + 4), 5, _haz_col(h.kind))
		_txt(ci, Vector2(30, 124), I18n.t("hud.left", {"m": maxi(0, (m.length - m.progress) / 100)}), 13, Kit.SOFT, _font_b)
	# Vitals (top right): Ama's health (with scars), heat, pressure, supplies.
	var x0 := Kit.W - 470.0
	var hr := Rect2(x0, 50, 440, 16)
	if not m.fight:
		_txt(ci, Vector2(x0, 40), I18n.t("hud.ama"), 16, Kit.IVORY)
		_bar(ci, hr, float(t.hp) / March.HP, Color("#7fbf6a") if t.hp > 3500 else Color("#e05a3a"))
	var scars: int = int(cfg.get("scars", 0))
	if scars > 0 and not m.fight:
		ci.draw_rect(Rect2(hr.end.x - hr.size.x * scars / float(March.HP), hr.position.y, hr.size.x * scars / float(March.HP), hr.size.y), Color(0.25, 0.08, 0.06, 0.9))
	var heat_r := Rect2(x0, 92, 440, 10)
	_bar(ci, heat_r, t.heat / 1000.0, Color("#ff8a3a"))
	ci.draw_rect(Rect2(heat_r.position.x + heat_r.size.x * 0.45, heat_r.position.y - 3, heat_r.size.x * 0.45, 2), Color("#7fd06a"))
	_txt(ci, Vector2(x0, 86), I18n.t("hud.heat"), 12, Color("#ffb070"), _font_b)
	var pr := Rect2(x0, 126, 440, 10)
	_bar(ci, pr, t.pressure / 1000.0, Color("#f0f0f0") if t.pressure < 800 else Color("#ff4a3a"))
	ci.draw_rect(Rect2(pr.position.x + pr.size.x * 0.85, pr.position.y - 3, pr.size.x * 0.15, 2), Color("#ff4a3a"))
	_txt(ci, Vector2(x0, 120), I18n.t("hud.pressure"), 12, Kit.SOFT, _font_b)
	_txt(ci, Vector2(x0, 162), I18n.t("hud.supplies", {"n": m.supplies}), 15, Kit.GOLD, _font_b)
	var pace_s := I18n.t("pace.%d" % m.pace) if not m.fight else ""
	_txt(ci, Vector2(x0 + 440, 162), pace_s, 15, Kit.IVORY, _font_b, HORIZONTAL_ALIGNMENT_RIGHT, 0)
	if t.anchor:
		_txt(ci, Vector2(x0 + 260, 162), I18n.t("hud.anchor"), 15, Color("#8fd0ff"), _font_b)
	# The fight: both titans' health and balance.
	if m.fight and m.foe:
		var f := m.foe
		var cx := Kit.W / 2.0
		_txt(ci, Vector2(cx - 470, 44), I18n.t("hud.ama"), 18, Kit.IVORY)
		_txt(ci, Vector2(cx + 470, 44), String(cfg.get("foe_name", I18n.t("hud.foe"))), 18, Color("#ff8a5a"), null, HORIZONTAL_ALIGNMENT_RIGHT, 0)
		_bar(ci, Rect2(cx - 470, 54, 440, 18), float(t.hp) / March.HP, Color("#7fbf6a"))
		var fmax := float(m.leg.get("foe_hp", 10000 + int(m.leg.get("level", 1)) * 4000))
		var fr := Rect2(cx + 30, 54, 440, 18)
		ci.draw_rect(fr.grow(2), Color(0, 0, 0, 0.55))
		var fw := fr.size.x * clampf(f.hp / fmax, 0.0, 1.0)
		ci.draw_rect(Rect2(fr.end.x - fw, fr.position.y, fw, fr.size.y), Color("#d8402a"))
		_bar(ci, Rect2(cx - 470, 80, 440 * t.balance / 1000.0, 6), 1.0, Color("#f0d080"), Color(0, 0, 0, 0))
		ci.draw_rect(Rect2(cx + 30 + 440 * (1.0 - f.balance / 1000.0), 80, 440 * f.balance / 1000.0, 6), Color("#f0d080"))
		_txt(ci, Vector2(cx, 98), I18n.t("hud.balance"), 12, Kit.MUTED, _font_b, HORIZONTAL_ALIGNMENT_CENTER, 0)
	# The posts (bottom centre).
	if not mode.contains("duel"):
		var w := 178.0
		var bx := Kit.W / 2.0 - w * 2.5
		var by := Kit.H - 128.0 - (40.0 if _local().size() > 0 and _local()[0].hint else 0.0)
		for k in 5:
			var r := Rect2(bx + k * w + 6, by, w - 12, 74)
			ci.draw_rect(r, Color(0.07, 0.05, 0.04, 0.82))
			var who := -1
			var walking := -1
			for c in m.crew:
				if c.post == k and c.down == 0:
					who = c.id
				elif c.goal == k and c.post != k:
					walking = c.id
			var raid := false
			for b in m.boarders:
				if b.post == k:
					raid = true
			var border := Color(1, 0.85, 0.6, 0.25)
			for p in players:
				if p.titan == 0 and m.crew[p.crew].post == k:
					border = PLAYER_COL[p.idx]
			if raid:
				border = Color(1, 0.3, 0.2, 0.6 + 0.4 * sin(_t * 10.0))
			ci.draw_rect(r, border, false, 2.0)
			if who >= 0:
				var tex := Kit.tex(Meta.crew_pt(gen, who))
				if tex:
					ci.draw_texture_rect_region(tex, Rect2(r.position + Vector2(4, 4), Vector2(54, 66)), Rect2(Vector2(60, 40), Vector2(264, 323)))
				_txt(ci, r.position + Vector2(64, 52), names[who], 13, Deck.LINEAGE[who].lightened(0.4), _font_b)
			elif walking >= 0:
				_txt(ci, r.position + Vector2(64, 52), "…" + names[walking], 13, Kit.MUTED, _font_b)
			else:
				_txt(ci, r.position + Vector2(64, 52), I18n.t("post.empty"), 13, Color("#c07a5a"), _font_b)
			_txt(ci, r.position + Vector2(64, 26), _post_name(k), 12, Kit.IVORY, _font_b)
			_txt(ci, r.position + Vector2(r.size.x - 6, 18), str(k + 1), 13, Kit.GOLD, null, HORIZONTAL_ALIGNMENT_RIGHT, 0)

func _haz_col(k: int) -> Color:
	match k:
		March.H.CHASM: return Color("#ff5a3a")
		March.H.ROCKS: return Color("#d8a060")
		March.H.GEYSER: return Color("#f0f0f0")
		March.H.OASIS: return Color("#6ad08a")
		March.H.STORM: return Color("#8a6a5a")
		March.H.WRECK: return Color("#f0c060")
	return Color.WHITE

## A player's own layer: the post's instruments and the markers over the world.
func _draw_player(p: Player) -> void:
	var ci := p.hud
	if p.titan == 1:
		_txt(ci, Vector2(p.rect.size.x - 30, p.rect.size.y - 90), I18n.t("hud.p2_titan"), 16, PLAYER_COL[1], null, HORIZONTAL_ALIGNMENT_RIGHT, 0)
		return
	var c: March.Crew = m.crew[p.crew]
	var col: Color = PLAYER_COL[p.idx]
	# Who am I.
	var tag := I18n.t("hud.you_are", {"name": names[p.crew], "post": _post_name(c.post) if c.post >= 0 else I18n.t("post.deck")})
	_txt(ci, Vector2(28, p.rect.size.y - 70), tag, 17, col)
	# Markers: my figure, seen hazards ahead, fires, raiders, wrecks.
	var me := _proj(p, world.deck.crew[p.crew].global_position + Vector3(0, 2.6, 0))
	if me.x >= 0 and c.post != March.P.GUN_L and c.post != March.P.GUN_R:
		ci.draw_colored_polygon(PackedVector2Array([me + Vector2(-8, -12), me + Vector2(8, -12), me]), col)
	for i in m.fires.size():
		var q := _proj(p, world.ama.body.global_transform * Deck.at(m.fires[i].x, m.fires[i].y, 2.5))
		if q.x >= 0:
			_txt(ci, q, "🔥", 18, Color("#ffb060"), _font_b, HORIZONTAL_ALIGNMENT_CENTER, 0)
	for b in m.boarders:
		var q := _proj(p, world.ama.body.global_transform * Deck.at(b.x, b.y, 2.6))
		if q.x >= 0:
			ci.draw_arc(q, 9, 0, TAU, 12, Color("#ff4a3a"), 2.0)
	var labelled := 0
	for h in m.hazards:
		if h.done or not h.seen:
			continue
		var ahead := h.at - m.progress
		if ahead < -2000 or ahead > m.vision:
			continue
		var q := _proj(p, World.p3(ahead, h.y - m.lateral, 1500))
		if q.x < 0:
			continue
		var hc := _haz_col(h.kind)
		ci.draw_circle(q, 6, hc)
		# Only the nearest few get a label, so far ones do not pile up on the horizon.
		if labelled < 3:
			labelled += 1
			_txt(ci, q + Vector2(10, 5), "%s  %d m" % [I18n.t("haz.%d" % h.kind), ahead / 100], 13, hc, _font_b)
	match c.post:
		March.P.GUN_L, March.P.GUN_R:
			_draw_reticle(p, ci, c)
		March.P.PILOT:
			if not m.fight:
				_draw_helm(p, ci)
		March.P.HEART:
			_draw_heart(p, ci)
		March.P.LOOKOUT:
			_draw_spyglass(p, ci)

func _draw_reticle(p: Player, ci: CanvasItem, c: March.Crew) -> void:
	var tw := world.turret_world(_gun_index(c.post))
	var aim := _proj(p, tw + _aim_dir(p) * 60.0)
	if aim.x < 0:
		aim = p.rect.size / 2.0
	var rl := float(c.reload) / March.RELOAD
	var col := Color(1, 0.9, 0.7, 0.9) if rl <= 0.0 else Color(1, 0.6, 0.4, 0.6)
	ci.draw_arc(aim, 18, 0, TAU, 32, col, 2.0)
	ci.draw_line(aim + Vector2(-30, 0), aim + Vector2(-22, 0), col, 2.0)
	ci.draw_line(aim + Vector2(22, 0), aim + Vector2(30, 0), col, 2.0)
	ci.draw_line(aim + Vector2(0, -30), aim + Vector2(0, -22), col, 2.0)
	ci.draw_line(aim + Vector2(0, 22), aim + Vector2(0, 30), col, 2.0)
	if rl > 0.0:
		ci.draw_arc(aim, 24, -PI / 2, -PI / 2 + TAU * (1.0 - rl), 32, Color(1, 0.8, 0.5, 0.8), 3.0)
	if p.lock_ok:
		var q := _proj(p, p.lock)
		if q.x >= 0:
			var s := 16.0 + 3.0 * sin(_t * 12.0)
			for k in 4:
				var a := k * PI / 2 + PI / 4
				var d := Vector2(cos(a), sin(a))
				ci.draw_line(q + d * s, q + d * (s - 8.0) + d.orthogonal() * 6.0, Color("#ffd060"), 2.5)
				ci.draw_line(q + d * s, q + d * (s - 8.0) - d.orthogonal() * 6.0, Color("#ffd060"), 2.5)
	# Range.
	_txt(ci, aim + Vector2(36, -8), "%d m" % int((p.lock - tw).length()), 13, col, _font_b)

func _draw_helm(p: Player, ci: CanvasItem) -> void:
	var cx := p.rect.size.x / 2.0
	var y := p.rect.size.y - 330.0
	ci.draw_arc(Vector2(cx, y + 140), 150, -PI * 0.8, -PI * 0.2, 48, Color(1, 0.85, 0.6, 0.35), 3.0)
	var a := -PI / 2 - m.steer / 8.0 * PI * 0.3
	ci.draw_line(Vector2(cx, y + 140) + Vector2(cos(a), sin(a)) * 120, Vector2(cx, y + 140) + Vector2(cos(a), sin(a)) * 160, Kit.GOLD, 5.0)
	_txt(ci, Vector2(cx, y + 30), I18n.t("pace.%d" % m.pace), 20, Kit.IVORY, null, HORIZONTAL_ALIGNMENT_CENTER, 0)
	# The route centre and how far Ama is off it.
	var off := m.lateral / 100
	if absi(off) > 5:
		_txt(ci, Vector2(cx, y + 56), I18n.t("hud.offroute", {"m": absi(off), "side": I18n.t("side.port") if off > 0 else I18n.t("side.star")}), 13, Kit.SOFT, _font_b, HORIZONTAL_ALIGNMENT_CENTER, 0)

func _draw_heart(p: Player, ci: CanvasItem) -> void:
	var x := p.rect.size.x - 150.0
	var y0 := 230.0
	var hgt := 380.0
	for i in 2:
		var gx := x + i * 70.0
		var k := m.titan.heat / 1000.0 if i == 0 else m.titan.pressure / 1000.0
		ci.draw_rect(Rect2(gx, y0, 34, hgt), Color(0, 0, 0, 0.6))
		var colr := Color("#ff8a3a") if i == 0 else (Color("#e8e8e8") if k < 0.8 else Color("#ff4a3a"))
		ci.draw_rect(Rect2(gx, y0 + hgt * (1.0 - k), 34, hgt * k), colr)
		if i == 0:
			ci.draw_rect(Rect2(gx - 6, y0 + hgt * 0.1, 4, hgt * 0.45), Color("#7fd06a"))
		else:
			ci.draw_rect(Rect2(gx - 6, y0, 4, hgt * 0.15), Color("#ff4a3a"))
		ci.draw_rect(Rect2(gx, y0, 34, hgt), Color(1, 0.85, 0.6, 0.4), false, 2.0)
		_txt(ci, Vector2(gx + 17, y0 + hgt + 26), I18n.t("hud.heat_s") if i == 0 else I18n.t("hud.pressure_s"), 13, Kit.IVORY, _font_b, HORIZONTAL_ALIGNMENT_CENTER, 0)

func _draw_spyglass(p: Player, ci: CanvasItem) -> void:
	if p.zoom:
		var c := p.rect.size / 2.0
		var r := minf(c.x, c.y) * 0.92
		var poly := PackedVector2Array()
		for i in 49:
			var a := TAU * i / 48
			poly.append(c + Vector2(cos(a), sin(a)) * r)
		ci.draw_polyline(poly, Color(0.1, 0.07, 0.04), 18.0)
	var y := 215.0
	_txt(ci, Vector2(p.rect.size.x - 30, y), I18n.t("hud.vision", {"m": m.vision / 100}), 15, Kit.GOLD, _font_b, HORIZONTAL_ALIGNMENT_RIGHT, 0)
	if m.fight:
		_txt(ci, Vector2(p.rect.size.x - 30, y + 26), I18n.t("hud.weak_seen"), 15, Color("#ffb060"), _font_b, HORIZONTAL_ALIGNMENT_RIGHT, 0)
