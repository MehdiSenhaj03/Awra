class_name GarageScreen
extends Screen
## The garage: spend the dossiers' earnings on the truck — armour, engine, probes, the long lens.

var _save: Dictionary
var _back: Callable
var _menu: UIMenu
var _money: Label
var _rows: Dictionary = {}

func _init(save: Dictionary, back: Callable) -> void:
	super()
	_save = save
	_back = back
	Kit.background(self, "vista_after", 3.0, 0.35)
	var tr := Kit.image(self, "truck", false)
	tr.position = Vector2(80, 300)
	tr.size = Vector2(900, 600)
	var p := Kit.panel(self)
	p.position = Vector2(1000, 120)
	var m := Kit.margin(p, 44, 36, 44, 36)
	var box := Kit.vbox(m, 14)
	Kit.label(box, I18n.t("garage.title"), 56, Kit.GOLD, "display", 700, 0.06)
	_money = Kit.label(box, "", 24, Kit.IVORY, "display", 700, 0.06)
	var intro := Kit.label(box, I18n.t("garage.intro"), 17, Kit.SOFT, "body", 400)
	intro.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	intro.custom_minimum_size.x = 760
	_menu = UIMenu.new(box)
	for k: String in ["armor", "engine", "probes", "zoom"]:
		var key := k
		var it := _menu.add(I18n.t("garage.%s" % k), func() -> void: _buy(key), {"sub": I18n.t("garage.%s_desc" % k), "size": 22, "width": 760, "value": ""})
		_rows[k] = it
	_menu.add(I18n.t("common.back"), _back, {"size": 20, "width": 760})
	_refresh()

func _refresh() -> void:
	_money.text = I18n.t("garage.money", {"n": _save.money})
	for k: String in _rows:
		var lv: int = int(_save.levels.get(k, 0))
		var spec: Dictionary = Meta.UPGRADES[k]
		var maxed := lv >= (spec.values as Array).size() - 1
		var it: MenuItem = _rows[k]
		var val := I18n.t("garage.val_%s" % k, {"v": spec.values[lv]})
		it.set_value(val + ("   ·   " + I18n.t("garage.max") if maxed else "   →   $%d" % int(spec.costs[lv + 1])))

func _buy(k: String) -> void:
	var lv: int = int(_save.levels.get(k, 0))
	var spec: Dictionary = Meta.UPGRADES[k]
	if lv >= (spec.values as Array).size() - 1:
		return
	var cost: int = spec.costs[lv + 1]
	if int(_save.money) < cost:
		Sound.sfx("ui", 0.6)
		return
	_save.money = int(_save.money) - cost
	_save.levels[k] = lv + 1
	Meta.write_save(_save)
	Sound.sfx("score")
	_refresh()

func update(_dt: float) -> void:
	if Keys.pressed("back"):
		_back.call()
		return
	_menu.handle()
