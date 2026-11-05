class_name OnlineScreen
extends Screen
## Host or join, then pick co-op (one vault, two thieves in lockstep) or duel (crack each other's vault).


var net: Online
var _status: Label
var _addr: LineEdit
var _menu: UIMenu
var _box: VBoxContainer
var _mode := ""
var _my_vault: Dictionary = {}
var _their_vault: Dictionary = {}

func _init(a: Node) -> void:
	super()
	app = a
	Kit.background(self, "bg_casino", 3.5, 0.3)
	var p := Kit.panel(self)
	var m := Kit.margin(p, 40, 34, 40, 34)
	_box = Kit.vbox(m, 14)
	Kit.label(_box, I18n.t("online.title"), 44, Kit.CREAM, "display", 400, 0.1)
	_status = Kit.label(_box, "", 17, Kit.BRASS, "body", 600)
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.custom_minimum_size.x = 820
	_addr = LineEdit.new()
	_addr.placeholder_text = I18n.t("online.address")
	_addr.custom_minimum_size = Vector2(820, 44)
	_addr.add_theme_font_override("font", Kit.font("mono", 400, 0.0, 18))
	_addr.add_theme_font_size_override("font_size", 18)
	_box.add_child(_addr)
	_menu = UIMenu.new(_box)
	_menu.add(I18n.t("online.host"), _host, {"size": 18, "width": 820})
	_menu.add(I18n.t("online.join"), _join, {"size": 18, "width": 820})
	_menu.add(I18n.t("common.back"), _back, {"size": 18, "width": 820})
	p.ready.connect(func() -> void: p.position = ((Vector2(Kit.W, Kit.H) - p.size) / 2).round())
	if app.online == null:
		app.online = Online.new()
		app.add_child(app.online)
	net = app.online
	net.connected.connect(_on_connected)
	net.disconnected.connect(func() -> void: _status.text = I18n.t("online.lost"))
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
	_status.text = I18n.t("online.connected")
	Sound.sfx("uiOk")
	if net.is_host:
		_menu.root.queue_free()
		_menu = UIMenu.new(_box)
		_menu.add(I18n.t("online.mode_coop"), func() -> void: _pick("coop"), {"size": 18, "width": 820})
		_menu.add(I18n.t("online.mode_duel"), func() -> void: _pick("duel"), {"size": 18, "width": 820})
	else:
		_status.text = I18n.t("online.connected") + "  " + I18n.t("online.duel_wait")

## Pick a vault: open campaign vaults and validated workshop vaults.
func _pick(mode: String) -> void:
	_mode = mode
	if net.is_host:
		net.send("mode", mode)
	_menu.root.queue_free()
	_menu = UIMenu.new(_box)
	_status.text = I18n.t("online.duel_pick") if mode == "duel" else I18n.t("coop.pick")
	var n := 0
	for id: String in Meta.order():
		if Meta.is_open(app.save, id) and n < 9:
			var lv := Meta.level(id)
			lv.data.name = I18n.t("vault.%s" % id)
			var d := lv.data
			_menu.add("%s  %s" % [id, d.name], func() -> void: _chosen(d), {"size": 15, "width": 820})
			n += 1
	for d: Dictionary in app.save.workshop:
		if d.get("validated", false):
			var dd := d
			_menu.add("✎  %s" % d.name, func() -> void: _chosen(dd), {"size": 15, "width": 820})

func _chosen(d: Dictionary) -> void:
	if _mode == "coop":
		net.send("coop", d)
		_start_coop(d)
	else:
		_my_vault = d
		net.send("duel_vault", d)
		_status.text = I18n.t("online.duel_wait")
		_maybe_duel()

func _on_message(kind: String, data: Variant) -> void:
	match kind:
		"upnp":
			_status.text = I18n.t("online.waiting", {"addr": net.local_addresses()}) + "\n" + I18n.t("online.upnp", {"s": data})
		"mode":
			_pick(data)
		"coop":
			_start_coop(data)
		"duel_vault":
			_their_vault = data
			_maybe_duel()

func _start_coop(d: Dictionary) -> void:
	net.reset_lockstep()
	app.start_online_coop(Level.new(d))

func _maybe_duel() -> void:
	if _my_vault.is_empty() or _their_vault.is_empty():
		return
	app.start_duel(Level.new(_their_vault))

func update(_dt: float) -> void:
	if _addr.has_focus():
		return
	if Keys.pressed("back"):
		_back()
		return
	_menu.handle()
