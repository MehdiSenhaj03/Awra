class_name ApprenticeScreen
extends Screen
## The Apprentice: ten chapters, duels against the masters and expeditions on the Two Banks.

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
var _teach: Control
var _shown := -1

func _init(save: Dictionary, focus: int, pick: Callable, back: Callable) -> void:
	super()
	_save = save
	_pick = pick
	_back = back
	_bg = Kit.background(self, "bg_title", 4.0, 0.3)
	var left := Kit.vbox(self, 4)
	left.position = Vector2(110, 80)
	Kit.label(left, I18n.t("campaign.title"), 66, Kit.GOLD, "deco", 400, 0.03, Color(0.5, 0.2, 0.05, 0.5), 18)
	Kit.label(left, I18n.t("campaign.sub"), 19, Kit.SOFT, "body", 600, 0.03)
	Kit.rect(left, Color(0, 0, 0, 0)).custom_minimum_size.y = 14
	_menu = UIMenu.new(left, false, 0)
	for c: Dictionary in Meta.CHAPTERS:
		var n: int = c.id
		var open := Meta.is_open(save, n)
		var mark := "✦ " if Meta.done(save, n) else ("" if open else "· ")
		var kind := I18n.t("campaign.duel") if c.kind == "duel" else I18n.t("campaign.rives")
		_menu.add("%s%s  ·  %s" % [mark, _roman(n), I18n.t("chapter.%d.name" % n)], func() -> void: _pick.call(n), {"sub": kind, "size": 19, "width": 660, "disabled": not open})
	_menu.add(I18n.t("common.back"), _back, {"size": 18, "width": 660})
	_menu.focus(clampi(focus - 1, 0, Meta.CHAPTERS.size() - 1), true)
	var card := Control.new()
	card.position = Vector2(880, 100)
	card.size = Vector2(940, 880)
	card.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(card)
	var frame := Kit.panel(card)
	frame.size = Vector2(940, 500)
	_img = Kit.image(frame, "bg_title")
	_img.custom_minimum_size = Vector2(920, 480)
	_pt = Kit.image(card, "pt_ysolde", false)
	_pt.position = Vector2(-70, 330)
	_pt.size = Vector2(420, 420)
	var info := Kit.vbox(card, 10)
	info.position = Vector2(360, 540)
	info.custom_minimum_size.x = 580
	_name = Kit.label(info, "", 38, Kit.IVORY, "deco", 400, 0.03)
	_desc = Kit.label(info, "", 20, Kit.SOFT, "body", 600)
	_desc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_desc.custom_minimum_size.x = 580
	_state = Kit.label(info, "", 16, Kit.BRASS, "display", 700, 0.2)
	_teach = Control.new()
	_teach.custom_minimum_size = Vector2(580, 70)
	_teach.mouse_filter = Control.MOUSE_FILTER_IGNORE
	info.add_child(_teach)
	_teach.draw.connect(_draw_teach)
	_show(_menu.index)

static func _roman(n: int) -> String:
	return ["I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"][clampi(n - 1, 0, 9)]

func _show(i: int) -> void:
	if i == _shown or i >= Meta.CHAPTERS.size():
		return
	_shown = i
	var c: Dictionary = Meta.CHAPTERS[i]
	var n: int = c.id
	var img: String = c.get("arena", "rives")
	_img.texture = Kit.tex(img)
	_pt.texture = Kit.tex("pt_%s" % c.rival) if c.kind == "duel" else Kit.tex("pt_ysolde")
	_name.text = "%s — %s" % [_roman(n), I18n.t("chapter.%d.name" % n)]
	_desc.text = I18n.t("chapter.%d.desc" % n)
	if Meta.done(_save, n):
		_state.text = I18n.t("campaign.done")
	elif not Meta.is_open(_save, n):
		_state.text = I18n.t("campaign.locked")
	else:
		_state.text = I18n.t("campaign.teach") if (c.teach as Array).size() > 0 else I18n.t("campaign.all")
	_teach.queue_redraw()
	for x: CanvasItem in [_img, _pt]:
		x.modulate.a = 0.0
		x.create_tween().tween_property(x, "modulate:a", 1.0, 0.35)

func _draw_teach() -> void:
	var c: Dictionary = Meta.CHAPTERS[_shown]
	var k := 0
	for g: int in c.teach:
		var p := Vector2(36 + k * 84, 36)
		_teach.draw_circle(p, 32, Color(0.08, 0.04, 0.02, 0.7))
		_teach.draw_arc(p, 32, 0, TAU, 32, Kit.GOLD, 1.5, true)
		GlyphArt.draw(_teach, g, p, 38, GlyphArt.COLORS[g], 2.5)
		k += 1

func update(_dt: float) -> void:
	if Keys.pressed("back"):
		_back.call()
		return
	_menu.handle()
	_show(_menu.index)
