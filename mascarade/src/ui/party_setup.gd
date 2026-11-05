class_name PartySetup
extends Screen
## The Assassins' Ball: 2 to 4 players on one screen, each a guest among a hundred. Pick the players and the venue.

var _menu: UIMenu
var _n := 2
var _venue := 0
var _start: Callable
var _back: Callable
var _devs: VBoxContainer
const VENUES := ["salon", "serre", "opera"]

func _init(start: Callable, back: Callable) -> void:
	super()
	_start = start
	_back = back
	Kit.background(self, "bg_opera", 3.5, 0.3)
	var p := Kit.panel(self)
	var m := Kit.margin(p, 44, 36, 44, 36)
	var box := Kit.vbox(m, 14)
	Kit.label(box, I18n.t("party.title"), 48, Kit.GOLD, "deco", 700, 0.06)
	var rules := Kit.label(box, I18n.t("party.rules"), 17, Kit.SOFT, "body", 400)
	rules.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	rules.custom_minimum_size.x = 900
	_menu = UIMenu.new(box)
	var it_n := _menu.add(I18n.t("party.players"), Callable(), {"size": 19, "width": 900, "value": str(_n)})
	it_n.adjust = func(d: int) -> void:
		_n = clampi(_n + d, 2, 4)
		it_n.set_value(str(_n))
		_list()
	it_n.action = func() -> void: it_n.adjust.call(1 if _n < 4 else -2)
	var it_v := _menu.add(I18n.t("party.venue"), Callable(), {"size": 19, "width": 900, "value": I18n.t("venue.salon")})
	it_v.adjust = func(d: int) -> void:
		_venue = (_venue + d + 3) % 3
		it_v.set_value(I18n.t("venue.%s" % VENUES[_venue]))
	it_v.action = func() -> void: it_v.adjust.call(1)
	_menu.add(I18n.t("party.start"), func() -> void: _start.call(sources(), VENUES[_venue]), {"size": 21, "width": 900})
	_menu.add(I18n.t("common.back"), _back, {"size": 19, "width": 900})
	_devs = Kit.vbox(box, 6)
	_list()
	p.ready.connect(func() -> void: p.position = ((Vector2(Kit.W, Kit.H) - p.size) / 2).round())

func sources() -> Array:
	var out: Array = []
	var pads := Input.get_connected_joypads()
	# Pads first for players 3 and 4; with enough pads, everyone may take one.
	var order: Array = ["kb", "kb2"]
	for i in pads.size():
		order.append("pad%d" % pads[i])
	if _n > order.size():
		for i in range(order.size(), _n):
			order.append("pad%d" % (i - 2))
	for i in _n:
		out.append(order[i])
	return out

func _list() -> void:
	for c in _devs.get_children():
		c.queue_free()
	var src := sources()
	for i in src.size():
		var s: String = src[i]
		var l := Kit.rich(_devs, Kit.html("[color=#%s]%s[/color]  —  %s" % [Ball.PLAYER_COLORS[i].to_html(false), I18n.t("ball.player", {"n": i + 1}), I18n.t("party.keys_%s" % s.substr(0, 3), _key_names())]), 16, Kit.SOFT)
		l.custom_minimum_size.x = 900

func _key_names() -> Dictionary:
	var kn := func(k: Key) -> String: return Keys.code_name("k%d" % k)
	return {"up": kn.call(KEY_W), "left": kn.call(KEY_A), "down": kn.call(KEY_S), "right": kn.call(KEY_D), "act": kn.call(KEY_E), "stab": kn.call(KEY_Q)}

func update(_dt: float) -> void:
	if Keys.pressed("back"):
		_back.call()
		return
	_menu.handle()
