extends Node
## TITAN: boot, screens, the Long March (the map and its crossroads, briefings, legs, the passing of generations,
## the Source), free legs and duels (alone or two on one screen), online expeditions and duels, the play loop
## (simulation at a fixed 60 Hz in the physics step).

var world_root: Node3D
var ui: Control
var save: Dictionary
var settings: Dictionary
var game: GameScreen = null
var online: Online = null
var software := false
var _screen: Screen = null
var _overlays: Array[Screen] = []
var _awake := false
var _last_cfg: Dictionary = {}
var _leg_kind := ""
# Automation: --shots <prefix> --seconds <n> --screen <name> --play <kind|fight|duel|coop> [--skip <seconds>].
var _shots := ""
var _seconds := 0.0
var _clock := 0.0
var _shot_i := 0
var _auto_post := -1

func _ready() -> void:
	save = Meta.load_save()
	settings = Meta.load_settings()
	Keys.apply(settings.bindings)
	I18n.set_lang(settings.lang)
	var adapter := RenderingServer.get_video_adapter_name().to_lower()
	software = adapter.contains("llvmpipe") or adapter.contains("swiftshader") or adapter.contains("software")
	world_root = Node3D.new()
	add_child(world_root)
	var layer := CanvasLayer.new()
	layer.layer = 1
	add_child(layer)
	ui = Control.new()
	ui.mouse_filter = Control.MOUSE_FILTER_IGNORE
	layer.add_child(ui)
	Kit.full(ui)
	var a := OS.get_cmdline_user_args()
	var scr := ""
	var play := ""
	var skip := 0
	for i in a.size():
		match a[i]:
			"--shots": _shots = a[i + 1]
			"--seconds": _seconds = float(a[i + 1])
			"--screen": scr = a[i + 1]
			"--play": play = a[i + 1]
			"--skip": skip = int(a[i + 1])
			"--post": _auto_post = int(a[i + 1])
	if software and scr == "" and play == "":
		settings.quality = 0
	if scr == "" and play == "":
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN if settings.fullscreen else DisplayServer.WINDOW_MODE_WINDOWED)
	_apply_settings()
	get_viewport().disable_3d = true
	if play != "":
		_awake = true
		var cfg := {"mode": "solo", "leg": {"kind": play, "length_m": 2600, "seed": 7, "level": 2}, "title": I18n.t("land.%s.name" % play), "subtitle": I18n.t("free.sub")}
		match play:
			"fight":
				cfg.leg = {"kind": "ash", "length_m": 1, "seed": 7, "level": 2, "boss": true, "foe_hp": 16000, "foe_power": 100}
				cfg.title = I18n.t("boss.belier.name")
			"roi":
				cfg.leg = {"kind": "ash", "length_m": 1, "seed": 7, "level": 4, "boss": true, "foe_hp": 25000, "foe_power": 118, "boss_id": "roi"}
				cfg.title = I18n.t("boss.roi.name")
				cfg.foe_name = cfg.title
			"duel":
				cfg.mode = "duel_ai"
				cfg.leg = {"kind": "ash", "seed": 7, "level": 2, "foe_hp": 16000, "foe_power": 100}
			"coop":
				cfg.mode = "coop"
				cfg.leg = {"kind": "ruins", "length_m": 2600, "seed": 7, "level": 2}
				cfg.title = I18n.t("land.ruins.name")
		start_leg(cfg)
		for k in skip * 60:
			game.step()
		if _auto_post >= 5:
			game.players[0].nudge = 2
		elif _auto_post >= 0:
			game._choose(game.players[0], _auto_post)
		return
	to_title()
	match scr:
		"map": to_map()
		"setup": _setup("free")
		"duel": _setup("duel")
		"online": _online()
		"options": _options()
		"inherit":
			var r := Meta.new_run()
			r.leg = 3
			_inherit(r)
		"intro": _cinematic("intro", func() -> void: pass)
		"brief":
			save.run = Meta.new_run()
			_pick_route("ruins")
		"leg1":
			save.run = Meta.new_run()
			save.seen.erase("tips")
			_leg_kind = "ash"
			get_viewport().disable_3d = false
			_play_leg()
		"finale": _cinematic("finale", func() -> void: pass)
		"result": _push(ResultScreen.new({"over": "done", "hp": 7200, "supplies": 640, "time": 260, "stats": {"birds": 6, "skiffs": 2, "salvage": 3, "boarders": 2, "fires": 1, "weak": 0}}, {"lost": 1800, "scars": 225}, [{"label": "OK", "action": func() -> void: pass}]))

func top_is_game() -> bool:
	return _overlays.is_empty()

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

func _end_leg() -> void:
	if game:
		game.queue_free()
		game = null
	get_viewport().disable_3d = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	Sound.quiet()

func to_title() -> void:
	_end_leg()
	_clear_overlays()
	Sound.music("title")
	var has_run: bool = save.run is Dictionary
	var items: Array = [
		{"label": I18n.t("title.march_continue") if has_run else I18n.t("title.march"), "sub": I18n.t("title.march_sub"), "action": func() -> void:
			if not has_run:
				_new_run()
			else:
				to_map()},
	]
	if has_run:
		items.append({"label": I18n.t("title.march_new"), "sub": I18n.t("title.march_new_sub"), "action": func() -> void:
			_push(Overlays.Confirm.new(I18n.t("title.confirm_new"), func() -> void:
				_pop()
				_new_run(), func() -> void: _pop()))})
	items.append_array([
		{"label": I18n.t("title.free"), "sub": I18n.t("title.free_sub"), "action": func() -> void: _setup("free")},
		{"label": I18n.t("title.duel"), "sub": I18n.t("title.duel_sub"), "action": func() -> void: _setup("duel")},
		{"label": I18n.t("title.online"), "sub": I18n.t("title.online_sub"), "action": _online},
		{"label": I18n.t("title.options"), "action": _options},
		{"label": I18n.t("title.quit"), "action": func() -> void: get_tree().quit()},
	])
	var t := TitleScreen.new(items, func() -> void: _awake = true, _awake)
	_set_screen(t)
	if software:
		t.warn(I18n.t("options.software"))

func _setup(kind: String) -> void:
	_set_screen(SetupScreen.new(kind, func(cfg: Dictionary) -> void: start_leg(cfg), to_title))

func _options() -> void:
	_push(OptionsScreen.new(settings, func() -> void: _pop(), _apply_settings))

func _apply_settings() -> void:
	Sound.set_volumes(settings.music, settings.sfx)
	if game:
		game.set_quality(int(settings.quality))
	Meta.write_settings(settings)

func _online() -> void:
	_set_screen(OnlineScreen.new(self))

func _cinematic(id: String, then: Callable) -> void:
	_end_leg()
	Sound.music("source" if id == "finale" else "title")
	var shots: Array = I18n.tl("cine.%s" % id)
	_set_screen(CinematicScreen.new(shots, then))

# ---- The Long March ----------------------------------------------------------------------------------------------------------

func _new_run() -> void:
	save.run = Meta.new_run()
	Meta.write_save(save)
	_cinematic("intro", to_map)

func to_map() -> void:
	_end_leg()
	_clear_overlays()
	Sound.music("title")
	if not (save.run is Dictionary):
		save.run = Meta.new_run()
	var run: Dictionary = save.run
	if int(run.leg) >= Meta.LEGS.size():
		_finale()
		return
	var ms := MapScreen.new(run, _pick_route, to_title)
	ms.app = self
	_set_screen(ms)

func _pick_route(kind: String) -> void:
	var run: Dictionary = save.run
	var i := int(run.leg)
	_leg_kind = kind
	var kinds: Array = run.kinds
	while kinds.size() > i:
		kinds.pop_back()
	kinds.append(kind)
	run.start = {"hp": run.hp, "supplies": run.supplies}
	Meta.write_save(save)
	var l := Meta.leg(i)
	var lines: Array = []
	for ln: Array in I18n.tl("brief.%s" % l.id):
		var who := int(ln[0])
		if who < 0:
			lines.append([I18n.t("names.narrator"), ln[1], Meta.NARRATOR])
		else:
			lines.append([Meta.crew_name(run, who), ln[1], Meta.crew_pt(Meta.gen(run), who)])
	lines.append([I18n.t("names.narrator"), I18n.t("land.%s.brief" % kind) if not l.has("boss") else I18n.t("boss.%s.brief" % l.boss), Meta.NARRATOR])
	_set_screen(Backdrop.new("vista_%s" % kind if not l.has("boss") else "duel"))
	_push(Overlays.Dialogue.new(lines, func() -> void:
		_pop()
		_play_leg()))

func _play_leg() -> void:
	var run: Dictionary = save.run
	var i := int(run.leg)
	var l := Meta.leg(i)
	var g := Meta.gen(run)
	var names: Array = []
	for k in 4:
		names.append(Meta.crew_name(run, k))
	var cfg := {"mode": "solo" if not settings.get("coop_campaign", false) else "coop", "leg": Meta.leg_cfg(run, _leg_kind), "crew": Meta.crew_info(run), "gen": g, "names": names,
		"title": I18n.t("map.leg", {"n": i + 1}) + " — " + (I18n.t("land.%s.name" % _leg_kind) if not l.has("boss") else I18n.t("boss.%s.name" % l.boss)),
		"subtitle": I18n.t("gen.%d" % g), "scars": int(run.scars), "campaign": true, "tips": i == 0 and not save.seen.has("tips"),
		"foe_name": I18n.t("boss.%s.name" % l.boss) if l.has("boss") else ""}
	if cfg.tips:
		save.seen["tips"] = true
		Meta.write_save(save)
	start_leg(cfg)

func start_leg(cfg: Dictionary) -> void:
	_end_leg()
	_set_screen(null)
	_clear_overlays()
	_last_cfg = cfg
	get_viewport().disable_3d = false
	game = GameScreen.new(cfg, world_root)
	game.app = self
	game.set_quality(int(settings.quality))
	game.finished.connect(_leg_done)
	if online and String(cfg.get("mode", "")).begins_with("net"):
		game.online = online
	ui.add_child(game)
	ui.move_child(game, 0)
	var fight: bool = cfg.leg.get("duel", false) or String(cfg.mode).contains("duel")
	Sound.music("fight" if fight else "march")

func _leg_done(r: Dictionary) -> void:
	if _shots != "":
		print("RESULT ", r)
	var actions: Array = []
	var change := {}
	var mode := String(_last_cfg.get("mode", ""))
	if _last_cfg.get("campaign", false):
		var run: Dictionary = save.run
		if r.over == "lost":
			# Ama falls: the leg starts again from where it began.
			var st: Dictionary = run.start if run.start is Dictionary and not run.start.is_empty() else {"hp": run.hp, "supplies": run.supplies}
			run.hp = maxi(int(st.hp), 3000)
			run.supplies = int(st.supplies)
			Meta.write_save(save)
			actions.append({"label": I18n.t("result.retry"), "action": func() -> void:
				_clear_overlays()
				run.start = {"hp": run.hp, "supplies": run.supplies}
				_play_leg()})
			actions.append({"label": I18n.t("result.map"), "action": to_map})
		else:
			change = Meta.leg_done(run, int(r.hp), int(r.supplies))
			save.best = maxi(int(save.best), int(run.leg))
			Meta.write_save(save)
			if int(run.leg) >= Meta.LEGS.size():
				actions.append({"label": I18n.t("common.continue"), "action": _finale})
			elif change.get("new_gen", false):
				actions.append({"label": I18n.t("common.continue"), "action": func() -> void: _inherit(run)})
			else:
				actions.append({"label": I18n.t("result.onward"), "action": to_map})
	elif mode.begins_with("net"):
		if online and online.is_host:
			actions.append({"label": I18n.t("online.again"), "action": func() -> void:
				online_start(_last_cfg.get("net", {}))})
		else:
			actions.append({"label": I18n.t("online.wait_host"), "action": func() -> void: pass})
		actions.append({"label": I18n.t("result.menu"), "action": func() -> void:
			if online:
				online.close()
			to_title()})
	else:
		actions.append({"label": I18n.t("result.again"), "action": func() -> void:
			_clear_overlays()
			var c := _last_cfg.duplicate(true)
			c.leg.seed = randi() % 100000
			start_leg(c)})
		actions.append({"label": I18n.t("result.menu"), "action": to_title})
	Sound.music("title")
	Sound.quiet()
	_push(ResultScreen.new(r, change, actions, _last_cfg))

func _inherit(run: Dictionary) -> void:
	_end_leg()
	_clear_overlays()
	Sound.music("title")
	_set_screen(InheritScreen.new(run, func() -> void:
		Meta.write_save(save)
		to_map()))

func _finale() -> void:
	save.done = true
	save.run = null
	Meta.write_save(save)
	_cinematic("finale", func() -> void:
		var cr: Array = [{"image": "ending", "text": I18n.t("ending.credits")}]
		_set_screen(CinematicScreen.new(cr, to_title)))

# ---- Online ---------------------------------------------------------------------------------------------------------------------

func online_start(d: Dictionary) -> void:
	online.send("start", d)
	_online_begin(d)

func _online_begin(d: Dictionary) -> void:
	online.reset_lockstep()
	var kind := String(d.get("kind", "ash"))
	var cfg := {"mode": d.mode, "host": online.is_host, "net": d,
		"leg": {"kind": kind, "length_m": 2400, "seed": int(d.seed), "level": int(d.get("level", 2)), "boss": d.get("boss", false),
			"foe_hp": 15000 + int(d.get("level", 2)) * 2000, "foe_power": 95 + int(d.get("level", 2)) * 5},
		"title": I18n.t("online.title") + " — " + (I18n.t("land.%s.name" % kind) if d.mode == "net_coop" else I18n.t("title.duel")),
		"subtitle": "", "foe_name": I18n.t("hud.foe") if d.mode == "net_coop" else I18n.t("hud.p2")}
	if d.mode == "net_duel":
		cfg.leg.foe_hp = March.HP
		cfg.leg.foe_power = 100
	start_leg(cfg)

func _on_net_message(kind: String, data: Variant) -> void:
	if kind == "start" and data is Dictionary:
		_online_begin(data)

# ---- Pause -----------------------------------------------------------------------------------------------------------------------

func _pause() -> void:
	if game and String(_last_cfg.get("mode", "")).begins_with("net"):
		return
	Sound.sfx("ui", 0.8)
	Sound.muffled = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	var close := func() -> void:
		Sound.muffled = false
		_pop()
	_push(Overlays.Pause.new(close, func() -> void:
		Sound.muffled = false
		_pop()
		if _last_cfg.get("campaign", false):
			var run: Dictionary = save.run
			if run.start is Dictionary and not run.start.is_empty():
				run.hp = run.start.hp
				run.supplies = run.start.supplies
			_play_leg()
		else:
			start_leg(_last_cfg.duplicate(true)), _options, func() -> void:
		Sound.muffled = false
		_pop()
		if _last_cfg.get("campaign", false):
			to_map()
		else:
			to_title()))

# ---- Loop -------------------------------------------------------------------------------------------------------------------------

func _physics_process(_d: float) -> void:
	if game == null:
		return
	if String(_last_cfg.get("mode", "")).begins_with("net"):
		if online:
			online.net_step(game)
		return
	if _overlays.size() > 0:
		return
	if Keys.pressed("pause"):
		_pause()
		return
	game.step()

func _process(dt: float) -> void:
	_clock += dt
	if online and not online.message.is_connected(_on_net_message):
		online.message.connect(_on_net_message)
	if game and (_overlays.is_empty() or String(_last_cfg.get("mode", "")).begins_with("net")):
		game.frame(minf(dt, 0.1))
	var top: Screen = _overlays.back() if _overlays.size() > 0 else _screen
	if top and top != game:
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
class Backdrop extends Screen:
	func _init(img: String) -> void:
		super()
		Kit.background(self, img, 2.0, 0.45)
