class_name MapScreen
extends Screen
## The journey map on parchment: the path of the nine legs over three generations to the Source, the lands
## already crossed, Ama's state (health, scars, supplies), the crew of this generation and their gifts, and the
## crossroads: the lands to choose between (or the wild titan waiting).

const NODES := [Vector2(0.06, 0.82), Vector2(0.12, 0.7), Vector2(0.19, 0.6), Vector2(0.27, 0.5), Vector2(0.33, 0.62), Vector2(0.4, 0.52),
	Vector2(0.46, 0.38), Vector2(0.5, 0.52), Vector2(0.55, 0.38), Vector2(0.58, 0.24), Vector2(0.62, 0.12)]

var run: Dictionary
var _menu: UIMenu
var _pick: Callable
var _back: Callable
var _art: TextureRect
var _desc: Label
var _choices: Array = []
var _t := 0.0
var _canvas: Control
var _shown := -1

func _init(r: Dictionary, pick: Callable, back: Callable) -> void:
	super()
	run = r
	_pick = pick
	_back = back
	Kit.background(self, "map", 0.0, 0.92)
	_canvas = Control.new()
	Kit.full(_canvas)
	_canvas.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_canvas)
	_canvas.draw.connect(_draw_map)
	var shade := Kit.rect(self, Color(0.05, 0.035, 0.025, 0.86), Vector2(1300, 0), Vector2(620, Kit.H))
	shade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var box := Kit.vbox(self, 10)
	box.position = Vector2(1340, 40)
	box.custom_minimum_size.x = 540
	var g := Meta.gen(run)
	Kit.label(box, I18n.t("map.title"), 40, Kit.GOLD, "display", 700, 0.08)
	Kit.label(box, I18n.t("gen.%d" % g) + "  ·  " + I18n.t("map.leg_of", {"n": int(run.leg) + 1}), 16, Kit.BRASS, "display", 400, 0.12)
	# Ama.
	var ama := Kit.hbox(box, 14)
	var hpk := float(run.hp) / March.HP
	var mx := float(Meta.max_hp(run)) / March.HP
	var bar := Control.new()
	bar.custom_minimum_size = Vector2(360, 18)
	bar.draw.connect(func() -> void:
		bar.draw_rect(Rect2(0, 0, 360, 18), Color(0, 0, 0, 0.6))
		bar.draw_rect(Rect2(0, 0, 360 * hpk, 18), Color("#7fbf6a"))
		bar.draw_rect(Rect2(360 * mx, 0, 360 * (1.0 - mx), 18), Color(0.3, 0.08, 0.05, 0.95))
		bar.draw_rect(Rect2(0, 0, 360, 18), Color(1, 0.85, 0.6, 0.35), false, 1.0))
	Kit.label(ama, I18n.t("hud.ama"), 15, Kit.IVORY, "display", 700, 0.1)
	ama.add_child(bar)
	Kit.label(box, I18n.t("map.ama_state", {"hp": int(run.hp), "max": Meta.max_hp(run), "sup": int(run.supplies)}), 14, Kit.SOFT)
	# The crew.
	Kit.label(box, I18n.t("map.crew"), 14, Kit.BRASS, "display", 400, 0.3)
	for k in 4:
		var row := Kit.hbox(box, 12)
		var pt := Kit.image(row, Meta.crew_pt(g, k), true)
		pt.custom_minimum_size = Vector2(62, 62)
		var v := Kit.vbox(row, 0)
		Kit.label(v, Meta.crew_name(run, k) + "  ·  " + I18n.t("post.%d" % Meta.LINES[k].role), 16, Deck.LINEAGE[k].lightened(0.4), "display", 700, 0.06)
		var gift: String = run.gifts[k]
		var gl := Kit.label(v, I18n.t("gift.%s.name" % gift) + " — " + I18n.t("gift.%s.desc" % gift), 13, Kit.SOFT)
		gl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		gl.custom_minimum_size.x = 450
	# The crossroads.
	var l := Meta.leg(int(run.leg))
	_art = Kit.image(box, "", true)
	_art.custom_minimum_size = Vector2(540, 200)
	_desc = Kit.label(box, "", 14, Kit.IVORY)
	_desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_desc.custom_minimum_size = Vector2(540, 60)
	_menu = UIMenu.new(box, false, 2)
	if l.has("boss"):
		_choices = [l.choices[0]]
		_menu.add(I18n.t("map.face", {"name": I18n.t("boss.%s.name" % l.boss)}), func() -> void: _pick.call(l.choices[0]), {"size": 20, "width": 540})
	else:
		for kind: String in l.choices:
			var k := kind
			_choices.append(k)
			_menu.add(I18n.t("map.go", {"land": I18n.t("land.%s.name" % k)}), func() -> void: _pick.call(k), {"size": 20, "width": 540})
	var two: bool = app.settings.get("coop_campaign", false) if app else false
	var it := _menu.add(I18n.t("map.players"), Callable(), {"size": 15, "width": 540, "value": I18n.t("setup.players%d" % (1 if two else 0))})
	var toggle := func(_d: int = 0) -> void:
		app.settings.coop_campaign = not app.settings.get("coop_campaign", false)
		Meta.write_settings(app.settings)
		it.set_value(I18n.t("setup.players%d" % (1 if app.settings.coop_campaign else 0)))
	it.action = toggle
	it.adjust = toggle
	_menu.add(I18n.t("common.back"), _back, {"size": 15, "width": 540})

func _ready() -> void:
	# The players toggle reads the settings through the app, set after _init.
	var two: bool = app.settings.get("coop_campaign", false) if app else false
	_menu.items[_choices.size()].set_value(I18n.t("setup.players%d" % (1 if two else 0)))

func _show(i: int) -> void:
	var l := Meta.leg(int(run.leg))
	if i >= _choices.size():
		return
	var k: String = _choices[i]
	if l.has("boss"):
		_art.texture = Kit.tex("wild_titan")
		_art.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		_desc.text = I18n.t("boss.%s.desc" % l.boss)
	else:
		_art.texture = Kit.tex("vista_%s" % k)
		_art.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
		_desc.text = I18n.t("land.%s.desc" % k)

func _pos(i: int) -> Vector2:
	var n: Vector2 = NODES[i]
	return Vector2(n.x * Kit.W, n.y * Kit.H)

func _draw_map() -> void:
	var ci := _canvas
	var cur := int(run.leg)
	var ink := Color(0.25, 0.14, 0.07, 0.85)
	for i in NODES.size() - 1:
		var a := _pos(i)
		var b := _pos(i + 1)
		var done := i < cur
		if done:
			ci.draw_line(a, b, Color(0.55, 0.15, 0.08, 0.9), 5.0)
		else:
			var d := b - a
			var n := int(d.length() / 18.0)
			for s in n:
				if s % 2 == 0:
					ci.draw_line(a + d * (float(s) / n), a + d * (float(s + 1) / n), ink, 3.0)
	var font := Kit.font("display", 700, 0.05, 18)
	for i in NODES.size():
		var p := _pos(i)
		if i == 0:
			ci.draw_circle(p, 10, ink)
			continue
		if i == NODES.size() - 1:
			ci.draw_circle(p, 18, Color(0.2, 0.55, 0.65, 0.9))
			ci.draw_arc(p, 24 + 3 * sin(_t * 2.0), 0, TAU, 32, Color(0.3, 0.7, 0.8, 0.8), 2.0)
			ci.draw_string_outline(font, p + Vector2(28, 8), I18n.t("map.source"), HORIZONTAL_ALIGNMENT_LEFT, -1, 22, 5, Color(0.95, 0.88, 0.72, 0.9))
			ci.draw_string(font, p + Vector2(28, 8), I18n.t("map.source"), HORIZONTAL_ALIGNMENT_LEFT, -1, 22, Color(0.15, 0.3, 0.35))
			continue
		var leg := Meta.leg(i - 1)
		var boss := leg.has("boss")
		var col := Color(0.6, 0.12, 0.08) if boss else ink
		var r := 15.0 if boss else 11.0
		if i - 1 < cur:
			ci.draw_circle(p, r, col)
		else:
			ci.draw_circle(p, r, Color(0.93, 0.86, 0.7, 0.9))
			ci.draw_arc(p, r, 0, TAU, 24, col, 3.0)
		if boss:
			ci.draw_arc(p + Vector2(-r, -r * 0.4), r * 0.7, PI * 0.6, PI * 1.6, 10, col, 3.0)
			ci.draw_arc(p + Vector2(r, -r * 0.4), r * 0.7, -PI * 0.6, PI * 0.4, 10, col, 3.0)
		if i - 1 == cur:
			ci.draw_arc(p, r + 8 + 3 * sin(_t * 4.0), 0, TAU, 32, Color(0.85, 0.5, 0.1, 0.95), 3.0)
		# The land chosen, for the legs behind.
		if i - 1 < cur and i - 1 < (run.kinds as Array).size():
			var lk: String = run.kinds[i - 1]
			ci.draw_string_outline(font, p + Vector2(18, -12), I18n.t("land.%s.short" % lk), HORIZONTAL_ALIGNMENT_LEFT, -1, 15, 4, Color(0.95, 0.88, 0.72, 0.8))
			ci.draw_string(font, p + Vector2(18, -12), I18n.t("land.%s.short" % lk), HORIZONTAL_ALIGNMENT_LEFT, -1, 15, ink)
	# Generations.
	for gi in 3:
		var p := _pos(gi * 3 + 2) + Vector2(-30, 52)
		var s: String = ["I", "II", "III"][gi]
		ci.draw_string_outline(font, p, s, HORIZONTAL_ALIGNMENT_LEFT, -1, 30, 5, Color(0.95, 0.88, 0.72, 0.8))
		ci.draw_string(font, p, s, HORIZONTAL_ALIGNMENT_LEFT, -1, 30, Color(0.45, 0.2, 0.1, 0.9))
	# Ama on the path: a little titan with her village.
	var at := _pos(cur).lerp(_pos(mini(cur + 1, NODES.size() - 1)), 0.18 + 0.04 * sin(_t * 1.5))
	ci.draw_rect(Rect2(at + Vector2(-16, -22), Vector2(32, 14)), Color(0.35, 0.3, 0.26))
	ci.draw_line(at + Vector2(-12, -8), at + Vector2(-14, 4), Color(0.3, 0.26, 0.22), 4.0)
	ci.draw_line(at + Vector2(12, -8), at + Vector2(14, 4), Color(0.3, 0.26, 0.22), 4.0)
	ci.draw_circle(at + Vector2(20, -18), 6, Color(0.35, 0.3, 0.26))
	ci.draw_line(at + Vector2(-4, -22), at + Vector2(-4, -36), ink, 2.0)
	ci.draw_colored_polygon(PackedVector2Array([at + Vector2(-4, -36), at + Vector2(8, -32), at + Vector2(-4, -27)]), Color(0.7, 0.2, 0.1))

func update(dt: float) -> void:
	_t += dt
	_canvas.queue_redraw()
	if Keys.pressed("back"):
		_back.call()
		return
	_menu.handle()
	if _menu.index != _shown:
		_shown = _menu.index
		_show(_menu.index)
