class_name Workshop
extends Screen
## The Workshop: design a vault on the live 3D view, crack it yourself to validate it, share it as a code.

const TOOLS := [
	["wall", "#"], ["floor", "."], ["glass", "g"], ["exit", "E"], ["spawn", "@"], ["safe", "safe"], ["door", "door"],
	["door_chan", "chan"], ["door_key", "key"], ["door_vault", "vault"], ["door_time", "time"], ["plate", "plate"],
	["button", "button"], ["lever", "lever"], ["vlever", "vlever"], ["term", "term"], ["laser", "laser"], ["cam", "cam"],
	["dark", "dark"], ["card", "card"], ["gem", "gem"], ["guard", "guard"], ["erase", "erase"],
]
const ICONS := {"wall": "ic_wall", "door": "ic_door", "door_chan": "ic_door", "door_key": "ic_keydoor", "door_vault": "ic_safe", "door_time": "ic_time",
	"plate": "ic_plate", "button": "ic_button", "lever": "ic_button", "vlever": "ic_button", "term": "ic_term", "laser": "ic_laser", "cam": "ic_cam",
	"dark": "ic_light", "card": "ic_card", "gem": "ic_safe", "guard": "ic_guard", "safe": "ic_safe", "exit": "ic_exit", "erase": "ic_wall"}


var save: Dictionary
var data: Dictionary
var slot := -1
var view: View
var sim: Sim
var tool := "wall"
var chan := 1
var _tool_items: Array[Boxed] = []
var _status: Label
var _info: Label
var _name: LineEdit
var _hover := Vector2i(-1, -1)
var _painting := 0
var _guard_edit := -1
var _drag_from := Vector2i(-1, -1)
var _list: Control = null

func _init(a: Node, s: Dictionary, d: Dictionary = {}, sl: int = -1) -> void:
	super()
	app = a
	save = s
	slot = sl
	data = d if not d.is_empty() else Level.blank(18, 11)
	if data.get("name", "") == "":
		data.name = I18n.t("workshop.new")
	data.author = app.settings.get("name", "Odile")
	mouse_filter = Control.MOUSE_FILTER_PASS
	view = app.view
	view.margin_top = 90.0
	view.margin_bottom = 110.0
	view.zoom = 0.82
	_build_ui()
	_rebuild()

func _build_ui() -> void:
	# Tool palette.
	var pal := Kit.panel(self)
	pal.position = Vector2(24, 110)
	var pm := Kit.margin(pal, 10, 10, 10, 10)
	var grid := GridContainer.new()
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", 6)
	grid.add_theme_constant_override("v_separation", 6)
	pm.add_child(grid)
	for t: Array in TOOLS:
		var b := Boxed.new()
		b.custom_minimum_size = Vector2(150, 34)
		b.lift = 0.0
		var ic := TextureRect.new()
		ic.texture = Kit.tex(ICONS.get(t[0], "ic_wall"))
		ic.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		ic.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		ic.position = Vector2(4, 3)
		ic.size = Vector2(28, 28)
		ic.mouse_filter = Control.MOUSE_FILTER_IGNORE
		b.add_child(ic)
		var l := Kit.label(b, I18n.t("tool.%s" % t[0]), 13, Kit.CREAM, "body", 600)
		l.position = Vector2(38, 8)
		var name: String = t[0]
		b.clicked.connect(func() -> void: _set_tool(name))
		grid.add_child(b)
		_tool_items.append(b)
	# Top bar: name, size, loop, budget.
	var top := Kit.hbox(self, 14)
	top.position = Vector2(24, 24)
	Kit.label(top, I18n.t("workshop.title"), 36, Kit.CREAM, "display", 400, 0.08)
	_name = LineEdit.new()
	_name.text = data.name
	_name.custom_minimum_size = Vector2(320, 40)
	_name.add_theme_font_override("font", Kit.font("deco", 400, 0.0, 20))
	_name.add_theme_font_size_override("font_size", 20)
	_name.text_changed.connect(func(t: String) -> void:
		data.name = t
		_invalidate())
	top.add_child(_name)
	for spec in [["size_w", "w", 8, 32], ["size_h", "h", 6, 20], ["loop", "loop", 0, 0], ["budget", "budget", 1, 6]]:
		_stepper(top, spec[0], spec[1], spec[2], spec[3])
	# Bottom: status and actions.
	_status = Kit.label(self, "", 15, Kit.BRASS, "body", 600, 0.05)
	_status.position = Vector2(220, Kit.H - 44)
	_info = Kit.label(self, I18n.t("workshop.help"), 13, Kit.MUTED, "body", 400)
	_info.position = Vector2(220, Kit.H - 76)
	var act := Kit.vbox(self, 6)
	act.position = Vector2(Kit.W - 330, 110)
	var m := UIMenuLite.new(act)
	m.add(I18n.t("workshop.test"), _test)
	m.add(I18n.t("workshop.publish"), _publish)
	m.add(I18n.t("workshop.mine"), _open_list)
	m.add(I18n.t("workshop.new"), func() -> void:
		data = Level.blank(18, 11)
		data.name = I18n.t("workshop.new")
		slot = -1
		_name.text = data.name
		_rebuild())
	m.add(I18n.t("common.back"), func() -> void:
		_store()
		app.to_title())
	_set_tool("wall")

func _stepper(parent: Node, key: String, field: String, lo: int, hi: int) -> void:
	var h := Kit.hbox(parent, 4)
	Kit.label(h, I18n.t("workshop.%s" % key), 13, Kit.MUTED, "body", 600)
	var v := Kit.label(h, "", 18, Kit.CREAM, "mono", 600)
	v.custom_minimum_size.x = 50
	v.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	var upd := func() -> void:
		v.text = ("%ds" % (int(data.loop) / 60)) if field == "loop" else str(data[field])
	for d in [-1, 1]:
		var b := Button.new()
		b.text = "−" if d < 0 else "+"
		b.custom_minimum_size = Vector2(34, 34)
		b.add_theme_font_override("font", Kit.font("mono", 600, 0.0, 18))
		b.focus_mode = Control.FOCUS_NONE
		b.pressed.connect(func() -> void:
			match field:
				"loop": data.loop = clampi(int(data.loop) + d * 240, 960, 3600)
				"budget": data.budget = clampi(int(data.budget) + d, lo, hi)
				"w", "h": _resize(field, clampi(int(data[field]) + d, lo, hi))
			upd.call()
			_invalidate()
			_rebuild())
		h.add_child(b)
		if d < 0:
			h.move_child(b, 1)
	upd.call()

func _resize(field: String, n: int) -> void:
	var w := int(data.w)
	var h := int(data.h)
	var nw := n if field == "w" else w
	var nh := n if field == "h" else h
	var rows: Array = []
	for y in nh:
		var r := ""
		for x in nw:
			var c := "#" if x == 0 or y == 0 or x == nw - 1 or y == nh - 1 else "."
			if x < w - 1 and y < h - 1 and x > 0 and y > 0:
				c = String(data.rows[y])[x]
			r += c
		rows.append(r)
	data.rows = rows
	data.w = nw
	data.h = nh
	var keep: Array = []
	for o: Dictionary in data.objs:
		if int(o.x) > 0 and int(o.y) > 0 and int(o.x) < nw - 1 and int(o.y) < nh - 1:
			keep.append(o)
	data.objs = keep
	var sp: Array = data.spawn
	data.spawn = [clampi(int(sp[0]), 1, nw - 2), clampi(int(sp[1]), 1, nh - 2)]

func _set_tool(t: String) -> void:
	tool = t
	for i in _tool_items.size():
		_tool_items[i].set_on(TOOLS[i][0] == t)
	_guard_edit = -1

func _invalidate() -> void:
	data.erase("validated")

func _rebuild() -> void:
	var lv := Level.new(data)
	sim = Sim.new(lv)
	sim.reset_echoes(0)
	view.build(lv, sim)
	_status.text = (I18n.t("workshop.validated", {"n": data.par}) if data.get("validated", false) else I18n.t("workshop.not_validated")) + "   ·   " + I18n.t("workshop.channel", {"n": chan})

func _tile_at_mouse() -> Vector2i:
	var mp := get_viewport().get_mouse_position()
	var o := view.camera.project_ray_origin(mp)
	var d := view.camera.project_ray_normal(mp)
	if absf(d.y) < 1e-4:
		return Vector2i(-1, -1)
	var k := -o.y / d.y
	var p := o + d * k
	var t := Vector2i(int(floorf(p.x)), int(floorf(p.z)))
	if t.x < 0 or t.y < 0 or t.x >= int(data.w) or t.y >= int(data.h):
		return Vector2i(-1, -1)
	return t

func _obj_at(t: Vector2i) -> int:
	for i in data.objs.size():
		var o: Dictionary = data.objs[i]
		if int(o.x) == t.x and int(o.y) == t.y and o.t != "dark":
			return i
	return -1

func _put_obj(t: Vector2i, o: Dictionary) -> void:
	var i := _obj_at(t)
	if i >= 0:
		data.objs.remove_at(i)
	o.x = t.x
	o.y = t.y
	data.objs.append(o)
	var lv := Level.new(data)
	if lv.tile(t.x, t.y) == "#" or lv.tile(t.x, t.y) == " ":
		lv.set_tile(t.x, t.y, ".")

func _apply(t: Vector2i, erase: bool) -> void:
	var lv := Level.new(data)
	if erase or tool == "erase":
		var i := _obj_at(t)
		if i >= 0:
			data.objs.remove_at(i)
		else:
			for gi in data.guards.size():
				var p0: Array = data.guards[gi].path[0]
				if int(p0[0]) == t.x and int(p0[1]) == t.y:
					data.guards.remove_at(gi)
					break
			for di in data.objs.size():
				var o: Dictionary = data.objs[di]
				if o.t == "dark" and t.x >= int(o.x) and t.y >= int(o.y) and t.x < int(o.x) + int(o.w) and t.y < int(o.y) + int(o.h):
					data.objs.remove_at(di)
					break
		_invalidate()
		return
	var edge := t.x == 0 or t.y == 0 or t.x == int(data.w) - 1 or t.y == int(data.h) - 1
	match tool:
		"wall", "floor", "glass", "exit":
			if edge and tool != "wall":
				return
			lv.set_tile(t.x, t.y, {"wall": "#", "floor": ".", "glass": "g", "exit": "E"}[tool])
			if tool == "wall":
				var i := _obj_at(t)
				if i >= 0:
					data.objs.remove_at(i)
		"spawn":
			if not edge:
				data.spawn = [t.x, t.y]
		"safe": _put_obj(t, {"t": "safe"})
		"door": _put_obj(t, {"t": "door", "k": "door"})
		"door_chan": _put_obj(t, {"t": "door", "k": "chan", "ch": chan})
		"door_key": _put_obj(t, {"t": "door", "k": "key", "color": (chan - 1) % 4})
		"door_vault": _put_obj(t, {"t": "door", "k": "vault", "ch": chan})
		"door_time": _put_obj(t, {"t": "door", "k": "time", "t1": 600, "t2": 1200})
		"plate": _put_obj(t, {"t": "plate", "ch": chan})
		"button": _put_obj(t, {"t": "button", "ch": chan, "dur": 180})
		"lever": _put_obj(t, {"t": "lever", "ch": chan})
		"vlever": _put_obj(t, {"t": "vlever", "ch": chan})
		"term": _put_obj(t, {"t": "term", "ch": chan, "dur": 600})
		"laser": _put_obj(t, {"t": "laser", "ch": chan, "dir": 0, "len": 3})
		"cam": _put_obj(t, {"t": "cam", "ch": chan, "a0": 32, "a1": 96, "range": 7, "speed": 4})
		"card": _put_obj(t, {"t": "card", "color": (chan - 1) % 4})
		"gem": _put_obj(t, {"t": "gem"})
		"dark":
			_drag_from = t
			return
		"guard":
			if _guard_edit >= 0:
				data.guards[_guard_edit].path.append([t.x, t.y])
			else:
				data.guards.append({"path": [[t.x, t.y]], "wait": 40, "range": 6, "dir": 0})
				_guard_edit = data.guards.size() - 1
	_invalidate()

## Wheel over an object: its main setting (shift: the second one).
func _tune(t: Vector2i, d: int, alt: bool) -> void:
	var i := _obj_at(t)
	if i < 0:
		for g: Dictionary in data.guards:
			var p0: Array = g.path[0]
			if int(p0[0]) == t.x and int(p0[1]) == t.y:
				if alt:
					g.range = clampi(int(g.get("range", 6)) + d, 2, 9)
				else:
					g.dir = (int(g.get("dir", 0)) + d * 32) & 255
				_invalidate()
		return
	var o: Dictionary = data.objs[i]
	match o.t:
		"laser":
			if alt: o.dir = 1 - int(o.dir)
			else: o.len = clampi(int(o.len) + d, 1, 16)
		"cam":
			if alt:
				o.a1 = (int(o.a1) + d * 8) & 255
			else:
				o.a0 = (int(o.a0) + d * 16) & 255
				o.a1 = (int(o.a1) + d * 16) & 255
		"button", "term": o.dur = clampi(int(o.dur) + d * 60, 60, 1800)
		"door":
			if o.k == "time":
				if alt: o.t2 = clampi(int(o.t2) + d * 60, int(o.t1) + 60, int(data.loop))
				else:
					o.t1 = clampi(int(o.t1) + d * 60, 0, int(o.t2) - 60)
	_invalidate()

func _gui_input(e: InputEvent) -> void:
	pass

func _unhandled_input(e: InputEvent) -> void:
	if _list != null or not is_visible_in_tree():
		return
	if e is InputEventMouseButton:
		var t := _tile_at_mouse()
		if e.pressed and e.button_index in [MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN] and t.x >= 0:
			_tune(t, 1 if e.button_index == MOUSE_BUTTON_WHEEL_UP else -1, e.shift_pressed)
			_rebuild()
			return
		if e.button_index == MOUSE_BUTTON_LEFT or e.button_index == MOUSE_BUTTON_RIGHT:
			if e.pressed and t.x >= 0:
				_painting = 1 if e.button_index == MOUSE_BUTTON_LEFT else 2
				if _painting == 2 and _guard_edit >= 0:
					_guard_edit = -1
				else:
					_apply(t, _painting == 2)
				_rebuild()
			elif not e.pressed:
				if tool == "dark" and _drag_from.x >= 0 and t.x >= 0 and _painting == 1:
					var x0 := mini(_drag_from.x, t.x)
					var y0 := mini(_drag_from.y, t.y)
					data.objs.append({"t": "dark", "x": x0, "y": y0, "w": absi(t.x - _drag_from.x) + 1, "h": absi(t.y - _drag_from.y) + 1, "ch": chan, "inv": true})
					_invalidate()
					_rebuild()
				_drag_from = Vector2i(-1, -1)
				_painting = 0
	elif e is InputEventMouseMotion and _painting > 0 and tool in ["wall", "floor", "glass", "exit", "erase"]:
		var t := _tile_at_mouse()
		if t.x >= 0 and t != _hover:
			_hover = t
			_apply(t, _painting == 2)
			_rebuild()
	elif e is InputEventKey and e.pressed and not e.echo and not _name.has_focus():
		if e.keycode >= KEY_1 and e.keycode <= KEY_8:
			chan = e.keycode - KEY_0
			_rebuild()
		elif e.keycode == KEY_G:
			_set_tool("guard")
		elif e.keycode == KEY_ENTER:
			_guard_edit = -1

func _valid_shape() -> bool:
	var has_safe := false
	for o: Dictionary in data.objs:
		if o.t == "safe":
			has_safe = true
	var has_exit := false
	for r: String in data.rows:
		if r.contains("E"):
			has_exit = true
	return has_safe and has_exit

func _test() -> void:
	if not _valid_shape():
		_status.text = I18n.t("workshop.need_safe")
		Sound.sfx("block")
		return
	_store()
	var d := data.duplicate(true)
	app.start_test(d, func(r: Dictionary) -> void:
		data.validated = true
		data.par = int(r.echoes)
		_store()
		app.reopen_workshop(data, slot))

func _publish() -> void:
	if not data.get("validated", false):
		_status.text = I18n.t("workshop.not_validated")
		Sound.sfx("block")
		return
	DisplayServer.clipboard_set(Level.encode(data))
	_status.text = I18n.t("common.copied")
	Sound.sfx("uiOk")

func _store() -> void:
	var list: Array = save.workshop
	if slot >= 0 and slot < list.size():
		list[slot] = data.duplicate(true)
	else:
		list.append(data.duplicate(true))
		slot = list.size() - 1
	Meta.write_save(save)

func _open_list() -> void:
	_store()
	_list = Control.new()
	_list.mouse_filter = Control.MOUSE_FILTER_STOP
	Kit.full(_list)
	add_child(_list)
	Kit.full(Kit.rect(_list, Color(0, 0, 0, 0.75)))
	var p := Kit.panel(_list)
	var m := Kit.margin(p, 30, 26, 30, 26)
	var v := Kit.vbox(m, 8)
	Kit.label(v, I18n.t("workshop.mine"), 30, Kit.CREAM, "display", 400, 0.08)
	var mm := UIMenuLite.new(v)
	var list: Array = save.workshop
	for i in list.size():
		var d: Dictionary = list[i]
		var idx := i
		mm.add("%s   %s" % [d.get("name", "?"), "✓" if d.get("validated", false) else ""], func() -> void:
			data = (save.workshop[idx] as Dictionary).duplicate(true)
			slot = idx
			_name.text = data.name
			_list.queue_free()
			_list = null
			_rebuild())
	mm.add(I18n.t("common.back"), func() -> void:
		_list.queue_free()
		_list = null)
	p.ready.connect(func() -> void: p.position = ((Vector2(Kit.W, Kit.H) - p.size) / 2).round())

func update(dt: float) -> void:
	view.render(sim, null, 1.0, dt, [])
	if Keys.pressed("back") and _list == null and not _name.has_focus():
		_store()
		app.to_title()

## A small mouse-first menu (the Workshop is a mouse tool).
class UIMenuLite extends RefCounted:
	var root: VBoxContainer
	func _init(parent: Node) -> void:
		root = Kit.vbox(parent, 6)
	func add(text: String, action: Callable) -> void:
		var b := Boxed.new()
		b.custom_minimum_size = Vector2(300, 44)
		b.lift = 0.0
		var l := Kit.label(b, text, 16, Kit.CREAM, "body", 600, 0.04)
		l.position = Vector2(14, 12)
		b.clicked.connect(func() -> void:
			Sound.sfx("uiOk")
			action.call())
		b.hovered.connect(func() -> void: b.set_on(true))
		b.mouse_exited.connect(func() -> void: b.set_on(false))
		root.add_child(b)
