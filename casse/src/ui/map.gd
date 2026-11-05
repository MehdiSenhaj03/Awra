class_name MapScreen
extends Screen
## Valmont: five districts, their vaults and medals. Pick a vault (campaign or co-op).

var _save: Dictionary
var _on_pick: Callable
var _on_back: Callable
var _nodes: Array[Dictionary] = []
var _index := 0
var _strip: Control
var _cards: Array[Control] = []
var _detail_name: Label
var _detail_sub: Label
var _detail_desc: Label
const CARD_W := 330.0
const GAP := 16.0

func _init(save: Dictionary, focus_id: String, on_pick: Callable, on_back: Callable, title_key: String = "map.title") -> void:
	super()
	_save = save
	_on_pick = on_pick
	_on_back = on_back
	Kit.background(self, "bg_title", 4.5, 0.32)
	var head := Kit.vbox(self, 6)
	head.position = Vector2(Kit.W * 0.05, 40)
	Kit.label(head, I18n.t(title_key), 56, Kit.CREAM, "display", 400, 0.1, Color(1, 0.7, 0.3, 0.35), 18)
	Kit.label(head, I18n.t("map.sub").to_upper(), 14, Kit.BRASS, "body", 600, 0.4)
	var total := 0
	var got := 0
	for id: String in Meta.order():
		total += 3
		got += Meta.medal(save, id)
	var med := Kit.label(self, "%d / %d  %s" % [got, total, I18n.t("map.medals")], 18, Kit.GOLD, "mono", 600)
	med.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	med.position = Vector2(Kit.W * 0.95 - 400, 60)
	med.size = Vector2(400, 30)
	_strip = Control.new()
	_strip.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_strip.position = Vector2(Kit.W * 0.05, 180)
	add_child(_strip)
	var h := 640.0
	for di in Meta.DISTRICTS.size():
		var d: Dictionary = Meta.DISTRICTS[di]
		var card := _district(d, h)
		card.position = Vector2(di * (CARD_W + GAP), 0)
		_strip.add_child(card)
		_cards.append(card)
	var dp := Kit.panel(self)
	dp.position = Vector2(Kit.W * 0.05, 180 + h + 24)
	dp.custom_minimum_size = Vector2(Kit.W * 0.9, 150)
	var m := Kit.margin(dp, 28, 18, 28, 18)
	var v := Kit.vbox(m, 6)
	_detail_name = Kit.label(v, "", 30, Kit.CREAM, "deco", 400, 0.04)
	_detail_sub = Kit.label(v, "", 15, Kit.BRASS, "body", 600, 0.2)
	_detail_desc = Kit.label(v, "", 16, Kit.SOFT)
	var fi := 0
	for i in _nodes.size():
		if _nodes[i].id == focus_id:
			fi = i
	if focus_id == "":
		for i in _nodes.size():
			if _nodes[i].open and not Meta.done(save, _nodes[i].id):
				fi = i
				break
	_focus(fi, true)

func _district(d: Dictionary, h: float) -> Control:
	var did: int = d.id
	var col: Color = Kit.DISTRICT_COLOR[did]
	var card := Control.new()
	card.size = Vector2(CARD_W, h)
	card.clip_contents = true
	card.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var art := Kit.image(card, Kit.DISTRICT_BG[did])
	art.size = card.size
	art.texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
	var fade := TextureRect.new()
	var g := GradientTexture2D.new()
	var gr := Gradient.new()
	gr.offsets = PackedFloat32Array([0.0, 0.25, 0.6, 1.0])
	gr.colors = PackedColorArray([Color(0, 0, 0, 0.1), Color(0, 0, 0, 0.2), Color(0, 0, 0, 0.92), Color(0, 0, 0, 0.96)])
	g.gradient = gr
	g.fill_to = Vector2(0, 1)
	g.width = 4
	g.height = 256
	fade.texture = g
	fade.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	fade.stretch_mode = TextureRect.STRETCH_SCALE
	fade.size = card.size
	fade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	card.add_child(fade)
	var num := Kit.label(card, "%s" % ["I", "II", "III", "IV", "V"][did - 1], 40, col, "display", 400, 0.0, Color(0, 0, 0, 0.8), 10)
	num.position = Vector2(20, 14)
	var nm := Kit.label(card, I18n.t("district.%d.name" % did), 22, Kit.CREAM, "deco", 400, 0.02, Color(0, 0, 0, 0.9), 8)
	nm.position = Vector2(20, 230)
	nm.size = Vector2(CARD_W - 40, 60)
	nm.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var any_open := false
	var y := 300.0
	for vid: String in d.vaults:
		var open := Meta.is_open(_save, vid)
		any_open = any_open or open
		var b := Boxed.new()
		b.position = Vector2(14, y)
		b.size = Vector2(CARD_W - 28, 50)
		b.bg_on = Color(col.r, col.g, col.b, 0.2)
		b.border_on = col
		b.lift = 0.0
		var lb := Kit.label(b, "%s  %s" % [vid, I18n.t("vault.%s" % vid)], 16, Kit.CREAM if open else Color(1, 1, 1, 0.3), "body", 600, 0.02)
		lb.position = Vector2(12, 14)
		lb.size = Vector2(CARD_W - 120, 24)
		lb.clip_text = true
		var md := Meta.medal(_save, vid)
		var stars := Kit.label(b, "●".repeat(md) + "○".repeat(3 - md), 14, Kit.GOLD if md > 0 else Color(1, 1, 1, 0.25), "mono", 600)
		stars.position = Vector2(CARD_W - 90, 15)
		var ni := _nodes.size()
		b.hovered.connect(func() -> void: _focus(ni))
		b.clicked.connect(func() -> void:
			_focus(ni)
			_pick())
		card.add_child(b)
		_nodes.append({"id": vid, "box": b, "open": open, "card": card, "d": did})
		y += 56
	if not any_open:
		art.modulate = Color(0.3, 0.3, 0.35)
	var border := Panel.new()
	var sb := StyleBoxFlat.new()
	sb.draw_center = false
	sb.border_color = Color(1, 1, 1, 0.12)
	sb.set_border_width_all(1)
	border.add_theme_stylebox_override("panel", sb)
	border.size = card.size
	border.mouse_filter = Control.MOUSE_FILTER_IGNORE
	card.add_child(border)
	card.set_meta("sb", sb)
	card.set_meta("col", col)
	return card

func _focus(i: int, quiet: bool = false) -> void:
	if _index < _nodes.size():
		(_nodes[_index].box as Boxed).set_on(false)
	_index = clampi(i, 0, _nodes.size() - 1)
	var n := _nodes[_index]
	(n.box as Boxed).set_on(true)
	for c in _cards:
		(c.get_meta("sb") as StyleBoxFlat).border_color = c.get_meta("col") if c == n.card else Color(1, 1, 1, 0.12)
	var lv := Meta.level(n.id)
	_detail_name.text = "%s — %s" % [n.id, I18n.t("vault.%s" % n.id)]
	var par := int(lv.data.get("par", 0))
	var best: Dictionary = _save.vaults.get(n.id, {})
	_detail_sub.text = "%s   ·   %s   ·   %s" % [I18n.t("map.echoes", {"n": int(lv.data.budget)}), I18n.t("map.par", {"n": par}), I18n.t("map.best", {"n": best.echoes}) if not best.is_empty() else ""]
	_detail_desc.text = I18n.t("district.%d.desc" % n.d) if n.open else I18n.t("common.locked")
	if not quiet:
		Sound.sfx("ui", 1.0 + (_index % 5) * 0.05)

func _pick() -> void:
	var n := _nodes[_index]
	if not n.open:
		Sound.sfx("block")
		return
	Sound.sfx("uiOk")
	_on_pick.call(n.id)

func _find(d: int, row: int) -> int:
	var c := -1
	for i in _nodes.size():
		if _nodes[i].d == d:
			c += 1
			if c == row:
				return i
	return -1

func update(_dt: float) -> void:
	var n := _nodes[_index]
	var row := 0
	for i in _index:
		if _nodes[i].d == n.d:
			row += 1
	if Keys.pressed("up"):
		_focus(maxi(0, _index - 1) if _index > 0 and _nodes[_index - 1].d == n.d else _index)
	if Keys.pressed("down"):
		if _index + 1 < _nodes.size() and _nodes[_index + 1].d == n.d:
			_focus(_index + 1)
	if Keys.pressed("left") and n.d > 1:
		var j := _find(n.d - 1, row)
		if j < 0:
			j = _find(n.d - 1, 0)
		_focus(j)
	if Keys.pressed("right") and n.d < Meta.DISTRICTS.size():
		var j := _find(n.d + 1, row)
		if j < 0:
			j = _find(n.d + 1, 0)
		_focus(j)
	if Keys.pressed("confirm") or Keys.pressed("use"):
		_pick()
	if Keys.pressed("back"):
		_on_back.call()
