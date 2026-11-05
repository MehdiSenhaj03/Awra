class_name ResultScreen
extends Screen
## After a leg or a fight: how it went (Ama's health, the supplies, the scavengers downed, the raiders repelled,
## the wrecks salvaged, the fires put out, the weak points struck), the scars it left, and what comes next.

var _menu: UIMenu

func _init(r: Dictionary, change: Dictionary, actions: Array, cfg: Dictionary = {}) -> void:
	super()
	dim(0.62)
	var p := Kit.panel(self)
	var m := Kit.margin(p, 50, 40, 50, 40)
	var box := Kit.vbox(m, 12)
	var over: String = r.get("over", "done")
	var mode: String = r.get("mode", "solo")
	var key := over
	if mode.contains("duel") and over == "lost":
		key = "duel_lost"
	elif mode.contains("duel") and over == "won":
		key = "duel_won"
	if mode == "duel_local":
		key = "p1_won" if over == "won" else "p2_won"
	Kit.label(box, I18n.t("result.%s" % key), 48, Kit.GOLD if over != "lost" else Color("#ff7a5a"), "display", 700, 0.08)
	var flavor := Kit.label(box, I18n.t("result.%s_sub" % key), 18, Kit.SOFT, "deco", 400, 0.02)
	flavor.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	flavor.custom_minimum_size.x = 780
	var st: Dictionary = r.get("stats", {})
	var grid := GridContainer.new()
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", 60)
	grid.add_theme_constant_override("v_separation", 6)
	box.add_child(grid)
	var row := func(label: String, val: String) -> void:
		Kit.label(grid, label, 17, Kit.IVORY, "body", 400)
		Kit.label(grid, val, 17, Kit.GOLD, "mono", 600)
	row.call(I18n.t("result.hp"), "%d / %d" % [int(r.get("hp", 0)), March.HP])
	if not mode.contains("duel"):
		row.call(I18n.t("result.supplies"), str(int(r.get("supplies", 0))))
		row.call(I18n.t("result.birds"), str(int(st.get("birds", 0))))
		row.call(I18n.t("result.skiffs"), str(int(st.get("skiffs", 0))))
		row.call(I18n.t("result.boarders"), str(int(st.get("boarders", 0))))
		row.call(I18n.t("result.salvage"), str(int(st.get("salvage", 0))))
		row.call(I18n.t("result.fires"), str(int(st.get("fires", 0))))
	if int(st.get("weak", 0)) > 0:
		row.call(I18n.t("result.weak"), str(int(st.get("weak", 0))))
	row.call(I18n.t("result.time"), "%d:%02d" % [int(r.get("time", 0)) / 60, int(r.get("time", 0)) % 60])
	if int(change.get("scars", 0)) > 0:
		var sc := Kit.label(box, I18n.t("result.scars", {"n": int(change.scars)}), 16, Color("#e0a080"), "body", 600)
		sc.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		sc.custom_minimum_size.x = 780
	if change.get("new_gen", false):
		Kit.label(box, I18n.t("result.new_gen"), 18, Kit.CREAM, "deco", 400)
	_menu = UIMenu.new(box)
	for a: Dictionary in actions:
		_menu.add(a.label, a.action, {"size": 20, "width": 780})
	p.ready.connect(func() -> void: p.position = ((Vector2(Kit.W, Kit.H) - p.size) / 2).round())

func update(_dt: float) -> void:
	_menu.handle()
