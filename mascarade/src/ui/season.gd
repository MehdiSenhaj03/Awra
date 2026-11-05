class_name SeasonScreen
extends Screen
## The Season: nine nights at the Palazzo Vermeil, in turn as the Raven (spy) and the Lynx (sniper).

var _menu: UIMenu
var _save: Dictionary
var _pick: Callable
var _back: Callable
var _bg: TextureRect
var _img: TextureRect
var _pt: TextureRect
var _name: Label
var _desc: Label
var _state: Label
var _shown := -1

func _init(save: Dictionary, focus: int, pick: Callable, back: Callable) -> void:
	super()
	_save = save
	_pick = pick
	_back = back
	_bg = Kit.background(self, "bg_salon", 4.0, 0.28)
	var left := Kit.vbox(self, 6)
	left.position = Vector2(110, 90)
	Kit.label(left, I18n.t("season.title"), 64, Kit.GOLD, "deco", 700, 0.08, Color(0.7, 0.2, 0.05, 0.4), 18)
	Kit.label(left, I18n.t("season.sub"), 18, Kit.SOFT, "body", 600, 0.05)
	Kit.rect(left, Color(0, 0, 0, 0)).custom_minimum_size.y = 18
	_menu = UIMenu.new(left, false, 2)
	for nt: Dictionary in Meta.NIGHTS:
		var n: int = nt.id
		var open := Meta.is_open(save, n)
		var done := Meta.done(save, n)
		var mark := "✦ " if done else ("" if open else "· ")
		var label := "%s%s  ·  %s" % [mark, I18n.t("season.night", {"n": _roman(n)}), I18n.t("night.%d.name" % n)]
		var sub := "%s  ·  %s" % [I18n.t("venue.%s" % nt.venue), I18n.t("season.as_spy") if nt.role == "spy" else I18n.t("season.as_hunter")]
		_menu.add(label, func() -> void: _pick.call(n), {"sub": sub, "size": 19, "width": 640, "disabled": not open})
	_menu.add(I18n.t("common.back"), _back, {"size": 18, "width": 640})
	_menu.focus(clampi(focus - 1, 0, 8), true)
	# Preview on the right.
	var card := Control.new()
	card.position = Vector2(860, 110)
	card.size = Vector2(960, 860)
	card.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(card)
	var frame := Kit.panel(card)
	frame.position = Vector2(0, 0)
	frame.size = Vector2(960, 520)
	_img = Kit.image(frame, "bg_salon")
	_img.custom_minimum_size = Vector2(940, 500)
	_pt = Kit.image(card, "pt_corbeau", false)
	_pt.position = Vector2(-60, 330)
	_pt.size = Vector2(420, 420)
	var info := Kit.vbox(card, 10)
	info.position = Vector2(370, 560)
	info.custom_minimum_size.x = 590
	_name = Kit.label(info, "", 36, Kit.IVORY, "display", 700, 0.06)
	_desc = Kit.label(info, "", 19, Kit.SOFT, "body", 400)
	_desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_desc.custom_minimum_size.x = 590
	_state = Kit.label(info, "", 16, Kit.BRASS, "display", 400, 0.2)
	_show(_menu.index)

static func _roman(n: int) -> String:
	return ["I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"][clampi(n - 1, 0, 8)]

func _show(i: int) -> void:
	if i == _shown or i >= Meta.NIGHTS.size():
		return
	_shown = i
	var nt: Dictionary = Meta.NIGHTS[i]
	var n: int = nt.id
	var bgn := "bg_%s" % nt.venue
	_img.texture = Kit.tex(bgn)
	_bg.texture = Kit.tex(bgn)
	_pt.texture = Kit.tex("pt_corbeau" if nt.role == "spy" else "pt_lynx")
	_name.text = "%s — %s" % [I18n.t("season.night", {"n": _roman(n)}), I18n.t("night.%d.name" % n)]
	_desc.text = I18n.t("night.%d.desc" % n)
	var r: Dictionary = _save.nights.get(str(n), {})
	if Meta.done(_save, n):
		var f: int = r.frame
		_state.text = I18n.t("season.done", {"t": "%d:%02d" % [f / 3600, (f / 60) % 60]})
	elif not Meta.is_open(_save, n):
		_state.text = I18n.t("season.locked")
	else:
		_state.text = I18n.t("season.open", {"skill": I18n.t("season.skill%d" % clampi(int(nt.skill * 3.0) - 1, 0, 3))})
	for c: CanvasItem in [_img, _pt]:
		c.modulate.a = 0.0
		c.create_tween().tween_property(c, "modulate:a", 1.0, 0.35)

func update(_dt: float) -> void:
	if Keys.pressed("back"):
		_back.call()
		return
	_menu.handle()
	_show(_menu.index)
