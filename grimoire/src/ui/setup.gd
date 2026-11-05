class_name SetupScreen
extends Screen
## Free play: a duel (against a master of your choice, or a second player with a gamepad) or an expedition on the
## Two Banks (alone, or with a second player). Every glyph is open.

const RIVALS := ["aldric", "cendres", "marees", "horloger", "reine", "p2"]
const LEVELS := [0.4, 0.75, 1.05, 1.4]
const ARENAS := ["ar_scriptorium", "ar_cendres", "ar_marees", "ar_horloge", "ar_reine"]

var _kind := "duel"
var _menu: UIMenu
var _start: Callable
var _back: Callable
var _rival := 1
var _level := 1
var _arena := 1
var _rounds := 1
var _players := 0
var _exp := 0

func _init(kind: String, start: Callable, back: Callable) -> void:
	super()
	_kind = kind
	_start = start
	_back = back
	Kit.background(self, "key_art" if kind == "duel" else "rives", 3.5, 0.3)
	var p := Kit.panel(self)
	var m := Kit.margin(p, 46, 36, 46, 36)
	var box := Kit.vbox(m, 14)
	Kit.label(box, I18n.t("setup.%s" % kind), 52, Kit.GOLD, "deco", 400, 0.03)
	var rules := Kit.label(box, I18n.t("setup.%s_rules" % kind), 18, Kit.SOFT, "body", 600)
	rules.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	rules.custom_minimum_size.x = 900
	_menu = UIMenu.new(box)
	if kind == "duel":
		_row(I18n.t("setup.opponent"), func() -> String: return I18n.t("setup.p2") if RIVALS[_rival] == "p2" else I18n.t("names.%s" % RIVALS[_rival]), func(d: int) -> void: _rival = (_rival + d + RIVALS.size()) % RIVALS.size())
		_row(I18n.t("setup.level"), func() -> String: return I18n.t("setup.level%d" % _level), func(d: int) -> void: _level = clampi(_level + d, 0, 3))
		_row(I18n.t("setup.arena"), func() -> String: return I18n.t("arena.%s" % ARENAS[_arena]), func(d: int) -> void: _arena = (_arena + d + ARENAS.size()) % ARENAS.size())
		_row(I18n.t("setup.rounds"), func() -> String: return str([1, 3, 5][_rounds]), func(d: int) -> void: _rounds = clampi(_rounds + d, 0, 2))
	else:
		_row(I18n.t("setup.players"), func() -> String: return I18n.t("setup.solo") if _players == 0 else I18n.t("setup.duo"), func(_d: int) -> void: _players = 1 - _players)
		_row(I18n.t("setup.expedition"), func() -> String: return I18n.t("setup.exp%d" % _exp), func(d: int) -> void: _exp = clampi(_exp + d, 0, 3))
	_menu.add(I18n.t("setup.go"), _go, {"size": 22, "width": 900})
	_menu.add(I18n.t("common.back"), _back, {"size": 19, "width": 900})
	p.ready.connect(func() -> void: p.position = ((Vector2(Kit.W, Kit.H) - p.size) / 2).round())

func _row(label: String, value: Callable, adjust: Callable) -> void:
	var it := _menu.add(label, Callable(), {"size": 19, "width": 900, "value": value.call()})
	it.adjust = func(d: int) -> void:
		adjust.call(d)
		it.set_value(value.call())
	it.action = func() -> void:
		adjust.call(1)
		it.set_value(value.call())

func _go() -> void:
	if _kind == "duel":
		var rv: String = RIVALS[_rival]
		var cfg := {"mode": "local" if rv == "p2" else "ai", "rival": "aldric" if rv == "p2" else rv, "skill": LEVELS[_level], "arena": ARENAS[_arena], "best_of": [1, 3, 5][_rounds], "seed": randi() % 100000}
		if rv == "p2":
			cfg.names = [I18n.t("names.ysolde"), I18n.t("names.aldric")]
			cfg.portraits = ["pt_ysolde", "pt_aldric"]
		_start.call("duel", cfg)
	else:
		_start.call("rives", {"mode": "solo" if _players == 0 else "local", "level": _exp, "boss": _exp == 3, "seed": randi() % 100000})

func update(_dt: float) -> void:
	if Keys.pressed("back"):
		_back.call()
		return
	_menu.handle()
