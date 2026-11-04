class_name LoadoutScreen
extends Screen
## Before a stage: pick a weapon, equip Overtones, spend Notes.

const WEAPON_ICON := {"prism": "ic_a0", "chord": "ic_a1", "lance": "ic_a2", "arc": "ic_a3", "swarm": "ic_a4"}

var _save: Dictionary
var _stage: String
var _on_play: Callable
var _on_back: Callable
var _nodes: Array[Dictionary] = []
var _index := 0
var _detail_t: Label
var _detail_d: Label
var _detail_p: Label
var _notes: Label
var _slots: Label
var _go: Control

func _init(save: Dictionary, stage_id: String, on_play: Callable, on_back: Callable) -> void:
	super()
	_save = save
	_stage = stage_id
	_on_play = on_play
	_on_back = on_back
	var d := Stages.get_def(stage_id)
	Kit.background(self, Kit.SECTOR_BG.get(int(d.sector), "bg_title"), 4.0, 0.3)
	var left := Kit.vbox(self, 14)
	left.position = Vector2(Kit.W * 0.06, 46)
	left.size = Vector2(900, Kit.H - 92)
	Kit.h_title(left, I18n.t("loadout.title"))
	Kit.h_sub(left, "%s — %s" % [I18n.t("sector.%d.name" % d.sector).to_upper(), I18n.t("stage.%d" % d.index).to_upper()])
	Kit.label(left, I18n.t("loadout.weapon").to_upper(), 11, Kit.MUTED, "display", 500, 0.4)
	var wc := GridContainer.new()
	wc.columns = 5
	wc.add_theme_constant_override("h_separation", 10)
	wc.add_theme_constant_override("v_separation", 10)
	left.add_child(wc)
	for w: String in Weapons.ORDER:
		_card(wc, "weapon", w, 0)
	_slots = Kit.label(left, "", 11, Kit.MUTED, "display", 500, 0.4)
	var oc := GridContainer.new()
	oc.columns = 4
	oc.add_theme_constant_override("h_separation", 10)
	oc.add_theme_constant_override("v_separation", 10)
	left.add_child(oc)
	for i in Meta.OVERTONES.size():
		_card(oc, "overtone", Meta.OVERTONES[i].id, 1 + i / 4)
	var right := Kit.vbox(self, 14)
	right.position = Vector2(Kit.W * 0.06 + 940, 46)
	right.size = Vector2(Kit.W * 0.88 - 940, Kit.H - 92)
	right.alignment = BoxContainer.ALIGNMENT_END
	var p := Kit.panel(right)
	p.custom_minimum_size.y = 150
	var pm := Kit.margin(p, 22, 22, 22, 22)
	var dv := Kit.vbox(pm, 8)
	_detail_t = Kit.label(dv, "", 20, Kit.PALE, "display", 500, 0.1)
	_detail_d = Kit.label(dv, "", 15, Color("#c3cde6"))
	_detail_d.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_detail_p = Kit.label(dv, "", 15, Kit.GOLD, "mono")
	var hint := Kit.label(right, I18n.t("loadout.hint"), 11, Kit.MUTED, "display", 500, 0.25)
	hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_go = _go_button(right)
	var gi := _nodes.size()
	_nodes.append({"box": _go, "kind": "play", "id": "play", "row": 5})
	_go.mouse_entered.connect(func() -> void: _focus(gi))
	_go.gui_input.connect(func(e: InputEvent) -> void:
		if e is InputEventMouseButton and e.pressed and e.button_index == MOUSE_BUTTON_LEFT:
			_activate(gi))
	var nb := Kit.hbox(self, 10)
	Kit.label(nb, I18n.t("common.notes").to_upper(), 12, Color("#c9a868"), "display", 500, 0.3)
	_notes = Kit.label(nb, "", 20, Kit.GOLD, "mono", 400, 0.0, Color(1, 0.76, 0.29, 0.5), 10)
	nb.position = Vector2(Kit.W * 0.94 - 260, 52)
	nb.size = Vector2(260, 30)
	nb.alignment = BoxContainer.ALIGNMENT_END
	_refresh()
	_focus(_nodes.size() - 1, true)

func _go_button(parent: Node) -> Control:
	var c := Control.new()
	c.custom_minimum_size = Vector2(0, 64)
	c.mouse_filter = Control.MOUSE_FILTER_STOP
	var bg := ColorRect.new()
	var m := ShaderMaterial.new()
	m.shader = preload("res://src/ui/shaders/go.gdshader")
	bg.material = m
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(bg)
	Kit.full(bg)
	var l := Kit.label(c, I18n.t("loadout.play").to_upper(), 18, Kit.INK, "display", 800, 0.3)
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	l.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	Kit.full(l)
	c.set_meta("mat", m)
	parent.add_child(c)
	return c

func _card(parent: Node, kind: String, id: String, row: int) -> void:
	var b := Boxed.new()
	b.custom_minimum_size = Vector2(170, 68)
	var icon := TextureRect.new()
	icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	icon.position = Vector2(12, 12)
	icon.size = Vector2(44, 44)
	icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.add_child(icon)
	var t := Kit.label(b, "", 12, Kit.PALE, "display", 500, 0.08)
	t.position = Vector2(66, 14)
	t.size = Vector2(100, 20)
	t.clip_text = true
	var p := Kit.label(b, "", 11, Kit.GOLD, "mono")
	p.position = Vector2(66, 36)
	var mark := Kit.label(b, "♪", 14, Kit.GOLD)
	mark.position = Vector2(150, 4)
	var i := _nodes.size()
	b.hovered.connect(func() -> void: _focus(i))
	b.clicked.connect(func() -> void: _activate(i))
	b.right_clicked.connect(func() -> void:
		_focus(i)
		_upgrade())
	parent.add_child(b)
	_nodes.append({"box": b, "kind": kind, "id": id, "row": row, "icon": icon, "t": t, "p": p, "mark": mark})

func _refresh() -> void:
	var s := _save
	_notes.text = Kit.fmt_int(s.notes)
	_slots.text = "%s — %s" % [I18n.t("loadout.overtones").to_upper(), I18n.t("loadout.slots", {"n": s.equipped.size(), "max": Meta.slots(s)})]
	for n in _nodes:
		if n.kind == "play":
			continue
		var b: Boxed = n.box
		var lock := false
		var eq := false
		if n.kind == "weapon":
			var open := Meta.weapon_unlocked(s, n.id)
			lock = not open
			eq = s.weapon == n.id
			(n.icon as TextureRect).texture = Kit.tex(WEAPON_ICON[n.id])
			(n.t as Label).text = I18n.t("weapon.%s.name" % n.id) if open else "???"
			(n.p as Label).text = ("%s %d" % [I18n.t("common.level"), int(s.weaponLevels.get(n.id, 1))]) if open else ""
		else:
			var o := Meta.overtone(n.id)
			var owned: bool = s.owned.has(o.id)
			lock = not owned
			eq = s.equipped.has(o.id)
			(n.icon as TextureRect).texture = Kit.tex("ic_%s" % o.icon)
			(n.t as Label).text = I18n.t("overtone.%s.name" % o.id)
			(n.p as Label).text = "" if owned else "%d ♪" % o.cost
		b.modulate.a = 0.35 if lock else 1.0
		b.inset = Kit.GOLD if eq else Color(0, 0, 0, 0)
		b.border = Kit.GOLD if eq else Color(1, 1, 1, 0.09)
		(n.mark as Label).visible = eq
		b.queue_redraw()
	_show_detail()

func _show_detail() -> void:
	var n := _nodes[_index]
	var s := _save
	var sec := int(Stages.get_def(_stage).sector)
	_detail_p.text = ""
	if n.kind == "play":
		_detail_t.text = I18n.t("sector.%d.name" % sec)
		_detail_d.text = I18n.t("sector.%d.desc" % sec)
		return
	if n.kind == "weapon":
		if not Meta.weapon_unlocked(s, n.id):
			_detail_t.text = "???"
			_detail_d.text = I18n.t("common.locked")
			return
		var lv := int(s.weaponLevels.get(n.id, 1))
		var cost: int = Meta.WEAPON_UPGRADE[lv] if lv < Meta.WEAPON_UPGRADE.size() else 0
		_detail_t.text = "%s · %s %d" % [I18n.t("weapon.%s.name" % n.id), I18n.t("common.level"), lv]
		_detail_d.text = I18n.t("weapon.%s.desc" % n.id)
		_detail_p.text = ("%s : %d ♪ — [%s]" % [I18n.t("common.upgrade"), cost, Keys.key_name("pulse")]) if cost else I18n.t("common.max")
		return
	var o := Meta.overtone(n.id)
	var owned: bool = s.owned.has(o.id)
	_detail_t.text = I18n.t("overtone.%s.name" % o.id)
	_detail_d.text = I18n.t("overtone.%s.desc" % o.id)
	_detail_p.text = (I18n.t("common.unequip") if s.equipped.has(o.id) else I18n.t("common.equip")) if owned else "%s : %d ♪" % [I18n.t("common.buy"), o.cost]

func _focus(i: int, quiet: bool = false) -> void:
	var prev := _nodes[_index]
	if prev.kind != "play":
		(prev.box as Boxed).set_on(false)
	_index = clampi(i, 0, _nodes.size() - 1)
	var n := _nodes[_index]
	if n.kind != "play":
		(n.box as Boxed).set_on(true)
	var gm: ShaderMaterial = _go.get_meta("mat")
	gm.set_shader_parameter("on", 1.0 if n.kind == "play" else 0.0)
	_show_detail()
	if not quiet:
		Sound.sfx("ui", 1.0 + (_index % 5) * 0.05)

func _activate(i: int) -> void:
	_focus(i, true)
	var n := _nodes[i]
	var s := _save
	if n.kind == "play":
		Sound.sfx("uiOk")
		_on_play.call()
		return
	if n.kind == "weapon":
		if not Meta.weapon_unlocked(s, n.id):
			Sound.sfx("block")
			return
		s.weapon = n.id
		Sound.sfx("uiOk")
	else:
		var o := Meta.overtone(n.id)
		if not s.owned.has(o.id):
			if s.notes < o.cost:
				Sound.sfx("block")
				return
			s.notes -= o.cost
			s.owned.append(o.id)
			Sound.sfx("fragment")
		elif s.equipped.has(o.id):
			s.equipped.erase(o.id)
			Sound.sfx("ui", 0.8)
		else:
			if s.equipped.size() >= Meta.slots(s):
				Sound.sfx("block")
				return
			s.equipped.append(o.id)
			Sound.sfx("uiOk")
	Meta.write_save(s)
	_refresh()

func _upgrade() -> void:
	var n := _nodes[_index]
	if n.kind != "weapon" or not Meta.weapon_unlocked(_save, n.id):
		return
	var lv := int(_save.weaponLevels.get(n.id, 1))
	var cost: int = Meta.WEAPON_UPGRADE[lv] if lv < Meta.WEAPON_UPGRADE.size() else 0
	if cost == 0 or _save.notes < cost:
		Sound.sfx("block")
		return
	_save.notes -= cost
	_save.weaponLevels[n.id] = lv + 1
	Meta.write_save(_save)
	Sound.sfx("fragment")
	_refresh()

func update(_dt: float) -> void:
	var n := _nodes[_index]
	if Keys.pressed("left"):
		_same_row(n, -1)
	if Keys.pressed("right"):
		_same_row(n, 1)
	if Keys.pressed("up"):
		_row_jump(n, -1)
	if Keys.pressed("down"):
		_row_jump(n, 1)
	if Keys.pressed("confirm") or Keys.pressed("fire"):
		_activate(_index)
	if Keys.pressed("pulse"):
		_upgrade()
	if Keys.pressed("back"):
		_on_back.call()

func _same_row(n: Dictionary, d: int) -> void:
	var j := _index + d
	if j >= 0 and j < _nodes.size() and _nodes[j].row == n.row:
		_focus(j)

func _row_jump(n: Dictionary, d: int) -> void:
	var target: int = n.row + d
	var cands: Array[int] = []
	var col := 0
	var c := 0
	for i in _nodes.size():
		if _nodes[i].row == target:
			cands.append(i)
		if _nodes[i].row == n.row:
			if i == _index:
				col = c
			c += 1
	if cands.is_empty():
		return
	_focus(cands[mini(col, cands.size() - 1)])
