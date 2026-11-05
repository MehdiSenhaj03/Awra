class_name SeasonScreen
extends Screen
## The Season: eight chase days from April to June, each with its forecast; the garage between them.

const VISTA := ["vista_mothership", "vista_tornado", "vista_hail", "vista_town", "vista_night", "vista_mothership", "vista_tornado", "key_art"]

var _menu: UIMenu
var _save: Dictionary
var _pick: Callable
var _garage: Callable
var _back: Callable
var _img: TextureRect
var _bg: TextureRect
var _name: Label
var _desc: Label
var _state: Label
var _risk: Control
var _shown := -1

func _init(save: Dictionary, focus: int, pick: Callable, garage: Callable, back: Callable) -> void:
	super()
	_save = save
	_pick = pick
	_garage = garage
	_back = back
	_bg = Kit.background(self, "bg_title", 4.0, 0.3)
	var left := Kit.vbox(self, 4)
	left.position = Vector2(110, 80)
	Kit.label(left, I18n.t("season.title"), 76, Kit.GOLD, "display", 700, 0.06, Color(0.5, 0.25, 0.0, 0.4), 16)
	Kit.label(left, I18n.t("season.sub", {"money": save.money}), 19, Kit.SOFT, "body", 600)
	Kit.rect(left, Color(0, 0, 0, 0)).custom_minimum_size.y = 14
	_menu = UIMenu.new(left, false, 0)
	for i in Meta.DAYS.size():
		var d: Dictionary = Meta.DAYS[i]
		var open := Meta.is_open(save, i)
		var mark := "✓ " if Meta.done(save, i) else ("" if open else "· ")
		var idx := i
		_menu.add("%s%s" % [mark, I18n.t("day.%s.name" % d.id)], func() -> void: _pick.call(idx), {"sub": I18n.t("day.%s.date" % d.id) + "  ·  " + I18n.t("risk.%d" % int(d.risk)), "size": 22, "width": 640, "disabled": not open})
	_menu.add(I18n.t("season.garage"), _garage, {"size": 22, "width": 640})
	_menu.add(I18n.t("common.back"), _back, {"size": 20, "width": 640})
	_menu.focus(clampi(focus, 0, Meta.DAYS.size() - 1), true)
	var card := Control.new()
	card.position = Vector2(860, 100)
	card.size = Vector2(960, 880)
	card.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(card)
	var frame := Kit.panel(card)
	frame.size = Vector2(960, 560)
	_img = Kit.image(frame, "bg_title")
	_img.custom_minimum_size = Vector2(940, 540)
	var info := Kit.vbox(card, 10)
	info.position = Vector2(10, 590)
	info.custom_minimum_size.x = 940
	_name = Kit.label(info, "", 44, Kit.IVORY, "display", 700, 0.04)
	_desc = Kit.label(info, "", 20, Kit.SOFT, "body", 400)
	_desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_desc.custom_minimum_size.x = 940
	_risk = Control.new()
	_risk.custom_minimum_size = Vector2(940, 40)
	_risk.mouse_filter = Control.MOUSE_FILTER_IGNORE
	info.add_child(_risk)
	_risk.draw.connect(_draw_risk)
	_state = Kit.label(info, "", 17, Kit.BRASS, "display", 700, 0.15)
	_show(_menu.index)

func _show(i: int) -> void:
	if i == _shown or i >= Meta.DAYS.size():
		return
	_shown = i
	var d: Dictionary = Meta.DAYS[i]
	_img.texture = Kit.tex(VISTA[i % VISTA.size()])
	_name.text = I18n.t("day.%s.name" % d.id)
	_desc.text = I18n.t("day.%s.desc" % d.id)
	var rec: Dictionary = _save.days.get(d.id, {})
	if Meta.done(_save, i):
		_state.text = I18n.t("season.done", {"n": rec.get("best", 0), "goal": d.goal})
	elif not Meta.is_open(_save, i):
		_state.text = I18n.t("season.locked")
	else:
		_state.text = I18n.t("season.goal", {"goal": d.goal}) + ("   ·   " + I18n.t("season.rival") if d.rival else "")
	_risk.queue_redraw()
	_img.modulate.a = 0.0
	_img.create_tween().tween_property(_img, "modulate:a", 1.0, 0.35)

func _draw_risk() -> void:
	var d: Dictionary = Meta.DAYS[_shown]
	var cols := [Color("#3fae49"), Color("#f6f655"), Color("#e6c228"), Color("#e8732b"), Color("#e2212a"), Color("#c71ce0")]
	for k in 5:
		var on := k < int(d.risk)
		_risk.draw_rect(Rect2(k * 70, 8, 62, 22), cols[k + 1] if on else Color(1, 1, 1, 0.08))
	_risk.draw_string(Kit.font("display", 700, 0.1, 20), Vector2(370, 28), I18n.t("risk.%d" % int(d.risk)).to_upper(), HORIZONTAL_ALIGNMENT_LEFT, -1, 20, cols[int(d.risk)])

func update(_dt: float) -> void:
	if Keys.pressed("back"):
		_back.call()
		return
	_menu.handle()
	_show(_menu.index)
