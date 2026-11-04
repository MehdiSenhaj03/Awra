class_name CodexScreen
extends Screen
## Memories, the Mutes and the Conductors.

const ENEMIES := [["mote", "s1_1"], ["spitter", "s1_1"], ["spinner", "s1_1"], ["lancer", "s1_1"], ["splitter", "s1_1"], ["bulwark", "s1_3"], ["mirror", "s2_1"], ["phantom", "s2_1"], ["mortar", "s3_1"], ["weaver", "s3_1"], ["sniper", "s4_1"], ["choir", "s4_1"], ["echo", "s1_1"]]
const BOSSES := ["ostinato", "crescendo", "fugue", "rubato", "staccato", "fermata"]

var _save: Dictionary
var _on_back: Callable
var _tabs: UIMenu
var _list: UIMenu
var _list_box: VBoxContainer
var _scroll: ScrollContainer
var _d_img: TextureRect
var _d_t: Label
var _d_sub: Label
var _d_d: Label
var _d_d2: Label
var _entries: Array = []
var _focus_tabs := true

func _init(save: Dictionary, on_back: Callable) -> void:
	super()
	_save = save
	_on_back = on_back
	Kit.background(self, "bg_title", 5.0, 0.3)
	var c := Kit.vbox(self, 18)
	c.position = Vector2(Kit.W * 0.06, 46)
	c.size = Vector2(Kit.W * 0.88, Kit.H - 92)
	Kit.h_title(c, I18n.t("codex.title"))
	_tabs = UIMenu.new(c, true, 10)
	for k in ["fragments", "bestiary", "conductors"]:
		var i: int = _tabs.items.size()
		_tabs.add(I18n.t("codex.%s" % k).to_upper(), func() -> void: _show(i), {"size": 13})
	var body := Kit.hbox(c, 24)
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var lp := Kit.panel(body)
	lp.custom_minimum_size.x = 380
	_scroll = ScrollContainer.new()
	_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	lp.add_child(_scroll)
	_list_box = Kit.vbox(_scroll, 0)
	_list_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var dp := Kit.panel(body)
	dp.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var dm := Kit.margin(dp, 34, 34, 34, 34)
	var dh := Kit.hbox(dm, 20)
	var dv := Kit.vbox(dh, 14)
	dv.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_d_t = Kit.label(dv, "", 28, Kit.PALE, "display", 500, 0.12)
	_d_sub = Kit.h_sub(dv, "")
	_d_d = Kit.label(dv, "", 17, Color("#d4dcf0"))
	_d_d.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_d_d.custom_minimum_size.x = 640
	_d_d2 = Kit.label(dv, "", 17, Color("#99eeee"))
	_d_d2.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_d_d2.custom_minimum_size.x = 640
	_d_img = TextureRect.new()
	_d_img.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_d_img.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_d_img.custom_minimum_size = Vector2(440, 480)
	_d_img.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	dh.add_child(_d_img)
	_show(0)

func _show(tab: int) -> void:
	for ch in _list_box.get_children():
		ch.queue_free()
	_list = UIMenu.new(_list_box)
	var s := _save
	_entries = []
	if tab == 0:
		for sec in range(1, 7):
			for k in range(1, 4):
				var f := "f%d_%d" % [sec, k]
				var got: bool = s.fragments.has(f)
				_entries.append({"label": I18n.t("fragments.%s.name" % f) if got else I18n.t("codex.unknown"), "t": I18n.t("fragments.%s.name" % f) if got else I18n.t("codex.unknown"), "d": I18n.t("fragments.%s.text" % f) if got else I18n.t("codex.hidden")})
	elif tab == 1:
		for e: Array in ENEMIES:
			var seen := Meta.cleared(s, e[1])
			_entries.append({"label": I18n.t("enemy.%s.name" % e[0]) if seen else I18n.t("codex.unknown"), "t": I18n.t("enemy.%s.name" % e[0]) if seen else I18n.t("codex.unknown"), "d": I18n.t("enemy.%s.desc" % e[0]) if seen else ""})
	else:
		for i in BOSSES.size():
			var b: String = BOSSES[i]
			var seen := Meta.cleared(s, "s%d_4" % (i + 1))
			if seen:
				_entries.append({"label": I18n.t("boss.%s.name" % b), "t": I18n.t("boss.%s.name" % b), "sub": I18n.t("boss.%s.title" % b).to_upper(), "d": I18n.t("boss.%s.desc" % b), "d2": I18n.t("story.after.%s" % b), "img": "boss_%s" % b})
			else:
				_entries.append({"label": I18n.t("codex.unknown"), "t": I18n.t("codex.unknown"), "d": ""})
	for i in _entries.size():
		var it := _list.add(_entries[i].label, func() -> void: _focus_tabs = false, {"size": 13, "width": 360})
		it.mouse_entered.connect(func() -> void: _detail(i))
	_detail(0)

func _detail(i: int) -> void:
	if i >= _entries.size():
		return
	var e: Dictionary = _entries[i]
	_d_t.text = e.t
	_d_sub.text = e.get("sub", "")
	_d_sub.visible = _d_sub.text != ""
	_d_d.text = e.d
	_d_d2.text = e.get("d2", "")
	_d_d2.visible = _d_d2.text != ""
	_d_img.texture = Kit.tex(e.img) if e.has("img") else null
	_d_img.visible = e.has("img")

func update(_dt: float) -> void:
	if Keys.pressed("back"):
		if not _focus_tabs:
			_focus_tabs = true
		else:
			_on_back.call()
		return
	if _focus_tabs:
		var before := _tabs.index
		_tabs.handle()
		if _tabs.index != before:
			_show(_tabs.index)
		if Keys.pressed("down"):
			_focus_tabs = false
	else:
		var before := _list.index
		if Keys.pressed("up") and _list.index == 0:
			_focus_tabs = true
			return
		_list.handle()
		if _list.index != before:
			_detail(_list.index)
			_scroll.ensure_control_visible(_list.items[_list.index])
