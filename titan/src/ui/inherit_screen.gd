class_name InheritScreen
extends Screen
## Between two generations: years have passed on Ama's back. Each elder hands their post to an heir, who keeps the
## elder's gift or brings one of their own.

var run: Dictionary
var _menu: UIMenu
var _done: Callable
var _opts: Array = []         # per lineage: [gift a, gift b]
var _pick: Array = [0, 0, 0, 0]
var _gift_labels: Array[Label] = []

func _init(r: Dictionary, done: Callable) -> void:
	super()
	run = r
	_done = done
	Kit.background(self, "bg_title", 2.0, 0.38)
	var g := Meta.gen(run)
	var t := Kit.label(self, I18n.t("inherit.title", {"gen": I18n.t("gen.%d" % g)}), 48, Kit.GOLD, "display", 700, 0.08, Color(0, 0, 0, 0.8), 14)
	t.position = Vector2(0, 50)
	t.size = Vector2(Kit.W, 60)
	t.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	var sub := Kit.label(self, I18n.t("inherit.sub.%d" % g), 19, Kit.CREAM, "deco", 400, 0.02, Color(0, 0, 0, 0.8), 10)
	sub.position = Vector2(260, 120)
	sub.size = Vector2(Kit.W - 520, 70)
	sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	sub.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	for k in 4:
		var l: Dictionary = Meta.LINES[k]
		var x := 150.0 + k * 410.0
		var old_name: String = l.names[g - 1]
		var new_name: String = l.names[g]
		var elder := Kit.image(self, l.elder if g == 1 else l.pt[g - 1], true)
		elder.position = Vector2(x, 220)
		elder.size = Vector2(150, 190)
		elder.modulate = Color(0.75, 0.68, 0.6, 0.85)
		var heir := Kit.image(self, l.pt[g], true)
		heir.position = Vector2(x + 130, 250)
		heir.size = Vector2(210, 270)
		var arrow := Kit.label(self, "➜", 30, Kit.GOLD, "body")
		arrow.position = Vector2(x + 96, 420)
		var nm := Kit.label(self, new_name, 26, Deck.LINEAGE[k].lightened(0.45), "display", 700, 0.08, Color(0, 0, 0, 0.8), 8)
		nm.position = Vector2(x + 130, 530)
		var role := Kit.label(self, I18n.t("post.%d" % l.role) + "  ·  " + I18n.t("inherit.from", {"name": old_name}), 14, Kit.SOFT, "body", 400, 0.0, Color(0, 0, 0, 0.8), 6)
		role.position = Vector2(x + 130, 568)
		var parent_gift: String = run.gifts[k]
		var other: String = l.gift if parent_gift != l.gift else (["stout", "brave"][k % 2])
		_opts.append([parent_gift, other])
		var gl := Kit.label(self, "", 15, Kit.IVORY, "body", 600, 0.0, Color(0, 0, 0, 0.85), 8)
		gl.position = Vector2(x, 610)
		gl.size = Vector2(380, 90)
		gl.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		_gift_labels.append(gl)
	var mb := Kit.vbox(self, 2)
	mb.position = Vector2(Kit.W / 2.0 - 380, 730)
	_menu = UIMenu.new(mb, false, 2)
	for k in 4:
		var kk := k
		var it := _menu.add(I18n.t("inherit.gift_of", {"name": Meta.LINES[k].names[g]}), Callable(), {"size": 16, "width": 760, "value": ""})
		var flip := func(_d: int = 0) -> void:
			_pick[kk] = 1 - int(_pick[kk])
			_refresh()
		it.action = flip
		it.adjust = flip
	_menu.add(I18n.t("inherit.go"), _go, {"size": 22, "width": 760})
	_refresh()

func _refresh() -> void:
	for k in 4:
		var gift: String = _opts[k][_pick[k]]
		var inherited := int(_pick[k]) == 0
		_menu.items[k].set_value(I18n.t("gift.%s.name" % gift))
		_gift_labels[k].text = (I18n.t("inherit.kept") if inherited else I18n.t("inherit.own")) + " " + I18n.t("gift.%s.name" % gift) + "\n" + I18n.t("gift.%s.desc" % gift)

func _go() -> void:
	for k in 4:
		run.gifts[k] = _opts[k][_pick[k]]
	Sound.sfx("bell")
	_done.call()

func update(_dt: float) -> void:
	_menu.handle()
