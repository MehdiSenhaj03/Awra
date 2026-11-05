class_name OnlineScreen
extends Screen
## Online: one hosts, the other joins by address; the host picks an expedition together (two crew members on Ama:
## a land, with or without a wild titan at the end) or a duel of titans (each pilots one).

var net: Online
var _status: Label
var _addr: LineEdit
var _menu: UIMenu
var _box: VBoxContainer

func _init(a: Node) -> void:
	super()
	app = a
	Kit.background(self, "duel", 3.5, 0.3)
	var p := Kit.panel(self)
	var m := Kit.margin(p, 44, 36, 44, 36)
	_box = Kit.vbox(m, 14)
	Kit.label(_box, I18n.t("online.title"), 56, Kit.GOLD, "display", 700, 0.06)
	var rules := Kit.label(_box, I18n.t("online.rules"), 17, Kit.SOFT, "body", 400)
	rules.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	rules.custom_minimum_size.x = 860
	_status = Kit.label(_box, "", 18, Kit.BRASS, "body", 700)
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.custom_minimum_size.x = 860
	_addr = LineEdit.new()
	_addr.placeholder_text = I18n.t("online.address")
	_addr.custom_minimum_size = Vector2(860, 46)
	_addr.add_theme_font_override("font", Kit.font("mono", 400, 0.0, 18))
	_addr.add_theme_font_size_override("font_size", 18)
	_box.add_child(_addr)
	_menu = UIMenu.new(_box)
	_menu.add(I18n.t("online.host"), _host, {"size": 19, "width": 860})
	_menu.add(I18n.t("online.join"), _join, {"size": 19, "width": 860})
	_menu.add(I18n.t("common.back"), _back, {"size": 19, "width": 860})
	p.ready.connect(func() -> void: p.position = ((Vector2(Kit.W, Kit.H) - p.size) / 2).round())
	if app.online == null:
		app.online = Online.new()
		app.add_child(app.online)
	net = app.online
	net.connected.connect(_on_connected)
	net.disconnected.connect(func() -> void:
		if is_instance_valid(_status):
			_status.text = I18n.t("online.lost"))
	net.message.connect(_on_message)

func _back() -> void:
	net.close()
	app.to_title()

func _host() -> void:
	if not net.host():
		_status.text = I18n.t("online.failed")
		return
	_status.text = I18n.t("online.waiting", {"addr": net.local_addresses()})

func _join() -> void:
	if _addr.text.strip_edges() == "" or not net.join(_addr.text):
		_status.text = I18n.t("online.failed")
		return
	_status.text = "…"

func _on_connected() -> void:
	if not is_instance_valid(_status):
		return
	_status.text = I18n.t("online.connected")
	Sound.sfx("uiOk")
	_menu.root.queue_free()
	_menu = UIMenu.new(_box)
	if net.is_host:
		_status.text += "  " + I18n.t("online.pick")
		for kind: String in ["ash", "ruins", "forest", "storm", "oasis"]:
			var k := kind
			_menu.add(I18n.t("online.coop", {"land": I18n.t("land.%s.name" % k)}), func() -> void:
				app.online_start({"mode": "net_coop", "kind": k, "level": 2, "boss": true, "seed": randi() % 100000}), {"size": 19, "width": 860})
		_menu.add(I18n.t("online.duel"), func() -> void: app.online_start({"mode": "net_duel", "kind": "ash", "level": 2, "seed": randi() % 100000}), {"size": 19, "width": 860})
	else:
		_status.text += "  " + I18n.t("online.wait_host")
	_menu.add(I18n.t("common.back"), _back, {"size": 19, "width": 860})

func _on_message(kind: String, data: Variant) -> void:
	if kind == "upnp" and is_instance_valid(_status):
		_status.text = I18n.t("online.waiting", {"addr": net.local_addresses()}) + "\n" + I18n.t("online.upnp", {"s": data})

func update(_dt: float) -> void:
	if _addr.has_focus():
		return
	if Keys.pressed("back"):
		_back()
		return
	_menu.handle()
