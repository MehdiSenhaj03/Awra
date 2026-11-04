extends Node
## The whole game: boot, screens, the play loop (simulation at a fixed 60 Hz in the physics step, rendering
## interpolated every frame).

const LOOKS := {
	1: [0x2ef2ff, 0xff2fb9, 0x02040c, "bg_seuil"],
	2: [0x40ffb0, 0x2ef2ff, 0x020a08, "bg_jardin"],
	3: [0xffa040, 0xff2fb9, 0x0a0402, "bg_fonderie"],
	4: [0xc8d4ff, 0x9b5cff, 0x050510, "bg_ocean"],
	5: [0xff2fb9, 0x9b5cff, 0x0a0210, "bg_ville"],
	6: [0xffffff, 0xffc34a, 0x050505, "bg_coeur"],
}
const ENEMY_HINTS := {"bulwark": "bulwark", "mirror": "mirror", "phantom": "phantom", "choir": "choir"}

var view: View
var session: Session = null
var save: Dictionary
var settings: Dictionary
var ui: Control
var _screen: Screen = null
var _overlays: Array[Screen] = []
var _passive: Array = []
var _awake := false
var _hints: Dictionary = {}
var _stage_id := ""
var software := false
var _post: ShaderMaterial
var _post_rect: ColorRect
# Automation (command line): --stage <id>, --bot, --shots <prefix>, --seconds <n>.
var _bot := false
var _shots := ""
var _seconds := 0.0
var _clock := 0.0
var _shot_i := 0
# Adaptive quality.
var _debug_screen := ""
var _slow := 0.0
var _watch_t := 0.0

func _ready() -> void:
	get_tree().auto_accept_quit = true
	save = Meta.load_save()
	settings = Meta.load_settings()
	Keys.apply(settings.bindings)
	I18n.set_lang(settings.lang)
	var adapter := RenderingServer.get_video_adapter_name().to_lower()
	software = adapter.contains("llvmpipe") or adapter.contains("swiftshader") or adapter.contains("software") or adapter.contains("basic render")
	view = View.new()
	add_child(view)
	var post_layer := CanvasLayer.new()
	post_layer.layer = 0
	add_child(post_layer)
	_post_rect = ColorRect.new()
	_post = ShaderMaterial.new()
	_post.shader = preload("res://src/render/shaders/post.gdshader")
	_post_rect.material = _post
	_post_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	post_layer.add_child(_post_rect)
	Kit.full(_post_rect)
	view.set_post(_post)
	var layer := CanvasLayer.new()
	layer.layer = 1
	add_child(layer)
	ui = Control.new()
	ui.mouse_filter = Control.MOUSE_FILTER_IGNORE
	layer.add_child(ui)
	Kit.full(ui)
	var args := OS.get_cmdline_user_args()
	var stage := ""
	for i in args.size():
		match args[i]:
			"--stage": stage = args[i + 1]
			"--bot": _bot = true
			"--shots": _shots = args[i + 1]
			"--seconds": _seconds = float(args[i + 1])
			"--screen": _debug_screen = args[i + 1]
	if software and stage == "":
		settings.quality = 0
	DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN if settings.fullscreen and stage == "" else DisplayServer.WINDOW_MODE_WINDOWED)
	_apply_settings()
	if stage != "":
		_awake = true
		_wake_audio()
		start_stage(stage, 0, true)
	else:
		to_title()
	match _debug_screen:
		"map": to_map("")
		"loadout": _to_loadout("s1_2")
		"codex": _set_screen(CodexScreen.new(save, to_title))
		"options": _options()
		"intro": _play()
		"choice": _finale()
		"results":
			var w := World.new(Stages.get_def("s1_1").arena, 128.0, 1)
			w.score = 48210
			w.stats.perfects = 40
			w.stats.greats = 10
			w.stats.frames = 3600 * 2
			_push(ResultsScreen.new(w, true, 3, true, 140, [{"label": I18n.t("results.next"), "action": func() -> void: pass}, {"label": I18n.t("results.retry"), "action": func() -> void: pass}]))
		"dialogue":
			start_stage("s1_1", 0, false)
		"boss":
			start_stage("s1_4", 3, true)

func _wake_audio() -> void:
	Sound.set_volumes(settings.music, settings.sfx)

# ---- Screens ---------------------------------------------------------------------------------------------------------

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

func _menu_music() -> void:
	if Sound.theme != "title":
		Sound.stop(0.8)
		Sound.start("title", 96)
	Sound.intensity = 0
	Sound.muffle(false)

func _end_session() -> void:
	if session:
		session.dispose()
		session = null
	get_viewport().disable_3d = true
	_post_rect.visible = false
	for b in _passive:
		b.queue_free()
	_passive.clear()

func to_title() -> void:
	_end_session()
	if _awake:
		_menu_music()
	var s := save
	var started: bool = s.stages.size() > 0
	var items: Array = [
		{"label": I18n.t("title.continue") if started else I18n.t("title.play"), "action": _play},
		{"label": I18n.t("title.encore") if Meta.endless_unlocked(s) else I18n.t("title.encoreLocked"), "action": func() -> void: start_stage("encore"), "disabled": not Meta.endless_unlocked(s)},
		{"label": I18n.t("title.codex"), "action": func() -> void: _set_screen(CodexScreen.new(save, to_title))},
		{"label": I18n.t("title.options"), "action": _options},
	]
	if started:
		items.append({"label": I18n.t("title.newGame"), "action": func() -> void:
			_push(Overlays.Confirm.new(I18n.t("title.confirmNew"), func() -> void:
				save = Meta.new_save()
				Meta.write_save(save)
				_pop()
				to_title(), func() -> void: _pop()))})
	items.append({"label": I18n.t("title.quit"), "action": func() -> void: get_tree().quit()})
	var t := TitleScreen.new(items, func() -> void:
		_awake = true
		_wake_audio()
		_menu_music(), _awake)
	_set_screen(t)
	if software:
		t.warn(I18n.t("options.software"))

func _options() -> void:
	_push(OptionsScreen.new(settings, func() -> void: _pop(), _apply_settings))

func _apply_settings() -> void:
	Sound.set_volumes(settings.music, settings.sfx)
	view.set_quality(int(settings.quality))
	view.shake_scale = settings.shake
	view.flash_scale = settings.flashes
	if session:
		session.world.beat.latency = settings.latency
	Meta.write_settings(settings)

func _play() -> void:
	if not save.seenIntro:
		save.seenIntro = true
		Meta.write_save(save)
		var lines := I18n.tl("story.intro")
		var imgs := ["cine_symphony", "cine_silence", "cine_awaken"]
		var shots: Array = []
		for i in lines.size():
			shots.append({"image": imgs[i], "text": lines[i]})
		_set_screen(CinematicScreen.new(shots, func() -> void: to_map("s1_1")))
		Sound.stop(1.5)
		return
	to_map("")

func to_map(focus: String) -> void:
	_end_session()
	_menu_music()
	_set_screen(MapScreen.new(save, focus, _to_loadout, to_title))

func _to_loadout(id: String) -> void:
	_set_screen(LoadoutScreen.new(save, id, func() -> void: start_stage(id, 0), func() -> void: to_map(id)))

# ---- Playing ---------------------------------------------------------------------------------------------------------

func start_stage(id: String, checkpoint: int = 0, skip_story: bool = false) -> void:
	_end_session()
	_set_screen(null)
	_stage_id = id
	var d := Stages.get_def(id)
	var look: Array = LOOKS[int(d.sector)]
	var s := save
	var setup := func(w: World) -> void:
		w.apply_mods(Meta.build_mods(s))
		w.player.weapon = s.weapon
		w.player.weapon_level = int(s.weaponLevels.get(s.weapon, 1))
		w.can_pulse = Meta.pulse_unlocked(s) or int(d.sector) > 1 or int(d.index) > 0
		w.can_overdrive = Meta.overdrive_unlocked(s)
		w.beat.latency = settings.latency
		if s.hard:
			w.player.max_hp = maxi(1, w.player.max_hp - 2)
			w.player.hp = w.player.max_hp
		if _bot:
			w.god = true
	Sound.stop(0.4)
	session = Session.new(d, view, ui, checkpoint, setup)
	ui.move_child(session.hud, 0)
	get_viewport().disable_3d = false
	_post_rect.visible = true
	view.shake_scale = settings.shake
	view.flash_scale = settings.flashes
	view.set_stage(session.world, look[0], look[1], look[2], Kit.tex(look[3]))
	session.on_end = _end_stage
	session.on_events = _game_events
	session.hud.banner(I18n.t("sector.%d.name" % d.sector).to_upper(), I18n.t("stage.%d" % d.index).to_upper())
	var first := not Meta.cleared(s, id)
	var lines := I18n.tl("story.%s" % id)
	if not skip_story and first and checkpoint == 0 and lines.size() > 0:
		_push(Overlays.Dialogue.new(lines, func() -> void: _pop()))
	session.frame(1.0, 1.0 / 60.0)
	if id == "s1_1" and first:
		_hint("move", 0)

func _hint(key: String, delay_frames: int) -> void:
	if _hints.has(key):
		return
	_hints[key] = true
	var vars := {}
	for a in ["up", "down", "left", "right", "fire", "dash", "pulse", "overdrive"]:
		vars[a] = Keys.key_name(a)
	var text := I18n.t("hint.%s" % key, vars)
	get_tree().create_timer(delay_frames / 60.0).timeout.connect(func() -> void:
		if session:
			session.hud.show_hint(text, 420))

func _game_events(events: Array[Dictionary]) -> void:
	var w := session.world
	for e in events:
		var t: String = e.t
		if t == "boss" and e.phase == 0:
			var b := Overlays.BossCard.new(e.name)
			_passive.append(b)
			ui.add_child(b)
		elif t == "boss" and e.phase > 1 and e.phase < 9:
			session.hud.banner(I18n.t("boss.phase", {"n": e.phase}))
		elif t == "boss" and e.phase == 9:
			session.hud.banner(I18n.t("boss.solo"))
		elif t == "wave":
			if _stage_id == "s1_1" and e.n == 2: _hint("beat", 30)
			if _stage_id == "s1_1" and e.n == 4: _hint("dash", 30)
			if _stage_id == "s1_2" and e.n == 1: _hint("pulse", 60)
			if _stage_id == "s1_2" and e.n == 4: _hint("rank", 60)
		elif t == "spawn":
			for en in w.enemies:
				if ENEMY_HINTS.has(en.kind):
					_hint(ENEMY_HINTS[en.kind], 60)
		elif t == "fragment":
			if not save.fragments.has(e.id):
				save.fragments.append(e.id)
				Meta.write_save(save)
			session.hud.banner(I18n.t("map.fragment").to_upper(), I18n.t("fragments.%s.name" % e.id))
	if w.can_overdrive and w.gauge >= 100.0:
		_hint("overdrive", 0)

func _end_stage(win: bool) -> void:
	var w := session.world
	var id := _stage_id
	var s := save
	s.playFrames += w.stats.frames
	if _bot and _shots != "":
		get_tree().quit()
		return
	if id == "encore":
		var wave := session.runner.wave_number
		var record: bool = w.score > s.endlessBest
		s.endlessBest = maxi(int(s.endlessBest), int(roundf(w.score)))
		s.endlessWave = maxi(int(s.endlessWave), wave)
		s.notes += w.stats.notes
		Meta.write_save(s)
		_push(ResultsScreen.new(w, false, 0, record, w.stats.notes, [
			{"label": I18n.t("results.retry"), "action": func() -> void:
				_pop()
				start_stage("encore", 0, true)},
			{"label": I18n.t("results.map"), "action": func() -> void:
				_pop()
				to_title()},
		]))
		return
	var g := Meta.grade(id, w.score, w.stats.hits, w.stats.max_rank) if win else 0
	var rec: Dictionary = s.stages.get(id, {"best": 0, "rank": 0, "cleared": false, "fragment": false})
	var first_clear: bool = win and not rec.cleared
	var record: bool = win and w.score > float(rec.best)
	var notes: int = w.stats.notes
	var d := Stages.get_def(id)
	if win:
		notes += 40 * int(d.sector) + g * 30
	s.notes += notes
	if win:
		rec.cleared = true
		rec.best = maxi(int(rec.best), int(roundf(w.score)))
		rec.rank = maxi(int(rec.rank), g)
	else:
		s.deaths += 1
	var frag: Variant = d.get("fragment")
	if frag is Dictionary and s.fragments.has(frag.id):
		rec.fragment = true
	s.stages[id] = rec
	Meta.write_save(s)
	var order := Stages.order()
	var idx := order.find(id)
	var next: String = order[idx + 1] if idx >= 0 and idx + 1 < order.size() else ""
	var actions: Array = []
	if win:
		actions = [
			{"label": I18n.t("results.next"), "action": func() -> void: _after_win(id, first_clear, next)},
			{"label": I18n.t("results.retry"), "action": func() -> void: start_stage(id)},
			{"label": I18n.t("results.map"), "action": func() -> void: to_map(id)},
		]
	else:
		var mv := session.runner.movement
		if mv > 0:
			actions.append({"label": I18n.t("results.checkpoint"), "action": func() -> void: start_stage(id, mv, true)})
		actions.append({"label": I18n.t("results.retry"), "action": func() -> void: start_stage(id, 0, true)})
		actions.append({"label": I18n.t("results.map"), "action": func() -> void: to_map(id)})
	Sound.intensity = 0
	session.hud.visible = false
	var wrapped: Array = []
	for a: Dictionary in actions:
		var f: Callable = a.action
		wrapped.append({"label": a.label, "action": func() -> void:
			_pop()
			f.call()})
	_push(ResultsScreen.new(w, win, g, record, notes, wrapped))

## Story beats and unlocks after a first clear, then onwards.
func _after_win(id: String, first_clear: bool, next: String) -> void:
	_end_session()
	Sound.start("title", 96)
	Sound.intensity = 0
	var queue: Array[Callable] = []
	var run := func(self_ref: Callable) -> void:
		if queue.size() > 0:
			queue.pop_front().call()
		elif id == "s6_4" and first_clear:
			_finale()
		else:
			to_map(next if next != "" else id)
	var go := func() -> void: run.call(run)
	var d := Stages.get_def(id)
	if first_clear:
		var boss: String = d.get("boss", "") if d.get("boss") is String else ""
		if boss != "":
			queue.append(func() -> void: _push(Overlays.Unlock.new(I18n.t("boss.%s.name" % boss), "", I18n.t("story.after.%s" % boss), "boss_%s" % boss, func() -> void:
				_pop()
				go.call())))
		var after := I18n.tl("story.%sb" % id)
		if after.size() > 0:
			queue.append(func() -> void: _push(Overlays.Dialogue.new(after, func() -> void:
				_pop()
				go.call())))
		var unlock := func(k: String, n: String, desc: String, icon: String) -> void:
			queue.append(func() -> void: _push(Overlays.Unlock.new(k, n, desc, icon, func() -> void:
				_pop()
				go.call())))
		if id == "s1_1":
			unlock.call("", I18n.t("unlock.pulse.name"), I18n.t("unlock.pulse.desc", {"key": Keys.key_name("pulse")}), "ic_a6")
		if id == "s1_4":
			unlock.call("", I18n.t("unlock.overdrive.name"), I18n.t("unlock.overdrive.desc", {"key": Keys.key_name("overdrive")}), "ic_a7")
			unlock.call(I18n.t("unlock.weapon"), I18n.t("weapon.chord.name"), I18n.t("weapon.chord.desc"), "ic_a1")
		if id == "s2_4":
			unlock.call(I18n.t("unlock.weapon"), I18n.t("weapon.lance.name"), I18n.t("weapon.lance.desc"), "ic_a2")
			unlock.call("", I18n.t("unlock.slot"), "", "")
		if id == "s3_4":
			unlock.call(I18n.t("unlock.weapon"), I18n.t("weapon.arc.name"), I18n.t("weapon.arc.desc"), "ic_a3")
			unlock.call("", I18n.t("unlock.encore"), I18n.t("encoreMode.desc"), "")
		if id == "s4_4":
			unlock.call(I18n.t("unlock.weapon"), I18n.t("weapon.swarm.name"), I18n.t("weapon.swarm.desc"), "ic_a4")
			unlock.call("", I18n.t("unlock.slot"), "", "")
	go.call()

func _finale() -> void:
	_end_session()
	_set_screen(Overlays.Choice.new(func() -> void: _ending("coda"), func() -> void: _ending("encore")))

func _ending(kind: String) -> void:
	save.ending = kind
	Meta.write_save(save)
	Sound.stop(2.0)
	var lines := I18n.tl("story.%s" % kind)
	var frames: int = save.playFrames
	var h := frames / 216000
	var m := (frames / 3600) % 60
	var shots: Array = []
	for text in lines:
		shots.append({"image": "cine_coda" if kind == "coda" else "cine_encore", "text": text})
	shots.append({"image": "cine_fermata", "text": "%s %s" % [I18n.t("ending.thanks"), I18n.t("ending.stats", {"time": "%d h %d min" % [h, m], "deaths": save.deaths})]})
	shots.append({"image": "key_art", "text": I18n.t("ending.credits")})
	_set_screen(CinematicScreen.new(shots, to_title))
	get_tree().create_timer(1.5).timeout.connect(func() -> void:
		Sound.start("title", 96)
		Sound.intensity = 0)

func _pause() -> void:
	Sound.muffle(true, 700.0)
	Sound.sfx("ui", 0.8)
	var close := func() -> void:
		_pop()
		Sound.muffle(false)
	_push(Overlays.Pause.new(close, func() -> void:
		close.call()
		start_stage(_stage_id, 0, true), _options, func() -> void:
		close.call()
		to_map(_stage_id)))

# ---- Loop ------------------------------------------------------------------------------------------------------------

func _physics_process(_delta: float) -> void:
	var s := session
	if s == null or _overlays.size() > 0 or s.ended:
		return
	if _bot:
		s.script_aim = "auto"
		var c := Bot.controls(s.world)
		s.script_keys = {"fire": true} if c.fire else {}
		if c.dash: s.script_keys["dash"] = true
		if c.pulse: s.script_keys["pulse"] = true
		if c.overdrive: s.script_keys["overdrive"] = true
	s.step()
	if Keys.pressed("pause") and not s.ended and not _bot:
		_pause()

## Drops one quality level when the game stays under ~45 fps for three seconds.
func _watch(raw: float) -> void:
	if session == null or _overlays.size() > 0 or raw > 0.25 or _bot:
		return
	_watch_t += raw
	if raw > 1.0 / 45.0:
		_slow += raw
	if _watch_t < 3.0:
		return
	var ratio := _slow / _watch_t
	_watch_t = 0.0
	_slow = 0.0
	if ratio > 0.6 and int(settings.quality) > 0:
		settings.quality = int(settings.quality) - 1
		view.set_quality(settings.quality)
		Meta.write_settings(settings)
		session.hud.show_hint(I18n.t("options.autoQuality", {"q": I18n.t("options.q%d" % settings.quality)}), 300)

func _process(dt: float) -> void:
	_clock += dt
	_watch(dt)
	dt = minf(dt, 0.1)
	var s := session
	var blocking := _overlays.size() > 0
	if s:
		s.frame(Engine.get_physics_interpolation_fraction() if not blocking else 1.0, 0.0 if blocking else dt)
	elif Sound.theme != "":
		Sound.update(_clock * 96.0 / 60.0, 96.0, dt)
	var top: Screen = _overlays.back() if _overlays.size() > 0 else _screen
	if top:
		top.update(dt)
	var i := _passive.size() - 1
	while i >= 0:
		var b: Overlays.BossCard = _passive[i]
		b.update(dt)
		if b.done:
			b.queue_free()
			_passive.remove_at(i)
		i -= 1
	if _shots != "" and _seconds > 0.0:
		var every := _seconds / 4.0
		if _clock >= every * (_shot_i + 1) and _shot_i < 4:
			_shot_i += 1
			_capture("%s_%d.png" % [_shots, _shot_i])
		if _clock > _seconds + 0.3:
			get_tree().quit()

func _capture(path: String) -> void:
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(path)
