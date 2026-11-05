class_name SetupScreen
extends Screen
## A free leg (the land, how hard, a wild titan at the end or not, alone or two on one screen) or a duel of titans
## (against the AI and how fierce, or two on one screen).

var _menu: UIMenu
var _start: Callable
var _back: Callable
var _kind := "free"
var _land := 0
var _level := 2
var _boss := 0
var _players := 0
var _foe := 1

const LANDS := ["ash", "ruins", "forest", "storm", "oasis"]

func _init(kind: String, start: Callable, back: Callable) -> void:
	super()
	_kind = kind
	_start = start
	_back = back
	Kit.background(self, "duel" if kind == "duel" else "vista_ash", 3.0, 0.32)
	var p := Kit.panel(self)
	var m := Kit.margin(p, 46, 36, 46, 36)
	var box := Kit.vbox(m, 14)
	Kit.label(box, I18n.t("setup.%s_title" % kind), 52, Kit.GOLD, "display", 700, 0.06)
	var rules := Kit.label(box, I18n.t("setup.%s_rules" % kind), 18, Kit.SOFT, "body", 400)
	rules.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	rules.custom_minimum_size.x = 900
	_menu = UIMenu.new(box)
	if kind == "free":
		_row(I18n.t("setup.land"), func() -> String: return I18n.t("land.%s.name" % LANDS[_land]), func(d: int) -> void: _land = (_land + d + LANDS.size()) % LANDS.size())
		_row(I18n.t("setup.level"), func() -> String: return I18n.t("setup.level%d" % _level), func(d: int) -> void: _level = clampi(_level + d, 1, 3))
		_row(I18n.t("setup.boss"), func() -> String: return I18n.t("setup.boss%d" % _boss), func(_d: int) -> void: _boss = 1 - _boss)
		_row(I18n.t("setup.players"), func() -> String: return I18n.t("setup.players%d" % _players), func(_d: int) -> void: _players = 1 - _players)
	else:
		_row(I18n.t("setup.opponent"), func() -> String: return I18n.t("setup.foe%d" % _foe), func(d: int) -> void: _foe = clampi(_foe + d, 0, 3))
	_menu.add(I18n.t("setup.go"), _go, {"size": 24, "width": 900})
	_menu.add(I18n.t("common.back"), _back, {"size": 20, "width": 900})
	p.ready.connect(func() -> void: p.position = ((Vector2(Kit.W, Kit.H) - p.size) / 2).round())

func _row(label: String, value: Callable, adjust: Callable) -> void:
	var it := _menu.add(label, Callable(), {"size": 22, "width": 900, "value": value.call()})
	it.adjust = func(d: int) -> void:
		adjust.call(d)
		it.set_value(value.call())
		Sound.sfx("ui", 1.1)
	it.action = func() -> void:
		adjust.call(1)
		it.set_value(value.call())
		Sound.sfx("ui", 1.1)

func _go() -> void:
	Sound.sfx("uiOk")
	var seed_v := randi() % 100000
	if _kind == "free":
		var land: String = LANDS[_land]
		var leg := {"kind": land, "length_m": 2400, "seed": seed_v, "level": _level}
		if _boss == 1:
			leg.boss = true
			leg.foe_hp = 12000 + _level * 3500
			leg.foe_power = 85 + _level * 10
		_start.call({"mode": "coop" if _players == 1 else "solo", "leg": leg, "title": I18n.t("land.%s.name" % land),
			"subtitle": I18n.t("free.sub") + " · " + I18n.t("setup.level%d" % _level), "tips": false})
	else:
		var two := _foe == 3
		var lv := clampi(_foe + 1, 1, 3)
		var leg2 := {"kind": "ash", "seed": seed_v, "level": lv, "foe_hp": 14000 + lv * 2000, "foe_power": 90 + lv * 8}
		if two:
			leg2.foe_hp = March.HP
			leg2.foe_power = 100
		_start.call({"mode": "duel_local" if two else "duel_ai", "leg": leg2, "title": I18n.t("title.duel"),
			"subtitle": I18n.t("setup.foe%d" % _foe), "foe_name": I18n.t("hud.p2") if two else I18n.t("hud.foe")})

func update(_dt: float) -> void:
	if Keys.pressed("back"):
		_back.call()
		return
	_menu.handle()
