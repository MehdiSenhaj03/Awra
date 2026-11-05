class_name Overlays
## Small overlays: dialogue, unlock card, boss card, pause, confirm, the end choice.

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
		_who.text = I18n.t("names.%s" % who)
		_pt.texture = Kit.tex(Kit.PORTRAIT[who]) if Kit.PORTRAIT.has(who) else null
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
		if Keys.pressed("confirm") or Keys.pressed("use") or Keys.pressed("back"):
			_next()

class Unlock extends Screen:
	var _t := 0.0
	var _on_done: Callable

	func _init(kicker: String, name_: String, desc: String, icon: String, on_done: Callable) -> void:
		super()
		_on_done = on_done
		var p := Kit.panel(self)
		var m := Kit.margin(p, 46, 26, 46, 26)
		var v := Kit.vbox(m, 10)
		v.alignment = BoxContainer.ALIGNMENT_CENTER
		if icon != "":
			var i := Kit.image(v, icon, false)
			i.custom_minimum_size = Vector2(84, 84)
		for x in [[kicker, 12, Kit.GOLD, 500, 0.5, 0], [name_, 40, Color.WHITE, 800, 0.2, 1], [desc, 16, Color("#c9d3ea"), 400, 0.0, 2]]:
			if x[0] == "":
				continue
			var l := Kit.label(v, x[0], x[1], x[2], "display" if x[5] < 2 else "body", x[3], x[4], Kit.CYAN if x[5] == 1 else Color(0, 0, 0, 0), 14)
			l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
			if x[5] == 2:
				l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
				l.custom_minimum_size.x = 620
		p.ready.connect(func() -> void:
			p.position = Vector2((Kit.W - p.size.x) / 2, Kit.H * 0.18)
			p.pivot_offset = p.size / 2
			p.scale = Vector2(0.9, 0.9)
			var tw := p.create_tween()
			tw.tween_property(p, "scale", Vector2.ONE, 0.6).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT))
		Sound.sfx("fragment")

	func _gui_input(e: InputEvent) -> void:
		if e is InputEventMouseButton and e.pressed:
			_close()

	func _close() -> void:
		if _t < 0.6:
			return
		_t = -99.0
		_on_done.call()

	func update(dt: float) -> void:
		_t += dt
		if Keys.pressed("confirm") or Keys.pressed("use") or Keys.pressed("back"):
			_close()

## The Conductor's name across the screen (non-blocking).
class BossCard extends Control:
	var _t := 0.0
	var done := false
	var _stripe: ColorRect
	var _art: TextureRect
	var _nm: Label
	var _tt: Label

	func _init(id: String) -> void:
		Kit.full(self)
		mouse_filter = Control.MOUSE_FILTER_IGNORE
		_stripe = Kit.rect(self, Color(0, 0, 0, 0.85), Vector2(-100, Kit.H / 2 - 110), Vector2(Kit.W + 200, 220))
		_stripe.rotation = deg_to_rad(-4)
		_stripe.pivot_offset = _stripe.size / 2
		_art = Kit.image(self, "boss_%s" % id, false)
		_art.size = Vector2(Kit.H * 0.62, Kit.H * 0.62)
		_art.position = Vector2(Kit.W * 0.92 - _art.size.x, Kit.H / 2 - _art.size.y / 2)
		_nm = Kit.label(self, I18n.t("boss.%s.name" % id), 120, Color.WHITE, "display", 800, 0.25, Kit.MAGENTA, 30)
		_nm.position = Vector2(Kit.W * 0.08, Kit.H / 2 - 90)
		_tt = Kit.label(self, I18n.t("boss.%s.title" % id), 13, Kit.MAGENTA, "display", 500, 0.5)
		_tt.position = Vector2(Kit.W * 0.1, Kit.H / 2 + 72)
		update(0.0)

	func update(dt: float) -> void:
		_t += dt
		var t := _t / 3.4
		var a := clampf(t / 0.15, 0.0, 1.0) * (1.0 - clampf((t - 0.85) / 0.15, 0.0, 1.0))
		_stripe.modulate.a = clampf(t / 0.12, 0.0, 1.0) * (1.0 - clampf((t - 0.85) / 0.15, 0.0, 1.0))
		_stripe.scale.x = clampf(t / 0.12, 0.0, 1.0)
		_nm.modulate.a = a
		_tt.modulate.a = a
		var ka := clampf(t / 0.18, 0.0, 1.0)
		_art.modulate.a = ka * (1.0 - clampf((t - 0.85) / 0.15, 0.0, 1.0))
		_art.position.x = Kit.W * 0.92 - _art.size.x + 80.0 * (1.0 - ka) - 20.0 * clampf((t - 0.85) / 0.15, 0.0, 1.0)
		if _t > 3.5:
			done = true

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

## The last bar: end the symphony, or play forever.
class Choice extends Screen:
	var _menu: UIMenu

	func _init(on_coda: Callable, on_encore: Callable) -> void:
		super()
		dim(0.78)
		var v := Kit.vbox(self, 30)
		v.alignment = BoxContainer.ALIGNMENT_CENTER
		Kit.full(v)
		var t := Kit.h_title(v, I18n.t("story.choice.title"))
		t.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		var c := CenterContainer.new()
		c.mouse_filter = Control.MOUSE_FILTER_IGNORE
		v.add_child(c)
		_menu = UIMenu.new(c, true, 30)
		_menu.add(I18n.t("story.choice.coda"), on_coda, {"sub": I18n.t("story.choice.codaDesc"), "width": 420})
		_menu.add(I18n.t("story.choice.encore"), on_encore, {"sub": I18n.t("story.choice.encoreDesc"), "width": 420})

	func update(_dt: float) -> void:
		_menu.handle()
