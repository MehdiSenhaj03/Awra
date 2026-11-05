extends Node
## CASSE: boot, screens and the play loop (simulation at a fixed 60 Hz in the physics step).

var view: View
var ui: Control
var save: Dictionary
var settings: Dictionary
var heist: Heist = null
var software := false
var _screen: Screen = null
var _overlays: Array[Screen] = []
var _awake := false
var _post: ShaderMaterial
var _post_rect: ColorRect
var _vault_id := ""
var _coop := false
var online: Node = null
# Automation: --level <json> --plans <json> --shots <prefix> --seconds <n> --screen <name>.
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
	view = View.new()
	add_child(view)
	var post_layer := CanvasLayer.new()
	add_child(post_layer)
	_post_rect = ColorRect.new()
	_post = ShaderMaterial.new()
	_post.shader = preload("res://src/render/shaders/post.gdshader")
	_post_rect.material = _post
	_post_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	post_layer.add_child(_post_rect)
	Kit.full(_post_rect)
	view.post = _post
	var layer := CanvasLayer.new()
	layer.layer = 1
	add_child(layer)
	ui = Control.new()
	ui.mouse_filter = Control.MOUSE_FILTER_IGNORE
	layer.add_child(ui)
	Kit.full(ui)
	var a := OS.get_cmdline_user_args()
	var lvp := ""
	var plans: Array = []
	var scr := ""
	for i in a.size():
		match a[i]:
			"--level": lvp = a[i + 1]
			"--plans": plans = JSON.parse_string(a[i + 1])
			"--shots": _shots = a[i + 1]
			"--seconds": _seconds = float(a[i + 1])
			"--screen": scr = a[i + 1]
	if software and lvp == "" and scr == "":
		settings.quality = 0
	if lvp == "" and scr == "":
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN if settings.fullscreen else DisplayServer.WINDOW_MODE_WINDOWED)
	_apply_settings()
	if lvp != "":
		_awake = true
		var lv := Level.new(JSON.parse_string(FileAccess.get_file_as_string(lvp)))
		_start_heist(lv, "", ["plan"] if plans.size() > 0 else ["solo"])
		heist.plans = plans
		return
	to_title()
	match scr:
		"map": to_map("")
		"workshop": _workshop()
		"code": _code()
		"options": _options()
		"intro": _campaign_pick("1-1")
		"results": _push(ResultsScreen.new({"medal": 2, "echoes": 3, "par": 2, "frame": 1234, "gems": 1, "gems_total": 2, "loops": 5, "replay": ""}, true, [{"label": I18n.t("results.next"), "action": func() -> void: pass}]))
		"online": _online()

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

func _end_heist() -> void:
	if heist:
		heist.queue_free()
		heist = null
	get_viewport().disable_3d = true
	_post_rect.visible = false

func to_title() -> void:
	_end_heist()
	_coop = false
	Sound.music("title")
	var items: Array = [
		{"label": I18n.t("title.campaign"), "action": func() -> void: to_map("")},
		{"label": I18n.t("title.coop"), "sub": I18n.t("title.coop_sub"), "action": func() -> void:
			_coop = true
			to_map("")},
		{"label": I18n.t("title.online"), "sub": I18n.t("title.online_sub"), "action": _online},
		{"label": I18n.t("title.workshop"), "sub": I18n.t("title.workshop_sub"), "action": _workshop},
		{"label": I18n.t("title.code"), "sub": I18n.t("title.code_sub"), "action": _code},
		{"label": I18n.t("title.options"), "action": _options},
		{"label": I18n.t("title.quit"), "action": func() -> void: get_tree().quit()},
	]
	var t := TitleScreen.new(items, func() -> void: _awake = true, _awake)
	_set_screen(t)
	if software:
		t.warn(I18n.t("options.software"))

func to_map(focus: String) -> void:
	_end_heist()
	Sound.music("title")
	_set_screen(MapScreen.new(save, focus, _campaign_pick, to_title, "coop.title" if _coop else "map.title"))

func _options() -> void:
	_push(OptionsScreen.new(settings, func() -> void: _pop(), _apply_settings))

func _apply_settings() -> void:
	Sound.set_volumes(settings.music, settings.sfx)
	view.set_quality(int(settings.quality))
	view.shake_scale = settings.shake
	Meta.write_settings(settings)

func _code() -> void:
	_set_screen(CodeScreen.new(func(d: Dictionary) -> void: _start_heist(Level.new(d), ""), func(r: Dictionary) -> void:
		_end_heist()
		_set_screen(null)
		heist = Heist.new(Level.new(r.level), view, 0, [], {}, r.rec)
		_show_heist(), to_title))

func _workshop() -> void:
	reopen_workshop({}, -1)

func reopen_workshop(d: Dictionary, sl: int) -> void:
	_end_heist()
	get_viewport().disable_3d = false
	_post_rect.visible = true
	Sound.music("title")
	_set_screen(Workshop.new(self, save, d, sl))

## The Workshop's test: crack your own vault; on a win, the vault is validated.
func start_test(d: Dictionary, on_win: Callable) -> void:
	var lv := Level.new(d)
	_end_heist()
	_set_screen(null)
	heist = Heist.new(lv, view, 1, ["solo"], {"id": ""})
	heist.finished.connect(func(r: Dictionary) -> void:
		on_win.call(r))
	heist.quit.connect(func() -> void: reopen_workshop(d, -1))
	_show_heist()

func start_online_coop(lv: Level) -> void:
	_end_heist()
	_set_screen(null)
	heist = Heist.new(lv, view, 2, ["net", "net"], {"id": ""})
	heist.net_mode = true
	heist.finished.connect(func(r: Dictionary) -> void: _heist_done(lv, "", r))
	heist.quit.connect(to_title)
	_show_heist()

func start_duel(lv: Level) -> void:
	_end_heist()
	_set_screen(null)
	heist = Heist.new(lv, view, 1, ["solo"], {"id": "", "duel": true})
	heist.finished.connect(func(r: Dictionary) -> void:
		online.send("duel_win", {"echoes": r.echoes, "frame": r.frame})
		heist.duel_result(true)
		_heist_done(lv, "", r))
	heist.quit.connect(to_title)
	online.message.connect(func(kind: String, data: Variant) -> void:
		if heist == null:
			return
		if kind == "duel_prog":
			heist.opp_text = I18n.t("online.opp", {"loop": data.loop, "echoes": data.echoes})
		elif kind == "duel_win":
			heist.duel_result(false))
	_show_heist()

func _online() -> void:
	_set_screen(OnlineScreen.new(self))

# ---- Campaign ----------------------------------------------------------------------------------------------------------

func _campaign_pick(id: String) -> void:
	_vault_id = id
	var go := func() -> void:
		var lines := I18n.tl("story.%s" % id)
		var start := func() -> void:
			var lv := Meta.level(id)
			lv.data.name = I18n.t("vault.%s" % id)
			_start_heist(lv, id, ["kb", _p2_source()] if _coop else ["solo"])
		if lines.size() > 0 and not _coop:
			_end_heist()
			_set_screen(null)
			Sound.music("title")
			_push(Overlays.Dialogue.new(lines, func() -> void:
				_pop()
				start.call()))
		else:
			start.call()
	var cine := ""
	if id == "1-1" and not save.seen.has("intro"):
		cine = "intro"
	elif id == "4-1" and not save.seen.has("betrayal"):
		cine = "betrayal"
	if cine != "" and not _coop:
		save.seen[cine] = true
		Meta.write_save(save)
		_cinematic(cine, go)
	else:
		go.call()

func _cinematic(id: String, then: Callable) -> void:
	_end_heist()
	Sound.music("title")
	var shots: Array = I18n.tl("cine.%s" % id)
	_set_screen(CinematicScreen.new(shots, then))

func _p2_source() -> String:
	var pads := Input.get_connected_joypads()
	return "pad%d" % pads[0] if pads.size() > 0 else "kb2"

func _start_heist(lv: Level, id: String, sources: Array = ["solo"]) -> void:
	_end_heist()
	_set_screen(null)
	for o in _overlays.duplicate():
		_pop(o)
	heist = Heist.new(lv, view, sources.size(), sources, {"id": id})
	heist.finished.connect(func(r: Dictionary) -> void: _heist_done(lv, id, r))
	heist.quit.connect(func() -> void:
		if id != "":
			to_map(id)
		else:
			to_title())
	_show_heist()

func _show_heist() -> void:
	ui.add_child(heist)
	ui.move_child(heist, 0)
	get_viewport().disable_3d = false
	_post_rect.visible = true
	Sound.music("heist")

func _heist_done(lv: Level, id: String, r: Dictionary) -> void:
	var record := false
	if id != "":
		record = Meta.record(save, id, r)
		Meta.write_save(save)
	var order := Meta.order()
	var i := order.find(id)
	var next: String = order[i + 1] if i >= 0 and i + 1 < order.size() else ""
	var actions: Array = []
	if id == "5-2" and not _coop:
		actions.append({"label": I18n.t("common.continue"), "action": func() -> void:
			_pop()
			save.seen["finale"] = true
			Meta.write_save(save)
			_cinematic("finale", func() -> void:
				var cr: Array = [{"image": "key_art", "text": I18n.t("ending.credits")}]
				_set_screen(CinematicScreen.new(cr, to_title)))})
	elif next != "":
		actions.append({"label": I18n.t("results.next"), "action": func() -> void:
			_pop()
			_campaign_pick(next)})
	actions.append({"label": I18n.t("results.retry"), "action": func() -> void:
		_pop()
		_start_heist(lv, id, heist.sources if heist else ["solo"])})
	actions.append({"label": I18n.t("results.replay"), "action": func() -> void:
		DisplayServer.clipboard_set(r.replay)
		Sound.sfx("uiOk")})
	actions.append({"label": I18n.t("results.map") if id != "" else I18n.t("common.back"), "action": func() -> void:
		_pop()
		if id != "":
			to_map(id)
		else:
			to_title()})
	_push(ResultsScreen.new(r, record, actions))

func _pause() -> void:
	Sound.sfx("ui", 0.8)
	var close := func() -> void: _pop()
	_push(Overlays.Pause.new(close, func() -> void:
		_pop()
		var lv := heist.level
		_start_heist(lv, _vault_id if heist.meta.get("id", "") != "" else "", heist.sources), _options, func() -> void:
		_pop()
		if heist and heist.meta.get("id", "") != "":
			to_map(heist.meta.id)
		else:
			to_title()))

# ---- Loop ---------------------------------------------------------------------------------------------------------------

func _physics_process(_d: float) -> void:
	if heist == null or _overlays.size() > 0:
		return
	if online and online.has_method("net_step") and heist.net_mode:
		online.net_step(heist)
		return
	if Keys.pressed("pause") and not heist.replay and heist._overlay == null:
		_pause()
		return
	var before := heist.run.loops
	heist.step()
	if heist and heist.meta.get("duel", false) and online and heist.run.loops != before:
		online.send("duel_prog", {"loop": heist.run.loops, "echoes": heist.run.rec.size()})

func _process(dt: float) -> void:
	_clock += dt
	if heist:
		heist.frame(minf(dt, 0.1))
	var top: Screen = _overlays.back() if _overlays.size() > 0 else _screen
	if top and top != heist:
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
