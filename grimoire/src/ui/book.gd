class_name BookScreen
extends Screen
## The Grimoire: every glyph on its page — its form traced again and again, what it does, what it costs — and a
## blank page to practise on, with the recogniser's verdict and score. As a lesson, it asks for each new glyph
## to be drawn well twice before the chapter starts.

var _known: Array
var _lesson: Array = []         # glyphs to learn now (lesson mode)
var _done: Dictionary = {}      # glyph -> successes
var _sel := 0
var _on_close: Callable
var _pen: Pen
var _page: Control
var _list: VBoxContainer
var _name: Label
var _desc: Label
var _cost: Label
var _verdict: Label
var _progress: Label
var _t := 0.0
var _last: Dictionary = {}
var _items: Array[Control] = []
var _close_btn: MenuItem
var _menu: UIMenu

func _init(known: Array, lesson: Array, on_close: Callable) -> void:
	super()
	_known = known
	_lesson = lesson
	_on_close = on_close
	Kit.full(Kit.rect(self, Color(0.03, 0.02, 0.01, 0.97)))
	var book := Kit.image(self, "book_bg", true)
	Kit.full(book)
	book.modulate = Color(1, 1, 1, 0.0) if book.texture == null else Color(1, 1, 1, 1)
	_page = Control.new()
	Kit.full(_page)
	_page.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_page)
	_page.draw.connect(_draw_page)
	var title := Kit.label(self, I18n.t("book.lesson") if not lesson.is_empty() else I18n.t("book.title"), 56, Kit.GOLD, "deco", 400, 0.04, Color(0.4, 0.1, 0.02, 0.6), 14)
	title.position = Vector2(120, 50)
	_list = Kit.vbox(self, 2)
	_list.position = Vector2(120, 150)
	var glyphs: Array = lesson if not lesson.is_empty() else range(Glyphs.COUNT)
	for g: int in glyphs:
		var it := Control.new()
		it.custom_minimum_size = Vector2(420, 62)
		it.mouse_filter = Control.MOUSE_FILTER_STOP
		var gg := g
		it.gui_input.connect(func(e: InputEvent) -> void:
			if e is InputEventMouseButton and e.pressed and e.button_index == MOUSE_BUTTON_LEFT:
				_select(gg))
		it.set_meta("g", g)
		it.draw.connect(func() -> void: _draw_item(it, gg))
		_list.add_child(it)
		_items.append(it)
	var info := Kit.vbox(self, 10)
	info.position = Vector2(620, 600)
	info.custom_minimum_size.x = 560
	_name = Kit.label(info, "", 44, Kit.IVORY, "deco", 400, 0.03)
	_desc = Kit.label(info, "", 21, Kit.SOFT, "body", 600)
	_desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_desc.custom_minimum_size.x = 560
	_cost = Kit.label(info, "", 17, Color("#79b6ff"), "display", 700, 0.15)
	_verdict = Kit.label(self, I18n.t("book.practice"), 26, Kit.GOLD, "deco", 400, 0.03)
	_verdict.position = Vector2(1240, 880)
	_verdict.size = Vector2(560, 40)
	_verdict.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_progress = Kit.label(self, "", 18, Kit.BRASS, "display", 700, 0.2)
	_progress.position = Vector2(1240, 930)
	_progress.size = Vector2(560, 30)
	_progress.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	var bottom := Kit.vbox(self, 0)
	bottom.position = Vector2(120, 960)
	_menu = UIMenu.new(bottom)
	_close_btn = _menu.add(I18n.t("book.start") if not lesson.is_empty() else I18n.t("common.back"), _close, {"size": 22, "disabled": not lesson.is_empty()})
	_pen = Pen.new("mouse")
	_pen.area = Rect2(1240, 150, 560, 700)
	_pen.glyph.connect(_on_glyph)
	_select(glyphs[0])

func _close() -> void:
	if not _lesson.is_empty() and not _lesson_done():
		return
	_on_close.call()

func _lesson_done() -> bool:
	for g: int in _lesson:
		if int(_done.get(g, 0)) < 2:
			return false
	return true

func _select(g: int) -> void:
	_sel = g
	_t = 0.0
	var known := _known.has(g) or _lesson.has(g)
	_name.text = I18n.t("glyph.%s" % Glyphs.NAMES[g]) if known else "? ? ?"
	_desc.text = I18n.t("glyph_desc.%s" % Glyphs.NAMES[g]) if known else I18n.t("book.unknown")
	_cost.text = I18n.t("book.cost", {"n": Duel.COST[g] / 100}) if known else ""
	for it in _items:
		it.queue_redraw()
	_update_progress()

func _update_progress() -> void:
	if _lesson.is_empty():
		_progress.text = ""
		return
	var parts: Array[String] = []
	for g: int in _lesson:
		parts.append("%s %s" % [I18n.t("glyph.%s" % Glyphs.NAMES[g]), "✦✦" if int(_done.get(g, 0)) >= 2 else ("✦·" if int(_done.get(g, 0)) == 1 else "··")])
	_progress.text = "   ".join(parts)
	_close_btn.disabled = not _lesson_done()
	_close_btn._refresh()

func _on_glyph(strokes: Array, _center: Vector2, _size: float) -> void:
	var r := Glyphs.recognize(strokes)
	_last = r
	var g: int = r.g
	if g < 0:
		_verdict.text = I18n.t("book.fizzle")
		_verdict.label_settings.font_color = Color("#ff8a6a")
		Sound.sfx("fizzle")
		return
	_verdict.text = "%s — %d%%" % [I18n.t("glyph.%s" % Glyphs.NAMES[g]), int(roundf(float(r.score) * 100.0))]
	_verdict.label_settings.font_color = GlyphArt.COLORS[g].lightened(0.2)
	if g == _sel:
		_done[g] = int(_done.get(g, 0)) + 1
		Sound.sfx("g_%s" % Glyphs.NAMES[g])
		if not _lesson.is_empty() and int(_done[g]) >= 2:
			# Next glyph of the lesson.
			for lg: int in _lesson:
				if int(_done.get(lg, 0)) < 2:
					_select(lg)
					break
	else:
		Sound.sfx("ui", 0.8)
	_update_progress()

func _draw_item(it: Control, g: int) -> void:
	var on := g == _sel
	var known := _known.has(g) or _lesson.has(g)
	if on:
		it.draw_rect(Rect2(Vector2.ZERO, it.size), Color(1.0, 0.75, 0.35, 0.12))
		it.draw_rect(Rect2(0, 6, 4, it.size.y - 12), Kit.GOLD)
	var col: Color = GlyphArt.COLORS[g] if known else Color(1, 1, 1, 0.2)
	GlyphArt.draw(it, g, Vector2(40, 31), 42, col, 2.5, 1.0, false)
	var f := Kit.font("deco", 400, 0.0, 24)
	it.draw_string(f, Vector2(84, 40), I18n.t("glyph.%s" % Glyphs.NAMES[g]) if known else "· · ·", HORIZONTAL_ALIGNMENT_LEFT, -1, 24, Kit.IVORY if known else Color(1, 1, 1, 0.3))
	if _lesson.has(g):
		it.draw_string(Kit.font("body", 700, 0.0, 18), Vector2(330, 40), "✦".repeat(mini(2, int(_done.get(g, 0)))), HORIZONTAL_ALIGNMENT_LEFT, -1, 18, Kit.GOLD)

func _draw_page() -> void:
	var ci := _page
	# The selected glyph, traced over and over.
	var known := _known.has(_sel) or _lesson.has(_sel)
	var c := Vector2(900, 360)
	ci.draw_circle(c, 230, Color(0.1, 0.05, 0.02, 0.5))
	ci.draw_arc(c, 230, 0, TAU, 64, Color(0.9, 0.7, 0.35, 0.5), 2.0, true)
	if known:
		var k := fmod(_t / 2.2, 1.0)
		GlyphArt.draw(ci, _sel, c, 300, Color(GlyphArt.COLORS[_sel].r, GlyphArt.COLORS[_sel].g, GlyphArt.COLORS[_sel].b, 0.25), 6.0, 1.0, false)
		GlyphArt.draw(ci, _sel, c, 300, GlyphArt.COLORS[_sel], 6.0, ease(k * 1.25, 0.8))
	# The practice page.
	var r := _pen.area
	ci.draw_rect(r, Color(0.92, 0.86, 0.72, 0.1))
	ci.draw_rect(r, Color(0.9, 0.7, 0.35, 0.6), false, 2.0)
	var f := Kit.font("body", 600, 0.0, 18)
	ci.draw_string(f, r.position + Vector2(20, 34), I18n.t("book.draw_here"), HORIZONTAL_ALIGNMENT_LEFT, -1, 18, Color(1, 0.9, 0.7, 0.6))
	GlyphArt.ink(ci, _pen.strokes, Color("#ffd27a"), 8.0)

func update(dt: float) -> void:
	_t += dt
	_pen.update(dt, get_viewport().get_mouse_position())
	_page.queue_redraw()
	if Keys.pressed("back") or Keys.pressed("book"):
		_close()
		return
	if Keys.pressed("down") or Keys.pressed("up"):
		var glyphs: Array = _lesson if not _lesson.is_empty() else range(Glyphs.COUNT)
		var i := glyphs.find(_sel)
		i = (i + (1 if Keys.pressed("down") else -1) + glyphs.size()) % glyphs.size()
		_select(glyphs[i])
	if Keys.pressed("confirm"):
		_close()
