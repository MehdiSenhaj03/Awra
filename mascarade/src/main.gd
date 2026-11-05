extends Node
## MASCARADE: boot, screens, the Season, the online Contract, the Assassins' Ball, and the play loop (simulation at a
## fixed 60 Hz in the physics step).

var world: Node3D
var ui: Control
var save: Dictionary
var settings: Dictionary
var ball: Ball = null
var online: Online = null
var software := false
var _screen: Screen = null
var _overlays: Array[Screen] = []
var _awake := false
var _post: ShaderMaterial
var _post_rect: ColorRect
var _night := 0
var _party_cfg: Dictionary = {}
var _round := 0
var _score := [0, 0]          # online rounds won: host, guest
var _venue_online := "salon"
# Automation: --shots <prefix> --seconds <n> --screen <name> --ball <venue> <role>.
var _shots := ""
var _seconds := 0.0
var _clock := 0.0
var _shot_i := 0

func _ready() -> void:
	save = Meta.load_save()
	settings = Meta.load_settings()
	Keys.apply(settings.bindings)
	I18n.set_lang(settings.lang)
	var adapter := RenderingServer.get_video_adapter_name().to_lower()
	software = adapter.contains("llvmpipe") or adapter.contains("swiftshader") or adapter.contains("software")
	world = Node3D.new()
	add_child(world)
	var post_layer := CanvasLayer.new()
	add_child(post_layer)
	_post_rect = ColorRect.new()
	_post = ShaderMaterial.new()
	_post.shader = preload("res://src/render/shaders/post.gdshader")
	_post_rect.material = _post
	_post_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	post_layer.add_child(_post_rect)
	Kit.full(_post_rect)
	_post_rect.visible = false
	for i in 3:
		_post.set_shader_parameter("u_shocks", [Vector4(0, 0, -1, 0), Vector4(0, 0, -1, 0), Vector4(0, 0, -1, 0)])
	var layer := CanvasLayer.new()
	layer.layer = 1
	add_child(layer)
	ui = Control.new()
	ui.mouse_filter = Control.MOUSE_FILTER_IGNORE
	layer.add_child(ui)
	Kit.full(ui)
	var a := OS.get_cmdline_user_args()
	var scr := ""
	var ball_args: Array = []
	for i in a.size():
		match a[i]:
			"--shots": _shots = a[i + 1]
			"--seconds": _seconds = float(a[i + 1])
			"--screen": scr = a[i + 1]
			"--ball": ball_args = [a[i + 1], a[i + 2]]
	if software and scr == "" and ball_args.is_empty():
		settings.quality = 0
	if scr == "" and ball_args.is_empty():
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN if settings.fullscreen else DisplayServer.WINDOW_MODE_WINDOWED)
	_apply_settings()
	get_viewport().disable_3d = true
	if not ball_args.is_empty():
		_awake = true
		if ball_args[1] == "party":
			start_party(["kb", "kb2", "pad0"], ball_args[0])
		else:
			_start_ball({"venue": ball_args[0], "mode": "contract", "role": ball_args[1], "skill": 1.0, "seed": 4, "missions": ["letter", "poison", "bug", "contact"], "need": 3})
		return
	to_title()
	match scr:
		"season": to_season()
		"party": _party_setup()
		"online": _online()
		"options": _options()
		"brief": _brief(3)
		"intro": _cinematic("intro", func() -> void: pass)
		"results": _push(ResultsScreen.new({"won": true, "winner": "spy", "why": "missions", "frame": 7300, "missions": 3, "need": 3, "shot": false, "mode": "contract"}, [{"label": I18n.t("results.next"), "action": func() -> void: pass}]))

# ---- Screens -----------------------------------------------------------------------------------------------------------

func _set_screen(s: Screen) -> void:
	if _screen:
		_screen.leave()
	_screen = s
	if s:
		s.app = self
		ui.add_child(s)
		ui.move_child(s, 0)

func _push(o: Screen) -> void:
	o.app = self
	_overlays.append(o)
	ui.add_child(o)

func _pop(o: Screen = null) -> void:
	var x: Screen = o if o else (_overlays.back() if _overlays.size() > 0 else null)
	if x == null:
		return
	_overlays.erase(x)
	x.queue_free()

func _clear_overlays() -> void:
	for o in _overlays.duplicate():
		_pop(o)

func _end_ball() -> void:
	if ball:
		ball.queue_free()
		ball = null
	get_viewport().disable_3d = true
	_post_rect.visible = false
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func to_title() -> void:
	_end_ball()
	_clear_overlays()
	Sound.music("title")
	var items: Array = [
		{"label": I18n.t("title.season"), "sub": I18n.t("title.season_sub"), "action": func() -> void: to_season()},
		{"label": I18n.t("title.online"), "sub": I18n.t("title.online_sub"), "action": _online},
		{"label": I18n.t("title.party"), "sub": I18n.t("title.party_sub"), "action": _party_setup},
		{"label": I18n.t("title.options"), "action": _options},
		{"label": I18n.t("title.quit"), "action": func() -> void: get_tree().quit()},
	]
	var t := TitleScreen.new(items, func() -> void: _awake = true, _awake)
	_set_screen(t)
	if software:
		t.warn(I18n.t("options.software"))

func to_season(focus: int = 0) -> void:
	_end_ball()
	_clear_overlays()
	Sound.music("title")
	if focus == 0:
		focus = 1
		for nt: Dictionary in Meta.NIGHTS:
			if Meta.is_open(save, nt.id):
				focus = nt.id
	_set_screen(SeasonScreen.new(save, focus, _night_pick, to_title))

func _options() -> void:
	_push(OptionsScreen.new(settings, func() -> void: _pop(), _apply_settings))

func _apply_settings() -> void:
	Sound.set_volumes(settings.music, settings.sfx)
	if ball:
		ball.set_quality(int(settings.quality))
	Meta.write_settings(settings)

func _online() -> void:
	_set_screen(OnlineScreen.new(self))

func _party_setup() -> void:
	_set_screen(PartySetup.new(start_party, to_title))

func _cinematic(id: String, then: Callable) -> void:
	_end_ball()
	Sound.music("title")
	var shots: Array = I18n.tl("cine.%s" % id)
	_set_screen(CinematicScreen.new(shots, then))

# ---- The Season ----------------------------------------------------------------------------------------------------------

func _night_pick(n: int) -> void:
	_night = n
	if n == 1 and not save.seen.has("intro"):
		save.seen["intro"] = true
		Meta.write_save(save)
		_cinematic("intro", func() -> void: _brief(n))
	else:
		_brief(n)

## The briefing: who sends you, what for, then the ball.
func _brief(n: int) -> void:
	_end_ball()
	_night = n
	var nt := Meta.night(n)
	Sound.music("title")
	var bg := SeasonBackdrop.new("bg_%s" % nt.venue)
	_set_screen(bg)
	var lines := I18n.tl("brief.%d" % n)
	if lines.is_empty():
		_start_night(n)
		return
	_push(Overlays.Dialogue.new(lines, func() -> void:
		_pop()
		_start_night(n)))

func _start_night(n: int) -> void:
	var nt := Meta.night(n)
	var seed_v := n * 1000 + int(save.nights.get(str(n), {}).get("tries", 0)) * 17 + randi() % 1000
	_start_ball({"venue": nt.venue, "mode": "contract", "role": nt.role, "skill": nt.skill, "missions": nt.missions, "need": nt.need, "seed": seed_v, "night": n})

func _start_ball(cfg: Dictionary) -> void:
	_end_ball()
	_set_screen(null)
	_clear_overlays()
	get_viewport().disable_3d = false
	ball = Ball.new(cfg, world)
	ball.app = self
	ball.set_quality(int(settings.quality))
	ball.finished.connect(_ball_done)
	ui.add_child(ball)
	ui.move_child(ball, 0)
	_post_rect.visible = true
	Sound.music("ball_%s" % cfg.get("venue", "salon"))

func _ball_done(r: Dictionary) -> void:
	if _shots != "":
		print("RESULT ", r)
	var actions: Array = []
	if r.mode == "party":
		actions.append({"label": I18n.t("results.again"), "action": func() -> void: start_party(_party_cfg.sources, _party_cfg.venue)})
		actions.append({"label": I18n.t("results.menu"), "action": to_title})
	elif ball and ball.net:
		_online_done(r, actions)
	else:
		var n: int = r.night
		if n > 0:
			Meta.record(save, n, r)
			save.stats.missions = int(save.stats.missions) + int(r.missions)
			Meta.write_save(save)
			if r.won and n == Meta.NIGHTS.size() and not save.seen.has("finale"):
				actions.append({"label": I18n.t("common.continue"), "action": func() -> void:
					save.seen["finale"] = true
					Meta.write_save(save)
					_cinematic("finale", func() -> void:
						var cr: Array = [{"image": "key_art", "text": I18n.t("ending.credits")}]
						_set_screen(CinematicScreen.new(cr, to_title)))})
			elif r.won and n < Meta.NIGHTS.size():
				actions.append({"label": I18n.t("results.next"), "action": func() -> void:
					_clear_overlays()
					_night_pick(n + 1)})
			actions.append({"label": I18n.t("results.retry"), "action": func() -> void:
				_clear_overlays()
				_start_night(n)})
			actions.append({"label": I18n.t("results.season"), "action": func() -> void: to_season(n)})
		else:
			actions.append({"label": I18n.t("results.retry"), "action": func() -> void: _start_ball(ball.cfg)})
			actions.append({"label": I18n.t("results.menu"), "action": to_title})
	Sound.music("title")
	_push(ResultsScreen.new(r, actions))

# ---- The Assassins' Ball --------------------------------------------------------------------------------------------------

func start_party(sources: Array, venue: String) -> void:
	_party_cfg = {"sources": sources, "venue": venue}
	_start_ball({"venue": venue, "mode": "party", "sources": sources, "seed": randi() % 100000})

# ---- Online contract --------------------------------------------------------------------------------------------------------

## Host: start round `r` (0: host is the spy, 1: host is the sniper, then alternate).
func online_round(venue: String, r: int) -> void:
	_venue_online = venue
	_round = r
	if r == 0:
		_score = [0, 0]
	var seed_v := randi() % 1000000
	var host_role := "spy" if r % 2 == 0 else "hunter"
	online.send("start", {"venue": venue, "seed": seed_v, "host_role": host_role, "round": r})
	_online_start(venue, seed_v, host_role, r)

func _online_start(venue: String, seed_v: int, host_role: String, r: int) -> void:
	_round = r
	_venue_online = venue
	online.reset_lockstep()
	var me := host_role if online.is_host else ("hunter" if host_role == "spy" else "spy")
	_start_ball({"venue": venue, "mode": "contract", "role": me, "net": true, "host_role": host_role, "seed": seed_v,
		"missions": ["letter", "poison", "bug", "contact"], "need": 3})

func _on_net_message(kind: String, data: Variant) -> void:
	if kind == "start" and data is Dictionary:
		_online_start(data.venue, int(data.seed), data.host_role, int(data.round))

func _online_done(r: Dictionary, actions: Array) -> void:
	var host_won: bool = r.won == online.is_host
	_score[0 if host_won else 1] += 1
	r["online_score"] = _score.duplicate()
	if online.is_host:
		actions.append({"label": I18n.t("online.next_round"), "action": func() -> void:
			_clear_overlays()
			online_round(_venue_online, _round + 1)})
	else:
		actions.append({"label": I18n.t("online.wait_host"), "action": func() -> void: pass})
	actions.append({"label": I18n.t("results.menu"), "action": func() -> void:
		online.close()
		to_title()})

# ---- Pause --------------------------------------------------------------------------------------------------------------------

func _pause() -> void:
	if ball and ball.net:
		return
	Sound.sfx("ui", 0.8)
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	Sound.muffled = true
	var close := func() -> void:
		Sound.muffled = false
		_pop()
		if ball and ball.role == "hunter" and ball.mode == "contract":
			Input.mouse_mode = Input.MOUSE_MODE_HIDDEN
	_push(Overlays.Pause.new(close, func() -> void:
		Sound.muffled = false
		_pop()
		if ball:
			var c := ball.cfg.duplicate()
			c.seed = int(c.get("seed", 1)) + 1
			_start_ball(c), _options, func() -> void:
		Sound.muffled = false
		_pop()
		if _night > 0 and ball and ball.mode == "contract":
			to_season(_night)
		else:
			to_title()))

# ---- Loop -----------------------------------------------------------------------------------------------------------------------

func _physics_process(_d: float) -> void:
	if ball == null:
		return
	if ball.net:
		if online:
			online.net_step(ball)
		return
	if _overlays.size() > 0:
		return
	if Keys.pressed("pause") and ball.over_t < 0.0:
		_pause()
		return
	ball.step()

func _process(dt: float) -> void:
	_clock += dt
	if online and not online.message.is_connected(_on_net_message):
		online.message.connect(_on_net_message)
	if ball and (_overlays.is_empty() or ball.net):
		ball.frame(minf(dt, 0.1))
	var top: Screen = _overlays.back() if _overlays.size() > 0 else _screen
	if top and top != ball:
		top.update(dt)
	if _shots != "" and _seconds > 0.0:
		if _clock >= _seconds * (_shot_i + 1) / 4.0 and _shot_i < 4:
			_shot_i += 1
			_capture("%s_%d.png" % [_shots, _shot_i])
		if _clock > _seconds + 0.3:
			get_tree().quit()

func _capture(path: String) -> void:
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(path)

## A painted backdrop behind the briefings.
class SeasonBackdrop extends Screen:
	func _init(img: String) -> void:
		super()
		Kit.background(self, img, 2.0, 0.45)
