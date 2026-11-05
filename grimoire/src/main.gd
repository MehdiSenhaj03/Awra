extends Node
## GRIMOIRE: boot, screens, the Apprentice's campaign, free duels and expeditions, online play, and the play loop
## (simulations at a fixed 60 Hz in the physics step).

var ui: Control
var save: Dictionary
var settings: Dictionary
var game: Screen = null          # the running DuelScreen or RivesScreen
var online: Online = null
var software := false
var _screen: Screen = null
var _overlays: Array[Screen] = []
var _awake := false
var _chapter := 0
var _last_cfg: Dictionary = {}
var _last_kind := ""
# Automation: --shots <prefix> --seconds <n> --screen <name> --play <duel|rives> [args].
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
	for i in a.size():
		match a[i]:
			"--shots": _shots = a[i + 1]
			"--seconds": _seconds = float(a[i + 1])
			"--screen": scr = a[i + 1]
			"--play": play = a[i + 1]
	if scr == "" and play == "":
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN if settings.fullscreen else DisplayServer.WINDOW_MODE_WINDOWED)
	_apply_settings()
	match play:
		"duel":
			_awake = true
			start_game("duel", {"mode": "ai", "rival": "cendres", "skill": 0.9, "arena": "ar_cendres", "best_of": 3, "seed": 5, "autoplay": "--auto" in a})
			return
		"rives":
			_awake = true
			start_game("rives", {"mode": "solo", "level": 2, "boss": false, "seed": 5})
			return
		"boss":
			_awake = true
			start_game("rives", {"mode": "solo", "level": 3, "boss": true, "seed": 5, "skip": "--skip" in a})
			return
	to_title()
	match scr:
		"campaign": to_campaign()
		"book": _book()
		"lesson": _push(BookScreen.new([], [Glyphs.FIRE, Glyphs.WATER, Glyphs.SHIELD], func() -> void: _pop()))
		"duel_setup": _setup("duel")
		"rives_setup": _setup("rives")
		"online": _online()
		"options": _options()
		"intro": _cinematic("intro", func() -> void: pass)
		"brief": _brief(3)
		"results": _push(ResultsScreen.new({"mode": "duel", "won": true, "winner": 0, "frame": 5400, "rounds": [2, 1], "chapter": 3}, [{"label": I18n.t("results.next"), "action": func() -> void: pass}], "cendres"))

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

func _end_game() -> void:
	if game:
		game.queue_free()
		game = null

func to_title() -> void:
	_end_game()
	_clear_overlays()
	Sound.music("title")
	var items: Array = [
		{"label": I18n.t("title.campaign"), "sub": I18n.t("title.campaign_sub"), "action": func() -> void: to_campaign()},
		{"label": I18n.t("title.duel"), "sub": I18n.t("title.duel_sub"), "action": func() -> void: _setup("duel")},
		{"label": I18n.t("title.rives"), "sub": I18n.t("title.rives_sub"), "action": func() -> void: _setup("rives")},
		{"label": I18n.t("title.online"), "sub": I18n.t("title.online_sub"), "action": _online},
		{"label": I18n.t("title.book"), "sub": I18n.t("title.book_sub"), "action": _book},
		{"label": I18n.t("title.options"), "action": _options},
		{"label": I18n.t("title.quit"), "action": func() -> void: get_tree().quit()},
	]
	var t := TitleScreen.new(items, func() -> void: _awake = true, _awake)
	_set_screen(t)

func to_campaign(focus: int = 0) -> void:
	_end_game()
	_clear_overlays()
	Sound.music("title")
	if focus == 0:
		focus = 1
		for c: Dictionary in Meta.CHAPTERS:
			if Meta.is_open(save, c.id):
				focus = c.id
	_set_screen(ApprenticeScreen.new(save, focus, _chapter_pick, to_title))

func _setup(kind: String) -> void:
	_set_screen(SetupScreen.new(kind, start_game, to_title))

func _book() -> void:
	_push(BookScreen.new(Meta.all_known(save) if not save.seen.has("free") else range(Glyphs.COUNT), [], func() -> void: _pop()))

func _options() -> void:
	_push(OptionsScreen.new(settings, func() -> void: _pop(), _apply_settings))

func _apply_settings() -> void:
	Sound.set_volumes(settings.music, settings.sfx)
	Meta.write_settings(settings)

func _online() -> void:
	_set_screen(OnlineScreen.new(self))

func _cinematic(id: String, then: Callable) -> void:
	_end_game()
	Sound.music("title")
	var shots: Array = I18n.tl("cine.%s" % id)
	_set_screen(CinematicScreen.new(shots, then))

# ---- The campaign -------------------------------------------------------------------------------------------------------

func _chapter_pick(n: int) -> void:
	_chapter = n
	if n == 1 and not save.seen.has("intro"):
		save.seen["intro"] = true
		Meta.write_save(save)
		_cinematic("intro", func() -> void: _brief(n))
	else:
		_brief(n)

## The briefing, then (when the chapter teaches glyphs not yet learned) the lesson, then the fight.
func _brief(n: int) -> void:
	_end_game()
	_chapter = n
	var c := Meta.chapter(n)
	Sound.music("title")
	_set_screen(Backdrop.new(c.get("arena", "rives")))
	var go := func() -> void:
		var teach: Array = c.teach
		if teach.size() > 0 and not save.lessons.has(str(n)):
			_push(BookScreen.new(Meta.known_at(n - 1), teach, func() -> void:
				save.lessons[str(n)] = true
				Meta.write_save(save)
				_pop()
				_start_chapter(n)))
		else:
			_start_chapter(n)
	var lines := I18n.tl("brief.%d" % n)
	if lines.is_empty():
		go.call()
		return
	_push(Overlays.Dialogue.new(lines, func() -> void:
		_pop()
		go.call()))

func _start_chapter(n: int) -> void:
	var c := Meta.chapter(n)
	var known := Meta.known_at(n)
	var seed_v := n * 101 + int(save.chapters.get(str(n), {}).get("tries", 0)) * 7 + randi() % 50
	if c.kind == "duel":
		start_game("duel", {"mode": "ai", "rival": c.rival, "skill": c.skill, "arena": c.arena, "best_of": c.best_of, "known": known, "ai_known": known, "chapter": n, "seed": seed_v})
	else:
		start_game("rives", {"mode": "solo", "level": c.level, "boss": c.get("boss", false), "known": known, "chapter": n, "seed": seed_v})

func start_game(kind: String, cfg: Dictionary) -> void:
	_end_game()
	_set_screen(null)
	_clear_overlays()
	_last_kind = kind
	_last_cfg = cfg
	if kind == "duel":
		game = DuelScreen.new(cfg)
		(game as DuelScreen).finished.connect(_game_done)
		Sound.music("duel")
	else:
		game = RivesScreen.new(cfg)
		(game as RivesScreen).finished.connect(_game_done)
		Sound.music("boss" if cfg.get("boss", false) else "rives")
	game.app = self
	if online and cfg.get("mode", "") == "net":
		game.set("online", online)
	ui.add_child(game)
	ui.move_child(game, 0)

func _game_done(r: Dictionary) -> void:
	var actions: Array = []
	var n: int = r.get("chapter", 0)
	var net: bool = _last_cfg.get("mode", "") == "net"
	if n > 0:
		Meta.record(save, n, r)
		Meta.write_save(save)
		if r.won and n == Meta.CHAPTERS.size() and not save.seen.has("finale"):
			actions.append({"label": I18n.t("common.continue"), "action": func() -> void:
				save.seen["finale"] = true
				save.seen["free"] = true
				Meta.write_save(save)
				_cinematic("finale", func() -> void:
					var cr: Array = [{"image": "key_art", "text": I18n.t("ending.credits")}]
					_set_screen(CinematicScreen.new(cr, to_title)))})
		elif r.won and n < Meta.CHAPTERS.size():
			actions.append({"label": I18n.t("results.next"), "action": func() -> void:
				_clear_overlays()
				_chapter_pick(n + 1)})
		actions.append({"label": I18n.t("results.retry"), "action": func() -> void:
			_clear_overlays()
			_start_chapter(n)})
		actions.append({"label": I18n.t("results.campaign"), "action": func() -> void: to_campaign(n)})
	elif net:
		if online.is_host:
			actions.append({"label": I18n.t("online.again"), "action": func() -> void:
				var c := _last_cfg.duplicate()
				c.seed = randi() % 100000
				online_start({"kind": _last_kind, "arena": c.get("arena", "ar_scriptorium"), "level": c.get("level", 0), "seed": c.seed})})
		else:
			actions.append({"label": I18n.t("online.wait_host"), "action": func() -> void: pass})
		actions.append({"label": I18n.t("results.menu"), "action": func() -> void:
			online.close()
			to_title()})
	else:
		actions.append({"label": I18n.t("results.retry"), "action": func() -> void:
			var c := _last_cfg.duplicate()
			c.seed = randi() % 100000
			start_game(_last_kind, c)})
		actions.append({"label": I18n.t("results.menu"), "action": to_title})
	Sound.music("title")
	_push(ResultsScreen.new(r, actions, _last_cfg.get("rival", "")))

# ---- Online -------------------------------------------------------------------------------------------------------------------

func online_start(d: Dictionary) -> void:
	online.send("start", d)
	_online_begin(d)

func _online_begin(d: Dictionary) -> void:
	online.reset_lockstep()
	if d.kind == "duel":
		start_game("duel", {"mode": "net", "host": online.is_host, "rival": "aldric", "skill": 1.0, "arena": d.arena, "best_of": 3, "seed": int(d.seed),
			"names": [I18n.t("names.ysolde"), I18n.t("names.aldric")], "portraits": ["pt_ysolde", "pt_aldric"]})
	else:
		start_game("rives", {"mode": "net", "host": online.is_host, "level": int(d.level), "boss": int(d.level) == 3, "seed": int(d.seed)})

func _on_net_message(kind: String, data: Variant) -> void:
	if kind == "start" and data is Dictionary:
		_online_begin(data)
	elif kind == "strokes" and game and game.has_method("remote_strokes"):
		game.call("remote_strokes", data)

# ---- Pause ----------------------------------------------------------------------------------------------------------------------

func _pause() -> void:
	if game and _last_cfg.get("mode", "") == "net":
		return
	Sound.sfx("ui", 0.8)
	Sound.muffled = true
	var close := func() -> void:
		Sound.muffled = false
		_pop()
	_push(Overlays.Pause.new(close, func() -> void:
		Sound.muffled = false
		_pop()
		var c := _last_cfg.duplicate()
		c.seed = int(c.get("seed", 1)) + 1
		start_game(_last_kind, c), _options, func() -> void:
		Sound.muffled = false
		_pop()
		if _chapter > 0 and _last_cfg.get("chapter", 0) > 0:
			to_campaign(_chapter)
		else:
			to_title()))

# ---- Loop -----------------------------------------------------------------------------------------------------------------------

func _physics_process(_d: float) -> void:
	if game == null:
		return
	if _last_cfg.get("mode", "") == "net":
		if online:
			online.net_step(game)
		return
	if _overlays.size() > 0:
		return
	if Keys.pressed("pause"):
		_pause()
		return
	if Keys.pressed("book") and _last_kind == "duel":
		var known: Array = _last_cfg.get("known", range(Glyphs.COUNT))
		_push(BookScreen.new(known, [], func() -> void: _pop()))
		return
	game.call("step")

func _process(dt: float) -> void:
	_clock += dt
	if online and not online.message.is_connected(_on_net_message):
		online.message.connect(_on_net_message)
	if game and (_overlays.is_empty() or _last_cfg.get("mode", "") == "net"):
		game.call("frame", minf(dt, 0.1))
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
