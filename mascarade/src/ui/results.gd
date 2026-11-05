class_name ResultsScreen
extends Screen
## After the ball: the winner's portrait, how it ended, the numbers, what next.

var _menu: UIMenu

func _init(r: Dictionary, actions: Array) -> void:
	super()
	dim(0.8)
	var won: bool = r.won
	var party: bool = r.get("mode", "contract") == "party"
	var who := "corbeau" if r.winner == "spy" else "lynx"
	if party:
		who = "dogesse"
	var pt := Kit.image(self, Kit.PORTRAIT[who], false)
	pt.position = Vector2(Kit.W * 0.08, Kit.H * 0.1)
	pt.size = Vector2(760, 860)
	pt.modulate.a = 0.0
	var tw := pt.create_tween().set_parallel()
	tw.tween_property(pt, "modulate:a", 1.0, 0.8)
	tw.tween_property(pt, "position:x", Kit.W * 0.1, 1.2).from(Kit.W * 0.06).set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
	var list := Kit.vbox(self, 0)
	list.position = Vector2(Kit.W * 0.52, Kit.H * 0.2)
	list.custom_minimum_size.x = 620
	var head := I18n.t("results.party", {"n": int(r.winner.substr(1)) + 1}) if party else (I18n.t("results.won") if won else I18n.t("results.lost"))
	Kit.label(list, head, 62, Kit.GOLD if won or party else Color("#e06a6a"), "deco", 700, 0.06, Color(0.6, 0.1, 0.05, 0.5), 18)
	var why := Kit.label(list, I18n.t("ball.party_why_%s" % r.why) if party else I18n.t("ball.why_%s_%s" % [r.winner, r.why]), 22, Kit.SOFT, "body", 600)
	why.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	why.custom_minimum_size.x = 620
	Kit.rect(list, Color(0, 0, 0, 0)).custom_minimum_size.y = 24
	var f: int = r.frame
	var rows: Array = [[I18n.t("results.time"), "%d:%02d" % [f / 3600, (f / 60) % 60]]]
	if party:
		var sc: Array = r.scores
		for i in sc.size():
			rows.append([I18n.t("ball.player", {"n": i + 1}), "✦ %d" % sc[i]])
	else:
		rows.append([I18n.t("results.missions"), "%d / %d" % [r.missions, r.need]])
		rows.append([I18n.t("results.shot"), I18n.t("results.shot_yes") if r.shot else I18n.t("results.shot_no")])
	for i in rows.size():
		var rr: Array = rows[i]
		var row := Control.new()
		row.custom_minimum_size = Vector2(620, 46)
		row.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var k := Kit.label(row, rr[0], 19, Kit.SOFT, "body", 600)
		k.position = Vector2(0, 10)
		var v := Kit.label(row, rr[1], 22, Kit.IVORY, "display", 700)
		v.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		v.position = Vector2(220, 8)
		v.size = Vector2(400, 40)
		Kit.rect(row, Color(1, 0.8, 0.5, 0.12), Vector2(0, 45), Vector2(620, 1))
		list.add_child(row)
		row.modulate.a = 0.0
		var t2 := row.create_tween()
		t2.tween_interval(0.4 + i * 0.12)
		t2.tween_property(row, "modulate:a", 1.0, 0.3)
	Kit.rect(list, Color(0, 0, 0, 0)).custom_minimum_size.y = 30
	_menu = UIMenu.new(list)
	for a: Dictionary in actions:
		_menu.add(a.label, a.action, {"size": 20})
	Sound.sfx("ui")

func update(_dt: float) -> void:
	_menu.handle()
