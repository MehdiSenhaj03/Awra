class_name SetupScreen
extends Screen
## Free chase: pick the risk, the crew (alone, or two on one screen: a gamepad drives, the mouse shoots) and
## whether a rival team chases the same storms.

var _menu: UIMenu
var _start: Callable
var _back: Callable
var _risk := 2
var _crew := 0
var _rival := 0

func _init(start: Callable, back: Callable) -> void:
	super()
	_start = start
	_back = back
	Kit.background(self, "vista_mothership", 3.5, 0.3)
	var p := Kit.panel(self)
	var m := Kit.margin(p, 46, 36, 46, 36)
	var box := Kit.vbox(m, 14)
	Kit.label(box, I18n.t("setup.title"), 56, Kit.GOLD, "display", 700, 0.06)
	var rules := Kit.label(box, I18n.t("setup.rules"), 18, Kit.SOFT, "body", 400)
	rules.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	rules.custom_minimum_size.x = 900
	_menu = UIMenu.new(box)
	_row(I18n.t("setup.risk"), func() -> String: return I18n.t("risk.%d" % _risk), func(d: int) -> void: _risk = clampi(_risk + d, 1, 5))
	_row(I18n.t("setup.crew"), func() -> String: return I18n.t("setup.crew%d" % _crew), func(_d: int) -> void: _crew = 1 - _crew)
	_row(I18n.t("setup.rival"), func() -> String: return I18n.t("setup.rival%d" % _rival), func(_d: int) -> void: _rival = 1 - _rival)
	_menu.add(I18n.t("setup.go"), _go, {"size": 24, "width": 900})
	_menu.add(I18n.t("common.back"), _back, {"size": 20, "width": 900})
	p.ready.connect(func() -> void: p.position = ((Vector2(Kit.W, Kit.H) - p.size) / 2).round())

func _row(label: String, value: Callable, adjust: Callable) -> void:
	var it := _menu.add(label, Callable(), {"size": 22, "width": 900, "value": value.call()})
	it.adjust = func(d: int) -> void:
		adjust.call(d)
		it.set_value(value.call())
	it.action = func() -> void:
		adjust.call(1)
		it.set_value(value.call())

## A generated day for a risk level.
static func free_day(risk: int, seed_v: int) -> Dictionary:
	var rng := Rng.new(seed_v)
	var storms: Array = []
	var n := 1 if risk <= 1 else (2 if risk <= 4 else 1)
	for i in n:
		storms.append({"at": i * 60 * rng.rint(60, 120), "x": rng.rint(1_000_000, 3_200_000), "y": rng.rint(1_500_000, 7_000_000),
			"sev": clampi(risk + rng.rint(-1, 0), 1, 5), "dir": rng.rint(480, 640), "speed": rng.rint(12, 16), "tornadoes": clampi(risk / 2 + rng.rint(0, 1), 1, 3)})
	return {"id": "free", "risk": risk, "seed": seed_v, "minutes": 9, "storms": storms, "goal": 0}

func _go() -> void:
	var day := free_day(_risk, randi() % 100000)
	var mode := "versus_ai" if _rival == 1 else ("duo" if _crew == 1 else "solo")
	_start.call({"day": day, "mode": mode, "rival_skill": 0.6 + _risk * 0.1})

func update(_dt: float) -> void:
	if Keys.pressed("back"):
		_back.call()
		return
	_menu.handle()
