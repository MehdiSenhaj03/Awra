class_name MapScreen
extends Screen
## The Score: six sectors, four movements each, grades earned.

var _nodes: Array[Dictionary] = []
var _index := 0
var _strip: Control
var _sectors: Array[Control] = []
var _on_pick: Callable
var _on_back: Callable
var _save: Dictionary
const CARD_W := 440.0
const GAP := 18.0

func _init(save: Dictionary, focus_id: String, on_pick: Callable, on_back: Callable) -> void:
	super()
	_save = save
	_on_pick = on_pick
	_on_back = on_back
	Kit.background(self, "bg_title", 5.0, 0.35)
	var head := Kit.vbox(self, 8)
	head.position = Vector2(Kit.W * 0.06, 46)
	Kit.h_title(head, I18n.t("map.title"))
	Kit.h_sub(head, I18n.t("map.subtitle").to_upper())
	var notes := Kit.hbox(self, 10)
	Kit.label(notes, I18n.t("common.notes").to_upper(), 12, Color("#c9a868"), "display", 500, 0.3)
	Kit.label(notes, Kit.fmt_int(save.notes), 20, Kit.GOLD, "mono", 400, 0.0, Color(1, 0.76, 0.29, 0.5), 10)
	notes.position = Vector2(Kit.W * 0.94 - 260, 52)
	notes.size = Vector2(260, 30)
	notes.alignment = BoxContainer.ALIGNMENT_END
	_strip = Control.new()
	_strip.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_strip.position = Vector2(Kit.W * 0.06, 170)
	add_child(_strip)
	var card_h := Kit.H - 170 - 110
	for s in range(1, 7):
		var sec := _sector(s, card_h)
		sec.position = Vector2((s - 1) * (CARD_W + GAP), 0)
		_strip.add_child(sec)
		_sectors.append(sec)
	var foot := Kit.hbox(self, 30)
	foot.position = Vector2(Kit.W * 0.06, Kit.H - 60)
	_keys(foot, ["←", "→"], I18n.t("map.subtitle"))
	_keys(foot, [I18n.t("keys.enter")], I18n.t("common.start"))
	_keys(foot, [I18n.t("keys.escape")], I18n.t("common.back"))
	# Focus: the requested stage, else the furthest open one.
	var fi := -1
	for i in _nodes.size():
		if _nodes[i].id == focus_id:
			fi = i
	if fi < 0:
		fi = 0
		for i in _nodes.size():
			if _nodes[i].open and not Meta.cleared(save, _nodes[i].id):
				fi = i
				break
	_focus(fi, true)

static func _keys(parent: Node, keys: Array, text: String) -> void:
	var h := Kit.hbox(parent, 6)
	for k: String in keys:
		var b := PanelContainer.new()
		var sb := StyleBoxFlat.new()
		sb.bg_color = Color(0, 0, 0, 0)
		sb.border_color = Color(1, 1, 1, 0.25)
		sb.set_border_width_all(1)
		sb.set_corner_radius_all(3)
		sb.content_margin_left = 7
		sb.content_margin_right = 7
		sb.content_margin_top = 1
		sb.content_margin_bottom = 1
		b.add_theme_stylebox_override("panel", sb)
		Kit.label(b, k, 12, Color.WHITE, "mono")
		h.add_child(b)
	Kit.label(h, text, 12, Kit.MUTED, "body", 400, 0.1)

func _sector(s: int, h: float) -> Control:
	var sc: Color = Kit.SECTOR_COLOR[s]
	var sec := Control.new()
	sec.size = Vector2(CARD_W, h)
	sec.clip_contents = true
	sec.mouse_filter = Control.MOUSE_FILTER_IGNORE
	sec.pivot_offset = sec.size / 2
	var art := Kit.image(sec, Kit.SECTOR_BG[s])
	art.size = sec.size
	art.pivot_offset = sec.size / 2
	var am := ShaderMaterial.new()
	am.shader = preload("res://src/ui/shaders/bg.gdshader")
	art.material = am
	sec.set_meta("art", art)
	var fade := TextureRect.new()
	var g := GradientTexture2D.new()
	var gr := Gradient.new()
	gr.offsets = PackedFloat32Array([0.0, 0.2, 0.75, 1.0])
	gr.colors = PackedColorArray([Color(0, 0, 0, 0), Color(0, 0, 0, 0), Color(0, 0, 0, 0.94), Color(0, 0, 0, 0.94)])
	g.gradient = gr
	g.fill_from = Vector2(0, 0)
	g.fill_to = Vector2(0, 1)
	g.width = 4
	g.height = 256
	fade.texture = g
	fade.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	fade.stretch_mode = TextureRect.STRETCH_SCALE
	fade.size = sec.size
	fade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	sec.add_child(fade)
	var info := Kit.vbox(sec, 0)
	info.position = Vector2(22, 0)
	info.size = Vector2(CARD_W - 44, 0)
	var emb := Kit.image(info, "ic_b%d" % (7 + s), false)
	emb.custom_minimum_size = Vector2(54, 54)
	emb.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
	Kit.label(info, "%02d / 06" % s, 12, sc, "mono", 400, 0.3).custom_minimum_size.y = 26
	var nm := Kit.label(info, I18n.t("sector.%d.name" % s), 24, PALE_C(), "display", 700, 0.08)
	nm.custom_minimum_size.y = 40
	var ds := Kit.label(info, I18n.t("sector.%d.desc" % s), 13, Color("#b8c3dc"))
	ds.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	ds.custom_minimum_size = Vector2(CARD_W - 44, 44)
	var row := Kit.hbox(info, 8)
	row.custom_minimum_size.y = 16
	var row2 := Kit.hbox(info, 8)
	var any_open := false
	for k in range(1, 5):
		var id := "s%d_%d" % [s, k]
		var rec: Variant = _save.stages.get(id)
		var open := Meta.stage_open(_save, id)
		any_open = any_open or open
		var got: bool = rec != null and bool(rec.cleared)
		var b := Boxed.new()
		b.custom_minimum_size = Vector2(0, 64)
		b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		b.bg_on = Color(sc.r, sc.g, sc.b, 0.22)
		b.border_on = sc
		b.lift = 3.0
		var gl := Kit.label(b, Meta.GRADES[int(rec.rank)] if got else "·", 22, Color.WHITE if got else Color(1, 1, 1, 0.25), "display", 800, 0.0, sc if got else Color(0, 0, 0, 0), 10)
		gl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		gl.position = Vector2(0, 6)
		gl.size = Vector2(90, 30)
		gl.set_anchors_preset(Control.PRESET_TOP_WIDE)
		var ll := Kit.label(b, I18n.t("map.boss").split(" ")[0] if k == 4 else "%d.%d" % [s, k], 10, Kit.MAGENTA if k == 4 else Kit.MUTED, "display", 500, 0.2)
		ll.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		ll.position = Vector2(0, 40)
		ll.set_anchors_preset(Control.PRESET_TOP_WIDE)
		if rec != null and bool(rec.get("fragment", false)):
			var fr := Kit.label(b, "◆", 10, Kit.GOLD)
			fr.position = Vector2(8, 4)
		if not open:
			b.modulate.a = 0.3
		var ni := _nodes.size()
		b.hovered.connect(func() -> void: _focus(ni))
		b.clicked.connect(func() -> void:
			_focus(ni)
			_pick())
		row2.add_child(b)
		_nodes.append({"id": id, "box": b, "open": open, "sector": sec, "s": s})
	if not any_open:
		(art.material as ShaderMaterial).set_shader_parameter("gray", 1.0)
		(art.material as ShaderMaterial).set_shader_parameter("bright", 0.35)
		info.modulate = Color(0.45, 0.45, 0.45)
	info.ready.connect(func() -> void:
		info.position.y = h - info.get_combined_minimum_size().y - 22)
	var border := Panel.new()
	var sb := StyleBoxFlat.new()
	sb.draw_center = false
	sb.border_color = Color(1, 1, 1, 0.11)
	sb.set_border_width_all(1)
	border.add_theme_stylebox_override("panel", sb)
	border.size = sec.size
	border.mouse_filter = Control.MOUSE_FILTER_IGNORE
	sec.add_child(border)
	sec.set_meta("border", sb)
	sec.set_meta("color", sc)
	return sec

static func PALE_C() -> Color:
	return Kit.PALE

func _focus(i: int, quiet: bool = false) -> void:
	if _index < _nodes.size():
		(_nodes[_index].box as Boxed).set_on(false)
	_index = clampi(i, 0, _nodes.size() - 1)
	var n := _nodes[_index]
	(n.box as Boxed).set_on(true)
	for sec in _sectors:
		var on: bool = sec == n.sector
		var sb: StyleBoxFlat = sec.get_meta("border")
		sb.border_color = sec.get_meta("color") if on else Color(1, 1, 1, 0.11)
		var art: Control = sec.get_meta("art")
		var tw := sec.create_tween().set_parallel()
		tw.tween_property(sec, "position:y", -6.0 if on else 0.0, 0.3)
		tw.tween_property(art, "scale", Vector2.ONE * (1.08 if on else 1.0), 6.0 if on else 0.6).set_ease(Tween.EASE_OUT)
	var s: int = n.s
	var tx := -maxf(0.0, s - 2) * (CARD_W + GAP) + Kit.W * 0.06
	var tw2 := _strip.create_tween()
	tw2.tween_property(_strip, "position:x", tx, 0.5).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
	if not quiet:
		Sound.sfx("ui", 1.0 + (_index % 4) * 0.06)

func _pick() -> void:
	var n := _nodes[_index]
	if not n.open:
		Sound.sfx("block")
		return
	Sound.sfx("uiOk")
	_on_pick.call(n.id)

func update(_dt: float) -> void:
	if Keys.pressed("left"):
		_focus(_index - 1)
	if Keys.pressed("right"):
		_focus(_index + 1)
	if Keys.pressed("up"):
		_focus(_index - 4)
	if Keys.pressed("down"):
		_focus(_index + 4)
	if Keys.pressed("confirm") or Keys.pressed("fire"):
		_pick()
	if Keys.pressed("back"):
		_on_back.call()
