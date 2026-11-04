class_name OptionsScreen
extends Screen
## Options: volumes, shake, flashes, quality, rhythm calibration, language, controls, fullscreen.

var _st: Dictionary
var _on_back: Callable
var _on_change: Callable
var _panel: PanelContainer
var _body: VBoxContainer
var _menu: UIMenu
var _sub := "main"
var _calib_t := 0.0
var _calib_next := 0.0
var _clicks: Array[float] = []
var _taps: Array[float] = []
var _dot: ColorRect
var _capture := ""
var _capture_item: MenuItem

func _init(st: Dictionary, on_back: Callable, on_change: Callable) -> void:
	super()
	dim(0.72)
	_st = st
	_on_back = on_back
	_on_change = on_change
	_panel = Kit.panel(self)
	var m := Kit.margin(_panel, 40, 36, 40, 36)
	_body = Kit.vbox(m, 22)
	_panel.custom_minimum_size.x = 760
	_build()

func _relayout() -> void:
	_panel.reset_size()

func _process(_dt: float) -> void:
	_panel.position = ((Vector2(Kit.W, Kit.H) - _panel.size) / 2).round()

func _clear() -> void:
	for c in _body.get_children():
		_body.remove_child(c)
		c.queue_free()

func _row(label: String, value: Callable, adjust: Callable) -> void:
	var it := _menu.add(label, Callable(), {"size": 15, "width": 680, "value": value.call()})
	it.adjust = func(d: int) -> void:
		adjust.call(d)
		it.set_value(value.call())
		_save()
	it.action = func() -> void:
		adjust.call(1)
		it.set_value(value.call())
		_save()

func _build() -> void:
	_sub = "main"
	_clear()
	Kit.h_title(_body, I18n.t("options.title"))
	_menu = UIMenu.new(_body)
	var st := _st
	var pct := func(v: float) -> String: return "%d %%" % int(roundf(v * 100.0))
	_row(I18n.t("options.music"), func() -> String: return pct.call(st.music), func(d: int) -> void:
		st.music = clampf(st.music + d * 0.1, 0.0, 1.0)
		Sound.set_volumes(st.music, st.sfx))
	_row(I18n.t("options.sfx"), func() -> String: return pct.call(st.sfx), func(d: int) -> void:
		st.sfx = clampf(st.sfx + d * 0.1, 0.0, 1.0)
		Sound.set_volumes(st.music, st.sfx)
		Sound.sfx("ui"))
	_row(I18n.t("options.shake"), func() -> String: return pct.call(st.shake), func(d: int) -> void: st.shake = clampf(st.shake + d * 0.25, 0.0, 1.0))
	_row(I18n.t("options.flashes"), func() -> String: return pct.call(st.flashes), func(d: int) -> void: st.flashes = clampf(st.flashes + d * 0.25, 0.0, 1.0))
	_row(I18n.t("options.quality"), func() -> String: return I18n.t("options.q%d" % int(st.quality)), func(d: int) -> void: st.quality = (int(st.quality) + (1 if d > 0 else 2)) % 3)
	_row(I18n.t("options.latency"), func() -> String: return "%d ms" % int(roundf(st.latency * 1000.0)), func(d: int) -> void: st.latency = clampf(st.latency + d * 0.005, -0.2, 0.3))
	_menu.add(I18n.t("options.calibrate"), _calibrate, {"size": 15, "width": 680, "value": "♪"})
	_row(I18n.t("options.lang"), func() -> String: return "Français" if st.lang == "fr" else "English", func(_d: int) -> void:
		st.lang = "en" if st.lang == "fr" else "fr"
		I18n.set_lang(st.lang)
		_build.call_deferred())
	_menu.add(I18n.t("options.controls"), _keys, {"size": 15, "width": 680, "value": "⌨"})
	_row(I18n.t("options.fullscreen"), func() -> String: return I18n.t("options.on") if st.fullscreen else I18n.t("options.off"), func(_d: int) -> void:
		st.fullscreen = not st.fullscreen
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN if st.fullscreen else DisplayServer.WINDOW_MODE_WINDOWED))
	_menu.add(I18n.t("common.back"), _on_back, {"size": 15, "width": 680})
	_relayout()

func _save() -> void:
	Meta.write_settings(_st)
	_on_change.call()

## Clicks on a steady 100 BPM; the player taps along; the median offset becomes the latency setting.
func _calibrate() -> void:
	_sub = "calib"
	_clear()
	Kit.h_title(_body, I18n.t("options.latency"))
	var h := Kit.hbox(_body, 12)
	var l := Kit.label(h, I18n.t("options.calibHelp"), 14, Kit.SOFT)
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.custom_minimum_size.x = 620
	_dot = Kit.rect(h, Kit.CYAN, Vector2.ZERO, Vector2(16, 16))
	_dot.custom_minimum_size = Vector2(16, 16)
	_dot.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	_dot.pivot_offset = Vector2(8, 8)
	_calib_t = 0.0
	_calib_next = 0.0
	_clicks.clear()
	_taps.clear()
	_relayout()

func _now() -> float:
	return Time.get_ticks_usec() / 1e6

func _keys() -> void:
	_sub = "keys"
	_clear()
	Kit.h_title(_body, I18n.t("options.controls"))
	_menu = UIMenu.new(_body)
	for a: String in Keys.BINDABLE:
		var it: MenuItem
		it = _menu.add(I18n.t("controls.%s" % a), func() -> void:
			it.set_value(I18n.t("options.press"))
			_capture = a
			_capture_item = it, {"size": 15, "width": 680, "value": _names(a)})
	_menu.add(I18n.t("controls.reset"), func() -> void:
		_st.bindings = null
		Keys.apply(null)
		_save()
		_keys(), {"size": 15, "width": 680})
	_menu.add(I18n.t("common.back"), _build, {"size": 15, "width": 680})
	_relayout()

func _names(a: String) -> String:
	var out: Array[String] = []
	for c: String in Keys.bindings[a]:
		out.append(Keys.code_name(c))
	return " / ".join(out)

func _input(e: InputEvent) -> void:
	if _capture == "":
		return
	if (e is InputEventKey and e.pressed and not e.echo) or (e is InputEventMouseButton and e.pressed):
		var code := Keys.code_of(e)
		if code == "":
			return
		get_viewport().set_input_as_handled()
		var a := _capture
		_capture = ""
		var list: Array = [code]
		for c: String in Keys.bindings[a]:
			if c != code:
				list.append(c)
		Keys.bindings[a] = list.slice(0, 2)
		_st.bindings = Keys.bindings.duplicate(true)
		Keys.apply(_st.bindings)
		_save()
		_capture_item.set_value(_names(a))

func update(dt: float) -> void:
	if _sub == "calib":
		_calib_t += dt
		if _calib_t > _calib_next and _clicks.size() < 16:
			Sound.sfx("calib")
			_clicks.append(_now() + AudioServer.get_output_latency())
			_calib_next += 0.6
			var tw := _dot.create_tween()
			tw.tween_property(_dot, "scale", Vector2(1.6, 1.6), 0.05)
			tw.tween_property(_dot, "scale", Vector2.ONE, 0.1)
		if Keys.pressed("dash") or Keys.pressed("confirm") or Keys.pressed("fire"):
			var tap := _now()
			var near := INF
			for c in _clicks:
				if absf(c - tap) < absf(near - tap):
					near = c
			if absf(near - tap) < 0.25:
				_taps.append(tap - near)
		if _clicks.size() >= 16 and _calib_t > _calib_next + 0.4:
			if _taps.size() >= 6:
				_taps.sort()
				_st.latency = roundf(_taps[_taps.size() / 2] * 1000.0) / 1000.0
				_save()
			_build()
		elif Keys.pressed("back"):
			_build()
		return
	if _capture != "":
		return
	if Keys.pressed("back"):
		if _sub == "keys":
			_build()
		else:
			_on_back.call()
		return
	_menu.handle()
