class_name ResultsScreen
extends Screen
## End of a stage: the grade slams in, the numbers roll.

const GRADE_COLOR := [Color("#7d8aa8"), Color("#7fb8ff"), Color("#2ef2ff"), Color("#40ffb0"), Color("#ffc34a"), Color("#ff2fb9")]
var _menu: UIMenu

func _init(w: World, win: bool, grade: int, record: bool, notes: int, actions: Array) -> void:
	super()
	var bgc := Kit.full(Kit.rect(self, Color(0.01, 0.02, 0.05, 0.9)))
	bgc.color = Color(0.01, 0.015, 0.04, 0.9)
	var wrap := Kit.hbox(self, 60)
	wrap.alignment = BoxContainer.ALIGNMENT_CENTER
	Kit.full(wrap)
	var gc: Color = GRADE_COLOR[grade] if win else Color("#ff3050")
	var gbox := Kit.vbox(wrap, 0)
	gbox.alignment = BoxContainer.ALIGNMENT_CENTER
	var g := Kit.label(gbox, Meta.GRADES[grade] if win else "✕", 220, Color.WHITE, "display", 800, 0.0, gc, 40)
	g.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	var gm := ShaderMaterial.new()
	gm.shader = preload("res://src/ui/shaders/gradient_text.gdshader")
	gm.set_shader_parameter("vertical", 1.0)
	gm.set_shader_parameter("c0", Color.WHITE)
	gm.set_shader_parameter("c1", gc)
	gm.set_shader_parameter("size", Vector2(300, 260))
	g.material = gm
	if win:
		var sm := Kit.label(gbox, I18n.t("results.grade"), 13, Kit.MUTED, "display", 500, 0.6)
		sm.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	g.modulate.a = 0.0
	g.pivot_offset = Vector2(150, 130)
	g.ready.connect(func() -> void:
		g.pivot_offset = g.size / 2
		g.scale = Vector2(3, 3)
		g.rotation = deg_to_rad(-10)
		var tw := g.create_tween().set_parallel()
		tw.tween_property(g, "modulate:a", 1.0, 0.4).set_delay(0.9)
		tw.tween_property(g, "scale", Vector2.ONE, 0.9).set_delay(0.9).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
		tw.tween_property(g, "rotation", 0.0, 0.9).set_delay(0.9).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT))
	var list := Kit.vbox(wrap, 0)
	list.custom_minimum_size.x = 520
	list.alignment = BoxContainer.ALIGNMENT_CENTER
	Kit.h_title(list, I18n.t("results.clear") if win else I18n.t("results.fail"), 34)
	Kit.rect(list, Color(0, 0, 0, 0), Vector2.ZERO, Vector2(0, 12)).custom_minimum_size.y = 12
	var s := w.stats
	var sec := int(roundf(s.frames / 60.0))
	var judged: int = s.perfects + s.greats + s.offs
	var rows := [
		[I18n.t("results.score"), Kit.fmt_int(w.score), true],
		[I18n.t("results.time"), "%d:%02d" % [sec / 60, sec % 60], false],
		[I18n.t("results.perfects"), str(s.perfects) + ((" (%d %%)" % int(roundf(float(s.perfects) / judged * 100.0))) if judged > 0 else ""), false],
		[I18n.t("results.parries"), str(s.parries), false],
		[I18n.t("results.grazes"), str(s.grazes), false],
		[I18n.t("results.hits"), str(s.hits), false],
		[I18n.t("results.maxRank"), "%s — %s" % [World.RANKS[s.max_rank], I18n.t("rank.%d" % s.max_rank)], false],
		[I18n.t("results.notes"), "+%d ♪" % notes, false],
	]
	for i in rows.size():
		var r: Array = rows[i]
		var row := Control.new()
		row.custom_minimum_size = Vector2(520, 60 if r[2] else 38)
		row.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var k := Kit.label(row, r[0], 15, Kit.SOFT)
		k.position = Vector2(0, 8 if not r[2] else 18)
		var v := Kit.label(row, r[1], 34 if r[2] else 15, Color.WHITE, "mono", 700 if r[2] else 400, 0.0, Kit.CYAN if r[2] else Color(0, 0, 0, 0), 16)
		v.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		v.position = Vector2(200, 4 if r[2] else 8)
		v.size = Vector2(320, 30)
		Kit.rect(row, Color(1, 1, 1, 0.07), Vector2(0, row.custom_minimum_size.y - 1), Vector2(520, 1))
		list.add_child(row)
		row.modulate.a = 0.0
		var tw := row.create_tween()
		tw.tween_interval(0.15 + i * 0.09)
		tw.tween_property(row, "modulate:a", 1.0, 0.3)
	if record:
		var rec := Kit.label(list, I18n.t("results.record"), 13, Kit.GOLD, "display", 500, 0.4)
		var bl := rec.create_tween().set_loops()
		bl.tween_property(rec, "modulate:a", 0.25, 0.5)
		bl.tween_property(rec, "modulate:a", 1.0, 0.5)
	Kit.rect(list, Color(0, 0, 0, 0)).custom_minimum_size.y = 26
	_menu = UIMenu.new(list)
	for a: Dictionary in actions:
		_menu.add(a.label, a.action)
	if win:
		get_tree_timer(0.9, func() -> void: Sound.sfx("rankUp", 1.2))

func get_tree_timer(t: float, f: Callable) -> void:
	ready.connect(func() -> void: get_tree().create_timer(t).timeout.connect(f))

func update(_dt: float) -> void:
	_menu.handle()
