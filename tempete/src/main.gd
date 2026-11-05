extends Node
## TEMPÊTE: boot, screens, the Season (days, briefings, garage), free chases, online crews and races, and the
## play loop (simulation at a fixed 60 Hz in the physics step).

var world_root: Node3D
var ui: Control
var save: Dictionary
var settings: Dictionary
var game: ChaseScreen = null
var online: Online = null
var software := false
var _screen: Screen = null
var _overlays: Array[Screen] = []
var _awake := false
var _day := -1
var _last_cfg: Dictionary = {}
# Automation: --shots <prefix> --seconds <n> --screen <name> --play [--auto] [--skip <seconds>].
var _shots := ""
var _seconds := 0.0
var _clock := 0.0
var _shot_i := 0
var _auto: ChaserAI = null

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
	var play := false
	var skip := 0
	for i in a.size():
		match a[i]:
			"--shots": _shots = a[i + 1]
			"--seconds": _seconds = float(a[i + 1])
			"--screen": scr = a[i + 1]
			"--play": play = true
			"--skip": skip = int(a[i + 1])
	if software and scr == "" and not play:
		settings.quality = 0
	if scr == "" and not play:
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN if settings.fullscreen else DisplayServer.WINDOW_MODE_WINDOWED)
	_apply_settings()
	get_viewport().disable_3d = true
	if play:
		_awake = true
		var d: Dictionary = Meta.day(3).duplicate(true)
		start_chase({"day": d, "mode": "solo", "day_id": d.id})
		if "--auto" in a:
			_auto = ChaserAI.new(game.chase, 0, 1.0, 3)
		for k in skip * 60:
			game.chase.step([_auto.update() if _auto else {}])
		if "--photo" in a:
			# Automation: open the viewfinder on the nearest tornado (or storm).
			game.photo = true
			var v: Chase.Vehicle = game.chase.vehicles[0]
			var tx := 0
			var ty := 0
			var bd := 1 << 60
			for tn in game.chase.tornadoes:
				var dd := absi(tn.x - v.x) + absi(tn.y - v.y)
				if not tn.dead and dd < bd:
					bd = dd
					tx = tn.x
					ty = tn.y
			var to := World.p3(tx, ty) - World.p3(v.x, v.y)
			game.yaw = atan2(-to.x, -to.z)
			game.pitch = 0.08
			game.zoom = 2.0
		return
	to_title()
	match scr:
		"season": to_season()
		"garage": _garage()
		"setup": _setup()
		"online": _online()
		"options": _options()
		"brief": _brief(2)
		"intro": _cinematic("intro", func() -> void: pass)

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

func _end_chase() -> void:
	if game:
		game.queue_free()
		game = null
	get_viewport().disable_3d = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func to_title() -> void:
	_end_chase()
	_clear_overlays()
	Sound.music("title")
	var items: Array = [
		{"label": I18n.t("title.season"), "sub": I18n.t("title.season_sub"), "action": func() -> void: to_season()},
		{"label": I18n.t("title.free"), "sub": I18n.t("title.free_sub"), "action": _setup},
		{"label": I18n.t("title.online"), "sub": I18n.t("title.online_sub"), "action": _online},
		{"label": I18n.t("title.garage"), "sub": I18n.t("title.garage_sub"), "action": _garage},
		{"label": I18n.t("title.options"), "action": _options},
		{"label": I18n.t("title.quit"), "action": func() -> void: get_tree().quit()},
	]
	var t := TitleScreen.new(items, func() -> void: _awake = true, _awake)
	_set_screen(t)
	if software:
		t.warn(I18n.t("options.software"))

func to_season(focus: int = -1) -> void:
	_end_chase()
	_clear_overlays()
	Sound.music("title")
	if focus < 0:
		focus = 0
		for i in Meta.DAYS.size():
			if Meta.is_open(save, i):
				focus = i
	_set_screen(SeasonScreen.new(save, focus, _day_pick, _garage, to_title))

func _garage() -> void:
	_set_screen(GarageScreen.new(save, func() -> void: to_season(_day if _day >= 0 else -1)))

func _setup() -> void:
	_set_screen(SetupScreen.new(func(cfg: Dictionary) -> void:
		cfg.upgrades = Meta.upgrades(save)
		start_chase(cfg), to_title))

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
	_end_chase()
	Sound.music("title")
	var shots: Array = I18n.tl("cine.%s" % id)
	_set_screen(CinematicScreen.new(shots, then))

# ---- The Season ----------------------------------------------------------------------------------------------------------

func _day_pick(i: int) -> void:
	_day = i
	if i == 0 and not save.seen.has("intro"):
		save.seen["intro"] = true
		Meta.write_save(save)
		_cinematic("intro", func() -> void: _brief(i))
	else:
		_brief(i)

func _brief(i: int) -> void:
	_end_chase()
	_day = i
	var d := Meta.day(i)
	Sound.music("title")
	_set_screen(Backdrop.new(SeasonScreen.VISTA[i % SeasonScreen.VISTA.size()]))
	var lines := I18n.tl("brief.%s" % d.id)
	var go := func() -> void:
		var cfg := {"day": d.duplicate(true), "mode": "versus_ai" if d.rival else "solo", "day_id": d.id, "upgrades": Meta.upgrades(save), "rival_skill": 0.7 + i * 0.05}
		start_chase(cfg)
	if lines.is_empty():
		go.call()
		return
	_push(Overlays.Dialogue.new(lines, func() -> void:
		_pop()
		go.call()))

func start_chase(cfg: Dictionary) -> void:
	_end_chase()
	_set_screen(null)
	_clear_overlays()
	_last_cfg = cfg
	get_viewport().disable_3d = false
	game = ChaseScreen.new(cfg, world_root)
	game.app = self
	game.set_quality(int(settings.quality))
	game.finished.connect(_chase_done)
	if online and str(cfg.get("mode", "")).begins_with("net"):
		game.online = online
	ui.add_child(game)
	ui.move_child(game, 0)
	Sound.music("chase")

func _chase_done(r: Dictionary) -> void:
	if _shots != "":
		print("RESULT ", r.dossier)
	var actions: Array = []
	var money := {}
	var goal := 0
	var dos: Dictionary = r.dossier
	var net: bool = str(_last_cfg.get("mode", "")).begins_with("net")
	if r.day_id != "" and r.day_id != "free" and not net:
		var i := Meta.day_index(r.day_id)
		goal = int(Meta.day(i).goal)
		money = Meta.record(save, i, int(dos.total), bool(dos.alive), int(dos.hp))
		_save_album(r)
		Meta.write_save(save)
		if money.ok and i + 1 < Meta.DAYS.size():
			actions.append({"label": I18n.t("dossier.next"), "action": func() -> void:
				_clear_overlays()
				_day_pick(i + 1)})
		elif money.ok and i + 1 == Meta.DAYS.size() and not save.seen.has("finale"):
			actions.append({"label": I18n.t("common.continue"), "action": func() -> void:
				save.seen["finale"] = true
				Meta.write_save(save)
				_cinematic("finale", func() -> void:
					var cr: Array = [{"image": "vista_after", "text": I18n.t("ending.credits")}]
					_set_screen(CinematicScreen.new(cr, to_title)))})
		actions.append({"label": I18n.t("dossier.retry"), "action": func() -> void:
			_clear_overlays()
			_brief(i)})
		actions.append({"label": I18n.t("season.garage"), "action": _garage})
		actions.append({"label": I18n.t("dossier.season"), "action": func() -> void: to_season(i)})
	elif net:
		if online.is_host:
			actions.append({"label": I18n.t("online.again"), "action": func() -> void:
				online_start({"mode": _last_cfg.mode, "risk": int(_last_cfg.get("risk", 3)), "seed": randi() % 100000})})
		else:
			actions.append({"label": I18n.t("online.wait_host"), "action": func() -> void: pass})
		actions.append({"label": I18n.t("dossier.menu"), "action": func() -> void:
			online.close()
			to_title()})
	else:
		actions.append({"label": I18n.t("dossier.retry"), "action": func() -> void:
			var c := _last_cfg.duplicate(true)
			c.day = SetupScreen.free_day(int(c.day.get("risk", 2)), randi() % 100000)
			start_chase(c)})
		actions.append({"label": I18n.t("dossier.menu"), "action": to_title})
	Sound.music("title")
	_push(DossierScreen.new(r, money, goal, actions))

## Keeps the season's best pictures on disk (the album on the title screen's garage wall).
func _save_album(r: Dictionary) -> void:
	var album: Dictionary = r.album
	for c: int in album:
		var a: Dictionary = album[c]
		if a.img == null:
			continue
		var path := "user://album_%s_%d.png" % [r.day_id, c]
		(a.img as Image).save_png(path)

# ---- Online ---------------------------------------------------------------------------------------------------------------------

func online_start(d: Dictionary) -> void:
	online.send("start", d)
	_online_begin(d)

func _online_begin(d: Dictionary) -> void:
	online.reset_lockstep()
	var day := SetupScreen.free_day(int(d.risk), int(d.seed))
	start_chase({"day": day, "mode": d.mode, "host": online.is_host, "risk": d.risk, "upgrades": {}})

func _on_net_message(kind: String, data: Variant) -> void:
	if kind == "start" and data is Dictionary:
		_online_begin(data)

# ---- Pause -----------------------------------------------------------------------------------------------------------------------

func _pause() -> void:
	if game and str(_last_cfg.get("mode", "")).begins_with("net"):
		return
	Sound.sfx("ui", 0.8)
	Sound.muffled = true
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	var close := func() -> void:
		Sound.muffled = false
		_pop()
		if game and game.photo:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	_push(Overlays.Pause.new(close, func() -> void:
		Sound.muffled = false
		_pop()
		start_chase(_last_cfg.duplicate(true)), _options, func() -> void:
		Sound.muffled = false
		_pop()
		if _day >= 0 and _last_cfg.has("day_id") and _last_cfg.day_id != "free":
			to_season(_day)
		else:
			to_title()))

# ---- Loop -------------------------------------------------------------------------------------------------------------------------

func _physics_process(_d: float) -> void:
	if game == null:
		return
	if str(_last_cfg.get("mode", "")).begins_with("net"):
		if online:
			online.net_step(game)
		return
	if _overlays.size() > 0:
		return
	if Keys.pressed("pause"):
		_pause()
		return
	if _auto:
		game.call("_tick", [_auto.update()])
		return
	game.step()

func _process(dt: float) -> void:
	_clock += dt
	if online and not online.message.is_connected(_on_net_message):
		online.message.connect(_on_net_message)
	if game and (_overlays.is_empty() or str(_last_cfg.get("mode", "")).begins_with("net")):
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
