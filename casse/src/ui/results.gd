class_name ResultsScreen
extends Screen
## The heist is done: the medal drops, the numbers come in, the replay code can be shared.

const MEDAL_COLOR := [Color("#7d8aa8"), Color("#c98a4b"), Color("#d8dde8"), Color("#ffc34a")]
var _menu: UIMenu

func _init(r: Dictionary, record: bool, actions: Array) -> void:
	super()
	dim(0.82)
	var wrap := Kit.hbox(self, 80)
	wrap.alignment = BoxContainer.ALIGNMENT_CENTER
	Kit.full(wrap)
	var mb := Kit.vbox(wrap, 6)
	mb.alignment = BoxContainer.ALIGNMENT_CENTER
	var medal: int = r.medal
	var m := MedalBadge.new(MEDAL_COLOR[medal], medal)
	mb.add_child(m)
	var ml := Kit.label(mb, I18n.t("results.m%d" % medal).to_upper(), 26, MEDAL_COLOR[medal], "display", 400, 0.2)
	ml.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	var list := Kit.vbox(wrap, 0)
	list.custom_minimum_size.x = 560
	list.alignment = BoxContainer.ALIGNMENT_CENTER
	Kit.label(list, I18n.t("results.title"), 50, Kit.CREAM, "display", 400, 0.1, Color(1, 0.7, 0.3, 0.4), 16)
	Kit.rect(list, Color(0, 0, 0, 0)).custom_minimum_size.y = 14
	var f: int = r.frame
	var rows := [
		[I18n.t("results.echoes"), "%d" % r.echoes, true],
		[I18n.t("results.par"), "%d" % r.par if int(r.par) > 0 else "—", false],
		[I18n.t("results.time"), "%d.%02d s" % [f / 60, (f % 60) * 100 / 60], false],
		[I18n.t("results.loops"), "%d" % r.loops, false],
	]
	if int(r.gems_total) > 0:
		rows.append([I18n.t("results.gems"), "%d / %d" % [r.gems, r.gems_total], false])
	for i in rows.size():
		var rr: Array = rows[i]
		var row := Control.new()
		row.custom_minimum_size = Vector2(560, 64 if rr[2] else 40)
		row.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var k := Kit.label(row, rr[0], 17, Kit.SOFT, "body", 400)
		k.position = Vector2(0, 22 if rr[2] else 9)
		var v := Kit.label(row, rr[1], 38 if rr[2] else 18, Kit.CREAM, "mono", 600, 0.0, Color(1, 0.75, 0.3, 0.5) if rr[2] else Color(0, 0, 0, 0), 14)
		v.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		v.position = Vector2(200, 4 if rr[2] else 8)
		v.size = Vector2(360, 40)
		Kit.rect(row, Color(1, 1, 1, 0.07), Vector2(0, row.custom_minimum_size.y - 1), Vector2(560, 1))
		list.add_child(row)
		row.modulate.a = 0.0
		var tw := row.create_tween()
		tw.tween_interval(0.3 + i * 0.1)
		tw.tween_property(row, "modulate:a", 1.0, 0.3)
	if medal < 3 and int(r.par) > 0:
		Kit.label(list, I18n.t("results.gold_hint", {"n": r.par}), 14, Kit.BRASS, "body", 600, 0.1)
	if record:
		var rec := Kit.label(list, I18n.t("results.record"), 15, Kit.GOLD, "body", 700, 0.4)
		var bl := rec.create_tween().set_loops()
		bl.tween_property(rec, "modulate:a", 0.3, 0.5)
		bl.tween_property(rec, "modulate:a", 1.0, 0.5)
	Kit.rect(list, Color(0, 0, 0, 0)).custom_minimum_size.y = 22
	_menu = UIMenu.new(list)
	for a: Dictionary in actions:
		_menu.add(a.label, a.action, {"size": 18})
	Sound.sfx("medal")

func update(_dt: float) -> void:
	_menu.handle()

## A brass medal with the number of its stars.
class MedalBadge extends Control:
	var color: Color
	var level := 0
	var _t := 0.0
	func _init(c: Color, l: int) -> void:
		color = c
		level = l
		custom_minimum_size = Vector2(260, 260)
		mouse_filter = Control.MOUSE_FILTER_IGNORE
	func _process(dt: float) -> void:
		_t += dt
		queue_redraw()
	func _draw() -> void:
		var k := clampf((_t - 0.2) / 0.5, 0.0, 1.0)
		var s := 1.0 + (1.0 - k) * 1.5
		var c := Vector2(130, 130)
		var a := k
		draw_circle(c, 108 * s, Color(color.r, color.g, color.b, 0.12 * a))
		draw_circle(c, 92 * s, Color(color.r * 0.5, color.g * 0.5, color.b * 0.5, a))
		draw_arc(c, 92 * s, 0, TAU, 64, Color(color.r, color.g, color.b, a), 6.0)
		draw_arc(c, 78 * s, 0, TAU, 64, Color(color.r, color.g, color.b, 0.6 * a), 2.0)
		for i in 12:
			var ang := i * TAU / 12.0
			draw_line(c + Vector2(cos(ang), sin(ang)) * 64 * s, c + Vector2(cos(ang), sin(ang)) * 74 * s, Color(color.r, color.g, color.b, 0.7 * a), 2.0)
		for i in 3:
			var on := i < level
			var p := c + Vector2((i - 1) * 40, 0) * s
			var pts := PackedVector2Array()
			for j in 10:
				var ang := -PI / 2 + j * TAU / 10.0
				var r := (20.0 if j % 2 == 0 else 8.5) * s
				pts.append(p + Vector2(cos(ang), sin(ang)) * r)
			draw_colored_polygon(pts, Color(color.r, color.g, color.b, a) if on else Color(1, 1, 1, 0.15 * a))
