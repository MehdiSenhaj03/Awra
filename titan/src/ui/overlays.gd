class_name Overlays
## Small overlays: dialogue, pause, confirm.

class Dialogue extends Screen:
	var _lines: Array
	var _on_done: Callable
	var _i := 0
	var _who: Label
	var _txt: Label
	var _pt: TextureRect
	var _box: Control
	var _shown := 0.0
	var done := false

	func _init(lines: Array, on_done: Callable) -> void:
		super()
		_lines = lines
		_on_done = on_done
		mouse_filter = Control.MOUSE_FILTER_STOP
		_box = Control.new()
		_box.mouse_filter = Control.MOUSE_FILTER_IGNORE
		_box.size = Vector2(1100, 270)
		_box.position = Vector2((Kit.W - 1100) / 2, Kit.H * 0.93 - 270)
		add_child(_box)
		var p := Kit.panel(_box)
		p.position = Vector2(200, 130)
		p.size = Vector2(900, 140)
		var m := Kit.margin(p, 54, 22, 30, 26)
		var v := Kit.vbox(m, 10)
		_who = Kit.label(v, "", 13, Kit.CYAN, "display", 700, 0.35)
		_txt = Kit.label(v, "", 20, Color("#eef4ff"))
		_txt.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		_txt.custom_minimum_size.x = 800
		var nxt := Kit.label(p, "▶", 11, Kit.MUTED)
		nxt.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
		nxt.position = Vector2(870, 112)
		_pt = Kit.image(_box, "pt_volta", false)
		_pt.size = Vector2(230, 260)
		_pt.position = Vector2(0, 10)
		_pt.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		_show()

	func _gui_input(e: InputEvent) -> void:
		if e is InputEventMouseButton and e.pressed:
			_next()

	func _show() -> void:
		var ln: Array = _lines[_i]
		var who: String = ln[0]
		_who.text = I18n.t("names.%s" % who) if I18n.has("names.%s" % who) else who
		_pt.texture = Kit.tex(ln[2]) if ln.size() > 2 else (Kit.tex(Kit.PORTRAIT[who]) if Kit.PORTRAIT.has(who) else null)
		_txt.text = ln[1]
		_txt.visible_characters = 0
		_shown = 0.0
		_box.modulate.a = 0.0
		_box.position.y = Kit.H * 0.93 - 270 + 30
		var tw := create_tween().set_parallel()
		tw.tween_property(_box, "modulate:a", 1.0, 0.35)
		tw.tween_property(_box, "position:y", Kit.H * 0.93 - 270, 0.35).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
		Sound.sfx("ui", 0.8)

	func _next() -> void:
		var full := _txt.text.length()
		if _shown < full:
			_shown = full
			_txt.visible_characters = -1
			return
		_i += 1
		if _i >= _lines.size():
			done = true
			_on_done.call()
			return
		_show()

	func update(dt: float) -> void:
		var full := _txt.text.length()
		if _shown < full:
			_shown = minf(full, _shown + dt * 70.0)
			_txt.visible_characters = int(_shown)
		if Keys.pressed("confirm") or Keys.pressed("back"):
			_next()

class Pause extends Screen:
	var _menu: UIMenu
	var _resume: Callable

	func _init(resume: Callable, restart: Callable, options: Callable, quit: Callable) -> void:
		super()
		dim(0.66)
		_resume = resume
		var p := Kit.panel(self)
		var m := Kit.margin(p, 40, 36, 40, 36)
		var v := Kit.vbox(m, 22)
		Kit.h_title(v, I18n.t("pause.title"))
		_menu = UIMenu.new(v)
		_menu.add(I18n.t("pause.resume"), resume, {"size": 15, "width": 680})
		_menu.add(I18n.t("pause.restart"), restart, {"size": 15, "width": 680})
		_menu.add(I18n.t("pause.options"), options, {"size": 15, "width": 680})
		_menu.add(I18n.t("pause.quit"), quit, {"size": 15, "width": 680})
		_center(p)

	func _center(p: Control) -> void:
		p.ready.connect(func() -> void: p.position = (Vector2(Kit.W, Kit.H) - p.size) / 2)

	func update(_dt: float) -> void:
		if Keys.pressed("pause") or Keys.pressed("back"):
			_resume.call()
			return
		_menu.handle()

class Confirm extends Screen:
	var _menu: UIMenu
	var _no: Callable

	func _init(question: String, yes: Callable, no: Callable) -> void:
		super()
		dim(0.7)
		_no = no
		var p := Kit.panel(self)
		var m := Kit.margin(p, 40, 36, 40, 36)
		var v := Kit.vbox(m, 22)
		var q := Kit.label(v, question, 15, Color("#c3cde6"))
		q.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		q.custom_minimum_size.x = 680
		_menu = UIMenu.new(v)
		_menu.add(I18n.t("common.no"), no, {"size": 15, "width": 680})
		_menu.add(I18n.t("common.yes"), yes, {"size": 15, "width": 680})
		p.ready.connect(func() -> void: p.position = (Vector2(Kit.W, Kit.H) - p.size) / 2)

	func update(_dt: float) -> void:
		if Keys.pressed("back"):
			_no.call()
			return
		_menu.handle()
